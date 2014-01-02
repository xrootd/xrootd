
#ifndef __XRDFILECACHE_CACHE_HH__
#define __XRDFILECACHE_CACHE_HH__
/******************************************************************************/
/*                                                                            */
/* (c) 2012 University of Nebraksa-Lincoln                                    */
/*     by Brian Bockelman                                                     */
/*                                                                            */
/******************************************************************************/

#include <XrdSys/XrdSysPthread.hh>
#include <XrdOuc/XrdOucCache.hh>


namespace XrdFileCache
{

extern const char* InfoExt;
extern const int InfoExtLen;
extern const bool IODisablePrefetch;
extern const long long PrefetchDefaultBufferSize;

class Cache : public XrdOucCache
{

    friend class IO;
    friend class IOBlocks;
    friend class Factory;

public:

    XrdOucCacheIO *Attach(XrdOucCacheIO *, int Options=0);

    int isAttached();

    virtual XrdOucCache*
    Create(XrdOucCache::Parms&, XrdOucCacheIO::aprParms*) {return NULL; }


    Cache(XrdOucCacheStats&);

private:

    void Detach(XrdOucCacheIO *);

    static Cache *m_cache;
    static XrdSysMutex m_cache_mutex;

    XrdSysMutex m_io_mutex;
    unsigned int m_attached;

    XrdOucCacheStats & m_stats;
};

}

#endif
