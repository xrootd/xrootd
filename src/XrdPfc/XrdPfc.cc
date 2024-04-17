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

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysTrace.hh"

#include "XrdXrootd/XrdXrootdGStream.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"
#include "XrdPfcFSctl.hh"
#include "XrdPfcInfo.hh"
#include "XrdPfcIOFile.hh"
#include "XrdPfcIOFileBlock.hh"

using namespace XrdPfc;

Cache * Cache::m_instance = 0;

XrdScheduler *Cache::schedP = 0;


void *ResourceMonitorHeartBeatThread(void*)
{
   Cache::GetInstance().ResourceMonitorHeartBeat();
   return 0;
}

void *PurgeThread(void*)
{
   Cache::GetInstance().Purge();
   return 0;
}

void *ProcessWriteTaskThread(void*)
{
   Cache::GetInstance().ProcessWriteTasks();
   return 0;
}

void *PrefetchThread(void*)
{
   Cache::GetInstance().Prefetch();
   return 0;
}

//==============================================================================

extern "C"
{
XrdOucCache *XrdOucGetCache(XrdSysLogger *logger,
                            const char   *config_filename,
                            const char   *parameters,
                            XrdOucEnv    *env)
{
   XrdSysError err(logger, "");
   err.Say("++++++ Proxy file cache initialization started.");

   if ( ! env ||
        ! (XrdPfc::Cache::schedP = (XrdScheduler*) env->GetPtr("XrdScheduler*")))
   {
      XrdPfc::Cache::schedP = new XrdScheduler;
      XrdPfc::Cache::schedP->Start();
   }

   Cache &instance = Cache::CreateInstance(logger, env);

   if (! instance.Config(config_filename, parameters))
   {
      err.Say("Config Proxy file cache initialization failed.");
      return 0;
   }
   err.Say("------ Proxy file cache initialization completed.");

   {
      pthread_t tid;

      for (int wti = 0; wti < instance.RefConfiguration().m_wqueue_threads; ++wti)
      {
         XrdSysThread::Run(&tid, ProcessWriteTaskThread, 0, 0, "XrdPfc WriteTasks ");
      }

      if (instance.RefConfiguration().m_prefetch_max_blocks > 0)
      {
         XrdSysThread::Run(&tid, PrefetchThread, 0, 0, "XrdPfc Prefetch ");
      }

      XrdSysThread::Run(&tid, ResourceMonitorHeartBeatThread, 0, 0, "XrdPfc ResourceMonitorHeartBeat");

      XrdSysThread::Run(&tid, PurgeThread, 0, 0, "XrdPfc Purge");
   }

   XrdPfcFSctl* pfcFSctl = new XrdPfcFSctl(instance, logger);
   env->PutPtr("XrdFSCtl_PC*", pfcFSctl);

   return &instance;
}
}

//==============================================================================

void Configuration::calculate_fractional_usages(long long  du,      long long  fu,
                                                double    &frac_du, double    &frac_fu)
{
  // Calculate fractional disk / file usage and clamp them to [0, 1].

  // Fractional total usage above LWM:
  // - can be > 1 if usage is above HWM;
  // - can be < 0 if triggered via age-based-purging.
  frac_du = (double) (du - m_diskUsageLWM) / (m_diskUsageHWM - m_diskUsageLWM);

  // Fractional file usage above baseline.
  // - can be > 1 if file usage is above max;
  // - can be < 0 if file usage is below baseline.
  frac_fu = (double) (fu - m_fileUsageBaseline) / (m_fileUsageMax - m_fileUsageBaseline);

  frac_du = std::min( std::max( frac_du, 0.0), 1.0 );
  frac_fu = std::min( std::max( frac_fu, 0.0), 1.0 );
}

//==============================================================================

Cache &Cache::CreateInstance(XrdSysLogger *logger, XrdOucEnv *env)
{
   assert (m_instance == 0);
   m_instance = new Cache(logger, env);
   return *m_instance;
}

      Cache&         Cache::GetInstance() { return *m_instance; }
const Cache&         Cache::TheOne()      { return *m_instance; }
const Configuration& Cache::Conf()        { return  m_instance->RefConfiguration(); }

bool Cache::Decide(XrdOucCacheIO* io)
{
   if (! m_decisionpoints.empty())
   {
      XrdCl::URL url(io->Path());
      std::string filename = url.GetPath();
      std::vector<Decision*>::const_iterator it;
      for (it = m_decisionpoints.begin(); it != m_decisionpoints.end(); ++it)
      {
         XrdPfc::Decision *d = *it;
         if (! d) continue;
         if (! d->Decide(filename, *m_oss))
         {
            return false;
         }
      }
   }

   return true;
}

Cache::Cache(XrdSysLogger *logger, XrdOucEnv *env) :
   XrdOucCache("pfc"),
   m_env(env),
   m_log(logger, "XrdPfc_"),
   m_trace(new XrdSysTrace("XrdPfc", logger)),
   m_traceID("Cache"),
   m_oss(0),
   m_gstream(0),
   m_prefetch_condVar(0),
   m_prefetch_enabled(false),
   m_RAM_used(0),
   m_RAM_write_queue(0),
   m_RAM_std_size(0),
   m_isClient(false),
   m_in_purge(false),
   m_active_cond(0),
   m_stats_n_purge_cond(0),
   m_fs_state(0),
   m_last_scan_duration(0),
   m_last_purge_duration(0),
   m_spt_state(SPTS_Idle)
{
   // Default log level is Warning.
   m_trace->What = 2;
}

XrdOucCacheIO *Cache::Attach(XrdOucCacheIO *io, int Options)
{
   const char* tpfx = "Attach() ";

   if (Cache::GetInstance().Decide(io))
   {
      TRACE(Info, tpfx << io->Path());

      IO *cio;

      if (Cache::GetInstance().RefConfiguration().m_hdfsmode)
      {
         cio = new IOFileBlock(io, *this);
      }
      else
      {
         IOFile *iof = new IOFile(io, *this);

         if ( ! iof->HasFile())
         {
           delete iof;
           // TODO - redirect instead. But this is kind of an awkward place for it.
           // errno is set during IOFile construction.
           TRACE(Error, tpfx << "Failed opening local file, falling back to remote access " << io->Path());
           return io;
         }

         cio = iof;
      }

      TRACE_PC(Debug, const char* loc = io->Location(), tpfx << io->Path() << " location: " <<
               ((loc && loc[0] != 0) ? loc : "<deferred open>"));

      return cio;
   }
   else
   {
      TRACE(Info, tpfx << "decision decline " << io->Path());
   }
   return io;
}

void Cache::AddWriteTask(Block* b, bool fromRead)
{
   TRACE(Dump, "AddWriteTask() offset=" <<  b->m_offset << ". file " << b->get_file()->GetLocalPath());

   {
      XrdSysMutexHelper lock(&m_RAM_mutex);
      m_RAM_write_queue += b->get_size();
   }

   m_writeQ.condVar.Lock();
   if (fromRead)
      m_writeQ.queue.push_back(b);
   else
      m_writeQ.queue.push_front(b);
   m_writeQ.size++;
   m_writeQ.condVar.Signal();
   m_writeQ.condVar.UnLock();
}

void Cache::RemoveWriteQEntriesFor(File *file)
{
   std::list<Block*> removed_blocks;
   long long         sum_size = 0;

   m_writeQ.condVar.Lock();
   std::list<Block*>::iterator i = m_writeQ.queue.begin();
   while (i != m_writeQ.queue.end())
   {
      if ((*i)->m_file == file)
      {
         TRACE(Dump, "Remove entries for " <<  (void*)(*i) << " path " <<  file->lPath());
         std::list<Block*>::iterator j = i++;
         removed_blocks.push_back(*j);
         sum_size += (*j)->get_size();
         m_writeQ.queue.erase(j);
         --m_writeQ.size;
      }
      else
      {
         ++i;
      }
   }
   m_writeQ.condVar.UnLock();

   {
      XrdSysMutexHelper lock(&m_RAM_mutex);
      m_RAM_write_queue -= sum_size;
   }

   file->BlocksRemovedFromWriteQ(removed_blocks);
}

void Cache::ProcessWriteTasks()
{
   std::vector<Block*> blks_to_write(m_configuration.m_wqueue_blocks);

   while (true)
   {
      m_writeQ.condVar.Lock();
      while (m_writeQ.size == 0)
      {
         m_writeQ.condVar.Wait();
      }

      // MT -- optimize to pop several blocks if they are available (or swap the list).
      // This makes sense especially for smallish block sizes.

      int       n_pushed = std::min(m_writeQ.size, m_configuration.m_wqueue_blocks);
      long long sum_size = 0;

      for (int bi = 0; bi < n_pushed; ++bi)
      {
         Block* block = m_writeQ.queue.front();
         m_writeQ.queue.pop_front();
         m_writeQ.writes_between_purges += block->get_size();
         sum_size += block->get_size();

         blks_to_write[bi] = block;

         TRACE(Dump, "ProcessWriteTasks for block " <<  (void*)(block) << " path " << block->m_file->lPath());
      }
      m_writeQ.size -= n_pushed;

      m_writeQ.condVar.UnLock();

      {
         XrdSysMutexHelper lock(&m_RAM_mutex);
         m_RAM_write_queue -= sum_size;
      }

      for (int bi = 0; bi < n_pushed; ++bi)
      {
         Block* block = blks_to_write[bi];

         block->m_file->WriteBlockToDisk(block);
      }
   }
}

//==============================================================================

char* Cache::RequestRAM(long long size)
{
   static const size_t s_block_align = sysconf(_SC_PAGESIZE);

   bool  std_size = (size == m_configuration.m_bufferSize);

   m_RAM_mutex.Lock();

   long long total = m_RAM_used + size;

   if (total <= m_configuration.m_RamAbsAvailable)
   {
      m_RAM_used = total;
      if (std_size && m_RAM_std_size > 0)
      {
         char *buf = m_RAM_std_blocks.back();
         m_RAM_std_blocks.pop_back();
         --m_RAM_std_size;

         m_RAM_mutex.UnLock();

         return buf;
      }
      else
      {
         m_RAM_mutex.UnLock();
         char *buf;
         if (posix_memalign((void**) &buf, s_block_align, (size_t) size))
         {
            // Report out of mem? Probably should report it at least the first time,
            // then periodically.
            return 0;
         }
         return buf;
      }
   }
   m_RAM_mutex.UnLock();
   return 0;
}

void Cache::ReleaseRAM(char* buf, long long size)
{
   bool std_size = (size == m_configuration.m_bufferSize);
   {
      XrdSysMutexHelper lock(&m_RAM_mutex);

      m_RAM_used -= size;

      if (std_size && m_RAM_std_size < m_configuration.m_RamKeepStdBlocks)
      {
         m_RAM_std_blocks.push_back(buf);
         ++m_RAM_std_size;
         return;
      }
   }
   free(buf);
}

File* Cache::GetFile(const std::string& path, IO* io, long long off, long long filesize)
{
   // Called from virtual IO::Attach
   
   TRACE(Debug, "GetFile " << path << ", io " << io);

   ActiveMap_i it;

   {
      XrdSysCondVarHelper lock(&m_active_cond);

      while (true)
      {
         it = m_active.find(path);

         // File is not open or being opened. Mark it as being opened and
         // proceed to opening it outside of while loop.
         if (it == m_active.end())
         {
            it = m_active.insert(std::make_pair(path, (File*) 0)).first;
            break;
         }

         if (it->second != 0)
         {
            it->second->AddIO(io);
            inc_ref_cnt(it->second, false, true);

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
      int res = io->Fstat(st);
      if (res < 0) {
         errno = res;
         TRACE(Error, "GetFile, could not get valid stat");
      } else if (res > 0) {
         errno = ENOTSUP;
         TRACE(Error, "GetFile, stat returned positive value, this should NOT happen here");
      } else {
         filesize = st.st_size;
      }
   }

   File *file = 0;

   if (filesize >= 0)
   {
      file = File::FileOpen(path, off, filesize);
   }

   {
      XrdSysCondVarHelper lock(&m_active_cond);

      if (file)
      {
         inc_ref_cnt(file, false, true);
         it->second = file;

         file->AddIO(io);
      }
      else
      {
         m_active.erase(it);
      }

      m_active_cond.Broadcast();
   }

   return file;
}

void Cache::ReleaseFile(File* f, IO* io)
{
   // Called from virtual IO::DetachFinalize.
   
   TRACE(Debug, "ReleaseFile " << f->GetLocalPath() << ", io " << io);
   
   {
     XrdSysCondVarHelper lock(&m_active_cond);

     f->RemoveIO(io);
   }
   dec_ref_cnt(f, true);
}

  
namespace
{

class DiskSyncer : public XrdJob
{
private:
   File *m_file;
   bool  m_high_debug;

public:
   DiskSyncer(File *f, bool high_debug, const char *desc = "") :
      XrdJob(desc),
      m_file(f),
      m_high_debug(high_debug)
   {}

   void DoIt()
   {
      m_file->Sync();
      Cache::GetInstance().FileSyncDone(m_file, m_high_debug);
      delete this;
   }
};


class CommandExecutor : public XrdJob
{
private:
   std::string m_command_url;

public:
   CommandExecutor(const std::string& command, const char *desc = "") :
      XrdJob(desc),
      m_command_url(command)
   {}

   void DoIt()
   {
      Cache::GetInstance().ExecuteCommandUrl(m_command_url);
      delete this;
   }
};

}

//==============================================================================

void Cache::schedule_file_sync(File* f, bool ref_cnt_already_set, bool high_debug)
{
   DiskSyncer* ds = new DiskSyncer(f, high_debug);

   if ( ! ref_cnt_already_set) inc_ref_cnt(f, true, high_debug);

   schedP->Schedule(ds);
}

void Cache::FileSyncDone(File* f, bool high_debug)
{
   dec_ref_cnt(f, high_debug);
}

void Cache::inc_ref_cnt(File* f, bool lock, bool high_debug)
{
   // called from GetFile() or SheduleFileSync();

   int tlvl = high_debug ? TRACE_Debug : TRACE_Dump;

   if (lock) m_active_cond.Lock();
   int rc = f->inc_ref_cnt();
   if (lock) m_active_cond.UnLock();

   TRACE_INT(tlvl, "inc_ref_cnt " << f->GetLocalPath() << ", cnt at exit = " << rc);
}

void Cache::dec_ref_cnt(File* f, bool high_debug)
{
   // Called from ReleaseFile() or DiskSync callback.

   int tlvl = high_debug ? TRACE_Debug : TRACE_Dump;
   int cnt;

   {
     XrdSysCondVarHelper lock(&m_active_cond);

     cnt = f->get_ref_cnt();

     if (f->is_in_emergency_shutdown())
     {
        // In this case file has been already removed from m_active map and
        // does not need to be synced.

        if (cnt == 1)
        {
           TRACE_INT(tlvl, "dec_ref_cnt " << f->GetLocalPath() << " is in shutdown, ref_cnt = " << cnt
                     << " -- deleting File object without further ado");
           delete f;
        }
        else
        {
           TRACE_INT(tlvl, "dec_ref_cnt " << f->GetLocalPath() << " is in shutdown, ref_cnt = " << cnt
                     << " -- waiting");
        }

        return;
     }
   }

   TRACE_INT(tlvl, "dec_ref_cnt " << f->GetLocalPath() << ", cnt at entry = " << cnt);

   if (cnt == 1)
   {
      if (f->FinalizeSyncBeforeExit())
      {
         // Note, here we "reuse" the existing reference count for the
         // final sync.

         TRACE(Debug, "dec_ref_cnt " << f->GetLocalPath() << ", scheduling final sync");
         schedule_file_sync(f, true, true);
         return;
      }
   }

   {
      XrdSysCondVarHelper lock(&m_active_cond);

      cnt = f->dec_ref_cnt();
      TRACE_INT(tlvl, "dec_ref_cnt " << f->GetLocalPath() << ", cnt after sync_check and dec_ref_cnt = " << cnt);
      if (cnt == 0)
      {
         ActiveMap_i it = m_active.find(f->GetLocalPath());
         m_active.erase(it);

         m_closed_files_stats.insert(std::make_pair(f->GetLocalPath(), f->DeltaStatsFromLastCall()));

         if (m_gstream)
         {
            const Stats       &st = f->RefStats();
            const Info::AStat *as = f->GetLastAccessStats();

            char buf[4096];
            int  len = snprintf(buf, 4096, "{\"event\":\"file_close\","
                                 "\"lfn\":\"%s\",\"size\":%lld,\"blk_size\":%d,\"n_blks\":%d,\"n_blks_done\":%d,"
                                 "\"access_cnt\":%lu,\"attach_t\":%lld,\"detach_t\":%lld,\"remotes\":%s,"
                                 "\"b_hit\":%lld,\"b_miss\":%lld,\"b_bypass\":%lld,\"n_cks_errs\":%d}",
                                 f->GetLocalPath().c_str(), f->GetFileSize(), f->GetBlockSize(),
                                 f->GetNBlocks(), f->GetNDownloadedBlocks(),
                                 (unsigned long) f->GetAccessCnt(), (long long) as->AttachTime, (long long) as->DetachTime,
                                 f->GetRemoteLocations().c_str(),
                                 as->BytesHit, as->BytesMissed, as->BytesBypassed, st.m_NCksumErrors
            );
            bool suc = false;
            if (len < 4096)
            {
               suc = m_gstream->Insert(buf, len + 1);
            }
            if ( ! suc)
            {
               TRACE(Error, "Failed g-stream insertion of file_close record, len=" << len);
            }
         }

         delete f;
      }
   }
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
   // Can be called with other locks held.

   if ( ! m_prefetch_enabled)
   {
      return;
   }

   m_prefetch_condVar.Lock();
   m_prefetchList.push_back(file);
   m_prefetch_condVar.Signal();
   m_prefetch_condVar.UnLock();
}


void Cache::DeRegisterPrefetchFile(File* file)
{
   // Can be called with other locks held.

   if ( ! m_prefetch_enabled)
   {
      return;
   }

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
   const long long limit_RAM = m_configuration.m_RamAbsAvailable * 7 / 10;

   while (true)
   {
      m_RAM_mutex.Lock();
      bool doPrefetch = (m_RAM_used < limit_RAM);
      m_RAM_mutex.UnLock();

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
//=== Virtuals from XrdOucCache
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

int Cache::LocalFilePath(const char *curl, char *buff, int blen,
                         LFP_Reason why, bool forall)
{
   static const mode_t groupReadable = S_IRUSR | S_IWUSR | S_IRGRP;
   static const mode_t worldReadable = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   static const char  *lfpReason[] = { "ForAccess", "ForInfo", "ForPath" };

   TRACE(Debug, "LocalFilePath '" << curl << "', why=" << lfpReason[why]);

   if (buff && blen > 0) buff[0] = 0;

   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();
   std::string i_name = f_name + Info::s_infoExtension;

   if (why == ForPath)
   {
     int ret = m_oss->Lfn2Pfn(f_name.c_str(), buff, blen);
     TRACE(Info, "LocalFilePath '" << curl << "', why=" << lfpReason[why] << " -> " << ret);
     return ret;
   }

   {
      XrdSysCondVarHelper lock(&m_active_cond);
      m_purge_delay_set.insert(f_name);
   }

   struct stat sbuff, sbuff2;
   if (m_oss->Stat(f_name.c_str(), &sbuff)  == XrdOssOK &&
       m_oss->Stat(i_name.c_str(), &sbuff2) == XrdOssOK)
   {
      if (S_ISDIR(sbuff.st_mode))
      {
         TRACE(Info, "LocalFilePath '" << curl << "', why=" << lfpReason[why] << " -> EISDIR");
         return -EISDIR;
      }
      else
      {
         bool read_ok     = false;
         bool is_complete = false;

         // Lock and check if the file is active. If NOT, keep the lock
         // and add dummy access after successful reading of info file.
         // If it IS active, just release the lock, this ongoing access will
         // assure the file continues to exist.

         // XXXX How can I just loop over the cinfo file when active?
         // Can I not get is_complete from the existing file?
         // Do I still want to inject access record?
         // Oh, it writes only if not active .... still let's try to use existing File.

         m_active_cond.Lock();

         bool is_active = m_active.find(f_name) != m_active.end();

         if (is_active) m_active_cond.UnLock();

         XrdOssDF* infoFile = m_oss->newFile(m_configuration.m_username.c_str());
         XrdOucEnv myEnv;
         int res = infoFile->Open(i_name.c_str(), O_RDWR, 0600, myEnv);
         if (res >= 0)
         {
            Info info(m_trace, 0);
            if (info.Read(infoFile, i_name.c_str()))
            {
               read_ok = true;

               is_complete = info.IsComplete();

               // Add full-size access if reason is for access.
               if ( ! is_active && is_complete && why == ForAccess)
               {
                  info.WriteIOStatSingle(info.GetFileSize());
                  info.Write(infoFile, i_name.c_str());
               }
            }
            infoFile->Close();
         }
         delete infoFile;

         if ( ! is_active) m_active_cond.UnLock();

         if (read_ok)
         {
            if ((is_complete || why == ForInfo) && buff != 0)
            {
               int res2 = m_oss->Lfn2Pfn(f_name.c_str(), buff, blen);
               if (res2 < 0)
                  return res2;

               // Normally, files are owned by us but when direct cache access
               // is wanted and possible, make sure the file is world readable.
               if (why == ForAccess)
                  {mode_t mode = (forall ? worldReadable : groupReadable);
                   if (((sbuff.st_mode & worldReadable) != mode)
                   &&  (m_oss->Chmod(f_name.c_str(), mode) != XrdOssOK))
                      {is_complete = false;
                       *buff = 0;
                      }
                  }
            }

            TRACE(Info, "LocalFilePath '" << curl << "', why=" << lfpReason[why] <<
                  (is_complete ? " -> FILE_COMPLETE_IN_CACHE" : " -> EREMOTE"));

            return is_complete ? 0 : -EREMOTE;
         }
      }
   }

   TRACE(Info, "LocalFilePath '" << curl << "', why=" << lfpReason[why] << " -> ENOENT");
   return -ENOENT;
}


//______________________________________________________________________________
//! Preapare the cache for a file open request. This method is called prior to
//! actually opening a file. This method is meant to allow defering an open
//! request or implementing the full I/O stack in the cache layer.
//! @return <0 Error has occurred, return value is -errno; fail open request.
//!         =0 Continue with open() request.
//!         >0 Defer open but treat the file as actually being open. Use the
//!            XrdOucCacheIO::Open() method to open the file at a later time.
//------------------------------------------------------------------------------

int Cache::Prepare(const char *curl, int oflags, mode_t mode)
{
   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();
   std::string i_name = f_name + Info::s_infoExtension;

   // Do not allow write access.
   if (oflags & (O_WRONLY | O_RDWR | O_APPEND | O_CREAT))
   {
      TRACE(Warning, "Prepare write access requested on file " << f_name << ". Denying access.");
      return -EROFS;
   }

   // Intercept xrdpfc_command requests.
   if (m_configuration.m_allow_xrdpfc_command && strncmp("/xrdpfc_command/", f_name.c_str(), 16) == 0)
   {
      // Schedule a job to process command request.
      {
         CommandExecutor *ce = new CommandExecutor(f_name, "CommandExecutor");

         schedP->Schedule(ce);
      }

      return -EAGAIN;
   }

   {
      XrdSysCondVarHelper lock(&m_active_cond);
      m_purge_delay_set.insert(f_name);
   }

   struct stat sbuff;
   int res = m_oss->Stat(i_name.c_str(), &sbuff);
   if (res == 0)
   {
      TRACE(Dump, "Prepare defer open " << f_name);
      return 1;
   }
   else
   {
      return 0;
   }
}

//______________________________________________________________________________
// virtual method of XrdOucCache.
//!
//! @return <0 - Stat failed, value is -errno.
//!         =0 - Stat succeeded, sbuff holds stat information.
//!         >0 - Stat could not be done, forward operation to next level.
//------------------------------------------------------------------------------

int Cache::Stat(const char *curl, struct stat &sbuff)
{
   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();

   {
      XrdSysCondVarHelper lock(&m_active_cond);
      m_purge_delay_set.insert(f_name);
   }

   if (m_oss->Stat(f_name.c_str(), &sbuff) == XrdOssOK)
   {
      if (S_ISDIR(sbuff.st_mode))
      {
         return 0;
      }
      else
      {
         bool success = false;
         XrdOssDF* infoFile = m_oss->newFile(m_configuration.m_username.c_str());
         XrdOucEnv myEnv;

         f_name += Info::s_infoExtension;
         int res = infoFile->Open(f_name.c_str(), O_RDONLY, 0600, myEnv);
         if (res >= 0)
         {
            Info info(m_trace, 0);
            if (info.Read(infoFile, f_name.c_str()))
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

//______________________________________________________________________________
// virtual method of XrdOucCache.
//!
//! @return <0 - Stat failed, value is -errno.
//!         =0 - Stat succeeded, sbuff holds stat information.
//------------------------------------------------------------------------------

int Cache::Unlink(const char *curl)
{
   XrdCl::URL url(curl);
   std::string f_name = url.GetPath();

   // printf("Unlink url=%s\n\t    fname=%s\n", curl, f_name.c_str());

   return UnlinkFile(f_name, false);
}

int Cache::UnlinkFile(const std::string& f_name, bool fail_if_open)
{
   ActiveMap_i  it;
   File        *file = 0;
   {
      XrdSysCondVarHelper lock(&m_active_cond);

      it = m_active.find(f_name);

      if (it != m_active.end())
      {
         if (fail_if_open)
         {
            TRACE(Info, "UnlinkCommon " << f_name << ", file currently open and force not requested - denying request");
            return -EBUSY;
         }

         // Null File* in m_active map means an operation is ongoing, probably
         // Attach() with possible File::Open(). Ask for retry.
         if (it->second == 0)
         {
            TRACE(Info, "UnlinkCommon " << f_name << ", an operation on this file is ongoing - denying request");
            return -EAGAIN;
         }

         file = it->second;
         file->initiate_emergency_shutdown();
         it->second = 0;
      }
      else
      {
         it = m_active.insert(std::make_pair(f_name, (File*) 0)).first;
      }
   }

   if (file)
   {
      RemoveWriteQEntriesFor(file);
   }

   std::string i_name = f_name + Info::s_infoExtension;

   // Unlink file & cinfo
   int f_ret = m_oss->Unlink(f_name.c_str());
   int i_ret = m_oss->Unlink(i_name.c_str());

   TRACE(Debug, "UnlinkCommon " << f_name << ", f_ret=" << f_ret << ", i_ret=" << i_ret);

   {
      XrdSysCondVarHelper lock(&m_active_cond);

      m_active.erase(it);
   }

   return std::min(f_ret, i_ret);
}
