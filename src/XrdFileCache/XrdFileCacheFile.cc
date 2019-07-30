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

const int BLOCK_WRITE_MAX_ATTEMPTS = 4;

Cache* cache() { return &Cache::GetInstance(); }

}

const char *File::m_traceID = "File";

//------------------------------------------------------------------------------

File::File(const std::string& path, long long iOffset, long long iFileSize) :
   m_ref_cnt(0),
   m_is_open(false),
   m_in_shutdown(false),
   m_output(0),
   m_infoFile(0),
   m_cfi(Cache::GetInstance().GetTrace(), Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks > 0),
   m_filename(path),
   m_offset(iOffset),
   m_fileSize(iFileSize),
   m_current_io(m_io_map.end()),
   m_ios_in_detach(0),
   m_non_flushed_cnt(0),
   m_in_sync(false),
   m_downloadCond(0),
   m_prefetchState(kOff),
   m_prefetchReadCnt(0),
   m_prefetchHitCnt(0),
   m_prefetchScore(1),
   m_detachTimeIsLogged(false)
{
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
   // Called from Cache::UnlinkCommon() when the file is currently open.
   // CacheUnlink is also called on FSync error.
   //
   // From this point onward the file will not be written to, cinfo file will
   // not be updated, and all new read requests will return -ENOENT.
   //
   // File's entry in the Cache's active map is set to nullptr and will be
   // removed from there shortly, in any case, well before this File object
   // shuts down. So we do not communicate to Cache about our destruction when
   // it happens.

   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      m_in_shutdown = true;

      if (m_prefetchState != kStopped && m_prefetchState != kComplete)
      {
         m_prefetchState = kStopped;
         cache()->DeRegisterPrefetchFile(this);
      }
   }
}

//------------------------------------------------------------------------------

void File::BlockRemovedFromWriteQ(Block* b)
{
   TRACEF(Dump, "File::BlockRemovedFromWriteQ() block = " << (void*) b << " idx= " << b->m_offset/m_cfi.GetBufferSize());

   XrdSysCondVarHelper _lck(m_downloadCond);
   dec_ref_count(b);
}

void File::BlocksRemovedFromWriteQ(std::list<Block*>& blocks)
{
   TRACEF(Dump, "File::BlocksRemovedFromWriteQ() n_blocks = " << blocks.size());

   XrdSysCondVarHelper _lck(m_downloadCond);

   for (std::list<Block*>::iterator i = blocks.begin(); i != blocks.end(); ++i)
   {
      dec_ref_count(*i);
   }
}

//------------------------------------------------------------------------------

bool File::ioActive(IO *io)
{
   // Retruns true if delay is needed

   TRACEF(Debug, "File::ioActive start for io " << io);

   {
      XrdSysCondVarHelper _lck(m_downloadCond);

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
                ", ioactive_false_reported " << mi->second.m_ioactive_false_reported <<
                ", ios_in_detach "           << m_ios_in_detach);
         TRACEF(Info,
                "\tio_map.size() "           << m_io_map.size() <<
                ", block_map.size() "        << m_block_map.size() << ", file");

         // It can happen that POSIX calls ioActive again after File already replied
         // false for a given IO.
         if (mi->second.m_ioactive_false_reported) return false;

         mi->second.m_allow_prefetching = false;

         // Check if any IO is still available for prfetching. If not, stop it.
         if (m_prefetchState == kOn || m_prefetchState == kHold)
         {
            if ( ! select_current_io_or_disable_prefetching(false) )
            {
               TRACEF(Debug, "ioActive stopping prefetching after io " << io << " retreat.");
            }
         }

         // On last IO, consider write queue blocks. Note, this also contains
         // blocks being prefetched.
         // For multiple IOs the ioActive queries can occur in order before
         // any of them are actually removed / detached.

         bool io_active_result;

         if (m_io_map.size() - m_ios_in_detach == 1)
         {
            io_active_result = ! m_block_map.empty();
         }
         else
         {
            io_active_result = mi->second.m_active_prefetches > 0;
         }

         if (io_active_result == false)
         {
            ++m_ios_in_detach;
            mi->second.m_ioactive_false_reported = true;
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
   XrdSysCondVarHelper _lck(m_downloadCond);
   m_detachTimeIsLogged = false;
}

bool File::FinalizeSyncBeforeExit()
{
   // Returns true if sync is required.
   // This method is called after corresponding IO is detached from PosixCache.

   XrdSysCondVarHelper _lck(m_downloadCond);
   if (m_is_open && ! m_in_shutdown)
   {
     if ( ! m_writes_during_sync.empty() || m_non_flushed_cnt > 0 || ! m_detachTimeIsLogged)
     {
       Stats loc_stats = m_stats.Clone();
       m_cfi.WriteIOStatDetach(loc_stats);
       m_detachTimeIsLogged = true;
       m_in_sync            = true;
       TRACEF(Debug, "File::FinalizeSyncBeforeExit requesting sync to write detach stats");
       return true;
     }
   }
   TRACEF(Debug, "File::FinalizeSyncBeforeExit sync not required");
   return false;
}

//------------------------------------------------------------------------------

void File::AddIO(IO *io)
{
   // Called from Cache::GetFile() when a new IO asks for the file.

   TRACEF(Debug, "File::AddIO() io = " << (void*)io);

   m_downloadCond.Lock();

   IoMap_i mi = m_io_map.find(io);

   if (mi == m_io_map.end())
   {
      m_io_map.insert(std::make_pair(io, IODetails()));

      if (m_prefetchState == kStopped)
      {
         m_prefetchState = kOn;
         cache()->RegisterPrefetchFile(this);
      }
   }
   else
   {
      TRACEF(Error, "File::AddIO() io = " << (void*)io << " already registered.");
   }

   m_downloadCond.UnLock();
}

//------------------------------------------------------------------------------

void File::RemoveIO(IO *io)
{
   // Called from Cache::ReleaseFile.

   TRACEF(Debug, "File::RemoveIO() io = " << (void*)io);

   m_downloadCond.Lock();

   IoMap_i mi = m_io_map.find(io);

   if (mi != m_io_map.end())
   {
      if (mi == m_current_io)
      {
         ++m_current_io;
      }

      m_io_map.erase(mi);
      --m_ios_in_detach;

      if (m_io_map.empty() && m_prefetchState != kStopped && m_prefetchState != kComplete)
      {
         TRACEF(Error, "File::RemoveIO() io = " << (void*)io << " Prefetching is not stopped/complete -- it should be by now.");
         m_prefetchState = kStopped;
         cache()->DeRegisterPrefetchFile(this);
      }
   }
   else
   {
      TRACEF(Error, "File::RemoveIO() io = " << (void*)io << " is NOT registered.");
   }

   m_downloadCond.UnLock();
}

//------------------------------------------------------------------------------

bool File::Open()
{
   // Sets errno accordingly.

   TRACEF(Dump, "File::Open() open file for disk cache");

   if (m_is_open)
   {
      TRACEF(Error, "File::Open() file is already opened.");
      return true;
   }

   const Configuration &conf = Cache::GetInstance().RefConfiguration();

   XrdOss     &myOss  = * Cache::GetInstance().GetOss();
   const char *myUser =   conf.m_username.c_str();
   XrdOucEnv   myEnv;
   struct stat data_stat, info_stat;

   std::string ifn = m_filename + Info::m_infoExtension;

   bool data_existed = (myOss.Stat(m_filename.c_str(), &data_stat) == XrdOssOK);
   bool info_existed = (myOss.Stat(ifn.c_str(),        &info_stat) == XrdOssOK);

   // Create the data file itself.
   char size_str[32]; sprintf(size_str, "%lld", m_fileSize);
   myEnv.Put("oss.asize",  size_str);
   myEnv.Put("oss.cgroup", conf.m_data_space.c_str());

   int res;

   if ((res = myOss.Create(myUser, m_filename.c_str(), 0600, myEnv, XRDOSS_mkpath)) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Create failed " << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      return false;
   }

   m_output = myOss.newFile(myUser);
   if ((res = m_output->Open(m_filename.c_str(), O_RDWR, 0600, myEnv)) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Open failed " << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      delete m_output; m_output = 0;
      return false;
   }

   // Create the info file.
   myEnv.Put("oss.asize", "64k"); // TODO: Calculate? Get it from configuration? Do not know length of access lists ...
   myEnv.Put("oss.cgroup", conf.m_meta_space.c_str());
   if ((res = myOss.Create(myUser, ifn.c_str(), 0600, myEnv, XRDOSS_mkpath)) != XrdOssOK)
   {
      TRACE(Error, "File::Open() Create failed for info file " << ifn << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      m_output->Close(); delete m_output; m_output = 0;
      return false;
   }

   m_infoFile = myOss.newFile(myUser);
   if ((res = m_infoFile->Open(ifn.c_str(), O_RDWR, 0600, myEnv)) != XrdOssOK)
   {
      TRACEF(Error, "File::Open() Open failed for info file " << ifn  << ERRNO_AND_ERRSTR(-res));
      errno = -res;
      delete m_infoFile; m_infoFile = 0;
      m_output->Close(); delete m_output;   m_output   = 0;
      return false;
   }

   bool initialize_info_file = true;

   if (info_existed && m_cfi.Read(m_infoFile, ifn))
   {
      TRACEF(Debug, "Open - reading existing info file. (data_existed=" << data_existed  <<
             ", data_size_stat=" << (data_existed ? data_stat.st_size : -1ll) <<
             ", data_size_from_last_block=" << m_cfi.GetExpectedDataFileSize() << ")");

      // Check if data file exists and is of reasonable size.
      if (data_existed && data_stat.st_size >= m_cfi.GetExpectedDataFileSize())
      {
         initialize_info_file = false;
      }
      else
      {
         TRACEF(Warning, "Open - basic sanity checks on data file failed, resetting info file.");
         m_cfi.ResetAllAccessStats();
      }
   }
   if (initialize_info_file)
   {
      m_cfi.SetBufferSize(conf.m_bufferSize);
      m_cfi.SetFileSize(m_fileSize);
      m_cfi.Write(m_infoFile);
      m_infoFile->Fsync();
      int ss = (m_fileSize - 1)/m_cfi.GetBufferSize() + 1;
      TRACEF(Debug, "Creating new file info, data size = " <<  m_fileSize << " num blocks = "  << ss);
   }

   m_cfi.WriteIOStatAttach();
   m_downloadCond.Lock();
   m_is_open = true;
   m_prefetchState = (m_cfi.IsComplete()) ? kComplete : kStopped; // Will engage in AddIO().
   m_downloadCond.UnLock();

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

   const long long BS   = m_cfi.GetBufferSize();
   const int last_block = m_cfi.GetSizeInBits() - 1;

   long long off     = i * BS;
   long long this_bs = (i == last_block) ? m_fileSize - off : BS;

   // 1. Should blocks be reused to avoid recreation? There is block pool in Xrd
   // 2, Memalign to page size
   Block *b = new (std::nothrow) Block(this, io, off, this_bs, prefetch);

   if (b)
   {
      m_block_map[i] = b;

      // Actual Read request is issued in ProcessBlockRequests().
      TRACEF(Dump, "File::PrepareBlockRequest() " <<  i << " prefetch " <<  prefetch << " address " << (void*) b);

      if (m_prefetchState == kOn && (int) m_block_map.size() >= Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
      {
         m_prefetchState = kHold;
         cache()->DeRegisterPrefetchFile(this);
      }
   }

   return b;
}

void File::ProcessBlockRequest(Block *b, bool prefetch)
{
   // This *must not* be called with block_map locked.

  BlockResponseHandler* oucCB = new BlockResponseHandler(b, prefetch);
  b->get_io()->GetInput()->Read(*oucCB, b->get_buff(), b->get_offset(), b->get_size());
}

void File::ProcessBlockRequests(BlockList_t& blks, bool prefetch)
{
   // This *must not* be called with block_map locked.

   for (BlockList_i bi = blks.begin(); bi != blks.end(); ++bi)
   {
      Block *b = *bi;
      BlockResponseHandler* oucCB = new BlockResponseHandler(b, prefetch);
      b->get_io()->GetInput()->Read(*oucCB, b->get_buff(), b->get_offset(), b->get_size());
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

   m_downloadCond.Lock();

   if ( ! m_is_open)
   {
      m_downloadCond.UnLock();
      TRACEF(Error, "File::Read file is not open");
      return io->GetInput()->Read(iUserBuff, iUserOff, iUserSize);
   }
   if (m_in_shutdown)
   {
      m_downloadCond.UnLock();
      return -ENOENT;
   }

   for (int block_idx = idx_first; block_idx <= idx_last; ++block_idx)
   {
      TRACEF(Dump, "File::Read() idx " << block_idx);
      BlockMap_i bi = m_block_map.find(block_idx);

      // In RAM or incoming?
      if (bi != m_block_map.end())
      {
         inc_ref_count(bi->second);
         TRACEF(Dump, "File::Read() " << (void*) iUserBuff << "inc_ref_count for existing block " << bi->second << " idx = " <<  block_idx);
         blks_to_process.push_front(bi->second);
      }
      // On disk?
      else if (m_cfi.TestBitWritten(offsetIdx(block_idx)))
      {
         TRACEF(Dump, "File::Read() read from disk " <<  (void*)iUserBuff << " idx = " << block_idx);
         blks_on_disk.push_back(block_idx);
      }
      // Then we have to get it ...
      else
      {
         // Is there room for one more RAM Block?
         Block *b;
         if (cache()->RequestRAMBlock() && (b = PrepareBlockRequest(block_idx, io, false)) != 0)
         {
            TRACEF(Dump, "File::Read() inc_ref_count new " <<  (void*)iUserBuff << " idx = " << block_idx);
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

   ProcessBlockRequests(blks_to_request, false);

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

      TRACEF(Dump, "File::Read() direct read requests sent out, size = " << direct_size);
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
         error_cond = rc;
         TRACEF(Error, "File::Read() failed read from disk");
      }
   }

   // Third, loop over blocks that are available or incoming
   int prefetchHitsRam = 0;
   while ( ! blks_to_process.empty())
   {
      BlockList_t finished;
      BlockList_t to_reissue;
      {
         XrdSysCondVarHelper _lck(m_downloadCond);

         BlockList_i bi = blks_to_process.begin();
         while (bi != blks_to_process.end())
         {
            if ((*bi)->is_failed() && (*bi)->get_io() != io)
            {
               TRACEF(Info, "File::Read() requested block " << (void*)(*bi) << " failed with another io " <<
                      (*bi)->get_io() << " - reissuing request with my io " << io);

               (*bi)->reset_error_and_set_io(io);
               to_reissue.push_back(*bi);
               ++bi;
            }
            else if ((*bi)->is_finished())
            {
               TRACEF(Dump, "File::Read() requested block finished " << (void*)(*bi) << ", is_failed()=" << (*bi)->is_failed());
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
            m_downloadCond.Wait();
            continue;
         }
      }

      ProcessBlockRequests(to_reissue, false);
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

            TRACEF(Dump, "File::Read() ub=" << (void*)iUserBuff  << " from finished block " << (*bi)->m_offset/BS << " size " << size_to_copy);
            memcpy(&iUserBuff[user_off], &((*bi)->m_buff[off_in_block]), size_to_copy);
            bytes_read += size_to_copy;
            loc_stats.m_BytesRam += size_to_copy;
            if ((*bi)->m_prefetch)
               prefetchHitsRam++;
         }
         else
         {
            // It has failed ... report only the first error.
            if ( ! error_cond)
            {
               error_cond = (*bi)->m_errno;
               TRACEF(Error, "File::Read() io " << io << ", block "<< (*bi)->m_offset/BS <<
                      " finished with error " << -error_cond << " " << strerror(-error_cond));
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
         // Set error and report only if this is the first error in this read.
         if ( ! error_cond)
         {
            error_cond = direct_handler->m_errno;
            TRACEF(Error, "File::Read(), direct read finished with error " << -error_cond << " " << strerror(-error_cond));
         }
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
         if (m_cfi.TestBitPrefetch(offsetIdx(*d)))
            m_prefetchHitCnt++;
      }
      m_prefetchScore = float(m_prefetchHitCnt)/m_prefetchReadCnt;
   }

   m_stats.AddStats(loc_stats);

   return error_cond ? error_cond : bytes_read;
}

//------------------------------------------------------------------------------

void File::WriteBlockToDisk(Block* b)
{
   // write block buffer into disk file
   long long   offset = b->m_offset - m_offset;
   long long   size   = (offset + m_cfi.GetBufferSize()) > m_fileSize ? (m_fileSize - offset) : m_cfi.GetBufferSize();
   const char *buff   = &b->m_buff[0];

   ssize_t retval = m_output->Write(buff, offset, size);

   if (retval < size)
   {
      if (retval < 0)
      {
         GetLog()->Emsg("File::WriteToDisk()", -retval, "write block to disk", GetLocalPath().c_str());
      }
      else
      {
         TRACEF(Error, "File::WriteToDisk() incomplete block write ret=" << retval << " (should be " << size << ")");
      }

      XrdSysCondVarHelper _lck(m_downloadCond);

      dec_ref_count(b);

      return;
   }

   const int blk_idx =  (b->m_offset - m_offset) / m_cfi.GetBufferSize();

   // Set written bit.
   TRACEF(Dump, "File::WriteToDisk() success set bit for block " <<  b->m_offset << " size=" <<  size);

   bool schedule_sync = false;
   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      m_cfi.SetBitWritten(blk_idx);

      if (b->m_prefetch)
         m_cfi.SetBitPrefetch(blk_idx);

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
   TRACEF(Dump, "File::Sync()");

   int ret     = m_output->Fsync();
   bool errorp = false;
   if (ret == XrdOssOK)
   {
      Stats loc_stats = m_stats.Clone();
      m_cfi.WriteIOStat(loc_stats);
      m_cfi.Write(m_infoFile);
      int cret = m_infoFile->Fsync();
      if (cret != XrdOssOK)
      {
         TRACEF(Error, "File::Sync cinfo file sync error " << cret);
         errorp = true;
      }
   }
   else
   {
      TRACEF(Error, "File::Sync data file sync error " << ret << ", cinfo file has not been updated");
      errorp = true;
   }

   if (errorp)
   {
      TRACEF(Error, "File::Sync failed, unlinking local files and initiating shutdown of File object");

      // Unlink will also call this->initiate_emergency_shutdown()
      Cache::GetInstance().Unlink(m_filename.c_str());

      XrdSysCondVarHelper _lck(&m_downloadCond);

      m_writes_during_sync.clear();
      m_in_sync = false;

      return;
   }

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
   TRACEF(Dump, "File::Sync "<< written_while_in_sync  << " blocks written during sync");
}

//------------------------------------------------------------------------------

void File::inc_ref_count(Block* b)
{
   // Method always called under lock.
   b->m_refcnt++;
   TRACEF(Dump, "File::inc_ref_count " << b << " refcnt  " << b->m_refcnt);
}

//------------------------------------------------------------------------------

void File::dec_ref_count(Block* b)
{
   // Method always called under lock.
   b->m_refcnt--;
   assert(b->m_refcnt >= 0);

   // File::Read() can decrease ref count before waiting for the block in case
   // of an error. Prefetch starts with refcnt 0.
   if (b->m_refcnt == 0 && b->is_finished())
   {
      free_block(b);
   }
}

void File::free_block(Block* b)
{
   // Method always called under lock.
   int i = b->m_offset / BufferSize();
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

   if (m_prefetchState == kHold && (int) m_block_map.size() < Cache::GetInstance().RefConfiguration().m_prefetch_max_blocks)
   {
      m_prefetchState = kOn;
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
      m_prefetchState = kStopped;
      cache()->DeRegisterPrefetchFile(this);
   }

   return io_ok;
}

//------------------------------------------------------------------------------

void File::ProcessBlockResponse(BlockResponseHandler* brh, int res)
{
   XrdSysCondVarHelper _lck(m_downloadCond);

   Block *b = brh->m_block;

   TRACEF(Dump, "File::ProcessBlockResponse " << (void*)b << "  " << b->m_offset/BufferSize());

   // Deregister block from IO's prefetch count, if needed.
   if (brh->m_for_prefetch)
   {
      IoMap_i mi = m_io_map.find(b->get_io());
      if (mi != m_io_map.end())
      {
         --mi->second.m_active_prefetches;

         // If failed and IO is still prefetching -- disable prefetching on this IO.
         if (res < 0 && mi->second.m_allow_prefetching)
         {
            TRACEF(Debug, "File::ProcessBlockResponse after failed prefetch on io " << b->get_io() << " disabling prefetching on this io.");
            mi->second.m_allow_prefetching = false;

            // Check if any IO is still available for prfetching. If not, stop it.
            if (m_prefetchState == kOn || m_prefetchState == kHold)
            {
               if ( ! select_current_io_or_disable_prefetching(false) )
               {
                  TRACEF(Debug, "ProcessBlockResponse stopping prefetching after io " <<  b->get_io() << " marked as bad.");
               }
            }
         }

         // If failed with no subscribers -- remove the block now.
         if (res < 0 && b->m_refcnt == 0)
         {
            free_block(b);
         }
      }
      else
      {
         TRACEF(Error, "File::ProcessBlockResponse io " << b->get_io() << " not found in IoMap.");
      }
   }

   if (res >= 0)
   {
      b->set_downloaded();
      // Increase ref-count for the writer.
      TRACEF(Dump, "File::ProcessBlockResponse inc_ref_count " <<  (int)(b->m_offset/BufferSize()));
      if ( ! m_in_shutdown)
      {
         inc_ref_count(b);
         cache()->AddWriteTask(b, true);
      }
   }
   else
   {
      TRACEF(Error, "File::ProcessBlockResponse block " << b << "  " << (int)(b->m_offset/BufferSize()) << " error=" << res);

      b->set_error(res);
   }

   m_downloadCond.Broadcast();
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

   TRACEF(Dump, "File::Prefetch enter to check download status");
   {
      XrdSysCondVarHelper _lck(m_downloadCond);

      if (m_prefetchState != kOn)
      {
         return;
      }

      if ( ! select_current_io_or_disable_prefetching(true) )
      {
         TRACEF(Error, "File::Prefetch no available IO object found, prefetching stopped. This should not happen, i.e., prefetching should be stopped before.");
         return;
      }

      // Select block(s) to fetch.
      for (int f = 0; f < m_cfi.GetSizeInBits(); ++f)
      {
         if ( ! m_cfi.TestBitWritten(f))
         {
            int f_act = f + m_offset / m_cfi.GetBufferSize();

            BlockMap_i bi = m_block_map.find(f_act);
            if (bi == m_block_map.end())
            {
               TRACEF(Dump, "File::Prefetch take block " << f_act);
               cache()->RequestRAMBlock();
               blks.push_back( PrepareBlockRequest(f_act, m_current_io->first, true) );
               m_prefetchReadCnt++;
               m_prefetchScore = float(m_prefetchHitCnt)/m_prefetchReadCnt;
               break;
            }
         }
      }

      if (blks.empty())
      {
         TRACEF(Debug, "File::Prefetch file is complete, stopping prefetch.");
         m_prefetchState = kComplete;
         cache()->DeRegisterPrefetchFile(this);
      }
      else
      {
         m_current_io->second.m_active_prefetches += (int) blks.size();
      }
   }

   if ( ! blks.empty())
   {
      ProcessBlockRequests(blks, true);
   }
}


//------------------------------------------------------------------------------

float File::GetPrefetchScore() const
{
   return m_prefetchScore;
}

XrdSysError* File::GetLog()
{
   return Cache::GetInstance().GetLog();
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
