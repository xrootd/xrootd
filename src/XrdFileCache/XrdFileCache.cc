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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include "XrdFileCache.hh"
#include "XrdFileCacheIOEntire.hh"
#include "XrdFileCacheIOBlock.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCachePrefetch.hh"
#include "XrdFileCacheLog.hh"


XrdFileCache::Cache *XrdFileCache::Cache::m_cache = NULL;
XrdSysMutex XrdFileCache::Cache::m_cache_mutex;


const char* InfoExt = ".cinfo";
const int InfoExtLen = int(strlen(InfoExt));
const bool IODisablePrefetch = false;
const long long PrefetchDefaultBufferSize = 1024*1024;


using namespace XrdFileCache;

Cache::Cache(XrdOucCacheStats & stats)
    : m_attached(0),
      m_stats(stats)
{}

XrdOucCacheIO *
Cache::Attach(XrdOucCacheIO *io, int Options)
{
    XrdSysMutexHelper lock(&m_io_mutex);
    m_attached++;

    aMsgIO(kInfo, io, "Cache::Attach()");
    if (io)
    {
        if (Factory::GetInstance().PrefetchFileBlocks())
            return new IOBlocks(*io, m_stats, *this);
        else 
            return new IOEntire(*io, m_stats, *this);
    }
    else
    {
       aMsgIO(kDebug, io, "Cache::Attache(), XrdOucCacheIO == NULL");
    }
    
    m_attached--;
    return io;
}

int
Cache::isAttached()
{
    XrdSysMutexHelper lock(&m_io_mutex);
    return m_attached;
}

void
Cache::Detach(XrdOucCacheIO* io)
{
    aMsgIO(kInfo, io, "Cache::Detach()");
    XrdSysMutexHelper lock(&m_io_mutex);
    m_attached--;

    aMsgIO(kDebug, io, "Cache::Detach(), deleting IO object. Attach count = %d", m_attached);


    delete io;
}


