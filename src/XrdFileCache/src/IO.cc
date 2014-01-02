#include "IO.hh"
#include "Context.hh"
#include "Factory.hh"
#include "CacheStats.hh"

#include <stdio.h>
#include <fcntl.h>
#include <utime.h>

#include "XrdClient/XrdClientConst.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
 
#include <XrdSys/XrdSysPthread.hh>

using namespace XrdFileCache;

void *
PrefetchRunner(void * prefetch_void)
{
   XrdFileCache::Prefetch *prefetch = static_cast<XrdFileCache::Prefetch *>(prefetch_void);
     if (prefetch)
       prefetch->Run();
    return NULL;
}
//______________________________________________________________________________


IO::IO(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache & cache)
    : m_io(io),
      m_statsGlobal(stats),
      m_cache(cache)
{
   aMsgIO(kInfo, &m_io, "IO::IO() [%p]", this);

    std::string fname;
    getFilePathFromURL(io.Path(), fname);
    fname = Factory::GetInstance().GetTempDirectory() + fname;

    m_prefetch = new Prefetch(io, fname, 0, io.FSize());
    pthread_t tid;
          XrdSysThread::Run(&tid, PrefetchRunner, (void *)(m_prefetch), 0, "XrdFileCache Prefetcher");
    
}

IO::~IO() 
{
}

XrdOucCacheIO *
IO::Detach()
{
    m_statsGlobal.Add(m_prefetch->GetStats());
    //  aMsgIO(kInfo, &m_io, "IO::Detach() [%p] bPrefCh[%lld] bPref[%lld] bDisk[%lld] bMiss[%lld]", this,
    //       m_stats->BytesCachedPrefetch,  m_stats->BytesPrefetch,m_stats->BytesDisk, m_stats->BytesPass);

    XrdOucCacheIO * io = &m_io;

    delete m_prefetch;
    m_prefetch = 0;

    // This will delete us!
    m_cache.Detach(this); 
    return io;
}

/*
 * Read from the cache; prefer to read from the Prefetch object, if possible.
 */
int
IO::Read (char *buff, long long off, int size)
{
   aMsgIO(kDebug, &m_io, "IO::Read() [%p]  %lld@%d", this, off, size);

    if (IODisablePrefetch) {
       // for testing purpose only
       return m_io.Read(buff, off, size);
    }

    ssize_t bytes_read = 0;
    ssize_t retval = 0;
    
    retval = m_prefetch->Read(buff, off, size);
        aMsgIO(kInfo, &m_io, "IO::Read() read from prefetch retval =  %d", retval);
    if (retval > 0) {    

        bytes_read += retval;
        buff += retval;
        size -= retval;
    }


    if ((size > 0))
    {
        aMsgIO(kDebug, &m_io, "IO::Read() missed %d bytes", size);
        if (retval > 0) bytes_read += retval;
    }

    if (retval < 0)
    {
        aMsgIO(kError, &m_io, "IO::Read(), origin bytes read %d", retval);
    }

    return (retval < 0) ? retval : bytes_read;
}



/*
 * Perform a readv from the cache
 */
#if defined(HAVE_READV)
int
IO::ReadV (const XrdOucIOVec *readV, int n)
{
   aMsgIO(kWarning, &m_io, "IO::ReadV(), get %d requests", n);
   /*
   ssize_t bytes_read = 0;
   size_t missing = 0;
   XrdOucIOVec missingReadV[READV_MAXCHUNKS];
   for (size_t i=0; i<n; i++)
   {
      XrdSfsXferSize size = readV[i].size;
      char * buff = readV[i].data;
      XrdSfsFileOffset off = readV[i].offset;
      if (m_prefetch.get())
      {
         ssize_t retval = Read(buff, off, size);
         if ((retval > 0) && (retval == size))
         {
            // TODO: could handle partial reads here
            bytes_read += size;
            continue;
         }
      }
      missingReadV[missing].size = size;
      missingReadV[missing].data = buff;
      missingReadV[missing].offset = off;
      missing++;
      if (missing >= READV_MAXCHUNKS)
      { // Something went wrong in construction of this request;
         // Should be limited in higher layers to a max of 512 chunks.
         aMsgIO(kError, &m_io, "IO::ReadV(), missing %d >  READV_MAXCHUNKS %d", missing, READV_MAXCHUNKS);
         return -1;
      }
   }

   return  bytes_read;*/
   return 0;
}

#endif


bool
IO::getFilePathFromURL(const char* url, std::string &result)
{
    std::string path = url;
    size_t split_loc = path.rfind("//");

    if (split_loc == path.npos)
        return false;

    size_t kloc = path.rfind("?");
    result = path.substr(split_loc+1,kloc-split_loc-1);

    if (kloc == path.npos)
        return false;

    return true;
}
