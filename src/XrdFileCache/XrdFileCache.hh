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

namespace XrdCl {
   class Log;
}
namespace XrdFileCache {
class Prefetch;
}

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Attaches/creates and detaches/deletes cache-io objects for disk based cache.
   //----------------------------------------------------------------------------
   class Cache : public XrdOucCache
   {
      friend class IOEntireFile;
      friend class IOFileBlock;

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

         // for disk performance allow only one 1M(default) read at the time
         static void AddWriteTask(Prefetch* p, int ramBlockidx, int fileBlockIdx, size_t size, bool fromRead);

         static bool HaveFreeWritingSlots();

         static void RemoveWriteQEntriesFor(Prefetch *p);
         static void ProcessWriteTasks();

         struct WriteTask
         {
            Prefetch* prefetch;
            int ramBlockIdx;
            int fileBlockIdx;
            size_t size;
            WriteTask(Prefetch* p, int ri, int fi, size_t s):prefetch(p), ramBlockIdx(ri), fileBlockIdx(fi), size(s){}
         };

         struct WriteQ
         {
            WriteQ() : mutex(0), size(0) {}
            XrdSysCondVar         mutex;
            size_t                size;
            std::list<WriteTask>  queue;
         };

         static WriteQ s_writeQ;

      private:
         void Detach(XrdOucCacheIO *);
         bool getFilePathFromURL(const char* url, std::string& res) const;

         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         XrdSysMutex        m_io_mutex; //!< central lock for this class
         unsigned int       m_attached; //!< number of attached IO objects
         XrdOucCacheStats  &m_stats;    //!< global cache usage statistics
         
   };


   //----------------------------------------------------------------------------
   //! Base cache-io class that implements XrdOucCacheIO abstract methods.
   //----------------------------------------------------------------------------
   class IO : public XrdOucCacheIO
   {
      friend class Prefetch;

      public:
         IO (XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache &cache) :
         m_io(io), m_statsGlobal(stats), m_cache(cache) {}

         //! Original data source.
         virtual XrdOucCacheIO *Base() { return &m_io; }

         //! Original data source URL.
         virtual long long FSize() { return m_io.FSize(); }

         //! Original data source URL.
         virtual const char *Path() { return m_io.Path(); }

         virtual int Sync() { return 0; }

         virtual int Trunc(long long Offset) { errno = ENOTSUP; return -1; }

         virtual int Write(char *Buffer, long long Offset, int Length)
         { errno = ENOTSUP; return -1; }

         virtual void StartPrefetch() {}

      protected:
         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         XrdOucCacheIO    &m_io;          //!< original data source
         XrdOucCacheStats &m_statsGlobal; //!< reference to Cache statistics
         Cache            &m_cache;       //!< reference to Cache needed in detach
   };
}

#endif
