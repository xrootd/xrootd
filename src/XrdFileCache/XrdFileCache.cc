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

#include "XrdFileCache.hh"
#include "XrdFileCacheIOEntireFile.hh"
#include "XrdFileCacheIOFileBlock.hh"
#include "XrdFileCacheFactory.hh"


using namespace XrdFileCache;

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
//______________________________________________________________________________


Cache::Cache(XrdOucCacheStats & stats) : XrdOucCache(),
     m_stats(stats),
     m_RAMblocks_used(0)
{
   pthread_t tid1;
   XrdSysThread::Run(&tid1, ProcessWriteTaskThread, (void*)this, 0, "XrdFileCache WriteTasks ");

   pthread_t tid2;
   XrdSysThread::Run(&tid2, PrefetchThread, (void*)this, 0, "XrdFileCache Prefetch ");
}

//______________________________________________________________________________

XrdOucCacheIO *Cache::Attach(XrdOucCacheIO *io, int Options)
{
   if (Factory::GetInstance().Decide(io))
   {
      clLog()->Info(XrdCl::AppMsg, "Cache::Attach() %s", io->Path());
      IO* cio;
      if (Factory::GetInstance().RefConfiguration().m_hdfsmode)
         cio = new IOFileBlock(*io, m_stats, *this);
      else
         cio = new IOEntireFile(*io, m_stats, *this);

      return cio;
   }
   else
   {
      clLog()->Info(XrdCl::AppMsg, "Cache::Attach() reject %s", io->Path());
   }
   return io;
}
//______________________________________________________________________________


int Cache::isAttached()
{
    // virutal function of XrdOucCache, don't see it used in pfc or posix layer
    return true;
}

void Cache::Detach(XrdOucCacheIO* io)
{
   clLog()->Info(XrdCl::AppMsg, "Cache::Detach() %s", io->Path());

   delete io;
}
//______________________________________________________________________________
bool
Cache::HaveFreeWritingSlots()
{
   const static size_t maxWriteWaits=500;
   if ( s_writeQ.size < maxWriteWaits) {
      return true;
   }
   else {
       XrdCl::DefaultEnv::GetLog()->Debug(XrdCl::AppMsg, "Cache::HaveFreeWritingSlots() negative", s_writeQ.size);
       return false;
   }
}
//______________________________________________________________________________
void
Cache::AddWriteTask(Block* b, bool fromRead)
{
   XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg, "Cache::AddWriteTask() bOff=%ld", b->m_offset);
   s_writeQ.condVar.Lock();
   if (fromRead)
       s_writeQ.queue.push_back(b);
   else
      s_writeQ.queue.push_front(b); // AMT should this not be the opposite
   s_writeQ.size++;
   s_writeQ.condVar.Signal();
   s_writeQ.condVar.UnLock();
}

//______________________________________________________________________________
void Cache::RemoveWriteQEntriesFor(File *iFile)
{
   s_writeQ.condVar.Lock();
   std::list<Block*>::iterator i = s_writeQ.queue.begin();
   while (i != s_writeQ.queue.end())
   {
      if ((*i)->m_file == iFile)
      {

          XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg, "Cache::Remove entries for %p path %s", (void*)(*i), iFile->lPath());
         std::list<Block*>::iterator j = i++;
          iFile->BlockRemovedFromWriteQ(*j);
         s_writeQ.queue.erase(j);
         --s_writeQ.size;
      }
      else
      {
         ++i;
      }
   }
   s_writeQ.condVar.UnLock();
}

//______________________________________________________________________________
void
Cache::ProcessWriteTasks()
{
   while (true)
   {
      s_writeQ.condVar.Lock();
      while (s_writeQ.queue.empty())
      {
         s_writeQ.condVar.Wait();
      }
      Block* block = s_writeQ.queue.front(); // AMT should not be back ???
      s_writeQ.queue.pop_front();
      s_writeQ.size--;
      XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg, "Cache::ProcessWriteTasks  for %p path %s", (void*)(block), block->m_file->lPath());
      s_writeQ.condVar.UnLock();

      block->m_file->WriteBlockToDisk(block);
   }
}

//______________________________________________________________________________

bool
Cache::RequestRAMBlock()
{
   XrdSysMutexHelper lock(&m_RAMblock_mutex);
   if ( m_RAMblocks_used < Factory::GetInstance().RefConfiguration().m_NRamBuffers )
   {
      m_RAMblocks_used++;
      return true;
   }
   return false;
}

void
Cache::RAMBlockReleased()
{
   XrdSysMutexHelper lock(&m_RAMblock_mutex);
   m_RAMblocks_used--;
}


//==============================================================================
//=======================  PREFETCH ===================================
//==============================================================================
/*
namespace {
struct prefetch_less_than
{
    inline bool operator() (const File* struct1, const File* struct2)
    {
        return (struct1->GetPrefetchScore() < struct2->GetPrefetchScore());
    }
}myobject;
}*/
//______________________________________________________________________________

void
Cache::RegisterPrefetchFile(File* file)
{
    //  called from File::Open()

    if (Factory::GetInstance().RefConfiguration().m_prefetch)
    {
            XrdSysMutexHelper lock(&m_prefetch_mutex);
            m_files.push_back(file);
    }
}

//______________________________________________________________________________

void
Cache::DeRegisterPrefetchFile(File* file)
{
   //  called from last line File::InitiateClose()

   XrdSysMutexHelper lock(&m_prefetch_mutex);
   for (FileList::iterator it = m_files.begin(); it != m_files.end(); ++it) {
      if (*it == file) {
         m_files.erase(it);
         break;
      }
   }
}
//______________________________________________________________________________

File* 
Cache::GetNextFileToPrefetch()
{
   XrdSysMutexHelper lock(&m_prefetch_mutex);
   if (m_files.empty()) {
      // XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg, "Cache::GetNextFileToPrefetch ... no open files");  
      return 0;
   }

   //  std::sort(m_files.begin(), m_files.end(), myobject);
   std::random_shuffle(m_files.begin(), m_files.end());
   File* f = m_files.back();
   f->MarkPrefetch();
   return f;
}

//______________________________________________________________________________


void 
Cache::Prefetch()
{
   const static int limitRAM= Factory::GetInstance().RefConfiguration().m_NRamBuffers * 0.7;

   XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg, "Cache::Prefetch thread start");

    while (true) {
      bool doPrefetch = false;
      m_RAMblock_mutex.Lock();
      if (m_RAMblocks_used < limitRAM && HaveFreeWritingSlots())
         doPrefetch = true;
      m_RAMblock_mutex.UnLock();

      if (doPrefetch) {
         File* f = GetNextFileToPrefetch();
         if (f) {
            f->Prefetch();
            // XrdSysTimer::Wait(1);
            continue;
         }
      }

      // wait for new file or more resources, AMT should I wait for the signal instead ???
      XrdSysTimer::Wait(10);
   }  
}
