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

      protected:
         //! Read from disk file.
         ssize_t Read(char * buff, off_t offset, size_t size);

         //! Write cache statistics in *cinfo file.
         void AppendIOStatToFileInfo();

   private:
         //----------------------------------------------------------------------
         //! A prefetching task -- a file region that requires preferential treatment.
         //----------------------------------------------------------------------
         struct Task
         {
            int            firstBlock; //!< begin download firstBlock*bufferSize
            int            lastBlock;  //!< end download lastBlock*bufferSize
            XrdSysCondVar *condVar;    //!< signal when complete

            Task(int fb = 0, int lb = 0, XrdSysCondVar* iCondVar = 0) :
               firstBlock(fb), lastBlock(lb), condVar(iCondVar) {}

            ~Task() {}
         };

         //! Adds a new task in queue.
         void AddTaskForRng(long long offset, int size, XrdSysCondVar* cond);

         //! Checks status of downloaded fragments
         bool GetStatForRng(long long offset, int size, int& pulled, int& nblocks);

         //! Stop Run thread.
         void CloseCleanly();

         //! Get blocks to prefetch.
         bool GetNextTask(Task&);

         //! Open file handle for data file and info file on local disk.
         bool Open();

         //! Write download state into cinfo file
         void RecordDownloadInfo();

         //! Get number of bytes to read for given block.
         int  GetBytesToRead(Task& task, int block) const;

         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         XrdOssDF       *m_output;         //!< file handle for data file on disk
         XrdOssDF       *m_infoFile;       //!< file handle for data-info file on disk
         Info            m_cfi;            //!< download status of file blocks and access statistics
         XrdOucCacheIO  &m_input;          //!< original data source
         std::string     m_temp_filename;  //!< filename of data file on disk
         long long       m_offset;         //!< offset of cached file for block-based operation
         long long       m_fileSize;       //!< size of cached disk file for block-based operation


         bool            m_started;  //!< state of run thread
         bool            m_failed;   //!< reading from original source or writing to disk has failed
         bool            m_stop;     //!< run thread should be stopped
         XrdSysCondVar   m_stateCond;

         XrdSysMutex      m_downloadStatusMutex; //!< mutex locking access to m_cfi object

         std::queue<Task> m_tasks_queue; //!< download queue
         XrdSysMutex      m_quequeMutex;

         Stats            m_stats;      //!< cache statistics, used in IO detach
   };
}
#endif
