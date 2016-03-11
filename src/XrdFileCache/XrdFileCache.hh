#ifndef __XRDFILECACHE_CACHE_HH__
#define __XRDFILECACHE_CACHE_HH__
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
#include <string>
#include <list>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdFileCacheFile.hh"

namespace XrdCl {
   class Log;
}
namespace XrdFileCache {
class File;
class IO;
}

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Attaches/creates and detaches/deletes cache-io objects for disk based cache.
   //----------------------------------------------------------------------------
   class Cache : public XrdOucCache
   {
      public:
         //---------------------------------------------------------------------
         //! Constructor
         //---------------------------------------------------------------------
         Cache(XrdOucCacheStats&);

         //---------------------------------------------------------------------
         //! Obtain a new IO object that fronts existing XrdOucCacheIO.
         //---------------------------------------------------------------------
         virtual XrdOucCacheIO *Attach(XrdOucCacheIO *, int Options=0);

         //---------------------------------------------------------------------
         //! Number of cache-io objects atteched through this cache.
         //---------------------------------------------------------------------
         virtual int isAttached();

         //---------------------------------------------------------------------
         //! \brief Unused abstract method. Plugin instantiation role is given
         //! to the Factory class.
         //---------------------------------------------------------------------
         virtual XrdOucCache* Create(XrdOucCache::Parms&, XrdOucCacheIO::aprParms*)
         { return NULL; }

         //---------------------------------------------------------------------
         //! Add downloaded block in write queue.
         //---------------------------------------------------------------------
         void AddWriteTask(Block* b, bool from_read);

         //---------------------------------------------------------------------
         //! Check write queue size is not over limit.
         //---------------------------------------------------------------------
         bool HaveFreeWritingSlots();

         //---------------------------------------------------------------------
         //!  \brief Remove blocks from write queue which belong to given prefetch.
         //! This method is used at the time of File destruction.
         //---------------------------------------------------------------------
         void RemoveWriteQEntriesFor(File *f);

         //---------------------------------------------------------------------
         //! Separate task which writes blocks from ram to disk.
         //---------------------------------------------------------------------
         void ProcessWriteTasks();

         bool RequestRAMBlock();

         void RAMBlockReleased();

         void RegisterPrefetchFile(File*);
         void DeRegisterPrefetchFile(File*);

         File* GetNextFileToPrefetch();

         void Prefetch();

         //! Decrease attached count. Called from IO::Detach().
         void Detach(XrdOucCacheIO *);

      private:

         //! Short log alias.
         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         XrdSysCondVar      m_prefetch_condVar; //!< central lock for this class
         XrdOucCacheStats  &m_stats;    //!< global cache usage statistics

         XrdSysMutex        m_RAMblock_mutex; //!< central lock for this class
         int                m_RAMblocks_used;

         struct WriteQ
         {
            WriteQ() : condVar(0), size(0) {}
            XrdSysCondVar         condVar;  //!< write list condVar
            size_t                size;     //!< cache size of a container
            std::list<Block*>     queue;    //!< container
         };

         WriteQ s_writeQ;

       // prefetching
       typedef std::vector<File*>  FileList;
       FileList  m_files;
   };

}

#endif
