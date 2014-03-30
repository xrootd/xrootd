#ifndef __XRDFILECACHE_PREFETCH_HH__
#define __XRDFILECACHE_PREFETCH_HH__
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

#include <string>
#include <queue>

#include "XrdCl/XrdClDefaultEnv.hh"

#include "XrdFileCacheInfo.hh"
#include "XrdFileCacheStats.hh"

class XrdOucIOVec;
namespace XrdCl
{
   class Log;
}

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Downloads data into a file on local disk and handles IO read requests.
   //----------------------------------------------------------------------------
   class Prefetch
   {
      friend class IOEntireFile;
      friend class IOFileBlock;
      enum ReadRamState_t { kReadWait, kReadSuccess, kReadFailed};

      struct Task;
      public:
         //------------------------------------------------------------------------
         //! Constructor.
         //------------------------------------------------------------------------
         Prefetch(XrdOucCacheIO& inputFile, std::string& path,
                  long long offset, long long fileSize);

         //------------------------------------------------------------------------
         //! Destructor.
         //------------------------------------------------------------------------
         ~Prefetch();

         //---------------------------------------------------------------------
         //! Thread function for file prefetching.
         //---------------------------------------------------------------------
         void Run();

         //----------------------------------------------------------------------
         //! Reference to prefetch statistics.
         //----------------------------------------------------------------------
         Stats& GetStats() { return m_stats; }

         //----------------------------------------------------------------------
         //! Write block to file on disk. Called from Cache.
         //----------------------------------------------------------------------
         void WriteBlockToDisk(int ramIdx, size_t size);

         //----------------------------------------------------------------------
         //! Decrease block reference count.
         //----------------------------------------------------------------------
         void DecRamBlockRefCount(int ramIdx);

         //----------------------------------------------------------------------
         //! \brief Initiate close. Return true if still IO active.
         //! Used in XrdPosixXrootd::Close()
         //----------------------------------------------------------------------
         bool InitiateClose();

      protected:
         //! Read from disk, RAM, task, or client.
         ssize_t Read(char * buff, off_t offset, size_t size);

         //! Vector read from disk if block is already downloaded, else ReadV from client.
         int ReadV (const XrdOucIOVec *readV, int n);

         //! Write cache statistics in *cinfo file.
         void AppendIOStatToFileInfo();

   private:
         //----------------------------------------------------------------------
         //! A prefetching task -- a file region that requires preferential treatment.
         //----------------------------------------------------------------------
         struct Task
         {
            int            ramBlockIdx;  //!< idx in the in-memory buffer
            size_t         size;         //!< cached, used for the end file block
            XrdSysCondVar *condVar;      //!< signal when complete

            Task(): ramBlockIdx(-1), size(0), condVar(0) {}
            Task(int r, size_t s, XrdSysCondVar *cv):
                ramBlockIdx(r), size(s), condVar(cv) {}
           ~Task() {}
         };

         struct RAMBlock {
             int  fileBlockIdx; //!< offset in output file
             int  refCount;     //!< read and write reference count
             bool fromRead;     //!< is ram requested from prefetch or read
             ReadRamState_t status;       //!< read from client status
             int readErrno; //!< posix error on read fail

             RAMBlock():fileBlockIdx(-1), refCount(0), fromRead(false), status(kReadWait) {}
         };

         struct RAM
         {
           int         m_numBlocks;    //!< number of in memory blocks
           char*       m_buffer;       //!< buffer m_numBlocks x size_of_block
           RAMBlock*   m_blockStates;  //!< referenced structure
           XrdSysCondVar m_writeMutex;   //!< write mutex

           RAM();
           ~RAM();
         };

         //! Stop Run thread.
         void CloseCleanly();

         //! Get blocks to prefetch.
         Task* GetNextTask();

         //! Open file handle for data file and info file on local disk.
         bool Open();

         //! Write download state into cinfo file.
         void RecordDownloadInfo();

         //! Short log alias.
         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         //! Split read in blocks.
         ssize_t ReadInBlocks( char* buff, off_t offset, size_t size);

         //! Prefetch block.
         Task*   CreateTaskForFirstUndownloadedBlock();

         //! Create task from read request and wait its completed.
         bool    ReadFromTask(int bIdx, char* buff, long long off, size_t size);

         //! Read from client into in memory cache, queue ram buffer for disk write.
         void    DoTask(Task* task);

         RAM             m_ram;            //!< in memory cache

         XrdOssDF       *m_output;         //!< file handle for data file on disk
         XrdOssDF       *m_infoFile;       //!< file handle for data-info file on disk
         Info            m_cfi;            //!< download status of file blocks and access statistics
         XrdOucCacheIO  &m_input;          //!< original data source

         std::string     m_temp_filename;  //!< filename of data file on disk
         long long       m_offset;         //!< offset of cached file for block-based operation
         long long       m_fileSize;       //!< size of cached disk file for block-based operation

         bool            m_started;   //!< state of run thread
         bool            m_failed;    //!< reading from original source or writing to disk has failed
         bool            m_stopping;  //!< run thread should be stopped
         bool            m_stopped;   //!< prefetch is stopped
         XrdSysCondVar   m_stateCond; //!< state condition variable

         XrdSysMutex      m_downloadStatusMutex; //!< mutex locking access to m_cfi object

         std::deque<Task*> m_tasks_queue;  //!< download queue
         XrdSysCondVar     m_queueCond;    //!< m_tasks_queue condition variable

         Stats            m_stats;      //!< cache statistics, used in IO detach
   };
}
#endif
