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
#include <sys/statvfs.h>

#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include "XrdFileCache.hh"
#include "XrdFileCachePrefetch.hh"
#include "XrdFileCacheIOEntireFile.hh"
#include "XrdFileCacheIOFileBlock.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCachePrefetch.hh"


XrdFileCache::Cache::WriteQ XrdFileCache::Cache::s_writeQ;

using namespace XrdFileCache;
void *ProcessWriteTaskThread(void* c)
{
   Cache *cache = static_cast<Cache*>(c);
   cache->ProcessWriteTasks();
   return NULL;
}

Cache::Cache(XrdOucCacheStats & stats)
   : m_attached(0),
     m_stats(stats)
{
   pthread_t tid;
   XrdSysThread::Run(&tid, ProcessWriteTaskThread, (void*)this, 0, "XrdFileCache WriteTasks ");
}
//______________________________________________________________________________

XrdOucCacheIO *Cache::Attach(XrdOucCacheIO *io, int Options)
{
   if (Factory::GetInstance().Decide(io))
   {
      clLog()->Info(XrdCl::AppMsg, "Cache::Attach() %s", io->Path());
      {
         XrdSysMutexHelper lock(&m_io_mutex);
         m_attached++;
      }
      IO* cio;
      if (Factory::GetInstance().RefConfiguration().m_hdfsmode)
         cio = new IOFileBlock(*io, m_stats, *this);
      else
         cio = new IOEntireFile(*io, m_stats, *this);

      cio->StartPrefetch();
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
   XrdSysMutexHelper lock(&m_io_mutex);
   return m_attached;
}

void Cache::Detach(XrdOucCacheIO* io)
{
   clLog()->Info(XrdCl::AppMsg, "Cache::Detach() %s", io->Path());
   {
      XrdSysMutexHelper lock(&m_io_mutex);
      m_attached--;
   }

   delete io;
}

//______________________________________________________________________________


void Cache::getFilePathFromURL(const char* iUrl, std::string &result) const
{
   XrdCl::URL url(iUrl);
   result = Factory::GetInstance().RefConfiguration().m_cache_dir + url.GetPath();
}

//______________________________________________________________________________
bool
Cache::HaveFreeWritingSlots()
{
   const static size_t maxWriteWaits=100;
   return s_writeQ.size < maxWriteWaits;
}


//______________________________________________________________________________
void
Cache::AddWriteTask(Prefetch* p, int ri, size_t s, bool fromRead)
{
   XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg, "Cache::AddWriteTask() wqsize = %d, bi=%d", s_writeQ.size, ri);
   s_writeQ.condVar.Lock();
   if (fromRead)
      s_writeQ.queue.push_back(WriteTask(p, ri, s));
   else
      s_writeQ.queue.push_front(WriteTask(p, ri, s));
   s_writeQ.size++;
   s_writeQ.condVar.Signal();
   s_writeQ.condVar.UnLock();
}

//______________________________________________________________________________
void Cache::RemoveWriteQEntriesFor(Prefetch *p)
{
   s_writeQ.condVar.Lock();
   std::list<WriteTask>::iterator i = s_writeQ.queue.begin();
   while (i != s_writeQ.queue.end())
   {
      if (i->prefetch == p)
      {
         std::list<WriteTask>::iterator j = i++;
         j->prefetch->DecRamBlockRefCount(j->ramBlockIdx);
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
      WriteTask t = s_writeQ.queue.front();
      s_writeQ.queue.pop_front();
      s_writeQ.size--;
      s_writeQ.condVar.UnLock();

      t.prefetch->WriteBlockToDisk(t.ramBlockIdx, t.size);
      t.prefetch->DecRamBlockRefCount(t.ramBlockIdx);
   }
}
