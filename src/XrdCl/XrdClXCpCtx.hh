//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef SRC_XRDCL_XRDCLXCPCTX_HH_
#define SRC_XRDCL_XRDCLXCPCTX_HH_

#include "XrdCl/XrdClSyncQueue.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <stdint.h>
#include <iostream>

namespace XrdCl
{

class XCpSrc;

class XCpCtx
{
  public:

    /**
     * Constructor
     *
     * @param urls           : list of replica urls
     * @param blockSize      : the default block size
     * @param parallelSrc    : maximum number of parallel sources
     * @param chunkSize      : the default chunk size
     * @param parallelChunks : the default number of parallel chunks per source
     * @param fileSize       : the file size if specified in the metalink file
     *                         (-1 indicates that the file size is not known and
     *                         a stat should be done)
     */
    XCpCtx( const std::vector<std::string> &urls, uint64_t blockSize, uint8_t parallelSrc, uint64_t chunkSize, uint64_t parallelChunks, int64_t fileSize );

    /**
     * Deletes the instance if the reference counter reached 0.
     */
    void Delete()
    {
      XrdSysMutexHelper lck( pMtx );
      --pRefCount;
      if( !pRefCount )
      {
        lck.UnLock();
        delete this;
      }
    }

    /**
     * Increments the reference counter.
     *
     * @return : myself.
     */
    XCpCtx* Self()
    {
      XrdSysMutexHelper lck( pMtx );
      ++pRefCount;
      return this;
    }

    /**
     * Gets the next URL from the list of file replicas
     *
     * @param url : the output parameter
     * @return    : true if a url has been written to the
     *              url parameter, false otherwise
     */
    bool GetNextUrl( std::string & url );

    /**
     * Get the 'weakest' sources
     *
     * @param exclude : the source that is excluded from the
     *                  search
     * @return        : the weakest source
     */
    XCpSrc* WeakestLink( XCpSrc *exclude );

    /**
     * Put a chunk into the sink
     *
     * @param chunk : the chunk
     */
    void PutChunk( ChunkInfo* chunk );

    /**
     * Get next block that has to be transfered
     *
     * @return : pair of offset and block size
     */
    std::pair<uint64_t, uint64_t> GetBlock();

    /**
     * Set the file size (GetSize will block until
     * SetFileSize will be called).
     * Also calculates the block size.
     *
     * @param size : file size
     */
    void SetFileSize( int64_t size );

    /**
     * Get file size. The call blocks until the file
     * size is being set using SetFileSize.
     */
    int64_t GetSize()
    {
      XrdSysCondVarHelper lck( pFileSizeCV );
      while( pFileSize < 0 && GetRunning() > 0 ) pFileSizeCV.Wait();
      return pFileSize;
    }

    /**
     * Starts one thread per source, each thread
     * tries to open a file, stat the file if necessary,
     * and then starts reading the file, all chunks read
     * go to the sink.
     *
     * @return Error if we were not able to create any threads
     */
    XRootDStatus Initialize();

    /**
     * Gets the next chunk from the sink, if the sink is empty blocks.
     *
     * @param ci : the chunk retrieved from sink (output parameter)
     * @retrun   : stError if we failed to transfer the file,
     *             stOK otherwise, with one of the following codes:
     *             - suDone     : the whole file has been transfered,
     *                            we are done
     *             - suContinue : a chunk has been written into ci,
     *                            continue calling GetChunk in order
     *                            to retrieve remaining chunks
     *             - suRetry    : a chunk has not been written into ci,
     *                            try again.
     */
    XRootDStatus GetChunk( XrdCl::ChunkInfo &ci );

    /**
     * Remove given source
     *
     * @param src : the source to be removed
     */
    void RemoveSrc( XCpSrc *src )
    {
      XrdSysMutexHelper lck( pMtx );
      pSources.remove( src );
    }

    /**
     * Notify idle sources, used in two case:
     * - if one of the sources failed and an
     *   idle source needs to take over
     * - or if we are done and all idle source
     *   should be stopped
     */
    void NotifyIdleSrc();

    /**
     * Returns true if all chunks have been transfered,
     * otherwise blocks until NotifyIdleSrc is called,
     * or a 1 minute timeout occurs.
     *
     * @return : true is all chunks have been transfered,
     *           false otherwise.
     */
    bool AllDone();

    /**
     * Notify those who are waiting for initialization.
     * In particular the GetSize() caller will be waiting
     * on the result of initialization.
     */
    void NotifyInitExpectant()
    {
      pFileSizeCV.Broadcast();
    }


  private:

    /**
     * Returns the number of active sources
     *
     * @return : number of active sources
     */
    size_t GetRunning();

    /**
     * Destructor (private).
     *
     * Use Delelte to destroy the object.
     */
    virtual ~XCpCtx();

    /**
     * The URLs of all the replicas that were provided
     * to us.
     */
    std::queue<std::string>    pUrls;

    /**
     * The size of the block allocated to a single  source.
     */
    uint64_t                   pBlockSize;

    /**
     * Number of parallel sources.
     */
    uint8_t                    pParallelSrc;

    /**
     * Chunk size.
     */
    uint32_t                   pChunkSize;

    /**
     * Number of parallel chunks per source.
     */
    uint8_t                    pParallelChunks;

    /**
     * Offset in the file (everything before the offset
     * has been allocated, everything after the offset
     * needs to be allocated)
     */
    uint64_t                   pOffset;

    /**
     * File size.
     */
    int64_t                    pFileSize;

    /**
     * File Size conditional variable.
     * (notifies waiters if the file size has been set)
     */
    XrdSysCondVar              pFileSizeCV;

    /**
     * List of sources. Those pointers are not owned by
     * this object.
     */
    std::list<XCpSrc*>         pSources;

    /**
     * A queue shared between all the sources (producers),
     * and the extreme copy context (consumer).
     */
    SyncQueue<ChunkInfo*>      pSink;

    /**
     * Total amount of data received
     */
    uint64_t                   pDataReceived;

    /**
     * A flag, true if all chunks have been received and we are done,
     * false otherwise
     */
    bool                       pDone;

    /**
     * A condition variable, idle sources wait on this cond var until
     * we are done, or until one of the active sources fails.
     */
    XrdSysCondVar              pDoneCV;

    /**
     * A mutex guarding the object
     */
    XrdSysMutex                pMtx;

    /**
     * Reference counter
     */
    size_t                     pRefCount;
};

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLXCPCTX_HH_ */
