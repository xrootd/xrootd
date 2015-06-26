//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel
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


#include "XrdFileCacheFile.hh"


#include <stdio.h>
#include <sstream>
#include <fcntl.h>
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdPosix/XrdPosixFile.hh"
#include "XrdPosix/XrdPosix.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCache.hh"
#include "Xrd/XrdScheduler.hh"

using namespace XrdFileCache;

namespace XrdPosixGlobals
{
   extern XrdScheduler *schedP;
}

namespace
{
const int PREFETCH_MAX_ATTEMPTS = 10;
class DiskSyncer : public XrdJob
{
private:
   File *m_file;
public:
   DiskSyncer(File *pref, const char *desc="") :
      XrdJob(desc),
      m_file(pref)
   {}
   void DoIt()
   {
      m_file->Sync();
   }
};
}

namespace
{
   Cache* cache() {return Factory::GetInstance().GetCache();}
}

File::File(XrdOucCacheIO &inputIO, std::string& disk_file_path, long long iOffset, long long iFileSize) :
m_input(inputIO),
m_output(NULL),
m_infoFile(NULL),

m_temp_filename(disk_file_path),
m_offset(iOffset),
m_fileSize(iFileSize),
m_stopping(false),
m_stateCond(0), // We will explicitly lock the condition before use.

m_syncer(new DiskSyncer(this, "XrdFileCache::DiskSyncer")),
m_non_flushed_cnt(0),
m_in_sync(false),
m_downloadCond(0),
m_prefetchReadCnt(0),
m_prefetchHitCnt(0),
m_prefetchScore(1),
m_prefetchCurrentCnt(0)
{
   clLog()->Debug(XrdCl::AppMsg, "File::File() %s", m_input.Path());
   Open();
}

File::~File()
{
   clLog()->Debug(XrdCl::AppMsg, "File::~File() %p %s", (void*)this, lPath());
   cache()->RemoveWriteQEntriesFor(this);
   clLog()->Info(XrdCl::AppMsg, "File::~File() check write queues ...%s", lPath());

   // can I do anythong to stop waiting for asyc read callbacks ?
   while (true)
   {
      m_stateCond.Lock();
      bool isStopped = m_stopping;
      bool isPrefetching = (m_prefetchCurrentCnt > 0);
      m_stateCond.UnLock();
      if ((isPrefetching == false) && isStopped)
      {
         m_downloadCond.Lock();
         bool blockMapEmpty =  m_block_map.empty();
         m_downloadCond.UnLock();
         if ( blockMapEmpty)
            break;
      }
      XrdSysTimer::Wait(10);
   }
   clLog()->Debug(XrdCl::AppMsg, "File::~File finished with writing %s",lPath() );


   // Wait disk sync
   bool do_sync = false;
   {
      XrdSysMutexHelper _lck(&m_syncStatusMutex);
      if (m_non_flushed_cnt > 0)
      {
         do_sync = true;
         m_in_sync = true;
         clLog()->Info(XrdCl::AppMsg, "File::~File sync unflushed %d\n", m_non_flushed_cnt);
      }
   }
   if (do_sync)
   {
      Sync();
   }
   // write statistics in *cinfo file

   // AMT append IO stat --- look new interface in master branch
   // XXXX MT -- OK, what needs to be here?
   AppendIOStatToFileInfo();
   // XXXX MT END

   clLog()->Info(XrdCl::AppMsg, "File::~File close data file %p",(void*)this , lPath());
   if (m_output)
   {
      m_output->Close();
      delete m_output;
      m_output = NULL;
   }
   if (m_infoFile)
   {
      clLog()->Info(XrdCl::AppMsg, "File::~File close info file");
      m_infoFile->Close();
      delete m_infoFile;
      m_infoFile = NULL;
   }
   delete m_syncer;
}

bool File::InitiateClose()
{
   // Retruns true if delay is needed
   clLog()->Debug(XrdCl::AppMsg, "File::Initiate close start", lPath());

   cache()->DeRegisterPrefetchFile(this);

   m_stateCond.Lock();
   m_stopping = true;
   m_stateCond.UnLock(); 
   if (m_cfi.IsComplete()) return false; // AMT maybe map size is here more meaningfull, but might hold block state lock
   return true;
}

//______________________________________________________________________________



//==============================================================================

bool File::Open()
{
   // clLog()->Debug(XrdCl::AppMsg, "File::Open() open file for disk cache %s", m_input.Path());

   XrdOss  &m_output_fs =  *Factory::GetInstance().GetOss();
   // Create the data file itself.
   XrdOucEnv myEnv;
   m_output_fs.Create(Factory::GetInstance().RefConfiguration().m_username.c_str(), m_temp_filename.c_str(), 0600, myEnv, XRDOSS_mkpath);
   m_output = m_output_fs.newFile(Factory::GetInstance().RefConfiguration().m_username.c_str());
   if (m_output)
   {
      int res = m_output->Open(m_temp_filename.c_str(), O_RDWR, 0600, myEnv);
      if (res < 0)
      {
         clLog()->Error(XrdCl::AppMsg, "File::Open() can't get data-FD for %s %s", m_temp_filename.c_str(), m_input.Path());
         delete m_output;
         m_output = 0;

         return false;
      }
   }
   else
   {
      clLog()->Error(XrdCl::AppMsg, "File::Open() can't get data holder ");
      return false;
   }

   // Create the info file
   std::string ifn = m_temp_filename + Info::m_infoExtension;
   m_output_fs.Create(Factory::GetInstance().RefConfiguration().m_username.c_str(), ifn.c_str(), 0600, myEnv, XRDOSS_mkpath);
   m_infoFile = m_output_fs.newFile(Factory::GetInstance().RefConfiguration().m_username.c_str());
   if (m_infoFile)
   {
      int res = m_infoFile->Open(ifn.c_str(), O_RDWR, 0600, myEnv);
      if (res < 0)
      {
         clLog()->Error(XrdCl::AppMsg, "File::Open() can't get info-FD %s  %s", ifn.c_str(), m_input.Path());
         delete m_infoFile;
         m_infoFile = 0;
         return false;
      }
   }
   else
   {
      return false;
   }

   if (m_cfi.Read(m_infoFile, Factory::GetInstance().RefConfiguration().m_prefetch) <= 0)
   {
      int ss = (m_fileSize - 1)/m_cfi.GetBufferSize() + 1;
      clLog()->Info(XrdCl::AppMsg, "Creating new file info with size %lld. Reserve space for %d blocks %s", m_fileSize,  ss, m_input.Path());
      m_cfi.ResizeBits(ss, Factory::GetInstance().RefConfiguration().m_prefetch);
      m_cfi.WriteHeader(m_infoFile);
   }
   else
   {
      clLog()->Debug(XrdCl::AppMsg, "Info file read from disk: %s", m_input.Path());
   }


   cache()->RegisterPrefetchFile(this);
   return true;
}


//==============================================================================
// Read and helpers
//==============================================================================



namespace
{
   bool overlap(int       blk,      // block to query
                long long blk_size, //
                long long req_off,  // offset of user request
                int       req_size, // size of user request
                // output:
                long long &off,     // offset in user buffer
                long long &blk_off, // offset in block
                long long &size)    // size to copy
   {
      const long long beg     = blk * blk_size;
      const long long end     = beg + blk_size;
      const long long req_end = req_off + req_size;

      if (req_off < end && req_end > beg)
      {
         const long long ovlp_beg = std::max(beg, req_off);
         const long long ovlp_end = std::min(end, req_end);

         off     = ovlp_beg - req_off;
         blk_off = ovlp_beg - beg;
         size    = ovlp_end - ovlp_beg;

         return true;
      }
      else
      {
         return false;
      }
   }
}

//------------------------------------------------------------------------------

Block* File::RequestBlock(int i, bool prefetch)
{
   // Must be called w/ block_map locked.
   // Checks on size etc should be done before.
   //
   // Reference count is 0 so increase it in calling function if you want to
   // catch the block while still in memory.
   clLog()->Debug(XrdCl::AppMsg, "RequestBlock() %d pOn=(%d)", i, prefetch);

   XrdCl::File &client = ((XrdPosixFile*)(&m_input))->clFile;

   const long long   BS = m_cfi.GetBufferSize();
   const int last_block = m_cfi.GetSizeInBits() - 1;

   long long off     = i * BS;
   long long this_bs = (i == last_block) ? m_input.FSize() - off : BS;

   Block *b = new Block(this, off, this_bs, prefetch); // should block be reused to avoid recreation
   m_block_map[i] = b;

   client.Read(off, this_bs, (void*)b->get_buff(), new BlockResponseHandler(b));

   clLog()->Debug(XrdCl::AppMsg, "RequestBlock() %d END", i);
   return b;
}

//------------------------------------------------------------------------------

int File::RequestBlocksDirect(DirectResponseHandler *handler, IntList_t& blocks,
                              char* req_buf, long long req_off, long long req_size)
{

    clLog()->Dump(XrdCl::AppMsg, "RequestBlockDirect %ld ", blocks.size());
   XrdCl::File &client = ((XrdPosixFile*)(&m_input))->clFile;

   const long long BS = m_cfi.GetBufferSize();

   // XXX Use readv to load more at the same time. 

   long long total = 0;

   for (IntList_i ii = blocks.begin() ; ii != blocks.end(); ++ii)
   {
      // overlap and request
      long long off;     // offset in user buffer
      long long blk_off; // offset in block
      long long size;    // size to copy

      overlap(*ii, BS, req_off, req_size, off, blk_off, size);

      client.Read(*ii * BS + blk_off, size, req_buf + off, handler);

      total += size;
   }

   return total;
}

//------------------------------------------------------------------------------

int File::ReadBlocksFromDisk(std::list<int>& blocks,
                             char* req_buf, long long req_off, long long req_size)
{

   clLog()->Dump(XrdCl::AppMsg, "File::ReadBlocksFromDisk %ld ", blocks.size());
   const long long BS = m_cfi.GetBufferSize();

   long long total = 0;

   // XXX Coalesce adjacent reads.

   for (IntList_i ii = blocks.begin() ; ii != blocks.end(); ++ii)
   {
      // overlap and read
      long long off;     // offset in user buffer
      long long blk_off; // offset in block
      long long size;    // size to copy

      overlap(*ii, BS, req_off, req_size, off, blk_off, size);

      long long rs = m_output->Read(req_buf + off, *ii * BS + blk_off, size);

      if (rs < 0)
         return rs;

      total += rs;

      CheckPrefetchStatDisk(*ii);
   } 

   return total;
}

//------------------------------------------------------------------------------

int File::Read(char* iUserBuff, long long iUserOff, int iUserSize)
{
   clLog()->Dump(XrdCl::AppMsg, "File::Read() begin ");

   const long long BS = m_cfi.GetBufferSize();

   // lock
   // loop over reqired blocks:
   //   - if on disk, ok;
   //   - if in ram or incoming, inc ref-count
   //   - if not available, request and inc ref count
   // before requesting the hell and more (esp. for sparse readv) assess if
   //   passing the req to client is actually better.
   // unlock

   m_downloadCond.Lock();

   size_t msize =  m_block_map.size();
   // XXX Check for blocks to free? Later ...

   const int idx_first = iUserOff / BS;
   const int idx_last  = (iUserOff + iUserSize - 1) / BS;

   BlockList_t  blks_to_process, blks_processed;
   IntList_t    blks_on_disk,    blks_direct;

   for (int block_idx = idx_first; block_idx <= idx_last; ++block_idx)
   {
      BlockMap_i bi = m_block_map.find(block_idx);  

      // In RAM or incoming?
      if (bi != m_block_map.end())
      {
         // XXXX if failed before -- retry if timestamp sufficient or fail?
         // XXXX Or just push it and handle errors in one place later?

         inc_ref_count(bi->second);
         blks_to_process.push_front(bi->second);
      }
      // On disk?
      else if (m_cfi.TestBit(block_idx))
      {
         blks_on_disk.push_back(block_idx);
      }
      // Then we have to get it ...
      else
      {
         // Is there room for one more RAM Block?
         if ( cache()->RequestRAMBlock())
         {
            clLog()->Debug(XrdCl::AppMsg, "File::Read() request block to fetch %d", block_idx);
            Block *b = RequestBlock(block_idx, false);
            inc_ref_count(b);
            blks_to_process.push_back(b);
            ++msize;
         }
         // Nope ... read this directly without caching.
         else
         {
            clLog()->Debug(XrdCl::AppMsg, "File::Read() direct block %d", block_idx);
            blks_direct.push_back(block_idx);
         }
      }
   }

   m_downloadCond.UnLock();

   long long bytes_read = 0;

   // First, send out any direct requests.
   // XXX Could send them all out in a single vector read.
   DirectResponseHandler *direct_handler = 0;
   int  direct_size = 0;

   if (!blks_direct.empty())
   {
      direct_handler = new DirectResponseHandler(blks_direct.size());

      direct_size = RequestBlocksDirect(direct_handler, blks_direct, iUserBuff, iUserOff, iUserSize);
      bytes_read += direct_size;  // AMT added
   }

   // Second, read blocks from disk.
   if (!blks_on_disk.empty()) {
       int rc = ReadBlocksFromDisk(blks_on_disk, iUserBuff, iUserOff, iUserSize);
       if (rc >= 0)
       {
           bytes_read += rc;
       }
       else
       {
           bytes_read = rc; // AMT ?? should there be an immediate return 
       }
   }
   // Third, loop over blocks that are available or incoming
   while ( ! blks_to_process.empty() && bytes_read >= 0) // AMT : do I need this loop ?
   {
       BlockList_t finished;

       {
           XrdSysCondVarHelper _lck(m_downloadCond);

           BlockList_i bi = blks_to_process.begin();
           while (bi != blks_to_process.end())
           {
               clLog()->Dump(XrdCl::AppMsg, "File::Read() searcing for blocks finished");
               if ((*bi)->is_finished())
               {
                   clLog()->Dump(XrdCl::AppMsg, "File::Read() found finished block");
                   finished.push_back(*bi);
                   BlockList_i bj = bi++;
                   blks_to_process.erase(bj);
               }
               else
               {
                   ++bi;
               }
           }

           if (finished.empty())
           {

               clLog()->Dump(XrdCl::AppMsg, "File::Read() wait block begin");

               m_downloadCond.Wait();

               clLog()->Dump(XrdCl::AppMsg, "File::Read() wait block end");

               continue;
           }
       }

      BlockList_i bi = finished.begin();
      while (bi != finished.end())
      {
         if ((*bi)->is_ok())
         {
           long long user_off;     // offset in user buffer
           long long off_in_block; // offset in block
           long long size_to_copy;    // size to copy

           clLog()->Dump(XrdCl::AppMsg, "File::Read() Block finished ok.");
           overlap((*bi)->m_offset/BS, BS, iUserOff, iUserSize, user_off, off_in_block, size_to_copy);
           memcpy(&iUserBuff[user_off], &((*bi)->m_buff[off_in_block]), size_to_copy);
           bytes_read += size_to_copy;

           CheckPrefetchStatRAM(*bi);
         }
         else // it has failed ... krap up.
         {
            clLog()->Error(XrdCl::AppMsg, "File::Read() Block finished with eorror.");
            bytes_read = -1;
            errno = (*bi)->m_errno;
            break;
         }
         ++bi;
      }

      std::copy(finished.begin(), finished.end(), std::back_inserter(blks_processed));
      finished.clear();
   }

   // Fourth, make sure all direct requests have arrived
   if (direct_handler != 0)
   {
      clLog()->Error(XrdCl::AppMsg, "File::Read() waiting for direct requests.");
      XrdSysCondVarHelper _lck(direct_handler->m_cond);

      if (direct_handler->m_to_wait > 0)
      {
         direct_handler->m_cond.Wait();
      }

      if (direct_handler->m_errno == 0)
      {
         bytes_read += direct_size;
      }
      else
      {
         errno = direct_handler->m_errno;
         bytes_read = -1;
      }

      delete direct_handler;
   }

   // Last, stamp and release blocks, release file.
   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      // XXXX stamp file
      // AMT ??? fetched status stampled in WriteDisk callback , what dies stamp mean ?? 

      // blks_to_process can be non-empty, if we're exiting with an error.
      std::copy(blks_to_process.begin(), blks_to_process.end(), std::back_inserter(blks_processed));

      for (BlockList_i bi = blks_processed.begin(); bi != blks_processed.end(); ++bi)
      {
         dec_ref_count(*bi);
         // XXXX stamp block
      }
   }

   return bytes_read;
}

//------------------------------------------------------------------------------

void File::WriteBlockToDisk(Block* b)
{
   int retval = 0;
   // write block buffer into disk file
   long long offset = b->m_offset - m_offset;
   long long size = (b->m_offset +  m_cfi.GetBufferSize()) > m_input.FSize() ? (m_input.FSize() - b->m_offset) : m_cfi.GetBufferSize();
   int buffer_remaining = size;
   int buffer_offset = 0;
   int cnt = 0;
   const char* buff = &b->m_buff[0];
   while ((buffer_remaining > 0) && // There is more to be written
          (((retval = m_output->Write(buff, offset + buffer_offset, buffer_remaining)) != -1)
           || (errno == EINTR))) // Write occurs without an error
   {
      buffer_remaining -= retval;
      buff += retval;
      cnt++;

      if (buffer_remaining)
      {
         clLog()->Warning(XrdCl::AppMsg, "File::WriteToDisk() reattempt[%d] writing missing %ld for block %d %s",
                          cnt, buffer_remaining, b->m_offset, lPath());
      }
      if (cnt > PREFETCH_MAX_ATTEMPTS)
      {
         clLog()->Error(XrdCl::AppMsg, "File::WriteToDisk() write failed too manny attempts %s", lPath());
         return;
      }
   }

   // set bit fetched
   clLog()->Dump(XrdCl::AppMsg, "File::WriteToDisk() success set bit for block [%ld] size [%d] %s", b->m_offset, size, lPath());
   int pfIdx =  (b->m_offset - m_offset)/m_cfi.GetBufferSize();

   m_downloadCond.Lock();
   m_cfi.SetBitFetched(pfIdx);
   m_downloadCond.UnLock();


   {
      XrdSysCondVarHelper _lck(m_downloadCond);
      dec_ref_count(b);
   }

   // set bit synced
   bool schedule_sync = false;
   {
      XrdSysMutexHelper _lck(&m_syncStatusMutex);

      if (m_in_sync)
      {
         m_writes_during_sync.push_back(pfIdx);
      }
      else
      {
         m_cfi.SetBitWriteCalled(pfIdx);
         ++m_non_flushed_cnt;
      }

      if (m_non_flushed_cnt >= 100)
      {
         schedule_sync     = true;
         m_in_sync         = true;
         m_non_flushed_cnt = 0;
      }
   }

   if (schedule_sync)
   {
      XrdPosixGlobals::schedP->Schedule(m_syncer);
   }
}

//------------------------------------------------------------------------------

void File::Sync()
{
   clLog()->Dump(XrdCl::AppMsg, "File::Sync %s", lPath());
   m_output->Fsync();
   m_cfi.WriteHeader(m_infoFile);
   int written_while_in_sync;
   {
      XrdSysMutexHelper _lck(&m_syncStatusMutex);
      for (std::vector<int>::iterator i = m_writes_during_sync.begin(); i != m_writes_during_sync.end(); ++i)
      {
         m_cfi.SetBitWriteCalled(*i);
      }
      written_while_in_sync = m_non_flushed_cnt = (int) m_writes_during_sync.size();
      m_writes_during_sync.clear();
      m_in_sync = false;
   }
   clLog()->Dump(XrdCl::AppMsg, "File::Sync %d blocks written during sync.", written_while_in_sync);
   m_infoFile->Fsync();
}


void File::inc_ref_count(Block* b)
{
   b->m_refcnt++;
}

void File::dec_ref_count(Block* b)
{
    // AMT this function could actually be member of File ... would be nicer
    // called under block lock
    b-> m_refcnt--;
    if ( b->m_refcnt == 0 ) {
        int i = b->m_offset/BufferSize();
        size_t ret = m_block_map.erase(i);
        if (ret != 1) {
            clLog()->Error(XrdCl::AppMsg, "File::OnBlockZeroRefCount did not erase %d from map.", i);
        }
        else {
           cache()->RAMBlockReleased();
        }
    }
}

//------------------------------------------------------------------------------

void File::ProcessBlockResponse(Block* b, XrdCl::XRootDStatus *status)
{
   clLog()->Dump(XrdCl::AppMsg, "File::ProcessBlockResponse %d ",(int)(b->m_offset/BufferSize()));

   m_downloadCond.Lock();

   if (status->IsOK()) 
   {
      b->m_downloaded = true;
      if (!m_stopping) { // AMT theoretically this should be under state lock, but then are double locks
        inc_ref_count(b);
        cache()->AddWriteTask(b, true);
      }
   }
   else
   {
      // AMT how long to keep?
      // when to retry?

      XrdPosixMap::Result(*status);

      b->set_error_and_free(errno);
      errno = 0;

      // ??? AMT temprary commented out
      // inc_ref_count(b);
   }

   m_downloadCond.Broadcast();

   m_downloadCond.UnLock();
}



 long long File::BufferSize() {
     return m_cfi.GetBufferSize();
 }

//______________________________________________________________________________


int File::ReadV (const XrdOucIOVec *readV, int n)
{
    return 0;
}

//______________________________________________________________________________
const char* File::lPath() const
{
return m_temp_filename.c_str();
}

// XXXX MT: is this needed ????
//______________________________________________________________________________
void File::AppendIOStatToFileInfo()
{
   // lock in case several IOs want to write in *cinfo file
   if (m_infoFile)
   {
      Info::AStat as;
      as.DetachTime = time(0);
      as.BytesDisk = m_stats.m_BytesDisk;
      as.BytesRam = m_stats.m_BytesRam;
      as.BytesMissed = m_stats.m_BytesMissed;
      m_cfi.AppendIOStat(as, (XrdOssDF*)m_infoFile);
   }
   else
   {
      clLog()->Warning(XrdCl::AppMsg, "Prefetch::AppendIOStatToFileInfo() info file not opened %s", lPath());
   }
}

//______________________________________________________________________________
void File::Prefetch()
{
   bool stopping = false;
   m_stateCond.Lock();
   stopping = m_stopping;
   m_stateCond.UnLock();

   
   if (!stopping) {

         clLog()->Dump(XrdCl::AppMsg, "Prefetch::Prefetch enter to check download status \n");
      XrdSysCondVarHelper _lck(m_downloadCond);
         clLog()->Dump(XrdCl::AppMsg, "Prefetch::Prefetch enter to check download status BEGIN \n");
      if (m_cfi.IsComplete() == false && m_block_map.size() < 1600)
      {
         clLog()->Debug(XrdCl::AppMsg, "Prefetch::Prefetch begin, block size %ld", m_block_map.size());

         // check index not on disk and not in RAM
         bool found = false;
         for (int f=0; f < m_cfi.GetSizeInBits(); ++f)
         {
            clLog()->Dump(XrdCl::AppMsg, "Prefetch::Prefetch test bit %d", f);
            if (!m_cfi.TestBit(f))
            {    
               BlockMap_i bi = m_block_map.find(f);
               if (bi == m_block_map.end()) {
                  clLog()->Dump(XrdCl::AppMsg, "Prefetch::Prefetch take block %d", f);
                  Block *b = RequestBlock(f, true);
                  inc_ref_count(b);
                  m_prefetchReadCnt++;
                  m_prefetchScore = m_prefetchHitCnt/m_prefetchReadCnt;
                  found = true;
                  break;
               }
            }
         }
         if (!found)  { 
            clLog()->Dump(XrdCl::AppMsg, "Prefetch::Prefetch no free blcok found ");
            m_cfi.CheckComplete();
            if (m_cfi.IsComplete() == false)
                  clLog()->Dump(XrdCl::AppMsg, "Prefetch::Prefetch This shoulf not happedn !!!"); 
         }
         clLog()->Debug(XrdCl::AppMsg, "Prefetch::Prefetch end");
      }
   }

   UnMarkPrefetch();
}


//______________________________________________________________________________
void File::CheckPrefetchStatRAM(Block* b)
{
   if (Factory::GetInstance().RefConfiguration().m_prefetch) {
      if (b->m_prefetch) {
         m_prefetchHitCnt++;
         m_prefetchScore = m_prefetchHitCnt/m_prefetchReadCnt;
      }
   }
}

//______________________________________________________________________________
void File::CheckPrefetchStatDisk(int idx)
{
   if (Factory::GetInstance().RefConfiguration().m_prefetch) {
      if (m_cfi.TestPrefetchBit(idx))
         m_prefetchHitCnt++;
   }
}

//______________________________________________________________________________
float File::GetPrefetchScore() const
{
   return m_prefetchScore;
}

//______________________________________________________________________________
void File::MarkPrefetch()
{

    XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg,"File::MarkPrefetch()");
   m_stateCond.Lock();
   m_prefetchCurrentCnt++;
   m_stateCond.UnLock();

}

//______________________________________________________________________________
void File::UnMarkPrefetch()
{
   m_stateCond.Lock();
   m_prefetchCurrentCnt--;
   m_stateCond.UnLock();
}

//==============================================================================
//==================    RESPONSE HANDLER      ==================================
//==============================================================================

void BlockResponseHandler::HandleResponse(XrdCl::XRootDStatus *status,
                                          XrdCl::AnyObject    *response)
{
    XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg,"BlockResponseHandler::HandleResponse()");
   m_block->m_file->ProcessBlockResponse(m_block, status);

   delete status;
   delete response;

   delete this;
}

//------------------------------------------------------------------------------

void DirectResponseHandler::HandleResponse(XrdCl::XRootDStatus *status,
                                           XrdCl::AnyObject    *response)
{
    XrdCl::DefaultEnv::GetLog()->Dump(XrdCl::AppMsg,"DirectResponseHandler::HandleRespons()");
   XrdSysCondVarHelper _lck(m_cond);

   --m_to_wait;

   if ( ! status->IsOK())
   {
      XrdPosixMap::Result(*status);
      m_errno = errno;
   }

   if (m_to_wait == 0)
   {
      m_cond.Signal();
   }
}


