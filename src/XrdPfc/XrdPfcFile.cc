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
   m_data_file(0),
   m_info_file(0),
   m_cfi(Cache::GetInstance().GetTrace(), Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks > 0),
   m_filename(path),
   m_offset(iOffset),
   m_file_size(iFileSize),
   m_current_io(m_io_set.end()),
   m_ios_in_detach(0),
   m_non_flushed_cnt(0),
   m_in_sync(false),
   m_detach_time_logged(false),
   m_in_shutdown(false),
   m_state_cond(0),
   m_block_size(0),
   m_num_blocks(0),
   m_prefetch_state(kOff),
   m_prefetch_read_cnt(0),
   m_prefetch_hit_cnt(0),
   m_prefetch_score(0)
{}

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
   TRACEF(Dump, "BlockRemovedFromWriteQ() block = " << (void*) b << " idx= " << b->m_offset/m_block_size);

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

      IoSet_i mi = m_io_set.find(io);

      if (mi != m_io_set.end())
      {
         unsigned int n_active_reads = io->m_active_read_reqs;

         TRACE(Info, "ioActive for io " << io <<
                ", active_reads "       << n_active_reads <<
                ", active_prefetches "  << io->m_active_prefetches <<
                ", allow_prefetching "  << io->m_allow_prefetching <<
                ", ios_in_detach "      << m_ios_in_detach);
         TRACEF(Info,
                "\tio_map.size() "      << m_io_set.size() <<
                ", block_map.size() "   << m_block_map.size() << ", file");

         insert_remote_location(loc);

         io->m_allow_prefetching = false;
         io->m_in_detach = true;

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

         if (n_active_reads > 0)
         {
            io_active_result = true;
         }
         else if (m_io_set.size() - m_ios_in_detach == 1)
         {
            io_active_result = ! m_block_map.empty();
         }
         else
         {
            io_active_result = io->m_active_prefetches > 0;
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
         TRACEF(Error, "ioActive io " << io <<" not found in IoSet. This should not happen.");
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
   if ( ! m_in_shutdown)
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

   IoSet_i mi = m_io_set.find(io);

   if (mi == m_io_set.end())
   {
      m_io_set.insert(io);
      io->m_attach_time = now;
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

   IoSet_i mi = m_io_set.find(io);

   if (mi != m_io_set.end())
   {
      if (mi == m_current_io)
      {
         ++m_current_io;
      }

      m_stats.IoDetach(now - io->m_attach_time);
      m_io_set.erase(mi);
      --m_ios_in_detach;

      if (m_io_set.empty() && m_prefetch_state != kStopped && m_prefetch_state != kComplete)
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
         // TODO: If the file is complete, we don't need to reset net cksums.
         m_cfi.DowngradeCkSumState(conf.get_cs_Chk());
      }
   }

   if (initialize_info_file)
   {
      m_cfi.SetBufferSizeFileSizeAndCreationTime(conf.m_bufferSize, m_file_size);
      m_cfi.SetCkSumState(conf.get_cs_Chk());
      m_cfi.ResetNoCkSumTime();
      m_cfi.Write(m_info_file, ifn.c_str());
      m_info_file->Fsync();
      TRACEF(Debug, tpfx << "Creating new file info, data size = " <<  m_file_size << " num blocks = "  << m_cfi.GetNBlocks());
   }

   m_cfi.WriteIOStatAttach();
   m_state_cond.Lock();
   m_block_size = m_cfi.GetBufferSize();
   m_num_blocks = m_cfi.GetNBlocks();
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
                   int       &size)    // size to copy
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
      size    = (int) (ovlp_end - ovlp_beg);

      assert(size <= blk_size);
      return true;
   }
   else
   {
      return false;
   }
}

//------------------------------------------------------------------------------

Block* File::PrepareBlockRequest(int i, IO *io, void *req_id, bool prefetch)
{
   // Must be called w/ state_cond locked.
   // Checks on size etc should be done before.
   //
   // Reference count is 0 so increase it in calling function if you want to
   // catch the block while still in memory.

   const long long off   = i * m_block_size;
   const int  last_block = m_num_blocks - 1;
   const bool cs_net     = cache()->RefConfiguration().is_cschk_net();

   int blk_size, req_size;
   if (i == last_block) {
      blk_size = req_size = m_file_size - off;
      if (cs_net && req_size & 0xFFF) req_size = (req_size & ~0xFFF) + 0x1000;
   } else {
      blk_size = req_size = m_block_size;
   }

   Block *b   = 0;
   char  *buf = cache()->RequestRAM(req_size);

   if (buf)
   {
      b = new (std::nothrow) Block(this, io, req_id, buf, off, blk_size, req_size, prefetch, cs_net);

      if (b)
      {
         m_block_map[i] = b;

         // Actual Read request is issued in ProcessBlockRequests().

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

   BlockResponseHandler* brh = new BlockResponseHandler(b);

   if (XRD_TRACE What >= TRACE_Dump) {
      char buf[256];
      snprintf(buf, 256, "idx=%lld, block=%p, prefetch=%d, off=%lld, req_size=%d, buff=%p, resp_handler=%p ",
         b->get_offset()/m_block_size, b, b->m_prefetch, b->get_offset(), b->get_req_size(), b->get_buff(), brh);
      TRACEF(Dump, "ProcessBlockRequest() " << buf);
   }

   if (b->req_cksum_net())
   {
      b->get_io()->GetInput()->pgRead(*brh, b->get_buff(), b->get_offset(), b->get_req_size(),
                                      b->ref_cksum_vec(), 0, b->ptr_n_cksum_errors());
   } else {
      b->get_io()->GetInput()->  Read(*brh, b->get_buff(), b->get_offset(), b->get_size());
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

void File::RequestBlocksDirect(IO *io, DirectResponseHandler *handler, std::vector<XrdOucIOVec>& ioVec, int expected_size)
{
   TRACEF(DumpXL, "RequestBlocksDirect() issuing ReadV for n_chunks = " << (int) ioVec.size() << ", total_size = " << expected_size);

   io->GetInput()->ReadV( *handler, ioVec.data(), (int) ioVec.size());
}

//------------------------------------------------------------------------------

int File::ReadBlocksFromDisk(std::vector<XrdOucIOVec>& ioVec, int expected_size)
{
   TRACEF(DumpXL, "ReadBlocksFromDisk() issuing ReadV for n_chunks = " << (int) ioVec.size() << ", total_size = " << expected_size);

   long long rs = m_data_file->ReadV(ioVec.data(), (int) ioVec.size());

   if (rs < 0)
   {
      TRACEF(Error, "ReadBlocksFromDisk neg retval = " <<  rs);
      return rs;
   }

   if (rs != expected_size)
   {
      TRACEF(Error, "ReadBlocksFromDisk incomplete size = " << rs);
      return -EIO;
   }

   return (int) rs;
}

//------------------------------------------------------------------------------

int File::Read(IO *io, char* iUserBuff, long long iUserOff, int iUserSize, ReadReqRH *rh)
{
   // rrc_func is ONLY called from async processing.
   // If this function returns anything other than -EWOULDBLOCK, rrc_func needs to be called by the caller.
   // This streamlines implementation of synchronous IO::Read().

   TRACEF(Dump, "Read() sid: " << Xrd::hex1 << rh->m_seq_id << " size: " << iUserSize);

   m_state_cond.Lock();

   if (m_in_shutdown || io->m_in_detach)
   {
      m_state_cond.UnLock();
      return m_in_shutdown ? -ENOENT : -EBADF;
   }

   // Shortcut -- file is fully downloaded.

   if (m_cfi.IsComplete())
   {
      m_state_cond.UnLock();
      int ret = m_data_file->Read(iUserBuff, iUserOff, iUserSize);
      if (ret > 0) m_stats.AddBytesHit(ret);
      return ret;
   }

   XrdOucIOVec readV( { iUserOff, iUserSize, 0, iUserBuff } );

   return ReadOpusCoalescere(io, &readV, 1, rh, "Read() ");
}

//------------------------------------------------------------------------------

int File::ReadV(IO *io, const XrdOucIOVec *readV, int readVnum, ReadReqRH *rh)
{
   TRACEF(Dump, "ReadV() for " << readVnum << " chunks.");

   m_state_cond.Lock();

   if (m_in_shutdown || io->m_in_detach)
   {
      m_state_cond.UnLock();
      return m_in_shutdown ? -ENOENT : -EBADF;
   }

   // Shortcut -- file is fully downloaded.

   if (m_cfi.IsComplete())
   {
      m_state_cond.UnLock();
      int ret = m_data_file->ReadV(const_cast<XrdOucIOVec*>(readV), readVnum);
      if (ret > 0) m_stats.AddBytesHit(ret);
      return ret;
   }

   return ReadOpusCoalescere(io, readV, readVnum, rh, "ReadV() ");
}

//------------------------------------------------------------------------------

int File::ReadOpusCoalescere(IO *io, const XrdOucIOVec *readV, int readVnum,
                             ReadReqRH *rh, const char *tpfx)
{
   // Non-trivial processing for Read and ReadV.
   // Entered under lock.
   //
   // loop over reqired blocks:
   //   - if on disk, ok;
   //   - if in ram or incoming, inc ref-count
   //   - otherwise request and inc ref count (unless RAM full => request direct)
   // unlock

   int prefetch_cnt = 0;

   ReadRequest *read_req = nullptr;
   BlockList_t  blks_to_request;     // blocks we are issuing a new remote request for

   std::unordered_map<Block*, std::vector<ChunkRequest>> blks_ready;

   std::vector<XrdOucIOVec> iovec_disk;
   std::vector<XrdOucIOVec> iovec_direct;
   int                      iovec_disk_total = 0;
   int                      iovec_direct_total = 0;

   for (int iov_idx = 0; iov_idx < readVnum; ++iov_idx)
   {
      const XrdOucIOVec &iov = readV[iov_idx];
      long long   iUserOff  = iov.offset;
      int         iUserSize = iov.size;
      char       *iUserBuff = iov.data;

      const int idx_first = iUserOff / m_block_size;
      const int idx_last  = (iUserOff + iUserSize - 1) / m_block_size;

      TRACEF(DumpXL, tpfx << "sid: " << Xrd::hex1 << rh->m_seq_id << " idx_first: " << idx_first << " idx_last: " << idx_last);

      enum LastBlock_e { LB_other, LB_disk, LB_direct };

      LastBlock_e lbe = LB_other;

      for (int block_idx = idx_first; block_idx <= idx_last; ++block_idx)
      {
         TRACEF(DumpXL, tpfx << "sid: " << Xrd::hex1 << rh->m_seq_id << " idx: " << block_idx);
         BlockMap_i bi = m_block_map.find(block_idx);

         // overlap and read
         long long off;     // offset in user buffer
         long long blk_off; // offset in block
         int       size;    // size to copy

         overlap(block_idx, m_block_size, iUserOff, iUserSize, off, blk_off, size);

         // In RAM or incoming?
         if (bi != m_block_map.end())
         {
            inc_ref_count(bi->second);
            TRACEF(Dump, tpfx << (void*) iUserBuff << " inc_ref_count for existing block " << bi->second << " idx = " <<  block_idx);

            if (bi->second->is_finished())
            {
               // note, blocks with error should not be here !!!
               // they should be either removed or reissued in ProcessBlockResponse()
               assert(bi->second->is_ok());

               blks_ready[bi->second].emplace_back( ChunkRequest(nullptr, iUserBuff + off, blk_off, size) );

               if (bi->second->m_prefetch)
                  ++prefetch_cnt;
            }
            else
            {
               if ( ! read_req)
                  read_req = new ReadRequest(io, rh);

               // We have a lock on state_cond --> as we register the request before releasing the lock,
               // we are sure to get a call-in via the ChunkRequest handling when this block arrives.

               bi->second->m_chunk_reqs.emplace_back( ChunkRequest(read_req, iUserBuff + off, blk_off, size) );
               ++read_req->m_n_chunk_reqs;
            }

            lbe = LB_other;
         }
         // On disk?
         else if (m_cfi.TestBitWritten(offsetIdx(block_idx)))
         {
            TRACEF(DumpXL, tpfx << "read from disk " <<  (void*)iUserBuff << " idx = " << block_idx);

            if (lbe == LB_disk)
               iovec_disk.back().size += size;
            else
               iovec_disk.push_back( { block_idx * m_block_size + blk_off, size, 0, iUserBuff + off } );
            iovec_disk_total += size;

            if (m_cfi.TestBitPrefetch(offsetIdx(block_idx)))
               ++prefetch_cnt;

            lbe = LB_disk;
         }
         // Neither ... then we have to go get it ...
         else
         {
            if ( ! read_req)
               read_req = new ReadRequest(io, rh);

            // Is there room for one more RAM Block?
            Block *b = PrepareBlockRequest(block_idx, io, read_req, false);
            if (b)
            {
               TRACEF(Dump, tpfx << "inc_ref_count new " <<  (void*)iUserBuff << " idx = " << block_idx);
               inc_ref_count(b);
               blks_to_request.push_back(b);

               b->m_chunk_reqs.emplace_back(ChunkRequest(read_req, iUserBuff + off, blk_off, size));
               ++read_req->m_n_chunk_reqs;

               lbe = LB_other;
            }
            else // Nope ... read this directly without caching.
            {
               TRACEF(DumpXL, tpfx << "direct block " << block_idx << ", blk_off " << blk_off << ", size " << size);

               if (lbe == LB_direct)
                  iovec_direct.back().size += size;
               else
                  iovec_direct.push_back( { block_idx * m_block_size + blk_off, size, 0, iUserBuff + off } );
               iovec_direct_total += size;
               read_req->m_direct_done = false;

               lbe = LB_direct;
            }
         }
      } // end for over blocks in an IOVec
   } // end for over readV IOVec

   inc_prefetch_hit_cnt(prefetch_cnt);

   m_state_cond.UnLock();

   // First, send out remote requests for new blocks.
   if ( ! blks_to_request.empty())
   {
      ProcessBlockRequests(blks_to_request);
      blks_to_request.clear();
   }

   // Second, send out remote direct read requests.
   if ( ! iovec_direct.empty())
   {
      DirectResponseHandler *direct_handler = new DirectResponseHandler(this, read_req, 1);
      RequestBlocksDirect(io, direct_handler, iovec_direct, iovec_direct_total);

      TRACEF(Dump, tpfx << "direct read requests sent out, n_chunks = " << (int) iovec_direct.size() << ", total_size = " << iovec_direct_total);
   }

   // Begin synchronous part where we process data that is already in RAM or on disk.

   long long bytes_read = 0;
   int       error_cond = 0; // to be set to -errno

   // Third, process blocks that are available in RAM.
   if ( ! blks_ready.empty())
   {
      for (auto &bvi : blks_ready)
      {
         for (auto &cr : bvi.second)
         {
            TRACEF(DumpXL, tpfx << "ub=" << (void*)cr.m_buf << " from pre-finished block " << bvi.first->m_offset/m_block_size << " size " << cr.m_size);
            memcpy(cr.m_buf, bvi.first->m_buff + cr.m_off, cr.m_size);
            bytes_read += cr.m_size;
         }
      }
   }

   // Fourth, read blocks from disk.
   if ( ! iovec_disk.empty())
   {
      int rc = ReadBlocksFromDisk(iovec_disk, iovec_disk_total);
      TRACEF(DumpXL, tpfx << "from disk finished size = " << rc);
      if (rc >= 0)
      {
         bytes_read += rc;
      }
      else
      {
         error_cond = rc;
         TRACEF(Error, tpfx << "failed read from disk");
      }
   }

   // End synchronous part -- update with sync stats and determine actual state of this read.
   // Note: remote reads might have already finished during disk-read!

   m_state_cond.Lock();

   for (auto &bvi : blks_ready)
      dec_ref_count(bvi.first, (int) bvi.second.size());

   if (read_req)
   {
      read_req->m_bytes_read += bytes_read;
      read_req->update_error_cond(error_cond);
      read_req->m_stats.m_BytesHit += bytes_read;
      read_req->m_sync_done = true;

      if (read_req->is_complete())
      {
         // Almost like FinalizeReadRequest(read_req) -- but no callout!
         m_state_cond.UnLock();

         m_stats.AddReadStats(read_req->m_stats);

         int ret = read_req->return_value();
         delete read_req;
         return ret;
      }
      else
      {
         m_state_cond.UnLock();
         return -EWOULDBLOCK;
      }
   }
   else
   {
      m_stats.m_BytesHit += bytes_read;
      m_state_cond.UnLock();

      // !!! No callout.

      return error_cond ? error_cond : bytes_read;
   }
}


//==============================================================================
// WriteBlock and Sync
//==============================================================================

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

   const int blk_idx =  (b->m_offset - m_offset) / m_block_size;

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
         if ((m_cfi.IsComplete() || m_non_flushed_cnt >= Cache::GetInstance().RefConfiguration().m_flushCnt) &&
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
      m_cfi.Write(m_info_file, m_filename.c_str());
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

   int  written_while_in_sync;
   bool resync = false;
   {
      XrdSysCondVarHelper _lck(&m_state_cond);
      for (std::vector<int>::iterator i = m_writes_during_sync.begin(); i != m_writes_during_sync.end(); ++i)
      {
         m_cfi.SetBitSynced(*i);
      }
      written_while_in_sync = m_non_flushed_cnt = (int) m_writes_during_sync.size();
      m_writes_during_sync.clear();

      // If there were writes during sync and the file is now complete,
      // let us call Sync again without resetting the m_in_sync flag.
      if (written_while_in_sync > 0 && m_cfi.IsComplete() && ! m_in_shutdown)
         resync = true;
      else
         m_in_sync = false;
   }
   TRACEF(Dump, "Sync "<< written_while_in_sync  << " blocks written during sync." << (resync ? " File is now complete - resyncing." : ""));

   if (resync)
      Sync();
}


//==============================================================================
// Block processing
//==============================================================================

void File::free_block(Block* b)
{
   // Method always called under lock.
   int i = b->m_offset / m_block_size;
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

   int  io_size = (int) m_io_set.size();
   bool io_ok   = false;

   if (io_size == 1)
   {
      io_ok = (*m_io_set.begin())->m_allow_prefetching;
      if (io_ok)
      {
         m_current_io = m_io_set.begin();
      }
   }
   else if (io_size > 1)
   {
      IoSet_i mi = m_current_io;
      if (skip_current && mi != m_io_set.end()) ++mi;

      for (int i = 0; i < io_size; ++i)
      {
         if (mi == m_io_set.end()) mi = m_io_set.begin();

         if ((*mi)->m_allow_prefetching)
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
      m_current_io    = m_io_set.end();
      m_prefetch_state = kStopped;
      cache()->DeRegisterPrefetchFile(this);
   }

   return io_ok;
}

//------------------------------------------------------------------------------

void File::ProcessDirectReadFinished(ReadRequest *rreq, int bytes_read, int error_cond)
{
   // Called from DirectResponseHandler.
   // NOT under lock.

   if (error_cond)
      TRACEF(Error, "Read(), direct read finished with error " << -error_cond << " " << XrdSysE2T(-error_cond));

   m_state_cond.Lock();

   if (error_cond)
      rreq->update_error_cond(error_cond);
   else {
      rreq->m_stats.m_BytesBypassed += bytes_read;
      rreq->m_bytes_read += bytes_read;
   }

   rreq->m_direct_done = true;

   bool rreq_complete = rreq->is_complete();

   m_state_cond.UnLock();

   if (rreq_complete)
      FinalizeReadRequest(rreq);
}

void File::ProcessBlockError(Block *b, ReadRequest *rreq)
{
   // Called from ProcessBlockResponse().
   // YES under lock -- we have to protect m_block_map for recovery through multiple IOs.
   // Does not manage m_read_req.
   // Will not complete the request.

   TRACEF(Error, "ProcessBlockError() io " << b->m_io << ", block "<< b->m_offset/m_block_size <<
                 " finished with error " << -b->get_error() << " " << XrdSysE2T(-b->get_error()));

   rreq->update_error_cond(b->get_error());
   --rreq->m_n_chunk_reqs;

   dec_ref_count(b);
}

void File::ProcessBlockSuccess(Block *b, ChunkRequest &creq)
{
   // Called from ProcessBlockResponse().
   // NOT under lock as it does memcopy ofor exisf block data.
   // Acquires lock for block, m_read_req and rreq state update.

   ReadRequest *rreq = creq.m_read_req;

   TRACEF(Dump, "ProcessBlockSuccess() ub=" << (void*)creq.m_buf  << " from finished block " << b->m_offset/m_block_size << " size " << creq.m_size);
   memcpy(creq.m_buf, b->m_buff + creq.m_off, creq.m_size);

   m_state_cond.Lock();

   rreq->m_bytes_read += creq.m_size;

   if (b->get_req_id() == (void*) rreq)
      rreq->m_stats.m_BytesMissed += creq.m_size;
   else
      rreq->m_stats.m_BytesHit    += creq.m_size;

   --rreq->m_n_chunk_reqs;

   if (b->m_prefetch)
      inc_prefetch_hit_cnt(1);

   dec_ref_count(b);

   bool rreq_complete = rreq->is_complete();

   m_state_cond.UnLock();

   if (rreq_complete)
      FinalizeReadRequest(rreq);
}

void File::FinalizeReadRequest(ReadRequest *rreq)
{
   // called from ProcessBlockResponse()
   // NOT under lock -- does callout

   m_stats.AddReadStats(rreq->m_stats);

   rreq->m_rh->Done(rreq->return_value());
   delete rreq;
}

void File::ProcessBlockResponse(Block *b, int res)
{
   static const char* tpfx = "ProcessBlockResponse ";

   TRACEF(Dump, tpfx << "block=" << b << ", idx=" << b->m_offset/m_block_size << ", off=" << b->m_offset << ", res=" << res);

   if (res >= 0 && res != b->get_size())
   {
      // Incorrect number of bytes received, apparently size of the file on the remote
      // is different than what the cache expects it to be.
      TRACEF(Error, tpfx << "Wrong number of bytes received, assuming remote/local file size mismatch, unlinking local files and initiating shutdown of File object");
      Cache::GetInstance().UnlinkFile(m_filename, false);
   }

   m_state_cond.Lock();

   // Deregister block from IO's prefetch count, if needed.
   if (b->m_prefetch)
   {
      IO     *io = b->get_io();
      IoSet_i mi = m_io_set.find(io);
      if (mi != m_io_set.end())
      {
         --io->m_active_prefetches;

         // If failed and IO is still prefetching -- disable prefetching on this IO.
         if (res < 0 && io->m_allow_prefetching)
         {
            TRACEF(Debug, tpfx << "after failed prefetch on io " << io << " disabling prefetching on this io.");
            io->m_allow_prefetching = false;

            // Check if any IO is still available for prfetching. If not, stop it.
            if (m_prefetch_state == kOn || m_prefetch_state == kHold)
            {
               if ( ! select_current_io_or_disable_prefetching(false) )
               {
                  TRACEF(Debug, tpfx << "stopping prefetching after io " <<  b->get_io() << " marked as bad.");
               }
            }
         }

         // If failed with no subscribers -- delete the block and exit.
         if (b->m_refcnt == 0 && (res < 0 || m_in_shutdown))
         {
            free_block(b);
            m_state_cond.UnLock();
            return;
         }
      }
      else
      {
         TRACEF(Error, tpfx << "io " << b->get_io() << " not found in IoSet.");
      }
   }

   if (res == b->get_size())
   {
      b->set_downloaded();
      TRACEF(Dump, tpfx << "inc_ref_count idx=" <<  b->m_offset/m_block_size);
      if ( ! m_in_shutdown)
      {
         // Increase ref-count for the writer.
         inc_ref_count(b);
         m_stats.AddWriteStats(b->get_size(), b->get_n_cksum_errors());
         cache()->AddWriteTask(b, true);
      }

      // Swap chunk-reqs vector out of Block, it will be processed outside of lock.
      vChunkRequest_t  creqs_to_notify;
      creqs_to_notify.swap( b->m_chunk_reqs );

      m_state_cond.UnLock();

      for (auto &creq : creqs_to_notify)
      {
         ProcessBlockSuccess(b, creq);
      }
   }
   else
   {
      if (res < 0) {
         TRACEF(Error, tpfx << "block " << b << ", idx=" << b->m_offset/m_block_size << ", off=" << b->m_offset << " error=" << res);
      } else {
         TRACEF(Error, tpfx << "block " << b << ", idx=" << b->m_offset/m_block_size << ", off=" << b->m_offset << " incomplete, got " << res << " expected " << b->get_size());
#if defined(__APPLE__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__)) || defined(__FreeBSD__)
         res = -EIO;
#else
         res = -EREMOTEIO;
#endif
      }
      b->set_error(res);

      // Loop over Block's chunk-reqs vector, error out ones with the same IO.
      // Collect others with a different IO, the first of them will be used to reissue the request.
      // This is then done outside of lock.
      std::list<ReadRequest*> rreqs_to_complete;
      vChunkRequest_t         creqs_to_keep;

      for(ChunkRequest &creq : b->m_chunk_reqs)
      {
         ReadRequest *rreq = creq.m_read_req;

         if (rreq->m_io == b->get_io())
         {
            ProcessBlockError(b, rreq);
            if (rreq->is_complete())
            {
               rreqs_to_complete.push_back(rreq);
            }
         }
         else
         {
            creqs_to_keep.push_back(creq);
         }
      }

      bool reissue = false;
      if ( ! creqs_to_keep.empty())
      {
         ReadRequest *rreq = creqs_to_keep.front().m_read_req;

         TRACEF(Info, "ProcessBlockResponse() requested block " << (void*)b << " failed with another io " <<
               b->get_io() << " - reissuing request with my io " << rreq->m_io);

         b->reset_error_and_set_io(rreq->m_io, rreq);
         b->m_chunk_reqs.swap( creqs_to_keep );
         reissue = true;
      }

      m_state_cond.UnLock();

      for (auto rreq : rreqs_to_complete)
         FinalizeReadRequest(rreq);

      if (reissue)
         ProcessBlockRequest(b);
   }
}

//------------------------------------------------------------------------------

const char* File::lPath() const
{
   return m_filename.c_str();
}

//------------------------------------------------------------------------------

int File::offsetIdx(int iIdx) const
{
   return iIdx - m_offset/m_block_size;
}


//------------------------------------------------------------------------------

void File::Prefetch()
{
   // Check that block is not on disk and not in RAM.
   // TODO: Could prefetch several blocks at once!
   //       blks_max could be an argument

   BlockList_t blks;

   TRACEF(DumpXL, "Prefetch() entering.");
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
      for (int f = 0; f < m_num_blocks; ++f)
      {
         if ( ! m_cfi.TestBitWritten(f))
         {
            int f_act = f + m_offset / m_block_size;

            BlockMap_i bi = m_block_map.find(f_act);
            if (bi == m_block_map.end())
            {
               Block *b = PrepareBlockRequest(f_act, *m_current_io, nullptr, true);
               if (b)
               {
                  TRACEF(Dump, "Prefetch take block " << f_act);
                  blks.push_back(b);
                  // Note: block ref_cnt not increased, it will be when placed into write queue.

                  inc_prefetch_read_cnt(1);
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
         (*m_current_io)->m_active_prefetches += (int) blks.size();
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
      m_remote_locations.insert(&loc[p != std::string::npos ? p + 1 : 0]);
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
   m_block->m_file->ProcessBlockResponse(m_block, res);
   delete this;
}

//------------------------------------------------------------------------------

void DirectResponseHandler::Done(int res)
{
   m_mutex.Lock();

   int n_left = --m_to_wait;

   if (res < 0) {
      if (m_errno == 0) m_errno = res; // store first reported error
   } else {
      m_bytes_read += res;
   }

   m_mutex.UnLock();

   if (n_left == 0)
   {
      m_file->ProcessDirectReadFinished(m_read_req, m_bytes_read, m_errno);
      delete this;
   }
}
