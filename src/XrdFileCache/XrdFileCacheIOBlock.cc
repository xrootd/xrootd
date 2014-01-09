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

#include "XrdFileCacheIOBlock.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheLog.hh"
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


using namespace XrdFileCache;

void *
PrefetchRunnerBl(void * prefetch_void)
{
    Prefetch *prefetch = static_cast<Prefetch*>(prefetch_void);
    prefetch->Run();
    return NULL;
}


IOBlock::IOBlock(XrdOucCacheIO &io, XrdOucCacheStats &statsGlobal, Cache & cache)
    : IO(io, statsGlobal, cache)
{
    m_blockSize = Factory::GetInstance().RefConfiguration().m_blockSize;
}

XrdOucCacheIO* IOBlock::Detach()
{
    aMsgIO(kInfo, &m_io,"IOBlock::Detach()");
    XrdOucCacheIO * io = &m_io;


    for (std::map<int, FileBlock*>::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
    {
        m_statsGlobal.Add(it->second->m_prefetch->GetStats());
        delete it->second->m_prefetch;
    }

    m_cache.Detach(this); // This will delete us!

    return io;
}

IOBlock::FileBlock* IOBlock::newBlockPrefetcher(long long off, int blocksize, XrdOucCacheIO*  io)
{
    FileBlock* fb = new FileBlock(off, io);

    XrdOucEnv myEnv;
    std::string fname;
    m_cache.getFilePathFromURL(io->Path(), fname);
    std::stringstream ss;
    ss  << fname;
    char offExt[64];
    // file format blabla_<blockSize>-<offset>
    sprintf(&offExt[0],".___%lld-%lld", m_blockSize, off );
    ss << &offExt[0];
    fname = ss.str();

    aMsgIO(kDebug, io, "FileBlock::FileBlock(), create XrdFileCachePrefetch.");
    fb->m_prefetch = new Prefetch(*io, fname, off, blocksize);
    pthread_t tid;
    XrdSysThread::Run(&tid, PrefetchRunnerBl, (void *)fb->m_prefetch, 0, "XrdHdfsCache Prefetcher");

    return fb;
}

int
IOBlock::Read (char *buff, long long off, int size)
{
    long long off0 = off;
    int idx_first = off0/m_blockSize;
    int idx_last = (off0+size-1)/m_blockSize;

    int bytes_read = 0;
    aMsgIO(kDebug, &m_io, "IOBlock::Read() %lld@%d block range [%d-%d] \n", off, size, idx_first, idx_last);

    for (int blockIdx = idx_first; blockIdx <= idx_last; ++blockIdx )
    {
        // locate block
        FileBlock* fb;
        m_mutex.Lock();
        std::map<int, FileBlock*>::iterator it = m_blocks.find(blockIdx);
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
                aMsgIO(kDebug, &m_io , "IOBlock::Read() last block, change output file size to %lld \n", pbs);
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
                aMsgIO(kDebug, &m_io , "IOBlock::Read() %s", "Read partially till the end of the block");
            }
            else if (blockIdx == idx_last)
            {
                readBlockSize = (off0+size) - blockIdx*m_blockSize;
                aMsgIO(kDebug, &m_io , "IOBlock::Read() s" , "Read partially from beginning of block");
            }
            else
            {
                readBlockSize = m_blockSize;
            }
        }
        assert(readBlockSize);

        aMsgIO(kInfo, &m_io, "IOBlock::Read() block[%d] read-block-size[%d], offset[%lld]", blockIdx, readBlockSize, off);

      
        // pass offset unmodified

        long long min  = blockIdx*m_blockSize;
        if ( off >= min) { assert(0);} 
        assert(off+readBlockSize <= (min + m_blockSize));
        int retvalBlock = fb->m_prefetch->Read(buff , off - fb->m_offset0, size);

        aMsgIO(kDebug, &m_io,  "IOBlock::Read()  Block read returned %d", retvalBlock );
        if (retvalBlock >=0 )
        {
            bytes_read += retvalBlock;
            buff += retvalBlock;
            off += retvalBlock;
            readBlockSize -= retvalBlock;

            // cancel read if not succssful
            if (readBlockSize > 0)
            {
                aMsgIO( kInfo, &m_io , "IOBlock::Read() Can't read from prefetch remain = %d", readBlockSize);
                return bytes_read;   
            }
        }
        else
        {
            aMsgIO( kError, &m_io , "IOBlock::Read() read error, retval %d", retvalBlock);
            return retvalBlock;
        }
    }

    return bytes_read;
}


