#ifndef __XRDFILECACHE_BLOCK_HH__
#define __XRDFILECACHE_BLOCK_HH__

class XrdOucCacheIO;
class XrdOssDF;
class XrdSysError;
class CacheStats;

//class Prefetch;

#include "Prefetch.hh"

namespace XrdFileCache
{
class FileBlock
{
public:
    FileBlock(off_t off, int blocksize, XrdOucCacheIO*  io);
   virtual ~FileBlock();

    int Read(char *buff, long long off, int size);

    XrdOssDF* m_diskDF;
    Prefetch* m_prefetch;
    long long m_offset0;
   XrdOucCacheIO* m_io;
};

}

#endif
