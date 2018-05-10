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
#include "XrdFileCacheIO.hh"
#include "XrdFileCacheTrace.hh"
#include <stdio.h>
#include <sstream>
#include <fcntl.h>
#include <assert.h>
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdFileCache.hh"


using namespace XrdFileCache;

namespace
{
const int PREFETCH_MAX_ATTEMPTS = 10;



Cache* cache() { return &Cache::GetInstance(); }
}

const char *File::m_traceID = "File";

//------------------------------------------------------------------------------

File::File(IO *io, const std::string& path, long long iOffset, long long iFileSize) :
   m_ref_cnt(0),
   m_is_open(false),
   m_io(io),
   m_output(0),
   m_infoFile(0),
   m_cfi(Cache::GetInstance().GetTrace(), Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks > 0),
   m_filename(path),
   m_offset(iOffset),
   m_fileSize(iFileSize),
   m_non_flushed_cnt(0),
   m_in_sync(false),
   m_downloadCond(0),
   m_prefetchState(kOff),
   m_prefetchReadCnt(0),
   m_prefetchHitCnt(0),
   m_prefetchScore(1),
   m_detachTimeIsLogged(false)
{
   Open();
}

File::~File()
{
   if (m_infoFile)
   {
      TRACEF(Debug, "File::~File() close info ");
      m_infoFile->Close();
      delete m_infoFile;
      m_infoFile = NULL;
   }

   if (m_output)
   {
      TRACEF(Debug, "File::~File() close output  ");
      m_output->Close();
      delete m_output;
      m_output = NULL;
   }

   TRACEF(Debug, "File::~File() ended, prefetch score = " <<  m_prefetchScore);
}

//------------------------------------------------------------------------------

void File::BlockRemovedFromWriteQ(Block* b)
{
   m_downloadCond.Lock();
   dec_ref_count(b);
   TRACEF(Dump, "File::BlockRemovedFromWriteQ() check write queues block = "
          << (void*)b << " idx= " << b->m_offset/m_cfi.GetBufferSize());
   m_downloadCond.UnLock();
}

//------------------------------------------------------------------------------

bool File::ioActive()
{
   // Retruns true if delay is needed

   TRACEF(Debug, "File::ioActive start");
   bool blockMapEmpty = false;
   {
      XrdSysCondVarHelper _lck(m_downloadCond);
      if (! m_is_open) return false;

      if (m_prefetchState != kStopped)
      {
         m_prefetchState = kStopped;
         cache()->DeRegisterPrefetchFile(this);
      }

      // High debug print
      // for (BlockMap_i it = m_block_map.begin(); it != m_block_map.end(); ++it)
      // {
      //    Block* b = it->second;
      //    TRACEF(Dump, "File::ioActive block idx = " <<  b->m_offset/m_cfi.GetBufferSize() << " prefetch = " << b->prefetch <<  " refcnt " << b->refcnt);
      // }
      TRACEF(Info, "ioActive block_map.size() = " << m_block_map.size());

      // remove failed blocks and check if map is empty
      BlockMap_i itr = m_block_map.begin();
      while (itr != m_block_map.end())
      {
         if (itr->second->is_failed() && itr->second->m_refcnt == 1)
         {
            BlockMap_i toErase = itr;
            ++itr;
            TRACEF(Debug, "Remove failed block " <<  toErase->second->m_offset/m_cfi.GetBufferSize());
            free_block(toErase->second);
         }
         else
         {
            ++itr;
         }
      }

      blockMapEmpty = m_block_map.empty();
   }

   return !blockMapEmpty;
}

//------------------------------------------------------------------------------

void File::RequestSyncOfDetachStats()
{
   XrdSysCondVarHelper _lck(m_downloadCond);
   m_detachTimeIsLogged = false;
}

bool File::FinalizeSyncBeforeExit()
{
   // Returns true if sync is required.
   // This method is called after corresponding IO is detached from PosixCache.

   XrdSysCondVarHelper _lck(m_downloadCond);
   if ( m_is_open )
   {
     if ( ! m_writes_during_sync.empty() || m_non_flushed_cnt > 0 || ! m_detachTimeIsLogged)
     {
       Stats loc_stats = m_stats.Clone();
       m_cfi.WriteIOStatDetach(loc_stats);
       m_detachTimeIsLogged = true;
       TRACEF(Debug, "File::FinalizeSyncBeforeExit scheduling sync to write detach stats");
       return true;
     }
   }
   TRACEF(Debug, "File::FinalizeSyncBeforeExit sync not required");
   return false;
}

//------------------------------------------------------------------------------

void File::ReleaseIO()
{
   // called from Cache::ReleaseFile

   m_downloadCond.Lock();
   m_io = 0;
   m_downloadCond.UnLock();
 }

//------------------------------------------------------------------------------

IO* File::SetIO(IO *io)
{
   // called if this object is recycled by other IO or detached from cache

   bool cacheActivatePrefetch = false;

   TRACEF(Debug, "File::SetIO()  " <<  (void*)io);
   IO* oldIO = m_io;
   m_downloadCond.Lock();
   m_io = io;
   if (io && m_prefetchState != kComplete)
   {
      cacheActivatePrefetch = true;
      m_prefetchState = kOn;
   }
   m_downloadCond.UnLock();
   
   if (cacheActivatePrefetch) cache()->RegisterPrefetchFile(this);
   return oldIO;
}

//------------------------------------------------------------------------------

bool File::Open()
{
   TRACEF(Dump, "File::Open() open file for disk cache ");

   XrdOss     &myOss  = * Cache::GetInstance().GetOss();
   const char *myUser =   Cache::GetInstance().RefConfiguration().m_username.c_str();
   XrdOucEnv myEnv;

   // Create the data file itself.
   char size_str[16]; sprintf(size_str, "%lld", m_fileSize);
   myEnv.Put("oss.asize",  size_str);
   myEnv.Put("oss.cgroup", Cache::GetInstance().RefConfiguration().m_data_space.c_str());
   if (myOss.Create(myUser, m_filename.c_str(), 0600, myEnv, XRDOSS_mkpath) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Create failed for data file " << m_filename
                                                                << ", err=" << strerror(errno));
      return false;
   }
   
   m_output = myOss.newFile(myUser);
   if (m_output->Open(m_filename.c_str(), O_RDWR, 0600, myEnv) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Open failed for data file " << m_filename
                                                              << ", err=" << strerror(errno));
      delete m_output; m_output = 0;
      return false;
   }

   // Create the info file
   std::string ifn = m_filename + Info::m_infoExtension;

   struct stat infoStat;
   bool fileExisted = (myOss.Stat(ifn.c_str(), &infoStat) == XrdOssOK);

   myEnv.Put("oss.asize", "64k"); // TODO: Calculate? Get it from configuration? Do not know length of access lists ...
   myEnv.Put("oss.cgroup", Cache::GetInstance().RefConfiguration().m_meta_space.c_str());
   if (myOss.Create(myUser, ifn.c_str(), 0600, myEnv, XRDOSS_mkpath) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Create failed for info file " << ifn
                                                                << ", err=" << strerror(errno));
      delete m_output; m_output = 0;
      return false;
   }

   m_infoFile = myOss.newFile(myUser);
   if (m_infoFile->Open(ifn.c_str(), O_RDWR, 0600, myEnv) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Open failed for info file " << ifn << ", err=" << strerror(errno));

      delete m_infoFile; m_infoFile = 0;
      delete m_output;   m_output   = 0;
      return false;
   }

   if (fileExisted && m_cfi.Read(m_infoFile, ifn))
   {
      TRACEF(Debug, "Read existing info file.");
   }
   else
   {
      m_cfi.SetBufferSize(Cache::GetInstance().RefConfiguration().m_bufferSize);
      m_cfi.SetFileSize(m_fileSize);
      m_cfi.Write(m_infoFile);
      m_infoFile->Fsync();
      int ss = (m_fileSize - 1)/m_cfi.GetBufferSize() + 1;
      TRACEF(Debug, "Creating new file info, data size = " <<  m_fileSize << " num blocks = "  << ss);
   }

   m_cfi.WriteIOStatAttach();
   m_downloadCond.Lock();
   m_is_open = true;
   m_prefetchState = (m_cfi.IsComplete()) ? kComplete : kOn;
   m_downloadCond.UnLock();

   if (m_prefetchState == kOn) cache()->RegisterPrefetchFile(this);
   return true;
}


//==============================================================================
// Read and helpers
//==============================================================================

bool File::overlap(int blk,            // block to query
                   long long blk_size, //
                   long long req_off,  // offset of user request
                   int req_size,       // size of user request
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

      assert(size <= blk_size);
      return true;
   }
   else
   {
      return false;
   }
}

//------------------------------------------------------------------------------

Block* File::PrepareBlockRequest(int i, bool prefetch)
{
   // Must be called w/ block_map locked.
   // Checks on size etc should be done before.
   //
   // Reference count is 0 so increase it in calling function if you want to
   // catch the block while still in memory.

   const long long BS = m_cfi.GetBufferSize();
   const int last_block = m_cfi.GetSizeInBits() - 1;

   long long off     = i * BS;
   long long this_bs = (i == last_block) ? m_fileSize - off : BS;

   Block *b = new Block(this, off, this_bs, prefetch); // should block be reused to avoid recreation

   m_block_map[i] = b;

   // Actual Read request is issued in ProcessBlockRequests().
   TRACEF(Dump, "File::PrepareBlockRequest() " <<  i << "prefetch" <<  prefetch << "address " << (void*)b);

   if (m_prefetchState == kOn && m_block_map.size() > Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
   {
      m_prefetchState = kHold;
      cache()->DeRegisterPrefetchFile(this);
   }

   return b;
}

void File::ProcessBlockRequests(BlockList_t& blks)
{
   // This *must not* be called with block_map locked.

   for (BlockList_i bi = blks.begin(); bi != blks.end(); ++bi)
   {
      Block *b = *bi;
      BlockResponseHandler* oucCB = new BlockResponseHandler(b);
      m_io->GetInput()->Read(*oucCB, b->get_buff(), b->get_offset(), b->get_size());
   }
}

//------------------------------------------------------------------------------

int File::RequestBlocksDirect(DirectResponseHandler *handler, IntList_t& blocks,
                              char* req_buf, long long req_off, long long req_size)
{
   const long long BS = m_cfi.GetBufferSize();

   // TODO Use readv to load more at the same time.

   long long total = 0;

   for (IntList_i ii = blocks.begin(); ii != blocks.end(); ++ii)
   {
      // overlap and request
      long long off;     // offset in user buffer
      long long blk_off; // offset in block
      long long size;    // size to copy

      overlap(*ii, BS, req_off, req_size, off, blk_off, size);

      m_io->GetInput()->Read( *handler, req_buf + off, *ii * BS + blk_off, size);
      TRACEF(Dump, "RequestBlockDirect success, idx = " <<  *ii << " size = " <<  size);

      total += size;
   }

   return total;
}

//------------------------------------------------------------------------------

int File::ReadBlocksFromDisk(std::list<int>& blocks,
                             char* req_buf, long long req_off, long long req_size)
{
   TRACEF(Dump, "File::ReadBlocksFromDisk " <<  blocks.size());
   const long long BS = m_cfi.GetBufferSize();

   long long total = 0;

   // Coalesce adjacent reads.

   for (IntList_i ii = blocks.begin(); ii != blocks.end(); ++ii)
   {
      // overlap and read
      long long off;     // offset in user buffer
      long long blk_off; // offset in block
      long long size;    // size to copy

      overlap(*ii, BS, req_off, req_size, off, blk_off, size);

      long long rs = m_output->Read(req_buf + off, *ii * BS + blk_off -m_offset, size);
      TRACEF(Dump, "File::ReadBlocksFromDisk block idx = " <<  *ii << " size= " << size);

      if (rs < 0)
      {
         TRACEF(Error, "File::ReadBlocksFromDisk neg retval = " <<  rs << " idx = " << *ii );
         return rs;
      }

      if (rs != size)
      {
         TRACEF(Error, "File::ReadBlocksFromDisk incomplete size = " <<  rs << " idx = " << *ii);
         return -1;
      }

      total += rs;
   }

   return total;
}

//------------------------------------------------------------------------------

int File::Read(char* iUserBuff, long long iUserOff, int iUserSize)
{
   if ( ! isOpen())
   {
      return m_io->GetInput()->Read(iUserBuff, iUserOff, iUserSize);
   }

   const long long BS = m_cfi.GetBufferSize();

   Stats loc_stats;

   // lock
   // loop over reqired blocks:
   //   - if on disk, ok;
   //   - if in ram or incoming, inc ref-count
   //   - if not available, request and inc ref count
   // before requesting the hell and more (esp. for sparse readv) assess if
   //   passing the req to client is actually better.
   // unlock

   BlockList_t blks;
   bool preProcOK = true;

   m_downloadCond.Lock();

   const int idx_first = iUserOff / BS;
   const int idx_last  = (iUserOff + iUserSize - 1) / BS;

   BlockList_t blks_to_request, blks_to_process, blks_processed;
   IntList_t blks_on_disk,    blks_direct;

   for (int block_idx = idx_first; block_idx <= idx_last; ++block_idx)
   {
      TRACEF(Dump, "File::Read() idx " << block_idx);
      BlockMap_i bi = m_block_map.find(block_idx);

      // In RAM or incoming?
      if (bi != m_block_map.end())
      {
         inc_ref_count(bi->second);
         TRACEF(Dump, "File::Read() " << iUserBuff << "inc_ref_count for existing block << " << bi->second << " idx = " <<  block_idx);
         blks_to_process.push_front(bi->second);
      }
      // On disk?
      else if (m_cfi.TestBit(offsetIdx(block_idx)))
      {
         TRACEF(Dump, "File::Read()  read from disk " <<  (void*)iUserBuff << " idx = " << block_idx);
         blks_on_disk.push_back(block_idx);
      }
      // Then we have to get it ...
      else
      {
         // Is there room for one more RAM Block?
         if (cache()->RequestRAMBlock())
         {
            TRACEF(Dump, "File::Read() inc_ref_count new " <<  (void*)iUserBuff << " idx = " << block_idx);
            Block *b = PrepareBlockRequest(block_idx, false);
            if ( ! b)
            {
               preProcOK = false;
               break;
            }
            inc_ref_count(b);
            blks_to_process.push_back(b);
            blks_to_request.push_back(b);
         }
         // Nope ... read this directly without caching.
         else
         {
            TRACEF(Dump, "File::Read() direct block " << block_idx);
            blks_direct.push_back(block_idx);
         }
      }
   }

   m_downloadCond.UnLock();

   if ( ! preProcOK)
   {
      for (BlockList_i i = blks_to_process.begin(); i != blks_to_process.end(); ++i)
         dec_ref_count(*i);
      return -1;
   }

   ProcessBlockRequests(blks_to_request);

   long long bytes_read = 0;

   // First, send out any direct requests.
   // TODO Could send them all out in a single vector read.
   DirectResponseHandler *direct_handler = 0;
   int direct_size = 0;

   if ( ! blks_direct.empty())
   {
      direct_handler = new DirectResponseHandler(blks_direct.size());

      direct_size = RequestBlocksDirect(direct_handler, blks_direct, iUserBuff, iUserOff, iUserSize);
      // failed to send direct client request
      if (direct_size < 0)
      {
         for (BlockList_i i = blks_to_process.begin(); i!= blks_to_process.end(); ++i )
            dec_ref_count(*i);
         delete direct_handler;
         return -1;
      }
      TRACEF(Dump, "File::Read() direct read finished, size = " << direct_size);
   }

   // Second, read blocks from disk.
   if ( ! blks_on_disk.empty() && bytes_read >= 0)
   {
      int rc = ReadBlocksFromDisk(blks_on_disk, iUserBuff, iUserOff, iUserSize);
      TRACEF(Dump, "File::Read() " << (void*)iUserBuff <<" from disk finished size = " << rc);
      if (rc >= 0)
      {
         bytes_read += rc;
         loc_stats.m_BytesDisk += rc;
      }
      else
      {
         bytes_read = rc;
         TRACEF(Error, "File::Read() failed read from disk");
         return -1;
      }
   }

   // Third, loop over blocks that are available or incoming
   int prefetchHitsRam = 0;
   while ( ! blks_to_process.empty() && bytes_read >= 0)
   {
      BlockList_t finished;

      {
         XrdSysCondVarHelper _lck(m_downloadCond);

         BlockList_i bi = blks_to_process.begin();
         while (bi != blks_to_process.end())
         {
            if ((*bi)->is_finished())
            {
               TRACEF(Dump, "File::Read() requested block downloaded " << (void*)(*bi));
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
            m_downloadCond.Wait();
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

            // clLog()->Dump(XrdCl::AppMsg, "File::Read() Block finished ok.");
            overlap((*bi)->m_offset/BS, BS, iUserOff, iUserSize, user_off, off_in_block, size_to_copy);

            TRACEF(Dump, "File::Read() ub=" << (void*)iUserBuff  << " from finished block " << (*bi)->m_offset/BS << " size " << size_to_copy);
            memcpy(&iUserBuff[user_off], &((*bi)->m_buff[off_in_block]), size_to_copy);
            bytes_read += size_to_copy;
            loc_stats.m_BytesRam += size_to_copy;
            if ((*bi)->m_prefetch)
               prefetchHitsRam++;
         }
         else // it has failed ... krap up.
         {
            bytes_read = -1;
            errno = -(*bi)->m_errno;
            TRACEF(Error, "File::Read(), block "<< (*bi)->m_offset/BS << " finished with error "
                                                << errno << " " << strerror(errno));
            break;
         }
         ++bi;
      }

      std::copy(finished.begin(), finished.end(), std::back_inserter(blks_processed));
      finished.clear();
   }

   // Fourth, make sure all direct requests have arrived
   if (direct_handler != 0 && bytes_read >= 0)
   {
      TRACEF(Dump, "File::Read() waiting for direct requests ");
      XrdSysCondVarHelper _lck(direct_handler->m_cond);

      while (direct_handler->m_to_wait > 0)
      {
         direct_handler->m_cond.Wait();
      }

      if (direct_handler->m_errno == 0)
      {
         bytes_read += direct_size;
         loc_stats.m_BytesMissed += direct_size;
      }
      else
      {
         errno = -direct_handler->m_errno;
         bytes_read = -1;
      }

      delete direct_handler;
   }
   assert(iUserSize >= bytes_read);

   // Last, stamp and release blocks, release file.
   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      // blks_to_process can be non-empty, if we're exiting with an error.
      std::copy(blks_to_process.begin(), blks_to_process.end(), std::back_inserter(blks_processed));

      for (BlockList_i bi = blks_processed.begin(); bi != blks_processed.end(); ++bi)
      {
         TRACEF(Dump, "File::Read() dec_ref_count " << (void*)(*bi) << " idx = " << (int)((*bi)->m_offset/BufferSize()));
         dec_ref_count(*bi);
      }

      // update prefetch score
      m_prefetchHitCnt += prefetchHitsRam;
      for (IntList_i d = blks_on_disk.begin(); d !=  blks_on_disk.end(); ++d)
      {
         if (m_cfi.TestPrefetchBit(offsetIdx(*d)))
            m_prefetchHitCnt++;
      }
      m_prefetchScore = float(m_prefetchHitCnt)/m_prefetchReadCnt;
   }

   m_stats.AddStats(loc_stats);

   return bytes_read;
}

//------------------------------------------------------------------------------

void File::WriteBlockToDisk(Block* b)
{
   int retval = 0;
   // write block buffer into disk file
   long long offset = b->m_offset - m_offset;
   long long size = (offset +  m_cfi.GetBufferSize()) > m_fileSize ? (m_fileSize - offset) : m_cfi.GetBufferSize();
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
         TRACEF(Warning, "File::WriteToDisk() reattempt " << cnt << " writing missing " << buffer_remaining << " for block  offset " << b->m_offset);
      }
      if (cnt > PREFETCH_MAX_ATTEMPTS)
      {
         TRACEF(Error, "File::WriteToDisk() write block with off = " <<  b->m_offset <<" failed too manny attempts ");
         return;
      }
   }

   // set bit fetched
   TRACEF(Dump, "File::WriteToDisk() success set bit for block " <<  b->m_offset << " size " <<  size);
   int pfIdx =  (b->m_offset - m_offset)/m_cfi.GetBufferSize();

   bool schedule_sync = false;
   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      m_cfi.SetBitWritten(pfIdx);

      if (b->m_prefetch)
         m_cfi.SetBitPrefetch(pfIdx);

      // clLog()->Dump(XrdCl::AppMsg, "File::WriteToDisk() dec_ref_count %d %s", pfIdx, lPath());
      dec_ref_count(b);


      // set bit synced
      if (m_in_sync)
      {
         m_writes_during_sync.push_back(pfIdx);
      }
      else
      {
         m_cfi.SetBitSynced(pfIdx);
         ++m_non_flushed_cnt;
         if (m_non_flushed_cnt >= Cache::GetInstance().RefConfiguration().m_flushCnt)
         {
            schedule_sync     = true;
            m_in_sync         = true;
            m_non_flushed_cnt = 0;
         }
      }
   }

   if (schedule_sync)
   {
      cache()->ScheduleFileSync(this);
   }
}

//------------------------------------------------------------------------------

void File::Sync()
{
   TRACEF(Dump, "File::Sync()");
   m_output->Fsync();

   m_cfi.Write(m_infoFile);
   m_infoFile->Fsync();

   int written_while_in_sync;
   {
      XrdSysCondVarHelper _lck(&m_downloadCond);
      for (std::vector<int>::iterator i = m_writes_during_sync.begin(); i != m_writes_during_sync.end(); ++i)
      {
         m_cfi.SetBitSynced(*i);
      }
      written_while_in_sync = m_non_flushed_cnt = (int) m_writes_during_sync.size();
      m_writes_during_sync.clear();
      m_in_sync = false;
   }
   TRACEF(Dump, "File::Sync() "<< written_while_in_sync  << " blocks written during sync.");
}

//------------------------------------------------------------------------------

void File::inc_ref_count(Block* b)
{
   // Method always called under lock
   b->m_refcnt++;
   TRACEF(Dump, "File::inc_ref_count " << b << " refcnt  " << b->m_refcnt);
}

//------------------------------------------------------------------------------

void File::dec_ref_count(Block* b)
{
   // Method always called under lock
   b->m_refcnt--;
   assert(b->m_refcnt >= 0);

   // File::Read() can decrease ref count before waiting to be , prefetch starts with refcnt 0
   if (b->m_refcnt == 0 && b->is_finished())
   {
      free_block(b);
   }
}

void File::free_block(Block* b)
{
   int i = b->m_offset/BufferSize();
   TRACEF(Dump, "File::free_block block " << b << "  idx =  " <<  i);
   size_t ret = m_block_map.erase(i);
   if (ret != 1)
   {
      // assert might be a better option than a warning
      TRACEF(Error, "File::free_block did not erase " <<  i  << " from map");
   }
   else
   {
      delete b;
      cache()->RAMBlockReleased();
   }

   if (m_prefetchState == kHold && m_block_map.size() < Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
   {
      m_prefetchState = kOn;
      cache()->RegisterPrefetchFile(this);
   }
}

//------------------------------------------------------------------------------

void File::ProcessBlockResponse(Block* b, int res)
{
   m_downloadCond.Lock();

   TRACEF(Dump, "File::ProcessBlockResponse " << (void*)b << "  " << b->m_offset/BufferSize());
   if (res >= 0)
   {
      b->m_downloaded = true;
      TRACEF(Dump, "File::ProcessBlockResponse inc_ref_count " <<  (int)(b->m_offset/BufferSize()));
      inc_ref_count(b);
      cache()->AddWriteTask(b, true);
   }
   else
   {
      // TODO: how long to keep? when to retry?
      TRACEF(Error, "File::ProcessBlockResponse block " << b << "  " << (int)(b->m_offset/BufferSize()) << " error=" << res);
      // XrdPosixMap::Result(*status);
      b->set_error_and_free(res);
      inc_ref_count(b);
   }

   m_downloadCond.Broadcast();

   m_downloadCond.UnLock();
}

long long File::BufferSize()
{
   return m_cfi.GetBufferSize();
}

//------------------------------------------------------------------------------

const char* File::lPath() const
{
   return m_filename.c_str();
}

//------------------------------------------------------------------------------

int File::offsetIdx(int iIdx)
{
   return iIdx - m_offset/m_cfi.GetBufferSize();
}


//------------------------------------------------------------------------------

void File::Prefetch()
{
   // Check that block is not on disk and not in RAM.
   // TODO: Could prefetch several blocks at once!

   BlockList_t blks;

   TRACEF(Dump, "File::Prefetch enter to check download status");
   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      if (m_prefetchState != kOn)
         return;

      for (int f = 0; f < m_cfi.GetSizeInBits(); ++f)
      {
         if ( ! m_cfi.TestBit(f))
         {
            f += m_offset/m_cfi.GetBufferSize();
            BlockMap_i bi = m_block_map.find(f);
            if (bi == m_block_map.end())
            {
               TRACEF(Dump, "File::Prefetch take block " << f);
               cache()->RequestRAMBlock();
               blks.push_back( PrepareBlockRequest(f, true) );
               m_prefetchReadCnt++;
               m_prefetchScore = float(m_prefetchHitCnt)/m_prefetchReadCnt;
               break;
            }
         }
      }
   }


   if ( ! blks.empty())
   {
      ProcessBlockRequests(blks);
   }
   else
   {
      TRACEF(Dump, "File::Prefetch no free block found ");
      m_downloadCond.Lock();
      m_prefetchState = kComplete;
      m_downloadCond.UnLock();
      cache()->DeRegisterPrefetchFile(this);
   }
}


//------------------------------------------------------------------------------

float File::GetPrefetchScore() const
{
   return m_prefetchScore;
}

XrdSysTrace* File::GetTrace()
{
   return Cache::GetInstance().GetTrace();
}

//==============================================================================
//=======================    RESPONSE HANDLERS    ==============================
//==============================================================================

void BlockResponseHandler::Done(int res)
{
   m_block->m_file->ProcessBlockResponse(m_block, res);

   delete this;
}

//------------------------------------------------------------------------------

void DirectResponseHandler::Done(int res)
{
   XrdSysCondVarHelper _lck(m_cond);

   --m_to_wait;

   if (res < 0)
   {
      m_errno = res;
   }

   if (m_to_wait == 0)
   {
      m_cond.Signal();
   }
}
