#include "IOBlocks.hh"
#include "XrdFileCache.hh"
#include "Log.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCacheFactory.hh"

#include <math.h>
#include <sstream>
#include <stdio.h>
#include <iostream>
#include <assert.h>
#include "XrdClient/XrdClientConst.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdOuc/XrdOucEnv.hh"

int s_blocksize = 1048576*128; // 128M 

using namespace XrdFileCache;

void *
PrefetchRunnerBl(void * prefetch_void)
{
    Prefetch *prefetch = static_cast<Prefetch*>(prefetch_void);
    prefetch->Run();
    return NULL;
}


IOBlocks::IOBlocks(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache & cache)
    : m_io(io),
      m_statsGlobal(stats),
      m_cache(cache)
{
    m_blockSize = s_blocksize;
    GetBlockSizeFromPath();
}

XrdOucCacheIO *
IOBlocks::Detach()
{
    aMsgIO(kInfo, &m_io,"IOBlocks::Detach()");
    XrdOucCacheIO * io = &m_io;


    for (BlockMap_t::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
    {
        m_statsGlobal.Add(it->second->m_prefetch->GetStats());
        delete it->second->m_prefetch;
    }

    m_cache.Detach(this); // This will delete us!

    return io;
}


void IOBlocks::GetBlockSizeFromPath()
{
    const static std::string tag = "hdfs_block_size=";
    std::string path= m_io.Path();
    size_t pos1 = path.find(tag);    
    size_t t = tag.length();
    if ( pos1 != path.npos)
    {
        pos1 += t;
        size_t pos2 = path.find("&", pos1 );
        if (pos2 != path.npos )
        {
            std::string bs = path.substr(pos1, pos2-pos1);   
            m_blockSize = atoi(bs.c_str());
        }
	else {
            m_blockSize = atoi(path.substr(pos1).c_str());
	}

        aMsg(kDebug, "IOBlocks::READ ===> BLOCKSIZE [%d].", m_blockSize);
    }
}

#include "XrdFileCacheIOEntire.hh" // AMT !!
IOBlocks::FileBlock* IOBlocks::newBlockPrefetcher(long long off, int blocksize, XrdOucCacheIO*  io)
{
    FileBlock* fb = new FileBlock(off, io);


    XrdOucEnv myEnv;
    std::string fname;
    IOEntire::getFilePathFromURL(io->Path(), fname);
    std::stringstream ss;
    ss << Factory::GetInstance().GetTempDirectory() << fname;
    char offExt[64];
    sprintf(&offExt[0],"_%lld", off );
    ss << &offExt[0];
    fname = ss.str();

    aMsgIO(kDebug, io, "FileBlock::FileBlock(), create Prefetch.");
    fb->m_prefetch = new Prefetch(*io, fname, off, blocksize);
    pthread_t tid;
    XrdSysThread::Run(&tid, PrefetchRunnerBl, (void *)fb->m_prefetch, 0, "XrdHdfsCache Prefetcher");

    return fb;
}

int
IOBlocks::Read (char *buff, long long off, int size)
{
    Stats stat_tmp; // AMT todo ...

    long long off0 = off;
    int idx_first = off0/m_blockSize;
    int idx_last = (off0+size-1)/m_blockSize;

    int bytes_read = 0;
    aMsgIO(kDebug, &m_io, "IOBlocks::Read() %lld@%d block range [%d-%d] \n", off, size, idx_first, idx_last);

    for (int blockIdx = idx_first; blockIdx <= idx_last; ++blockIdx )
    {
        // locate block
        FileBlock* fb;
        m_mutex.Lock();
        BlockMap_t::iterator it = m_blocks.find(blockIdx);
        if ( it != m_blocks.end() )
        {
            fb = it->second;
        }
        else
        {
            size_t pbs = m_blockSize;
            // check if this is last block
            int lastIOBlock = (m_io.FSize()-1)/m_blockSize; 
            if (blockIdx == lastIOBlock ) { 
                pbs =  m_io.FSize() - blockIdx*m_blockSize;
                aMsgIO(kDebug, &m_io , "IOBlocks::Read() last block, change output file size to %lld \n", pbs);
            }
            //            fb = new FileBlock(blockIdx*m_blockSize, pbs, &m_io);
            fb = newBlockPrefetcher(blockIdx*m_blockSize, pbs, &m_io);
            m_blocks.insert(std::pair<int,FileBlock*>(blockIdx, (FileBlock*) fb));
        }
        m_mutex.UnLock();

        // edit size if read request is reaching more than a block
        int readBlockSize = size;
        if (idx_first != idx_last)
        {
            if (blockIdx == idx_first)
            {
                readBlockSize = (blockIdx + 1) *m_blockSize - off0;
                aMsgIO(kDebug, &m_io , "IOBlocks::Read() %s", "Read partially till the end of the block");
            }
            else if (blockIdx == idx_last)
            {
                readBlockSize = (off0+size) - blockIdx*m_blockSize;
                aMsgIO(kDebug, &m_io , "IOBlocks::Read() s" , "Read partially from beginning of block");
            }
            else
            {
                readBlockSize = m_blockSize;
            }
        }
        assert(readBlockSize);

        aMsgIO(kInfo, &m_io, "IOBlocks::Read() block[%d] read-block-size[%d], offset[%lld]", blockIdx, readBlockSize, off);

      
        // pass offset unmodified

        long long min  = blockIdx*m_blockSize;
        long long max = min + m_blockSize;
        assert ( off >= min);
        assert(off+readBlockSize <= max);
        int retvalBlock = fb->m_prefetch->Read(buff , off - fb->m_offset0, size);

        aMsgIO(kDebug, &m_io,  "IOBlocks::Read()  Block read returned %d", retvalBlock );
        if (retvalBlock >=0 )
        {
            bytes_read += retvalBlock;
            buff += retvalBlock;
            off += retvalBlock;
            readBlockSize -= retvalBlock;

            // cancel read if not succssful
            if (readBlockSize > 0)
            {
                aMsgIO( kInfo, &m_io , "IOBlocks::Read() Can't read from prefetch remain = %d", readBlockSize);
                return bytes_read;   
            }
        }
        else
        {
            aMsgIO( kError, &m_io , "IOBlocks::Read() read error, retval %d", retvalBlock);
            return retvalBlock;
        }
    }

    return bytes_read;
}


