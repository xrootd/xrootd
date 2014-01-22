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

#include <vector>
#include <stdio.h>
#include <sstream>
#include <fcntl.h>

#include "XrdFileCachePrefetch.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheLog.hh"

#include <XrdCl/XrdClFile.hh>
#include <XrdCl/XrdClXRootDResponses.hh>
#include "XrdOuc/XrdOucEnv.hh"

using namespace XrdFileCache;

Prefetch::Prefetch(XrdOucCacheIO &inputIO, std::string& disk_file_path, long long iOffset, long long iFileSize) :
   m_output(NULL),
   m_infoFile(NULL),
   m_input(inputIO),
   m_temp_filename(disk_file_path),
   m_offset(iOffset),
   m_fileSize(iFileSize),
   m_started(false),
   m_failed(false),
   m_stop(false),
   m_numMissBlock(0),
   m_numHitBlock(0),
   m_stateCond(0)    // We will explicitly lock the condition before use.
{
   xfcMsgIO(kDebug, &m_input, "Prefetch::Prefetch()");
}

Prefetch::~Prefetch()
{
   xfcMsgIO(kInfo, &m_input, "Prefetch::~Prefetch() hit[%d] miss[%d]",  m_numHitBlock, m_numMissBlock);

   // see if we have to shut down
   m_downloadStatusMutex.Lock();
   m_cfi.checkComplete();
   m_downloadStatusMutex.UnLock();

   if (m_started == false) return;

   if (m_cfi.isComplete() == false)
   {
      xfcMsgIO(kInfo, &m_input, "Prefetch::~Prefetch() file not complete...");
      fflush(stdout);
      XrdSysCondVarHelper monitor(m_stateCond);
      if (m_stop == false)
      {
         m_stop = true;
         xfcMsgIO(kInfo, &m_input, "Prefetch::~Prefetch() waiting to stop Run() thread ...");
         m_stateCond.Wait();
      }
   }

   // write statistics in *cinfo file
   AppendIOStatToFileInfo();

   xfcMsgIO(kInfo, &m_input, "Prefetch::~Prefetch close data file");

   if (m_output)
   {
      m_output->Close();
      delete m_output;
      m_output = NULL;
   }
   if (m_infoFile)
   {
      RecordDownloadInfo();
      xfcMsgIO(kInfo, &m_input, "Prefetch::~Prefetch close info file");

      m_infoFile->Close();
      delete m_infoFile;
      m_infoFile = NULL;
   }
}

//_________________________________________________________________________________________________


int PREFETCH_MAX_ATTEMPTS = 10;
int Prefetch::getBytesToRead(Task& task, int block) const
{
   if (block == (m_cfi.getSizeInBits() -1))
   {
      return m_fileSize - task.lastBlock * m_cfi.getBufferSize();
   }
   else
   {
      return m_cfi.getBufferSize();
   }
}

void Prefetch::Run()
{
   {
      XrdSysCondVarHelper monitor(m_stateCond);
      if (m_started)
      {
         return;
      }
      m_started = true;

      if ( !Open())
      {
         m_failed = true;

         // Broadcast to possible io-read waiting objects
         m_stateCond.Broadcast();

         return;
      }
   }
   assert(m_infoFile);
   xfcMsgIO(kDebug, &m_input, "Prefetch::Run()");

   std::vector<char> buff;
   buff.reserve(m_cfi.getBufferSize());
   int retval = 0;

   Task task;
   while (GetNextTask(task))
   {
      if ((retval < 0) && (retval != -EINTR))
      {
         break;
      }

      xfcMsgIO(kDebug, &m_input, "Prefetch::Run() new task block[%d, %d], condVar [%c]", task.firstBlock, task.lastBlock, task.condVar ? 'x' : 'o');
      for (int block = task.firstBlock; block <= task.lastBlock; block++)
      {
         bool already;
         m_downloadStatusMutex.Lock();
         already = m_cfi.testBit(block);
         //  m_cfi.print();
         m_downloadStatusMutex.UnLock();
         if (already)
         {
            xfcMsgIO(kDump, &m_input, "Prefetch::Run() block [%d] already done, continue ...", block);
            continue;
         }
         else
         {
            xfcMsgIO(kDump, &m_input, "Prefetch::Run() download block [%d]", block);
         }

         int numBytes = getBytesToRead(task, block);
         // read block into buffer
         {
            int missing =  numBytes;
            long long offset = block * m_cfi.getBufferSize();
            int cnt = 0;
            while (missing)
            {

               retval = m_input.Read(&buff[0], offset + m_offset, missing);

               if (retval < 0)
               {
                  xfcMsgIO(kWarning, &m_input, "Prefetch::Run() Failed for negative ret %d block %d", retval, block);
                  XrdSysCondVarHelper monitor(m_stateCond);
                  m_failed = true;
                  retval = -EINTR;
                  break;
               }

               missing -= retval;
               offset += retval;
               cnt++;
               if (cnt > PREFETCH_MAX_ATTEMPTS)
               {
                  xfcMsgIO(kError, &m_input, "Prefetch::Run() too many attempts\n");
                  m_failed = true;
                  retval = -EINTR;
                  break;
               }

               if (missing)
               {
                  xfcMsgIO(kWarning, &m_input, "Prefetch::Run() reattemot writing missing %d for block %d", missing, block);
               }
            }
         }

         // write block buffer into disk file
         {
            long long offset = block * m_cfi.getBufferSize();
            int buffer_remaining = numBytes;
            int buffer_offset = 0;
            while ((buffer_remaining > 0) &&     // There is more to be written
                   (((retval = m_output->Write(&buff[buffer_offset], offset + buffer_offset, buffer_remaining)) != -1)
                    || (errno == EINTR)))      // Write occurs without an error
            {
               buffer_remaining -= retval;
               buffer_offset += retval;
               if (buffer_remaining)
               {
                  xfcMsgIO(kWarning, &m_input, "Prefetch::Run() reattemot writing missing %d for block %d", buffer_remaining, block);
               }
            }
         }

         // set downloaded bits
         xfcMsgIO(kDump, &m_input, "Prefetch::Run() set bit for block [%d]", block);
         m_downloadStatusMutex.Lock();
         m_cfi.setBit(block);
         m_downloadStatusMutex.UnLock();


         // statistics
         if (task.condVar)
            m_numMissBlock++;
         else
            m_numHitBlock++;
         if (m_numHitBlock % 10)
            RecordDownloadInfo();

         task.cntFetched++;
      }   // loop blocks in task


      xfcMsgIO(kDebug, &m_input, "Prefetch::Run() task completed ");
      if (task.condVar)
      {
         xfcMsgIO(kDebug, &m_input, "Prefetch::Run() task *Signal* begin");
         XrdSysCondVarHelper(*task.condVar);
         task.condVar->Signal();
         xfcMsgIO(kDebug, &m_input, "Prefetch::Run() task *Signal* end");
      }


      // after completeing a task, check if IO wants to break
      if (m_stop)
      {
         xfcMsgIO(kDebug, &m_input, "Prefetch::Run() %s", "stopping for a clean cause");
         retval = -EINTR;
         m_stateCond.Signal();
         return;
      }

   }  // loop tasks
   m_cfi.checkComplete();
   xfcMsgIO(kDebug, &m_input, "Prefetch::Run() exits, download %s  !", m_cfi.isComplete() ? " completed " : "unfinished");


   RecordDownloadInfo();
} // end Run()


//______________________________________________________________________________
void Prefetch::Task::Dump()
{
   xfcMsg(kDebug, "Task firstBlock = %d, lastBlock =  %d,  cond = %p", firstBlock, lastBlock, (void*)condVar);
}

//______________________________________________________________________________

bool Prefetch::Open()
{
   xfcMsgIO(kDebug, &m_input, "Prefetch::Open() open file for disk cache");
   XrdOss  &m_output_fs =  *Factory::GetInstance().GetOss();
   // Create the data file itself.
   XrdOucEnv myEnv;
   m_output_fs.Create(Factory::GetInstance().RefConfiguration().m_username.c_str(), m_temp_filename.c_str(), 0600, myEnv, XRDOSS_mkpath);
   m_output = m_output_fs.newFile(Factory::GetInstance().RefConfiguration().m_username.c_str());
   if (m_output)
   {
      int res = m_output->Open(m_temp_filename.c_str(), O_RDWR, 0600, myEnv);
      if ( res < 0)
      {
         xfcMsgIO(kError, &m_input, "Prefetch::Open() can't get data-FD for %s", m_temp_filename.c_str());
         delete m_output;
         m_output = NULL;
         return false;
      }
   }
   // Create the info file
   std::string ifn = m_temp_filename + Info::m_infoExtension;
   m_output_fs.Create(Factory::GetInstance().RefConfiguration().m_username.c_str(), ifn.c_str(), 0600, myEnv, XRDOSS_mkpath);
   m_infoFile = m_output_fs.newFile(Factory::GetInstance().RefConfiguration().m_username.c_str());
   if (m_infoFile)
   {

      int res = m_infoFile->Open(ifn.c_str(), O_RDWR, 0600, myEnv);
      if ( res < 0 )
      {
         xfcMsgIO(kError, &m_input, "Prefetch::Open() can't get info-FD %s ", ifn.c_str());
         delete m_output;
         m_output = NULL;
         delete m_infoFile;
         m_infoFile = NULL;

         return false;
      }
   }
   if ( m_cfi.Read(m_infoFile) <= 0)
   {
      assert(m_fileSize > 0);
      int ss = (m_fileSize -1)/m_cfi.getBufferSize() + 1;
      xfcMsgIO(kInfo, &m_input, "Creating new file info with size %lld. Reserve space for %d blocks", m_fileSize,  ss);
      m_cfi.resizeBits(ss);
      RecordDownloadInfo();
   }
   else
   {
      xfcMsgIO(kDebug, &m_input, "Info file already exists");
      // m_cfi.print();
   }

   return true;
}

//______________________________________________________________________________
void Prefetch::RecordDownloadInfo()
{
   xfcMsgIO(kDebug, &m_input, "Prefetch record Info file");
   m_cfi.WriteHeader(m_infoFile);
   m_infoFile->Fsync();
   // m_cfi.print();
}

//______________________________________________________________________________
void Prefetch::AddTaskForRng(long long offset, int size, XrdSysCondVar* cond)
{
   xfcMsgIO(kDebug, &m_input, "Prefetch::AddTask %lld %d cond= %p", offset, size, (void*)cond);
   m_downloadStatusMutex.Lock();
   int first_block = offset / m_cfi.getBufferSize();
   int last_block  = (offset + size -1)/ m_cfi.getBufferSize();
   m_tasks_queue.push(Task(first_block, last_block, cond));
   m_downloadStatusMutex.UnLock();
}
//______________________________________________________________________________



bool Prefetch::GetNextTask(Task& t )
{
   bool res = false;
   m_quequeMutex.Lock();
   if (m_tasks_queue.empty())
   {
      // give one block-attoms which has not been downloaded from beginning to end
      m_downloadStatusMutex.Lock();
      for (int i = 0; i < m_cfi.getSizeInBits(); ++i)
      {
         if (m_cfi.testBit(i) == false)
         {
            t.firstBlock = i;
            t.lastBlock = t.firstBlock;
            t.condVar = 0;

            xfcMsgIO(kDebug, &m_input, "Prefetch::GetNextTask() read first undread block");
            res = true;
            break;
         }
      }

      m_downloadStatusMutex.UnLock();
   }
   else
   {
      xfcMsgIO(kDebug, &m_input, "Prefetch::GetNextTask() from queue");
      t = m_tasks_queue.front();
      m_tasks_queue.pop();
      res = true;
   }
   m_quequeMutex.UnLock();

   return res;
}



//______________________________________________________________________________

bool Prefetch::GetStatForRng(long long offset, int size, int& pulled, int& nblocks)
{
   int first_block = offset /m_cfi.getBufferSize();
   int last_block  = (offset + size -1)/ m_cfi.getBufferSize();
   nblocks         = last_block - first_block + 1;

   // check if prefetch is initialised
   {
      XrdSysCondVarHelper monitor(m_stateCond);

      // Alternatively it could wait. Code temporarily commented out.

      if (m_failed || !m_started ) return false;
      /*
         if ( ! m_started)
         {
          m_stateCond.Wait();
          if (m_failed) return false;
          }*/
   }

   pulled = 0;
   m_downloadStatusMutex.Lock();
   if (m_cfi.isComplete())
   {
      pulled = nblocks;
   }
   else
   {
      pulled = 0;
      for (int i = first_block; i <= last_block; ++i)
      {
         pulled += m_cfi.testBit(i);
      }
   }
   m_downloadStatusMutex.UnLock();

   xfcMsgIO(kDump, &m_input, "Prefetch::GetStatForRng() bolcksPulled[%d] needed[%d]", pulled, nblocks);

   return true;
}

//______________________________________________________________________________
void Prefetch::AppendIOStatToFileInfo()
{
   // lock in case several IOs want to write in *cinfo file
   m_downloadStatusMutex.Lock();
   if (m_infoFile)
   {
      m_cfi.AppendIOStat(&m_stats, (XrdOssDF*)m_infoFile);
   }
   else
   {
      xfcMsgIO(kWarning, &m_input, "Prefetch::AppendIOStatToFileInfo() info file not opened");
   }
   m_downloadStatusMutex.UnLock();
}

//______________________________________________________________________________

ssize_t Prefetch::Read(char *buff, off_t off, size_t size)
{
   xfcMsgIO(kDump, &m_input, "prefetch::Read()  off = %lld size = %lld.", off, size);
   int nbb;  // num of blocks needed
   int nbp;  //  num of blocks pulled
   ssize_t retval = 0;
   if ( GetStatForRng(off, size, nbp, nbb))
   {
      if (nbp < nbb)
      {
         {
            XrdSysCondVarHelper monitor(m_stateCond);
            if (m_stop) return 0;
         }
         XrdSysCondVar newTaskCond(0);
         AddTaskForRng(off, size, &newTaskCond);
         XrdSysCondVarHelper xx(newTaskCond);
         newTaskCond.Wait();
         xfcMsgIO(kDump, &m_input, "IO::Read() use prefetch, cond.Wait() finsihed.");
      }

      retval =  m_output->Read(buff, off, size);

      m_stats.HitsPrefetch += 1;
      m_stats.BytesCachedPrefetch += nbp * m_cfi.getBufferSize();
      m_stats.BytesPrefetch += (nbb - nbp) * m_cfi.getBufferSize();
      m_stats.Hits += nbp;
      m_stats.Miss += nbb-nbp;
   }
   else
   {
      xfcMsgIO(kWarning, &m_input, "Prefetch::Read() failed to get status for read off = %lld size = %lld.", off, size);
      if (m_stop || m_failed)
         retval = -1;
      else
         retval = 0;
   }

   return retval;
}



