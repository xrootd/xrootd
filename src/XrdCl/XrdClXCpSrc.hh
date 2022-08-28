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

#ifndef SRC_XRDCL_XRDCLXCPSRC_HH_
#define SRC_XRDCL_XRDCLXCPSRC_HH_

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClSyncQueue.hh"
#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{

class XCpCtx;

class XCpSrc
{
    friend class ChunkHandler;

  public:

    /**
     * Constructor.
     *
     * @param chunkSize : default chunk size
     * @param parallel  : number of parallel chunks
     * @param fileSize  : file size if available (e.g. in metalink file),
     *                    should be set to -1 if not available, in this case
     *                    a stat will be performed during initialization
     * @param ctx       : Extreme Copy context
     */
    XCpSrc( uint32_t chunkSize, uint8_t parallel, int64_t fileSize, XCpCtx *ctx );

    /**
     * Creates new thread with XCpSrc::Run as the start routine.
     */
    void Start();

    /**
     * Stops the thread.
     */
    void Stop()
    {
      pRunning = false;
    }

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
    XCpSrc* Self()
    {
      XrdSysMutexHelper lck( pMtx );
      ++pRefCount;
      return this;
    }

    /**
     * @return : true if the thread is running, false otherwise
     */
    bool IsRunning()
    {
      return pRunning;
    }

    /**
     * @return true if the source has a block of non zero
     *         size / some chunks allocated, false otherwise
     */
    bool HasData()
    {
      XrdSysMutexHelper lck( pMtx );
      return pCurrentOffset < pBlkEnd || !pRecovered.empty() || !pOngoing.empty();
    }



    /**
     * Get the transfer rate for current source
     *
     * @return : transfer rate for current source [B/s]
     */
    uint64_t TransferRate();

    /**
     * Delete ChunkInfo object, and set the pointer to null.
     *
     * @param chunk : the chunk to be deleted
     */
    static void DeleteChunk( PageInfo *&chunk )
    {
      if( chunk )
      {
        delete[] static_cast<char*>( chunk->GetBuffer() );
        delete   chunk;
        chunk = 0;
      }
    }

  private:

    /**
     * Destructor (private).
     *
     * Use Delelte() method to destroy the object.
     */
    virtual ~XCpSrc();

    /**
     * The start routine.
     */
    static void* Run( void* arg );

    /**
     * Initializes the object first.
     * Afterwards, starts the download.
     */
    void StartDownloading();

    /**
     * Initializes the object:
     * - Opens a file (retries with another
     *   URL, in case of failure)
     * - Stats the file if necessary
     * - Gets the first block (offset and size)
     *   for download
     *
     * @return : error in case the object could not be initialized
     */
    XRootDStatus Initialize();

    /**
     * Tries to open the file at the next available URL.
     * Moves all ongoing chunk to recovered.
     *
     * @return : error if run out of URLs to try,
     *           success otherwise
     */
    XRootDStatus Recover();

    /**
     * Asynchronously reads consecutive chunks.
     *
     * @return : operation status:
     *           - suContinue : I still have work to do
     *           - suPartial  : I only have ongoing transfers,
     *                          but the block has been consumed
     *           - suDone     : We are done, the block has been
     *                          consumed, there are no ongoing
     *                          transfers, and there are no new
     *                          data
     */
    XRootDStatus ReadChunks();

    /**
     * Steal work from given source.
     *
     * - if it is a failed source we can have everything
     * - otherwise, if the source has a block of size
     *   greater than 0, steal respective fraction of
     *   the block
     * - otherwise, if the source has recovered chunks,
     *   steal respective fraction of those chunks
     * - otherwise, steal respective fraction of ongoing
     *   chunks, if we are a faster source
     *
     * @param src : the source from whom we are stealing
     */
    void Steal( XCpSrc *src );

    /**
     * Get more work.
     * First try to get a new block.
     * If there are no blocks remaining,
     * try stealing from others.
     *
     * @return : error if didn't got any data to transfer
     */
    XRootDStatus GetWork();

    /**
     * This method is used by ChunkHandler to report the result of a write,
     * to the source object.
     *
     * @param status : operation status
     * @param chunk  : the read chunk (if operation failed, should be null)
     * @param handle : the file object used to read the chunk
     */
    void ReportResponse( XRootDStatus *status, PageInfo *chunk, File *handle );

    /**
     * Delets a pointer and sets it to null.
     */
    template<typename T>
    static void DeletePtr( T *&obj )
    {
      delete obj;
      obj = 0;
    }

    /**
     * Check if two file object point to the same URL.
     *
     * @return : true if both files point to the same URL,
     *           false otherwise
     */
    static bool FilesEqual( File *f1, File *f2 )
    {
      if( !f1 || !f2 ) return false;

      const std::string lastURL = "LastURL";
      std::string url1, url2;

      f1->GetProperty( lastURL, url1 );
      f2->GetProperty( lastURL, url2 );

      // remove cgi information
      size_t pos = url1.find( '?' );
      if( pos != std::string::npos )
        url1 = url1.substr( 0 , pos );
      pos = url2.find( '?' );
      if( pos != std::string::npos )
        url2 = url2.substr( 0 , pos );

      return url1 == url2;
    }

    /**
     * Default chunk size
     */
    uint32_t                      pChunkSize;

    /**
     * Number of parallel chunks
     */
    uint8_t                       pParallel;

    /**
     * The file size
     */
    int64_t                       pFileSize;

    /**
     * Thread id
     */
    pthread_t                     pThread;

    /**
     * Extreme Copy context
     */
    XCpCtx                       *pCtx;

    /**
     * Source URL.
     */
    std::string                   pUrl;

    /**
     * Handle to the file.
     */
    File                         *pFile;

    std::map<File*, uint8_t>      pFailed;

    /**
     * The offset of the next chunk to be transferred.
     */
    uint64_t                      pCurrentOffset;

    /**
     * End of the our block.
     */
    uint64_t                      pBlkEnd;

    /**
     * Total number of data transferred from this source.
     */
    uint64_t                      pDataTransfered;

    /**
     * A map of ongoing transfers (the offset is the key,
     * the chunk size is the value).
     */
    std::map<uint64_t, uint64_t>  pOngoing;

    /**
     * A map of stolen chunks (again the offset is the key,
     * the chunk size is the value).
     */
    std::map<uint64_t, uint64_t>  pRecovered;

    /**
     * Sync queue with reports (statuses) from async reads
     * that have been issued. An error appears only once
     * per URL (independently of how many concurrent async
     * reads are allowed).
     */
    SyncQueue<XRootDStatus*>      pReports;

    /**
     * A mutex guarding the object.
     */
    XrdSysRecMutex                pMtx;

    /**
     * Reference counter
     */
    size_t                        pRefCount;

    /**
     * A flag, true means the source is running,
     * false means the source has been stopped,
     * or failed.
     */
    bool                          pRunning;

    /**
     * The time when we started / restarted  chunks
     */
    time_t                        pStartTime;

    /**
     * The total time we were transferring data, before
     * the restart
     */
    time_t                        pTransferTime;

    /**
     * The total time we were transferring data, before
     * the restart
     */
    bool                          pUsePgRead;
};

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLXCPSRC_HH_ */
