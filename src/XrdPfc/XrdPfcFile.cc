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


#include "XrdPfcFile.hh"
#include "XrdPfcIO.hh"
#include "XrdPfcTrace.hh"
#include <cstdio>
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
#include "XrdPfc.hh"


using namespace XrdPfc;

namespace
{

const int BLOCK_WRITE_MAX_ATTEMPTS = 4;

Cache* cache() { return &Cache::GetInstance(); }

}

const char *File::m_traceID = "File";

//------------------------------------------------------------------------------

File::File(const std::string& path, long long iOffset, long long iFileSize) :
   m_ref_cnt(0),
   m_is_open(false),
   m_in_shutdown(false),
   m_data_file(0),
   m_info_file(0),
   m_cfi(Cache::GetInstance().GetTrace(), Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks > 0),
   m_filename(path),
   m_offset(iOffset),
   m_file_size(iFileSize),
   m_current_io(m_io_map.end()),
   m_ios_in_detach(0),
   m_non_flushed_cnt(0),
   m_in_sync(false),
   m_state_cond(0),
   m_prefetch_state(kOff),
   m_prefetch_read_cnt(0),
   m_prefetch_hit_cnt(0),
   m_prefetch_score(1),
   m_detach_time_logged(false)
{
}

File::~File()
{
   if (m_info_file)
   {
      TRACEF(Debug, "~File() close info ");
      m_info_file->Close();
      delete m_info_file;
      m_info_file = NULL;
   }

   if (m_data_file)
   {
      TRACEF(Debug, "~File() close output  ");
      m_data_file->Close();
      delete m_data_file;
      m_data_file = NULL;
   }

   TRACEF(Debug, "~File() ended, prefetch score = " <<  m_prefetch_score);
}

//------------------------------------------------------------------------------

File* File::FileOpen(const std::string &path, long long offset, long long fileSize)
{
   File *file = new File(path, offset, fileSize);
   if ( ! file->Open())
   {
      delete file;
      file = 0;
   }
   return file;
}

//------------------------------------------------------------------------------

void File::initiate_emergency_shutdown()
{
   // Called from Cache::Unlink() when the file is currently open.
   // Cache::Unlink is also called on FSync error and when wrong number of bytes
   // is received from a remote read.
   //
   // From this point onward the file will not be written to, cinfo file will
   // not be updated, and all new read requests will return -ENOENT.
   //
   // File's entry in the Cache's active map is set to nullptr and will be
   // removed from there shortly, in any case, well before this File object
   // shuts down. So we do not communicate to Cache about our destruction when
   // it happens.

   {
      XrdSysCondVarHelper _lck(m_state_cond);

      m_in_shutdown = true;

      if (m_prefetch_state != kStopped && m_prefetch_state != kComplete)
      {
         m_prefetch_state = kStopped;
         cache()->DeRegisterPrefetchFile(this);
      }
   }

}

//------------------------------------------------------------------------------

Stats File::DeltaStatsFromLastCall()
{
   // Not locked, only used from Cache / Purge thread.

   Stats delta = m_last_stats;

   m_last_stats = m_stats.Clone();

   delta.DeltaToReference(m_last_stats);

   return delta;
}

//------------------------------------------------------------------------------

void File::BlockRemovedFromWriteQ(Block* b)
{
   TRACEF(Dump, "BlockRemovedFromWriteQ() block = " << (void*) b << " idx= " << b->m_offset/m_cfi.GetBufferSize());

   XrdSysCondVarHelper _lck(m_state_cond);
   dec_ref_count(b);
}

void File::BlocksRemovedFromWriteQ(std::list<Block*>& blocks)
{
   TRACEF(Dump, "BlocksRemovedFromWriteQ() n_blocks = " << blocks.size());

   XrdSysCondVarHelper _lck(m_state_cond);

   for (std::list<Block*>::iterator i = blocks.begin(); i != blocks.end(); ++i)
   {
      dec_ref_count(*i);
   }
}

//------------------------------------------------------------------------------

void File::ioUpdated(IO *io)
{
   std::string loc(io->GetLocation());
   XrdSysCondVarHelper _lck(m_state_cond);
   insert_remote_location(loc);
}

//------------------------------------------------------------------------------

bool File::ioActive(IO *io)
{
   // Returns true if delay is needed.

   TRACEF(Debug, "ioActive start for io " << io);

   std::string loc(io->GetLocation());

   {
      XrdSysCondVarHelper _lck(m_state_cond);

      if ( ! m_is_open)
      {
         TRACEF(Error, "ioActive for io " << io <<" called on a closed file. This should not happen.");
         return false;
      }

      IoMap_i mi = m_io_map.find(io);

      if (mi != m_io_map.end())
      {
         TRACE(Info, "ioActive for io " << io <<
                ", active_prefetches "       << mi->second.m_active_prefetches <<
                ", allow_prefetching "       << mi->second.m_allow_prefetching <<
                ", ios_in_detach "           << m_ios_in_detach);
         TRACEF(Info,
                "\tio_map.size() "           << m_io_map.size() <<
                ", block_map.size() "        << m_block_map.size() << ", file");

         insert_remote_location(loc);

         mi->second.m_allow_prefetching = false;

         // Check if any IO is still available for prfetching. If not, stop it.
         if (m_prefetch_state == kOn || m_prefetch_state == kHold)
         {
            if ( ! select_current_io_or_disable_prefetching(false) )
            {
               TRACEF(Debug, "ioActive stopping prefetching after io " << io << " retreat.");
            }
         }

         // On last IO, consider write queue blocks. Note, this also contains
         // blocks being prefetched.

         bool io_active_result;

         if (m_io_map.size() - m_ios_in_detach == 1)
         {
            io_active_result = ! m_block_map.empty();
         }
         else
         {
            io_active_result = mi->second.m_active_prefetches > 0;
         }

         if ( ! io_active_result)
         {
            ++m_ios_in_detach;
         }

         TRACEF(Info, "ioActive for io " << io << " returning " << io_active_result << ", file");

         return io_active_result;
      }
      else
      {
         TRACEF(Error, "ioActive io " << io <<" not found in IoMap. This should not happen.");
         return false;
      }
   }
}

//------------------------------------------------------------------------------

void File::RequestSyncOfDetachStats()
{
   XrdSysCondVarHelper _lck(m_state_cond);
   m_detach_time_logged = false;
}

bool File::FinalizeSyncBeforeExit()
{
   // Returns true if sync is required.
   // This method is called after corresponding IO is detached from PosixCache.

   XrdSysCondVarHelper _lck(m_state_cond);
   if (m_is_open && ! m_in_shutdown)
   {
     if ( ! m_writes_during_sync.empty() || m_non_flushed_cnt > 0 || ! m_detach_time_logged)
     {
       Stats loc_stats = m_stats.Clone();
       m_cfi.WriteIOStatDetach(loc_stats);
       m_detach_time_logged = true;
       m_in_sync            = true;
       TRACEF(Debug, "FinalizeSyncBeforeExit requesting sync to write detach stats");
       return true;
     }
   }
   TRACEF(Debug, "FinalizeSyncBeforeExit sync not required");
   return false;
}

//------------------------------------------------------------------------------

void File::AddIO(IO *io)
{
   // Called from Cache::GetFile() when a new IO asks for the file.

   TRACEF(Debug, "AddIO() io = " << (void*)io);

   time_t      now = time(0);
   std::string loc(io->GetLocation());

   m_state_cond.Lock();

   IoMap_i mi = m_io_map.find(io);

   if (mi == m_io_map.end())
   {
      m_io_map.insert(std::make_pair(io, IODetails(now)));
      m_stats.IoAttach();

      insert_remote_location(loc);

      if (m_prefetch_state == kStopped)
      {
         m_prefetch_state = kOn;
         cache()->RegisterPrefetchFile(this);
      }
   }
   else
   {
      TRACEF(Error, "AddIO() io = " << (void*)io << " already registered.");
   }

   m_state_cond.UnLock();
}

//------------------------------------------------------------------------------

void File::RemoveIO(IO *io)
{
   // Called from Cache::ReleaseFile.

   TRACEF(Debug, "RemoveIO() io = " << (void*)io);

   time_t now = time(0);

   m_state_cond.Lock();

   IoMap_i mi = m_io_map.find(io);

   if (mi != m_io_map.end())
   {
      if (mi == m_current_io)
      {
         ++m_current_io;
      }

      m_stats.IoDetach(now - mi->second.m_attach_time);
      m_io_map.erase(mi);
      --m_ios_in_detach;

      if (m_io_map.empty() && m_prefetch_state != kStopped && m_prefetch_state != kComplete)
      {
         TRACEF(Error, "RemoveIO() io = " << (void*)io << " Prefetching is not stopped/complete -- it should be by now.");
         m_prefetch_state = kStopped;
         cache()->DeRegisterPrefetchFile(this);
      }
   }
   else
   {
      TRACEF(Error, "RemoveIO() io = " << (void*)io << " is NOT registered.");
   }

   m_state_cond.UnLock();
}

//------------------------------------------------------------------------------

bool File::Open()
{
   // Sets errno accordingly.

   static const char *tpfx = "Open() ";

   TRACEF(Dump, tpfx << "open file for disk cache");

   if (m_is_open)
   {
      TRACEF(Error, tpfx << "file is already opened.");
      return true;
   }

   const Configuration &conf = Cache::GetInstance().RefConfiguration();

   XrdOss     &myOss  = * Cache::GetInstance().GetOss();
   const char *myUser =   conf.m_username.c_str();
   XrdOucEnv   myEnv;
   struct stat data_stat, info_stat;

   std::string ifn = m_filename + Info::s_infoExtension;

   bool data_existed = (myOss.Stat(m_filename.c_str(), &data_stat) == XrdOssOK);
   bool info_existed = (myOss.Stat(ifn.c_str(),        &info_stat) == XrdOssOK);

   // Create the data file itself.
   char size_str[32]; sprintf(size_str, "%lld", m_file_size);
   myEnv.Put("oss.asize",  size_str);
   myEnv.Put("oss.cgroup", conf.m_data_space.c_str());

   int res;

   if ((res = myOss.Create(myUser, m_filename.c_str(), 0600, myEnv, XRDOSS_mkpath)) != XrdOssOK)
   {
      TRACEF(Error, tpfx << "Create failed " << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      return false;
   }

   m_data_file = myOss.newFile(myUser);
   if ((res = m_data_file->Open(m_filename.c_str(), O_RDWR, 0600, myEnv)) != XrdOssOK)
   {
      TRACEF(Error, tpfx << "Open failed " << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      delete m_data_file; m_data_file = 0;
      return false;
   }

   myEnv.Put("oss.asize", "64k"); // TODO: Calculate? Get it from configuration? Do not know length of access lists ...
   myEnv.Put("oss.cgroup", conf.m_meta_space.c_str());
   if ((res = myOss.Create(myUser, ifn.c_str(), 0600, myEnv, XRDOSS_mkpath)) != XrdOssOK)
   {
      TRACE(Error, tpfx << "Create failed for info file " << ifn << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      m_data_file->Close(); delete m_data_file; m_data_file = 0;
      return false;
   }

   m_info_file = myOss.newFile(myUser);
   if ((res = m_info_file->Open(ifn.c_str(), O_RDWR, 0600, myEnv)) != XrdOssOK)
   {
      TRACEF(Error, tpfx << "Failed for info file " << ifn  << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      delete m_info_file; m_info_file = 0;
      m_data_file->Close(); delete m_data_file;   m_data_file   = 0;
      return false;
   }

   bool initialize_info_file = true;

   if (info_existed && m_cfi.Read(m_info_file, ifn.c_str()))
   {
      TRACEF(Debug, tpfx << "Reading existing info file. (data_existed=" << data_existed  <<
             ", data_size_stat=" << (data_existed ? data_stat.st_size : -1ll) <<
             ", data_size_from_last_block=" << m_cfi.GetExpectedDataFileSize() << ")");

      // Check if data file exists and is of reasonable size.
      if (data_existed && data_stat.st_size >= m_cfi.GetExpectedDataFileSize())
      {
         initialize_info_file = false;
      } else {
         TRACEF(Warning, tpfx << "Basic sanity checks on data file failed, resetting info file, truncating data file.");
         m_cfi.ResetAllAccessStats();
         m_data_file->Ftruncate(0);
      }
   }

   if ( ! initialize_info_file && m_cfi.GetCkSumState() != conf.get_cs_Chk())
   {
      if (conf.does_cschk_have_missing_bits(m_cfi.GetCkSumState()) &&
          conf.should_uvkeep_purge(time(0) - m_cfi.GetNoCkSumTimeForUVKeep()))
      {
         TRACEF(Info, tpfx << "Cksum state of file insufficient, uvkeep test failed, resetting info file, truncating data file.");
         initialize_info_file = true;
         m_cfi.ResetAllAccessStats();
         m_data_file->Ftruncate(0);
      } else {
         // If a file is complete, we don't really need to reset net cksums ... well, maybe next time.
         m_cfi.DowngradeCkSumState(conf.get_cs_Chk());
      }
   }

   if (initialize_info_file)
   {
      m_cfi.SetBufferSize(conf.m_bufferSize);
      m_cfi.SetFileSizeAndCreationTime(m_file_size);
      m_cfi.SetCkSumState(conf.get_cs_Chk());
      m_cfi.Write(m_info_file, ifn.c_str());
      m_info_file->Fsync();
      TRACEF(Debug, tpfx << "Creating new file info, data size = " <<  m_file_size << " num blocks = "  << m_cfi.GetNBlocks());
   }

   m_cfi.WriteIOStatAttach();
   m_state_cond.Lock();
   m_is_open = true;
   m_prefetch_state = (m_cfi.IsComplete()) ? kComplete : kStopped; // Will engage in AddIO().
   m_state_cond.UnLock();

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

Block* File::PrepareBlockRequest(int i, IO *io, bool prefetch)
{
   // Must be called w/ block_map locked.
   // Checks on size etc should be done before.
   //
   // Reference count is 0 so increase it in calling function if you want to
   // catch the block while still in memory.

   const long long BS    = m_cfi.GetBufferSize();
   const long long off   = i * BS;
   const int  last_block = m_cfi.GetNBlocks() - 1;
   const bool cs_net     = cache()->RefConfiguration().is_cschk_net();

   int blk_size, req_size;
   if (i == last_block) {
      blk_size = req_size = m_file_size - off;
      if (cs_net && req_size & 0xFFF) req_size = (req_size & ~0xFFF) + 0x1000;
   } else {
      blk_size = req_size = BS;
   }

   Block *b   = 0;
   char  *buf = cache()->RequestRAM(req_size);

   if (buf)
   {
      b = new (std::nothrow) Block(this, io, buf, off, blk_size, req_size, prefetch, cs_net);

      if (b)
      {
         m_block_map[i] = b;

         // Actual Read request is issued in ProcessBlockRequests().
         TRACEF(Dump, "PrepareBlockRequest() idx=" << i << ", block=" << (void*) b << ", prefetch=" <<  prefetch << 
                      ", offset=" << off << ", size=" << blk_size << ", buffer=" << (void*) buf);

         if (m_prefetch_state == kOn && (int) m_block_map.size() >= Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
         {
            m_prefetch_state = kHold;
            cache()->DeRegisterPrefetchFile(this);
         }
      }
      else
      {
         TRACEF(Dump, "PrepareBlockRequest() " <<  i << " prefetch " <<  prefetch << ", allocation failed.");
      }
   }

   return b;
}

void File::ProcessBlockRequest(Block *b)
{
   // This *must not* be called with block_map locked.

   BlockResponseHandler* oucCB = new BlockResponseHandler(b);
   if (b->req_cksum_net())
   {
      b->get_io()->GetInput()->pgRead(*oucCB, b->get_buff(), b->get_offset(), b->get_req_size(),
                                      b->ref_cksum_vec(), 0, b->ptr_n_cksum_errors());
   } else {
      b->get_io()->GetInput()->  Read(*oucCB, b->get_buff(), b->get_offset(), b->get_size());
   }
}

void File::ProcessBlockRequests(BlockList_t& blks)
{
   // This *must not* be called with block_map locked.

   for (BlockList_i bi = blks.begin(); bi != blks.end(); ++bi)
   {
      ProcessBlockRequest(*bi);
   }
}

//------------------------------------------------------------------------------

int File::RequestBlocksDirect(IO *io, DirectResponseHandler *handler, IntList_t& blocks,
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

      io->GetInput()->Read( *handler, req_buf + off, *ii * BS + blk_off, size);
      TRACEF(Dump, "RequestBlockDirect success, idx = " <<  *ii << " size = " <<  size);

      total += size;
   }

   return total;
}

//------------------------------------------------------------------------------

int File::ReadBlocksFromDisk(std::list<int>& blocks,
                             char* req_buf, long long req_off, long long req_size)
{
   TRACEF(Dump, "ReadBlocksFromDisk " <<  blocks.size());
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

      long long rs = m_data_file->Read(req_buf + off, *ii * BS + blk_off -m_offset, size);
      TRACEF(Dump, "ReadBlocksFromDisk block idx = " <<  *ii << " size= " << size);

      if (rs < 0)
      {
         TRACEF(Error, "ReadBlocksFromDisk neg retval = " <<  rs << " idx = " << *ii );
         return rs;
      }

      if (rs != size)
      {
         TRACEF(Error, "ReadBlocksFromDisk incomplete size = " <<  rs << " idx = " << *ii);
         return -EIO;
      }

      total += rs;
   }

   return total;
}

//------------------------------------------------------------------------------

int File::Read(IO *io, char* iUserBuff, long long iUserOff, int iUserSize)
{
   const long long BS = m_cfi.GetBufferSize();

   Stats loc_stats;

   BlockList_t blks;

   const int idx_first = iUserOff / BS;
   const int idx_last  = (iUserOff + iUserSize - 1) / BS;

   BlockSet_t  requested_blocks;
   BlockList_t blks_to_request, blks_to_process, blks_processed;
   IntList_t   blks_on_disk,    blks_direct;

   // lock
   // loop over reqired blocks:
   //   - if on disk, ok;
   //   - if in ram or incoming, inc ref-count
   //   - if not available, request and inc ref count before requesting the
   //     hell and more (esp. for sparse readvs).
   //     assess if passing the req to client is actually better.
   // unlock

   m_state_cond.Lock();

   if ( ! m_is_open)
   {
      m_state_cond.UnLock();
      TRACEF(Error, "Read file is not open");
      return io->GetInput()->Read(iUserBuff, iUserOff, iUserSize);
   }
   if (m_in_shutdown)
   {
      m_state_cond.UnLock();
      return -ENOENT;
   }

   for (int block_idx = idx_first; block_idx <= idx_last; ++block_idx)
   {
      TRACEF(Dump, "Read() idx " << block_idx);
      BlockMap_i bi = m_block_map.find(block_idx);

      // In RAM or incoming?
      if (bi != m_block_map.end())
      {
         inc_ref_count(bi->second);
         TRACEF(Dump, "Read() " << (void*) iUserBuff << "inc_ref_count for existing block " << bi->second << " idx = " <<  block_idx);
         blks_to_process.push_front(bi->second);
      }
      // On disk?
      else if (m_cfi.TestBitWritten(offsetIdx(block_idx)))
      {
         TRACEF(Dump, "Read() read from disk " <<  (void*)iUserBuff << " idx = " << block_idx);
         blks_on_disk.push_back(block_idx);
      }
      // Then we have to get it ...
      else
      {
         // Is there room for one more RAM Block?
         Block *b = PrepareBlockRequest(block_idx, io, false);
         if (b)
         {
            TRACEF(Dump, "Read() inc_ref_count new " <<  (void*)iUserBuff << " idx = " << block_idx);
            inc_ref_count(b);
            blks_to_process.push_back(b);
            blks_to_request.push_back(b);
            requested_blocks.insert(b);
         }
         // Nope ... read this directly without caching.
         else
         {
            TRACEF(Dump, "Read() direct block " << block_idx);
            blks_direct.push_back(block_idx);
         }
      }
   }

   m_state_cond.UnLock();

   ProcessBlockRequests(blks_to_request);

   long long bytes_read = 0;
   int       error_cond = 0; // to be set to -errno

   // First, send out any direct requests.
   // TODO Could send them all out in a single vector read.
   DirectResponseHandler *direct_handler = 0;
   int direct_size = 0;

   if ( ! blks_direct.empty())
   {
      direct_handler = new DirectResponseHandler(blks_direct.size());

      direct_size = RequestBlocksDirect(io, direct_handler, blks_direct, iUserBuff, iUserOff, iUserSize);

      TRACEF(Dump, "Read() direct read requests sent out, size = " << direct_size);
   }

   // Second, read blocks from disk.
   if ( ! blks_on_disk.empty() && bytes_read >= 0)
   {
      int rc = ReadBlocksFromDisk(blks_on_disk, iUserBuff, iUserOff, iUserSize);
      TRACEF(Dump, "Read() " << (void*)iUserBuff <<" from disk finished size = " << rc);
      if (rc >= 0)
      {
         bytes_read += rc;
         loc_stats.m_BytesHit += rc;
      }
      else
      {
         error_cond = rc;
         TRACEF(Error, "Read() failed read from disk");
      }
   }

   // Third, loop over blocks that are available or incoming
   int prefetchHitsRam = 0;
   while ( ! blks_to_process.empty())
   {
      BlockList_t finished;
      BlockList_t to_reissue;
      {
         XrdSysCondVarHelper _lck(m_state_cond);

         BlockList_i bi = blks_to_process.begin();
         while (bi != blks_to_process.end())
         {
            if ((*bi)->is_failed() && (*bi)->get_io() != io)
            {
               TRACEF(Info, "Read() requested block " << (void*)(*bi) << " failed with another io " <<
                      (*bi)->get_io() << " - reissuing request with my io " << io);

               (*bi)->reset_error_and_set_io(io);
               to_reissue.push_back(*bi);
               ++bi;
            }
            else if ((*bi)->is_finished())
            {
               TRACEF(Dump, "Read() requested block finished " << (void*)(*bi) << ", is_failed()=" << (*bi)->is_failed());
               finished.push_back(*bi);
               BlockList_i bj = bi++;
               blks_to_process.erase(bj);
            }
            else
            {
               ++bi;
            }
         }

         if (finished.empty() && to_reissue.empty())
         {
            m_state_cond.Wait();
            continue;
         }
      }

      ProcessBlockRequests(to_reissue);
      to_reissue.clear();

      BlockList_i bi = finished.begin();
      while (bi != finished.end())
      {
         if ((*bi)->is_ok())
         {
            long long user_off;     // offset in user buffer
            long long off_in_block; // offset in block
            long long size_to_copy; // size to copy

            overlap((*bi)->m_offset/BS, BS, iUserOff, iUserSize, user_off, off_in_block, size_to_copy);

            TRACEF(Dump, "Read() ub=" << (void*)iUserBuff  << " from finished block " << (*bi)->m_offset/BS << " size " << size_to_copy);
            memcpy(&iUserBuff[user_off], &((*bi)->m_buff[off_in_block]), size_to_copy);
            bytes_read += size_to_copy;

            if (requested_blocks.find(*bi) == requested_blocks.end())
               loc_stats.m_BytesHit    += size_to_copy;
            else
               loc_stats.m_BytesMissed += size_to_copy;

            if ((*bi)->m_prefetch)
               prefetchHitsRam++;
         }
         else
         {
            // It has failed ... report only the first error.
            if ( ! error_cond)
            {
               error_cond = (*bi)->m_errno;
               TRACEF(Error, "Read() io " << io << ", block "<< (*bi)->m_offset/BS <<
                      " finished with error " << -error_cond << " " << XrdSysE2T(-error_cond));
            }
         }
         ++bi;
      }

      std::copy(finished.begin(), finished.end(), std::back_inserter(blks_processed));
      finished.clear();
   }

   // Fourth, make sure all direct requests have arrived.
   // This can not be skipped as responses write into request memory buffers.
   if (direct_handler != 0)
   {
      TRACEF(Dump, "Read() waiting for direct requests ");

      XrdSysCondVarHelper _lck(direct_handler->m_cond);

      while (direct_handler->m_to_wait > 0)
      {
         direct_handler->m_cond.Wait();
      }

      if (direct_handler->m_errno == 0)
      {
         bytes_read += direct_size;
         loc_stats.m_BytesBypassed += direct_size;
      }
      else
      {
         // Set error and report only if this is the first error in this read.
         if ( ! error_cond)
         {
            error_cond = direct_handler->m_errno;
            TRACEF(Error, "Read(), direct read finished with error " << -error_cond << " " << XrdSysE2T(-error_cond));
         }
      }

      delete direct_handler;
   }
   assert(iUserSize >= bytes_read);

   // Last, stamp and release blocks, release file.
   {
      XrdSysCondVarHelper _lck(m_state_cond);

      // blks_to_process can be non-empty, if we're exiting with an error.
      std::copy(blks_to_process.begin(), blks_to_process.end(), std::back_inserter(blks_processed));

      for (BlockList_i bi = blks_processed.begin(); bi != blks_processed.end(); ++bi)
      {
         TRACEF(Dump, "Read() dec_ref_count " << (void*)(*bi) << " idx = " << (int)((*bi)->m_offset/BufferSize()));
         dec_ref_count(*bi);
      }

      // update prefetch score
      m_prefetch_hit_cnt += prefetchHitsRam;
      for (IntList_i d = blks_on_disk.begin(); d !=  blks_on_disk.end(); ++d)
      {
         if (m_cfi.TestBitPrefetch(offsetIdx(*d)))
            m_prefetch_hit_cnt++;
      }
      m_prefetch_score = float(m_prefetch_hit_cnt)/m_prefetch_read_cnt;
   }

   m_stats.AddReadStats(loc_stats);

   return error_cond ? error_cond : bytes_read;
}

//------------------------------------------------------------------------------

void File::WriteBlockToDisk(Block* b)
{
   // write block buffer into disk file
   long long   offset = b->m_offset - m_offset;
   long long   size   = b->get_size();
   ssize_t     retval;

   if (m_cfi.IsCkSumCache())
      if (b->has_cksums())
         retval = m_data_file->pgWrite(b->get_buff(), offset, size, b->ref_cksum_vec().data(), 0);
      else
         retval = m_data_file->pgWrite(b->get_buff(), offset, size, 0, 0);
   else
      retval = m_data_file->Write(b->get_buff(), offset, size);

   if (retval < size)
   {
      if (retval < 0)
      {
         GetLog()->Emsg("WriteToDisk()", -retval, "write block to disk", GetLocalPath().c_str());
      }
      else
      {
         TRACEF(Error, "WriteToDisk() incomplete block write ret=" << retval << " (should be " << size << ")");
      }

      XrdSysCondVarHelper _lck(m_state_cond);

      dec_ref_count(b);

      return;
   }

   const int blk_idx =  (b->m_offset - m_offset) / m_cfi.GetBufferSize();

   // Set written bit.
   TRACEF(Dump, "WriteToDisk() success set bit for block " <<  b->m_offset << " size=" <<  size);

   bool schedule_sync = false;
   {
      XrdSysCondVarHelper _lck(m_state_cond);

      m_cfi.SetBitWritten(blk_idx);

      if (b->m_prefetch)
      {
         m_cfi.SetBitPrefetch(blk_idx);
      }
      if (b->req_cksum_net() && ! b->has_cksums() && m_cfi.IsCkSumNet())
      {
         m_cfi.ResetCkSumNet();
      }

      dec_ref_count(b);

      // Set synced bit or stash block index if in actual sync.
      // Synced state is only written out to cinfo file when data file is synced.
      if (m_in_sync)
      {
         m_writes_during_sync.push_back(blk_idx);
      }
      else
      {
         m_cfi.SetBitSynced(blk_idx);
         ++m_non_flushed_cnt;
         if (m_non_flushed_cnt >= Cache::GetInstance().RefConfiguration().m_flushCnt &&
             ! m_in_shutdown)
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
   TRACEF(Dump, "Sync()");

   int ret     = m_data_file->Fsync();
   bool errorp = false;
   if (ret == XrdOssOK)
   {
      Stats loc_stats = m_stats.Clone();
      m_cfi.WriteIOStat(loc_stats);
      m_cfi.Write(m_info_file,m_filename.c_str());
      int cret = m_info_file->Fsync();
      if (cret != XrdOssOK)
      {
         TRACEF(Error, "Sync cinfo file sync error " << cret);
         errorp = true;
      }
   }
   else
   {
      TRACEF(Error, "Sync data file sync error " << ret << ", cinfo file has not been updated");
      errorp = true;
   }

   if (errorp)
   {
      TRACEF(Error, "Sync failed, unlinking local files and initiating shutdown of File object");

      // Unlink will also call this->initiate_emergency_shutdown()
      Cache::GetInstance().UnlinkFile(m_filename, false);

      XrdSysCondVarHelper _lck(&m_state_cond);

      m_writes_during_sync.clear();
      m_in_sync = false;

      return;
   }

   int written_while_in_sync;
   {
      XrdSysCondVarHelper _lck(&m_state_cond);
      for (std::vector<int>::iterator i = m_writes_during_sync.begin(); i != m_writes_during_sync.end(); ++i)
      {
         m_cfi.SetBitSynced(*i);
      }
      written_while_in_sync = m_non_flushed_cnt = (int) m_writes_during_sync.size();
      m_writes_during_sync.clear();
      m_in_sync = false;
   }
   TRACEF(Dump, "Sync "<< written_while_in_sync  << " blocks written during sync");
}

//------------------------------------------------------------------------------

void File::inc_ref_count(Block* b)
{
   // Method always called under lock.
   b->m_refcnt++;
   TRACEF(Dump, "inc_ref_count " << b << " refcnt  " << b->m_refcnt);
}

//------------------------------------------------------------------------------

void File::dec_ref_count(Block* b)
{
   // Method always called under lock.
   assert(b->is_finished());
   b->m_refcnt--;
   assert(b->m_refcnt >= 0);

   if (b->m_refcnt == 0)
   {
      free_block(b);
   }
}

void File::free_block(Block* b)
{
   // Method always called under lock.
   int i = b->m_offset / BufferSize();
   TRACEF(Dump, "free_block block " << b << "  idx =  " <<  i);
   size_t ret = m_block_map.erase(i);
   if (ret != 1)
   {
      // assert might be a better option than a warning
      TRACEF(Error, "free_block did not erase " <<  i  << " from map");
   }
   else
   {
      cache()->ReleaseRAM(b->m_buff, b->m_req_size);
      delete b;
   }

   if (m_prefetch_state == kHold && (int) m_block_map.size() < Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
   {
      m_prefetch_state = kOn;
      cache()->RegisterPrefetchFile(this);
   }
}

//------------------------------------------------------------------------------

bool File::select_current_io_or_disable_prefetching(bool skip_current)
{
   // Method always called under lock. It also expects prefetch to be active.

   int  io_size = (int) m_io_map.size();
   bool io_ok   = false;

   if (io_size == 1)
   {
      io_ok = m_io_map.begin()->second.m_allow_prefetching;
      if (io_ok)
      {
         m_current_io = m_io_map.begin();
      }
   }
   else if (io_size > 1)
   {
      IoMap_i mi = m_current_io;
      if (skip_current && mi != m_io_map.end()) ++mi;

      for (int i = 0; i < io_size; ++i)
      {
         if (mi == m_io_map.end()) mi = m_io_map.begin();

         if (mi->second.m_allow_prefetching)
         {
            m_current_io = mi;
            io_ok = true;
            break;
         }
         ++mi;
      }
   }

   if ( ! io_ok)
   {
      m_current_io    = m_io_map.end();
      m_prefetch_state = kStopped;
      cache()->DeRegisterPrefetchFile(this);
   }

   return io_ok;
}

//------------------------------------------------------------------------------

void File::ProcessBlockResponse(BlockResponseHandler* brh, int res)
{
   static const char* tpfx = "ProcessBlockResponse ";

   Block *b = brh->m_block;

   TRACEF(Dump, tpfx << "block=" << b << ", idx=" << b->m_offset/BufferSize() << ", off=" << b->m_offset << ", res=" << res);

   if (res >= 0 && res != b->get_size())
   {
      // Incorrect number of bytes received, apparently size of the file on the remote
      // is different than what the cache expects it to be.
      TRACEF(Error, tpfx << "Wrong number of bytes received, assuming remote/local file size mismatch, unlinking local files and initiating shutdown of File object");
      Cache::GetInstance().UnlinkFile(m_filename, false);
   }

   XrdSysCondVarHelper _lck(m_state_cond);

   // Deregister block from IO's prefetch count, if needed.
   if (b->m_prefetch)
   {
      IoMap_i mi = m_io_map.find(b->get_io());
      if (mi != m_io_map.end())
      {
         --mi->second.m_active_prefetches;

         // If failed and IO is still prefetching -- disable prefetching on this IO.
         if (res < 0 && mi->second.m_allow_prefetching)
         {
            TRACEF(Debug, tpfx << "after failed prefetch on io " << b->get_io() << " disabling prefetching on this io.");
            mi->second.m_allow_prefetching = false;

            // Check if any IO is still available for prfetching. If not, stop it.
            if (m_prefetch_state == kOn || m_prefetch_state == kHold)
            {
               if ( ! select_current_io_or_disable_prefetching(false) )
               {
                  TRACEF(Debug, tpfx << "stopping prefetching after io " <<  b->get_io() << " marked as bad.");
               }
            }
         }

         // If failed with no subscribers -- remove the block now.
         if (b->m_refcnt == 0 && (res < 0 || m_in_shutdown))
         {
            free_block(b);
         }
      }
      else
      {
         TRACEF(Error, tpfx << "io " << b->get_io() << " not found in IoMap.");
      }
   }

   if (res == b->get_size())
   {
      b->set_downloaded();
      // Increase ref-count for the writer.
      TRACEF(Dump, tpfx << "inc_ref_count idx=" <<  b->m_offset/BufferSize());
      if ( ! m_in_shutdown)
      {
         inc_ref_count(b);
         m_stats.AddWriteStats(b->get_size(), b->get_n_cksum_errors());
         cache()->AddWriteTask(b, true);
      }
   }
   else
   {
      if (res < 0) {
         TRACEF(Error, tpfx << "block " << b << ", idx=" << b->m_offset/BufferSize() << ", off=" << b->m_offset << " error=" << res);
      } else {
         TRACEF(Error, tpfx << "block " << b << ", idx=" << b->m_offset/BufferSize() << ", off=" << b->m_offset << " incomplete, got " << res << " expected " << b->get_size());
#if defined(__APPLE__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
         res = -EIO;
#else
         res = -EREMOTEIO;
#endif
      }
      b->set_error(res);
   }

   m_state_cond.Broadcast();
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
   //       blks_max could be an argument

   BlockList_t blks;

   TRACEF(Dump, "Prefetch enter to check download status");
   {
      XrdSysCondVarHelper _lck(m_state_cond);

      if (m_prefetch_state != kOn)
      {
         return;
      }

      if ( ! select_current_io_or_disable_prefetching(true) )
      {
         TRACEF(Error, "Prefetch no available IO object found, prefetching stopped. This should not happen, i.e., prefetching should be stopped before.");
         return;
      }

      // Select block(s) to fetch.
      for (int f = 0; f < m_cfi.GetNBlocks(); ++f)
      {
         if ( ! m_cfi.TestBitWritten(f))
         {
            int f_act = f + m_offset / m_cfi.GetBufferSize();

            BlockMap_i bi = m_block_map.find(f_act);
            if (bi == m_block_map.end())
            {
               Block *b = PrepareBlockRequest(f_act, m_current_io->first, true);
               if (b)
               {
                  TRACEF(Dump, "Prefetch take block " << f_act);
                  blks.push_back(b);
                  // Note: block ref_cnt not increased, it will be when placed into write queue.
                  m_prefetch_read_cnt++;
                  m_prefetch_score = float(m_prefetch_hit_cnt)/m_prefetch_read_cnt;
               }
               else
               {
                  // This shouldn't happen as prefetching stops when RAM is 70% full.
                  TRACEF(Warning, "Prefetch allocation failed for block " << f_act);
               }
               break;
            }
         }
      }

      if (blks.empty())
      {
         TRACEF(Debug, "Prefetch file is complete, stopping prefetch.");
         m_prefetch_state = kComplete;
         cache()->DeRegisterPrefetchFile(this);
      }
      else
      {
         m_current_io->second.m_active_prefetches += (int) blks.size();
      }
   }

   if ( ! blks.empty())
   {
      ProcessBlockRequests(blks);
   }
}


//------------------------------------------------------------------------------

float File::GetPrefetchScore() const
{
   return m_prefetch_score;
}

XrdSysError* File::GetLog()
{
   return Cache::GetInstance().GetLog();
}

XrdSysTrace* File::GetTrace()
{
   return Cache::GetInstance().GetTrace();
}

void File::insert_remote_location(const std::string &loc)
{
   if ( ! loc.empty())
   {
      size_t p = loc.find_first_of('@');
      m_remote_locations.insert(&loc[p != string::npos ? p + 1 : 0]);
   }
}

std::string File::GetRemoteLocations() const
{
   std::string s;
   if ( ! m_remote_locations.empty())
   {
      size_t      sl = 0;
      int         nl = 0;
      for (std::set<std::string>::iterator i = m_remote_locations.begin(); i != m_remote_locations.end(); ++i, ++nl)
      {
         sl += i->size();
      }
      s.reserve(2 + sl + 2*nl + nl - 1 + 1);
      s = '[';
      int j = 1;
      for (std::set<std::string>::iterator i = m_remote_locations.begin(); i != m_remote_locations.end(); ++i, ++j)
      {
         s += '"'; s += *i; s += '"';
         if (j < nl) s += ',';
      }
      s += ']';
   }
   else
   {
      s = "[]";
   }
   return s;
}

//==============================================================================
//=======================    RESPONSE HANDLERS    ==============================
//==============================================================================

void BlockResponseHandler::Done(int res)
{
   m_block->m_file->ProcessBlockResponse(this, res);

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
