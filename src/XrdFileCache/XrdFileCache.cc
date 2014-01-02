#include "XrdFileCache.hh"

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <fcntl.h>
#include <sstream>


#include "IO.hh"
#include "IOBlocks.hh"
#include "XrdFileCacheFactory.hh"
#include "Prefetch.hh"
#include "Log.hh"


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
            return new IO(*io, m_stats, *this);
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


