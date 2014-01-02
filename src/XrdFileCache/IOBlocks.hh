#ifndef __XRDFILECACHEIOBL_HH__
#define __XRDFILECACHEIOBL_HH__
/******************************************************************************/
/*                                                                            */
/* (c) 2012 University of Nebraska-Lincoln                                    */
/*     by Brian Bockelman                                                     */
/*                                                                            */
/******************************************************************************/

/*
 * The XrdHdfsCacheIO object is used as a proxy for the original source
 */

#include <XrdOuc/XrdOucCache.hh>
#include "XrdSys/XrdSysPthread.hh"

#include "Cache.hh"
#include "Prefetch.hh"

#include <map>
#include <string>

class XrdSysError;
class XrdOssDF;


namespace XrdFileCache
{

class IOBlocks : public XrdOucCacheIO
{
private:
    struct FileBlock {

        FileBlock(off_t off, XrdOucCacheIO*  io) :  m_prefetch(0), m_offset0(off) {}

        // XrdOssDF* m_diskDF;
        Prefetch* m_prefetch;
        long long m_offset0;
        //   XrdOucCacheIO* m_io;
    };

public:
    IOBlocks(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache &cache);
    ~IOBlocks() {}

    XrdOucCacheIO *
    Base() {return &m_io; }

    virtual XrdOucCacheIO *Detach();

    long long
    FSize() {return m_io.FSize(); }

    const char *
    Path() {return m_io.Path(); }

    int Read (char  *Buffer, long long Offset, int Length);

    int
    Sync() {return 0; }

    int
    Trunc(long long Offset) { errno = ENOTSUP; return -1; }

    int
    Write(char *Buffer, long long Offset, int Length) { errno = ENOTSUP; return -1; }    


    int Read (XrdOucCacheStats &Now, char *Buffer, long long Offs, int Length);

private:
    FileBlock* newBlockPrefetcher(long long off, int blocksize, XrdOucCacheIO*  io);

    XrdOucCacheIO & m_io;
    XrdOucCacheStats & m_statsGlobal;
    Cache& m_cache;

   long long  m_blockSize;

   typedef  std::map<int, FileBlock*> BlockMap_t;
    BlockMap_t m_blocks;

    void  GetBlockSizeFromPath();
    XrdSysMutex m_mutex;

};

}
#endif
