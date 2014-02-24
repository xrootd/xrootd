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

#include <stdio.h>
#include <sstream>
#include <fcntl.h>

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucEnv.hh"


#include "XrdFileCachePrefetch.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCache.hh"

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
   m_stateCond(0)    // We will explicitly lock the condition before use.
{
   clLog()->Debug(XrdCl::AppMsg, "Prefetch::Prefetch() %s", m_input.Path());
}

Prefetch::~Prefetch()
{
   // see if we have to shut down
   m_downloadStatusMutex.Lock();
   m_cfi.CheckComplete();
   m_downloadStatusMutex.UnLock();

   if (m_started == false) return;

   if (m_cfi.IsComplete() == false)
   {
      clLog()->Info(XrdCl::AppMsg, "Prefetch::~Prefetch() file not complete... %s", m_input.Path());
      fflush(stdout);
      XrdSysCondVarHelper monitor(m_stateCond);
      if (m_stop == false)
      {
         m_stop = true;
         clLog()->Info(XrdCl::AppMsg, "Prefetch::~Prefetch() waiting to stop Run() thread ... %s", m_input.Path());
         m_stateCond.Wait();
      }
   }

   // write statistics in *cinfo file
   AppendIOStatToFileInfo();

   clLog()->Info(XrdCl::AppMsg, "Prefetch::~Prefetch close data file %s", m_input.Path());

   if (m_output)
   {
      m_output->Close();
      delete m_output;
      m_output = NULL;
   }
   if (m_infoFile)
   {
      RecordDownloadInfo();
      clLog()->Info(XrdCl::AppMsg, "Prefetch::~Prefetch close info file %s", m_input.Path());

      m_infoFile->Close();
      delete m_infoFile;
      m_infoFile = NULL;
   }
}

//_________________________________________________________________________________________________


int PREFETCH_MAX_ATTEMPTS = 10;
int Prefetch::GetBytesToRead(Task& task, int block) const
{
   if (block == (m_cfi.GetSizeInBits() -1))
   {
      return m_fileSize - task.lastBlock * m_cfi.GetBufferSize();
   }
   else
   {
      return m_cfi.GetBufferSize();
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
      }
      // Broadcast to possible io-read waiting objects
      m_stateCond.Broadcast();

      if (m_failed) return;
      
   }
   assert(m_infoFile);
   clLog()->Debug(XrdCl::AppMsg, "Prefetch::Run() %s", m_input.Path());

   std::vector<char> buff;
   buff.reserve(m_cfi.GetBufferSize());
   int retval = 0;

   Task task;
   int numHitBlock = 0;
   while (GetNextTask(task))
   {
      if ((retval < 0) && (retval != -EINTR))
      {
         break;
      }

      clLog()->Debug(XrdCl::AppMsg, "Prefetch::Run() new task block[%d, %d], condVar [%c] %s", task.firstBlock, task.lastBlock, task.condVar ? 'x' : 'o', m_input.Path());
      for (int block = task.firstBlock; block <= task.lastBlock; block++)
      {
         bool already;
         m_downloadStatusMutex.Lock();
         already = m_cfi.TestBit(block);
         //  m_cfi.print();
         m_downloadStatusMutex.UnLock();
         if (already)
         {
            clLog()->Dump(XrdCl::AppMsg, "Prefetch::Run() block [%d] already done, continue ... %s", block, m_input.Path());
            continue;
         }
         else
         {
            clLog()->Dump(XrdCl::AppMsg, "Prefetch::Run() download block [%d] %s", block, m_input.Path());
         }

         int numBytes = GetBytesToRead(task, block);
         // read block into buffer
         {
            int missing =  numBytes;
            long long offset = block * m_cfi.GetBufferSize();
            int cnt = 0;
            while (missing)
            {

               retval = m_input.Read(&buff[0], offset + m_offset, missing);

               if (retval < 0)
               {
                  clLog()->Warning(XrdCl::AppMsg, "Prefetch::Run() Failed for negative ret %d block %d %s", retval, block, m_input.Path());
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
                  clLog()->Error(XrdCl::AppMsg, "Prefetch::Run() too many attempts\n %s", m_input.Path());
                  m_failed = true;
                  retval = -EINTR;
                  break;
               }

               if (missing)
               {
                  clLog()->Warning(XrdCl::AppMsg, "Prefetch::Run() reattempt writing missing %d for block %d %s", missing, block, m_input.Path());
               }
            }
         }

         // write block buffer into disk file
         {
            long long offset = block * m_cfi.GetBufferSize();
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
                  clLog()->Warning(XrdCl::AppMsg, "Prefetch::Run() reattempt writing missing %d for block %d %s", buffer_remaining, block, m_input.Path());
               }
            }
         }

         // set downloaded bits
         clLog()->Dump(XrdCl::AppMsg, "Prefetch::Run() set bit for block [%d] %s", block, m_input.Path());
         m_downloadStatusMutex.Lock();
         m_cfi.SetBit(block);
         m_downloadStatusMutex.UnLock();

         numHitBlock++;
         if (numHitBlock % 10)
            RecordDownloadInfo();

      }   // loop blocks in task


      clLog()->Debug(XrdCl::AppMsg, "Prefetch::Run() task completed  %s", m_input.Path());
      if (task.condVar)
      {
         clLog()->Debug(XrdCl::AppMsg, "Prefetch::Run() task *Signal* begin %s", m_input.Path());
         XrdSysCondVarHelper(*task.condVar);
         task.condVar->Signal();
         clLog()->Debug(XrdCl::AppMsg, "Prefetch::Run() task *Signal* end %s", m_input.Path());
      }


      // after completing a task, check if IO wants to break
      if (m_stop)
      {
         clLog()->Debug(XrdCl::AppMsg, "stopping for a clean cause %s", m_input.Path());
         retval = -EINTR;
         m_stateCond.Signal();
         return;
      }

   }  // loop tasks
   m_cfi.CheckComplete();
   clLog()->Debug(XrdCl::AppMsg, "Prefetch::Run() exits, download %s  !", m_cfi.IsComplete() ? " completed " : "unfinished %s", m_input.Path());


   RecordDownloadInfo();
} // end Run()


//______________________________________________________________________________

bool Prefetch::Open()
{
   clLog()->Debug(XrdCl::AppMsg, "Prefetch::Open() open file for disk cache %s", m_input.Path());
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
         clLog()->Error(XrdCl::AppMsg, "Prefetch::Open() can't get data-FD for %s %s", m_temp_filename.c_str(), m_input.Path());
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
         clLog()->Error(XrdCl::AppMsg, "Prefetch::Open() can't get info-FD %s  %s", ifn.c_str(), m_input.Path());
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
      int ss = (m_fileSize -1)/m_cfi.GetBufferSize() + 1;
      clLog()->Info(XrdCl::AppMsg, "Creating new file info with size %lld. Reserve space for %d blocks %s", m_fileSize,  ss, m_input.Path());
      m_cfi.ResizeBits(ss);
      RecordDownloadInfo();
   }
   else
   {
      clLog()->Debug(XrdCl::AppMsg, "Info file already exists %s", m_input.Path());
      // m_cfi.Print();
   }

   return true;
}

//______________________________________________________________________________
void Prefetch::RecordDownloadInfo()
{
   clLog()->Debug(XrdCl::AppMsg, "Prefetch record Info file %s", m_input.Path());
   m_cfi.WriteHeader(m_infoFile);
   m_infoFile->Fsync();
   // m_cfi.Print();
}

//______________________________________________________________________________
void Prefetch::AddTaskForRng(long long offset, int size, XrdSysCondVar* cond)
{
   clLog()->Debug(XrdCl::AppMsg, "Prefetch::AddTask %lld %d cond= %p %s", offset, size, (void*)cond, m_input.Path());
   m_downloadStatusMutex.Lock();
   int first_block = offset / m_cfi.GetBufferSize();
   int last_block  = (offset + size -1)/ m_cfi.GetBufferSize();
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
      // give one block-atoms which has not been downloaded from beginning to end
      m_downloadStatusMutex.Lock();
      for (int i = 0; i < m_cfi.GetSizeInBits(); ++i)
      {
         if (m_cfi.TestBit(i) == false)
         {
            t.firstBlock = i;
            t.lastBlock = t.firstBlock;
            t.condVar = 0;

            clLog()->Debug(XrdCl::AppMsg, "Prefetch::GetNextTask() read first undread block %s", m_input.Path());
            res = true;
            break;
         }
      }

      m_downloadStatusMutex.UnLock();
   }
   else
   {
      clLog()->Debug(XrdCl::AppMsg, "Prefetch::GetNextTask() from queue %s", m_input.Path());
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
   int first_block = offset /m_cfi.GetBufferSize();
   int last_block  = (offset + size -1)/ m_cfi.GetBufferSize();
   nblocks         = last_block - first_block + 1;

   // check if prefetch is initialized
   {
      XrdSysCondVarHelper monitor(m_stateCond);

      if (m_failed) return false;
      
      if ( ! m_started)
      {
         m_stateCond.Wait();
         if (m_failed) return false;
      }
   }

   pulled = 0;
   m_downloadStatusMutex.Lock();
   if (m_cfi.IsComplete())
   {
      pulled = nblocks;
   }
   else
   {
      pulled = 0;
      for (int i = first_block; i <= last_block; ++i)
      {
         pulled += m_cfi.TestBit(i);
      }
   }
   m_downloadStatusMutex.UnLock();

   clLog()->Dump(XrdCl::AppMsg, "Prefetch::GetStatForRng() bolcksPulled[%d] needed[%d] %s", pulled, nblocks, m_input.Path());

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
      clLog()->Warning(XrdCl::AppMsg, "Prefetch::AppendIOStatToFileInfo() info file not opened %s", m_input.Path());
   }
   m_downloadStatusMutex.UnLock();
}

//______________________________________________________________________________

ssize_t Prefetch::Read(char *buff, off_t off, size_t size)
{
   clLog()->Dump(XrdCl::AppMsg, "Prefetch::Read()  off = %lld size = %lld. %s", off, size, m_input.Path());
   int nbb;  // num of blocks needed
   int nbp;  // num of blocks pulled
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
         clLog()->Dump(XrdCl::AppMsg, "Prefetch::Read() cond.Wait() finsihed. %s", m_input.Path());
      }

      retval =  m_output->Read(buff, off, size);

      // statistics
      m_stats.m_BytesCached  += nbp * m_cfi.GetBufferSize();
      m_stats.m_BytesRemote  += (nbb - nbp) * m_cfi.GetBufferSize();

      if (nbp == nbb)
      {
         m_stats.m_HitsCached  += 1;
         m_stats.m_HitsPartial[11] += 1;
      }
      else if (nbp == 0)
      {
         m_stats.m_HitsRemote  += 1;
         m_stats.m_HitsPartial[0] += 1;
      }
      else
      {
         // [0] - 0;   [1] 0-10%, [2] 10-20% .... [10] 90-100%; [11] 100%
         int idx = (nbb - nbp)*10/nbb;
         m_stats.m_HitsPartial[idx] += 1;
      }
   }
   else
   {
      clLog()->Warning(XrdCl::AppMsg, "Prefetch::Read() failed to get status for read off = %lld size = %lld. %s", off, size, m_input.Path());
      if (m_stop || m_failed)
         retval = -1;
      else
         retval = 0;
   }

   return retval;
}



