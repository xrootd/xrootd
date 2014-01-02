#ifndef __XRDFILECACHEIOEntire_HH__
#define __XRDFILECACHEIOEntire_HH__
/******************************************************************************/
/*                                                                            */
/* (c) 2012 University of Nebraska-Lincoln                                    */
/*     by Brian Bockelman                                                     */
/*                                                                            */
/******************************************************************************/

/*
 * The XrdFileCacheIOEntire object is used as a proxy for the original source
 */

#include <XrdOuc/XrdOucCache.hh>
#include "XrdSys/XrdSysPthread.hh"
#include <string>

#include "XrdFileCachePrefetch.hh"
#include "XrdFileCache.hh"

class XrdSysError;
class XrdOssDF;
class XfcStats;


namespace XrdFileCache
{


class IOEntire : public XrdOucCacheIO
{
    friend class Cache;

public:

    XrdOucCacheIO *
    Base() {return &m_io; }

    virtual XrdOucCacheIO *Detach();

    long long
    FSize() {return m_io.FSize(); }

    const char *
    Path() {return m_io.Path(); }

    int Read (char  *Buffer, long long Offset, int Length);

#if defined(HAVE_READV)
    virtual int  ReadV (const XrdOucIOEntireVec *readV, int n);

#endif

    int
    Sync() {return 0; }

    int
    Trunc(long long Offset) { errno = ENOTSUP; return -1; }

    int
    Write(char *Buffer, long long Offset, int Length) { errno = ENOTSUP; return -1; }
  static bool getFilePathFromURL(const char* url, std::string& res);

protected:
    IOEntire(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache &cache);

private:
   ~IOEntire();
 
    XrdOucCacheIO & m_io;
    XrdOucCacheStats & m_statsGlobal;
    Cache & m_cache;
    Prefetch* m_prefetch;
};

}
#endif
