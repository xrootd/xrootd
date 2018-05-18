//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <fcntl.h>
#include <sstream>
#include <algorithm>
#include <sys/statvfs.h>

#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysTrace.hh"

#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"
#include "XrdFileCacheInfo.hh"
#include "XrdFileCacheIOEntireFile.hh"
#include "XrdFileCacheIOFileBlock.hh"

using namespace XrdFileCache;

Cache * Cache::m_factory = NULL;

XrdScheduler *Cache::schedP = NULL;


void *CacheDirCleanupThread(void* cache_void)
{
   Cache::GetInstance().CacheDirCleanup();
   return NULL;
}

void *ProcessWriteTaskThread(void* c)
{
   Cache *cache = static_cast<Cache*>(c);
   cache->ProcessWriteTasks();
   return NULL;
}

void *PrefetchThread(void* ptr)
{
   Cache* cache = static_cast<Cache*>(ptr);
   cache->Prefetch();
   return NULL;
}

extern "C"
{
XrdOucCache2 *XrdOucGetCache2(XrdSysLogger *logger,
                              const char   *config_filename,
                              const char   *parameters)
{
   XrdSysError err(0, "");
   err.logger(logger);
   err.Emsg("Retrieve", "Retrieving a caching proxy factory.");
   Cache &factory = Cache::GetInstance();
   if (! factory.Config(logger, config_filename, parameters))
   {
      err.Emsg("Retrieve", "Error - unable to create a factory.");
      return NULL;
   }
   err.Emsg("Retrieve", "Success - returning a factory.");

   pthread_t tid1;
   XrdSysThread::Run(&tid1, ProcessWriteTaskThread, (void*)(&factory), 0, "XrdFileCache WriteTasks ");

   pthread_t tid2;
   XrdSysThread::Run(&tid2, PrefetchThread, (void*)(&factory), 0, "XrdFileCache Prefetch ");

   pthread_t tid;
   XrdSysThread::Run(&tid, CacheDirCleanupThread, NULL, 0, "XrdFileCache CacheDirCleanup");
   
   
   return &factory;
}
}

Cache &Cache::GetInstance()
{
   if (m_factory == NULL)
      m_factory = new Cache();
   return *m_factory;
}

class DiskSyncer : public XrdJob
{
private:
File *m_file;
public:
DiskSyncer(File *f, const char *desc = "") :
   XrdJob(desc),
   m_file(f)
{}
void DoIt()
{
   m_file->Sync();
   Cache::GetInstance().FileSyncDone(m_file);
   delete this;
}
};

bool Cache::Decide(XrdOucCacheIO* io)
{
   if (! m_decisionpoints.empty())
   {
      XrdCl::URL url(io->Path());
      std::string filename = url.GetPath();
      std::vector<Decision*>::const_iterator it;
      for (it = m_decisionpoints.begin(); it != m_decisionpoints.end(); ++it)
      {
         XrdFileCache::Decision *d = *it;
         if (! d) continue;
         if (! d->Decide(filename, *m_output_fs))
         {
            return false;
         }
      }
   }

   return true;
}

Cache::Cache() :
   XrdOucCache(),
   m_log(0, "XrdFileCache_"),
   m_trace(0),
   m_traceID("Manager"),
   m_prefetch_condVar(0),
   m_RAMblocks_used(0),
   m_isClient(false),
   m_in_purge(false),
   m_active_cond(0)
{
   m_trace = new XrdSysTrace("XrdFileCache");
   // default log level is Warning
   m_trace->What = 2;
}


XrdOucCacheIO2 *Cache::Attach(XrdOucCacheIO2 *io, int Options)
{
   if (Cache::GetInstance().Decide(io))
   {
      TRACE(Info, "Cache::Attach() " << io->Path());
      IO* cio;
      if (Cache::GetInstance().RefConfiguration().m_hdfsmode)
         cio = new IOFileBlock(io, m_stats, *this);
      else
         cio = new IOEntireFile(io, m_stats, *this);

      TRACE_PC(Debug, const char* loc = io->Location(),
               "Cache::Attach() " << io->Path() << " location: " <<
               ((loc && loc[0] != 0) ? loc : "<deferred open>"));
      return cio;
   }
   else
   {
      TRACE(Info, "Cache::Attach() decision decline " << io->Path());
   }
   return io;
}


int Cache::isAttached()
{
   // virutal function of XrdOucCache, don't see it used in pfc or posix layer
   return true;
}


void Cache::AddWriteTask(Block* b, bool fromRead)
{
   TRACE(Dump, "Cache::AddWriteTask() bOff=%ld " <<  b->m_offset);
   m_writeQ.condVar.Lock();
   if (fromRead)
      m_writeQ.queue.push_back(b);
   else
      m_writeQ.queue.push_front(b);
   m_writeQ.size++;
   m_writeQ.condVar.Signal();
   m_writeQ.condVar.UnLock();
}


void Cache::RemoveWriteQEntriesFor(File *iFile)
{
   m_writeQ.condVar.Lock();
   std::list<Block*>::iterator i = m_writeQ.queue.begin();
   while (i != m_writeQ.queue.end())
   {
      if ((*i)->m_file == iFile)
      {
         TRACE(Dump, "Cache::Remove entries for " <<  (void*)(*i) << " path " <<  iFile->lPath());
         std::list<Block*>::iterator j = i++;
         iFile->BlockRemovedFromWriteQ(*j);
         m_writeQ.queue.erase(j);
         --m_writeQ.size;
      }
      else
      {
         ++i;
      }
   }
   m_writeQ.condVar.UnLock();
}


void Cache::ProcessWriteTasks()
{
   while (true)
   {
      m_writeQ.condVar.Lock();
      while (m_writeQ.queue.empty())
      {
         m_writeQ.condVar.Wait();
      }
      Block* block = m_writeQ.queue.front();
      m_writeQ.queue.pop_front();
      m_writeQ.size--;
      TRACE(Dump, "Cache::ProcessWriteTasks  for %p " <<  (void*)(block) << " path " << block->m_file->lPath());
      m_writeQ.condVar.UnLock();

      block->m_file->WriteBlockToDisk(block);
   }
}


bool Cache::RequestRAMBlock()
{
   XrdSysMutexHelper lock(&m_RAMblock_mutex);
   if ( m_RAMblocks_used < Cache::GetInstance().RefConfiguration().m_NRamBuffers )
   {
      m_RAMblocks_used++;
      return true;
   }
   return false;
}


void Cache::RAMBlockReleased()
{
   XrdSysMutexHelper lock(&m_RAMblock_mutex);
   m_RAMblocks_used--;
}


File* Cache::GetFile(const std::string& path, IO* iIO, long long off, long long filesize)
{
   // Called from virtual IO::Attach
   
   TRACE(Debug, "Cache::GetFile " << path);
   
   {
      XrdSysCondVarHelper lock(&m_active_cond);

      while (true)
      {
         ActiveMap_i it = m_active.find(path);

         // File is not open or being opened. Mark it as being opened and
         // proceed to opening it outside of while loop.
         if (it == m_active.end())
         {
            m_active[path] = 0;
            break;
         }

         if (it->second != 0)
         {
            IO* prevIO = it->second->SetIO(iIO);
            if (prevIO)
            {
               prevIO->RelinquishFile(it->second);
            }
            else
            {
               inc_ref_cnt(it->second, false);
            }
            return it->second;
         }
         else
         {
            // Wait for some change in m_active, then recheck.
            m_active_cond.Wait();
         }
      }
   }

   if (filesize == 0)
   {
      struct stat st;
      int res = iIO->Fstat(st);
      if (res) {
         TRACE(Error, "Cache::GetFile, could not get valid stat");
         return 0;
      }

      filesize = st.st_size;
   }

   File* file = new File(iIO, path, off, filesize);

   {
      XrdSysCondVarHelper lock(&m_active_cond);

      inc_ref_cnt(file, false);
      m_active[file->GetLocalPath()] = file;

      m_active_cond.Broadcast();
   }

   return file;
}


void Cache::ReleaseFile(File* f)
{
   // Called from virtual IO::Detach
   
   TRACE(Debug, "Cache::ReleaseFile " << f->GetLocalPath());
   
   f->ReleaseIO();
   dec_ref_cnt(f);
}

  
namespace
{
void *callDoIt(void *pp)
{
     XrdJob *jP = (XrdJob *)pp;
     jP->DoIt();
     return (void *)0;
}
};


void Cache::schedule_file_sync(File* f, bool ref_cnt_already_set)
{
   DiskSyncer* ds = new DiskSyncer(f);
   if ( ! ref_cnt_already_set) inc_ref_cnt(f, true);
   if (m_isClient) ds->DoIt();
      else if (schedP) schedP->Schedule(ds);
              else {pthread_t tid;
                    XrdSysThread::Run(&tid, callDoIt, ds, 0, "DiskSyncer");
                   }
}


void Cache::FileSyncDone(File* f)
{
   dec_ref_cnt(f);
}


void Cache::inc_ref_cnt(File* f, bool lock)
{
   // called from GetFile() or SheduleFileSync();
   
   TRACE(Debug, "Cache::inc_ref_cnt " << f->GetLocalPath());
   if (lock) m_active_cond.Lock();
   f->inc_ref_cnt();
   if (lock) m_active_cond.UnLock();
   
}


void Cache::dec_ref_cnt(File* f)
{
   // called  from ReleaseFile() or DiskSync callback
   
   m_active_cond.Lock();
   int cnt = f->get_ref_cnt();
   m_active_cond.UnLock();

   TRACE(Debug, "Cache::dec_ref_cnt " << f->GetLocalPath() << ", cnt at entry = " << cnt);

   if (cnt == 1)
   {
      if (f->FinalizeSyncBeforeExit())
      {
         TRACE(Debug, "Cache::dec_ref_cnt " << f->GetLocalPath() << ", scheduling final sync");
         schedule_file_sync(f, true);
         return;
      }
   }

   m_active_cond.Lock();
   cnt = f->dec_ref_cnt();
   TRACE(Debug, "Cache::dec_ref_cnt " << f->GetLocalPath() << ", cnt after sync_check and dec_ref_cnt = " << cnt);
   if (cnt == 0)
   {
      ActiveMap_i it = m_active.find(f->GetLocalPath());
      m_active.erase(it);
      delete f;
   }
   m_active_cond.UnLock();
}


bool Cache::IsFileActiveOrPurgeProtected(const std::string& path)
{
   XrdSysCondVarHelper lock(&m_active_cond);

   return m_active.find(path)          != m_active.end() ||
          m_purge_delay_set.find(path) != m_purge_delay_set.end();
}


//==============================================================================
//=== PREFETCH
//==============================================================================

void Cache::RegisterPrefetchFile(File* file)
{
   //  called from File::Open()

   if (Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
   {
      m_prefetch_condVar.Lock();
      m_prefetchList.push_back(file);
      m_prefetch_condVar.Signal();
      m_prefetch_condVar.UnLock();
   }
}


void Cache::DeRegisterPrefetchFile(File* file)
{
   //  called from last line File::InitiateClose()

   m_prefetch_condVar.Lock();
   for (PrefetchList::iterator it = m_prefetchList.begin(); it != m_prefetchList.end(); ++it)
   {
      if (*it == file)
      {
         m_prefetchList.erase(it);
         break;
      }
   }
   m_prefetch_condVar.UnLock();
}


File* Cache::GetNextFileToPrefetch()
{
   m_prefetch_condVar.Lock();
   while (m_prefetchList.empty())
   {
      m_prefetch_condVar.Wait();
   }

   //  std::sort(m_prefetchList.begin(), m_prefetchList.end(), myobject);

   size_t l = m_prefetchList.size();
   int idx = rand() % l;
   File* f = m_prefetchList[idx];

   m_prefetch_condVar.UnLock();
   return f;
}


void Cache::Prefetch()
{
   int limitRAM = int( Cache::GetInstance().RefConfiguration().m_NRamBuffers * 0.7 );
   while (true)
   {
      m_RAMblock_mutex.Lock();
      bool doPrefetch = (m_RAMblocks_used < limitRAM);
      m_RAMblock_mutex.UnLock();

      if (doPrefetch)
      {
         File* f = GetNextFileToPrefetch();
         f->Prefetch();
      }
      else
      {
         XrdSysTimer::Wait(5);
      }
   }
}


//==============================================================================
//=== Virtuals from XrdOucCache2
//==============================================================================

//------------------------------------------------------------------------------
//! Get the path to a file that is complete in the local cache. By default, the
//! file must be complete in the cache (i.e. no blocks are missing). This can
//! be overridden. This path can be used to access the file on the local node.
//!
//! @return 0      - the file is complete and the local path to the file is in
//!                  the buffer, if it has been supllied.
//!
//! @return <0     - the request could not be fulfilled. The return value is
//!                  -errno describing why. If a buffer was supplied and a
//!                  path could be generated it is returned only if "why" is
//!                  ForCheck or ForInfo. Otherwise, a null path is returned.
//!
//! @return >0     - Reserved for future use.

int Cache::LocalFilePath(const char *curl, char *buff, int blen, LFP_Reason why)
{
   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();
   std::string i_name = f_name + ".cinfo";
   {
      XrdSysCondVarHelper lock(&m_active_cond);
      m_purge_delay_set.insert(f_name);
   }

   if (buff && blen > 0) buff[0] = 0;

   struct stat sbuff, sbuff2;
   if (m_output_fs->Stat(f_name.c_str(), &sbuff)  == XrdOssOK &&
       m_output_fs->Stat(i_name.c_str(), &sbuff2) == XrdOssOK)
   {
      if ( S_ISDIR(sbuff.st_mode))
      {
         return -EISDIR; // Andy ... this violates ForInfo docs.
                         // In fact, ForInfo is actually silly.
      }
      else
      {
         bool read_ok     = false;
         bool is_complete = false;

         // Lock and check if file is active. If NOT, keep the lock
         // and add dummy access after successful reading of info file.
         // If it IS active, just release the lock, this ongoing access will
         // assure the file continues to exist.

         m_active_cond.Lock();

         bool is_active = m_active.find(f_name) != m_active.end();

         if (is_active) m_active_cond.UnLock();

         XrdOssDF* infoFile = m_output_fs->newFile(m_configuration.m_username.c_str());
         XrdOucEnv myEnv;
         int res = infoFile->Open(i_name.c_str(), O_RDWR, 0600, myEnv);
         if (res >= 0)
         {
            Info info(m_trace, 0);
            if (info.Read(infoFile, i_name))
            {
               read_ok = true;

               is_complete = info.IsComplete();

               // Add full-size access if reason is for access.
               if ( ! is_active && is_complete && why == ForAccess)
               {
                  info.WriteIOStatSingle(info.GetFileSize());
                  info.Write(infoFile);
               }
            }
            infoFile->Close();
         }
         delete infoFile;

         if ( ! is_active) m_active_cond.UnLock();

         if (read_ok)
         {
            if (is_complete || why != ForAccess)
            {
               int res2 = m_output_fs->Lfn2Pfn(f_name.c_str(), buff, blen);
               if (res2 < 0)
                  return res2;
            }

            if (is_complete || why == ForInfo) return 0;

            return -EREMOTE;
         }
      }
   }

   return -ENOENT;
}


//______________________________________________________________________________
//! Preapare the cache for a file open request. This method is called prior to
//! actually opening a file. This method is meant to allow defering an open
//! request or implementing the full I/O stack in the cache layer.
//! @return <0 Error has occurred, return value is -errno; fail open request.
//!         =0 Continue with open() request.
//!         >0 Defer open but treat the file as actually being open. Use the
//!            XrdOucCacheIO2::Open() method to open the file at a later time.
//------------------------------------------------------------------------------

int Cache::Prepare(const char *curl, int oflags, mode_t mode)
{
   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();
   std::string i_name = f_name + ".cinfo";

   {
      XrdSysCondVarHelper lock(&m_active_cond);
      m_purge_delay_set.insert(f_name);
   }

   struct stat sbuff;
   int res = m_output_fs->Stat(i_name.c_str(), &sbuff);
   if (res == 0)
   {
      TRACE( Dump, "Cache::Prefetch defer open " << f_name);
      return 1;
   }
   else
   {
      return 0;
   }
}

//______________________________________________________________________________
// virtual method of XrdOucCache2::Stat()
//!
//! @return <0 - Stat failed, value is -errno.
//!         =0 - Stat succeeded, sbuff holds stat information.
//!         >0 - Stat could not be done, forward operation to next level.
//------------------------------------------------------------------------------

int Cache::Stat(const char *curl, struct stat &sbuff)
{
   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();
   std::string i_name = f_name + ".cinfo";

   {
      XrdSysCondVarHelper lock(&m_active_cond);
      m_purge_delay_set.insert(f_name);
   }

   if (m_output_fs->Stat(f_name.c_str(), &sbuff) == XrdOssOK)
   {
      if ( S_ISDIR(sbuff.st_mode))
      {
         return 0;
      }
      else
      {
         bool success = false;
         XrdOssDF* infoFile = m_output_fs->newFile(m_configuration.m_username.c_str());
         XrdOucEnv myEnv;
         int res = infoFile->Open(i_name.c_str(), O_RDONLY, 0600, myEnv);
         if (res >= 0)
         {
            Info info(m_trace, 0);
            if (info.Read(infoFile, i_name))
            {
               sbuff.st_size = info.GetFileSize();
               success = true;
            }
         }
         infoFile->Close();
         delete infoFile;
         return success ? 0 : 1;
      }
   }

   return 1;
}
