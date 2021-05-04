#include "XrdPfcFile.hh"
#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"

#include "XrdPfcInfo.hh"
#include "XrdPfcStats.hh"
#include "XrdPfcIO.hh"

#include "XrdOss/XrdOss.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdPfc
{
// A list of IOVec chuncks that match a given block index.
// arr vector holds chunk readv indicies.
struct ReadVChunkListDisk
{
   ReadVChunkListDisk(int i) : block_idx(i) {}

   int              block_idx;
   std::vector<int> arr;
};

struct ReadVChunkListRAM
{
   ReadVChunkListRAM(Block* b, std::vector <int>* iarr, bool ireq) : block(b), arr(iarr), req(ireq) {}

   Block            *block;
   std::vector<int> *arr;
   bool              req; // requested here
};

// RAM
struct ReadVBlockListRAM
{
   std::vector<ReadVChunkListRAM> bv;

   bool AddEntry(Block* block, int chunkIdx, bool ireq)
   {
      for (std::vector<ReadVChunkListRAM>::iterator i = bv.begin(); i != bv.end(); ++i)
      {
         if (i->block == block)
         {
            i->arr->push_back(chunkIdx);
            return false;
         }
      }
      bv.push_back(ReadVChunkListRAM(block, new std::vector<int>, ireq));
      bv.back().arr->push_back(chunkIdx);
      return true;
   }
};

// Disk
struct ReadVBlockListDisk
{
   std::vector<ReadVChunkListDisk> bv;

   void AddEntry(int blockIdx, int chunkIdx)
   {
      for (std::vector<ReadVChunkListDisk>::iterator i = bv.begin(); i != bv.end(); ++i)
      {
         if (i->block_idx == blockIdx)
         {
            i->arr.push_back(chunkIdx);
            return;
         }
      }
      bv.push_back(ReadVChunkListDisk(blockIdx));
      bv.back().arr.push_back(chunkIdx);
   }
};
}

using namespace XrdPfc;

//------------------------------------------------------------------------------

int File::ReadV(IO *io, const XrdOucIOVec *readV, int n)
{
   TRACEF(Dump, "ReadV for " << n << " chunks.");

   if ( ! VReadValidate(readV, n))
   {
      return -EINVAL;
   }

   Stats loc_stats;

   int bytes_read = 0;
   int error_cond = 0; // to be set to -errno

   BlockList_t                    blks_to_request;
   ReadVBlockListRAM              blocks_to_process;
   std::vector<ReadVChunkListRAM> blks_processed;
   ReadVBlockListDisk             blocks_on_disk;
   std::vector<XrdOucIOVec>       chunkVec;
   DirectResponseHandler         *direct_handler = 0;

   m_state_cond.Lock();

   if ( ! m_is_open)
   {
      m_state_cond.UnLock();
      TRACEF(Error, "ReadV file is not open");
      return io->GetInput()->ReadV(readV, n);
   }

   if (m_in_shutdown)
   {
      m_state_cond.UnLock();
      return -ENOENT;
   }

   VReadPreProcess(io, readV, n, blks_to_request, blocks_to_process, blocks_on_disk, chunkVec);

   m_state_cond.UnLock();

   // ----------------------------------------------------------------

   // Request blocks that need to be fetched.
   ProcessBlockRequests(blks_to_request);

   // Issue direct / bypass requests if any.
   if ( ! chunkVec.empty())
   {
      direct_handler = new DirectResponseHandler(1);
      io->GetInput()->ReadV(*direct_handler, &chunkVec[0], chunkVec.size());
   }

   // Read data from disk.
   {
      int dr = VReadFromDisk(readV, n, blocks_on_disk);
      if (dr >= 0)
      {
         bytes_read += dr;
         loc_stats.m_BytesHit += dr;
      }
      else error_cond = dr;
   }

   // Fill response buffer with data from blocks in RAM.
   {
      long long b_hit = 0, b_missed = 0;
      int br = VReadProcessBlocks(io, readV, n, blocks_to_process.bv, blks_processed, b_hit, b_missed);
      if (br >= 0)
      {
         bytes_read += br;
         loc_stats.m_BytesHit    += b_hit;
         loc_stats.m_BytesMissed += b_missed;
      }
      else if ( ! error_cond) error_cond = br;
   }

   // Wait for direct requests to arrive.
   if (direct_handler != 0)
   {
      XrdSysCondVarHelper _lck(direct_handler->m_cond);

      while (direct_handler->m_to_wait > 0)
      {
         direct_handler->m_cond.Wait();
      }

      if (direct_handler->m_errno == 0)
      {
         for (std::vector<XrdOucIOVec>::iterator i = chunkVec.begin(); i != chunkVec.end(); ++i)
         {
            bytes_read += i->size;
            loc_stats.m_BytesBypassed += i->size;
         }
      }
      else if ( ! error_cond) error_cond = direct_handler->m_errno;

      delete direct_handler;
   }

   { // Release processed blocks.
      XrdSysCondVarHelper _lck(m_state_cond);

      for (std::vector<ReadVChunkListRAM>::iterator i = blks_processed.begin(); i != blks_processed.end(); ++i)
      {
         dec_ref_count(i->block);
      }
   }
   assert (blocks_to_process.bv.empty());

   for (std::vector<ReadVChunkListRAM>::iterator i = blks_processed.begin(); i != blks_processed.end(); ++i)
      delete i->arr;

   m_stats.AddReadStats(loc_stats);

   TRACEF(Dump, "VRead exit, error_cond=" << error_cond << ", bytes_read=" << bytes_read);
   return error_cond ? error_cond : bytes_read;
}

//------------------------------------------------------------------------------

bool File::VReadValidate(const XrdOucIOVec *vr, int n)
{
   for (int i = 0; i < n; ++i)
   {
      if (vr[i].offset < 0 || vr[i].offset >= m_file_size ||
          vr[i].offset + vr[i].size > m_file_size)
      {
         return false;
      }
   }
   return true;
}

//------------------------------------------------------------------------------

void File::VReadPreProcess(IO *io, const XrdOucIOVec *readV, int n,
                           BlockList_t              &blks_to_request,
                           ReadVBlockListRAM        &blocks_to_process,
                           ReadVBlockListDisk       &blocks_on_disk,
                           std::vector<XrdOucIOVec> &chunkVec)
{
   // Must be called under m_state_cond lock.

   for (int iov_idx = 0; iov_idx < n; iov_idx++)
   {
      const int blck_idx_first =  readV[iov_idx].offset / m_cfi.GetBufferSize();
      const int blck_idx_last  = (readV[iov_idx].offset + readV[iov_idx].size - 1) / m_cfi.GetBufferSize();

      for (int block_idx = blck_idx_first; block_idx <= blck_idx_last; ++block_idx)
      {
         TRACEF(Dump, "VReadPreProcess chunk "<<  readV[iov_idx].size << "@"<< readV[iov_idx].offset);

         BlockMap_i bi = m_block_map.find(block_idx);
         if (bi != m_block_map.end())
         {
            if (blocks_to_process.AddEntry(bi->second, iov_idx, false))
               inc_ref_count(bi->second);

            TRACEF(Dump, "VReadPreProcess block "<< block_idx <<" in map");
         }
         else if (m_cfi.TestBitWritten(offsetIdx(block_idx)))
         {
            blocks_on_disk.AddEntry(block_idx, iov_idx);

            TRACEF(Dump, "VReadPreProcess block "<< block_idx <<" , chunk idx = " << iov_idx << " on disk");
         }
         else
         {
            Block *b = PrepareBlockRequest(block_idx, io, false);

            if (b)
            {
               inc_ref_count(b);
               blocks_to_process.AddEntry(b, iov_idx, true);
               blks_to_request.push_back(b);

               TRACEF(Dump, "VReadPreProcess request block " << block_idx);
            }
            else
            {
               long long off;      // offset in user buffer
               long long blk_off;  // offset in block
               long long size;     // size to copy
               const long long BS = m_cfi.GetBufferSize();
               overlap(block_idx, BS, readV[iov_idx].offset, readV[iov_idx].size, off, blk_off, size);
               chunkVec.push_back(XrdOucIOVec2(readV[iov_idx].data+off, BS*block_idx + blk_off,size));

               TRACEF(Dump, "VReadPreProcess direct read " << block_idx);
            }
         }
      }
   }
}

//------------------------------------------------------------------------------

int File::VReadFromDisk(const XrdOucIOVec *readV, int n, ReadVBlockListDisk& blocks_on_disk)
{
   int bytes_read = 0;
   for (std::vector<ReadVChunkListDisk>::iterator bit = blocks_on_disk.bv.begin(); bit != blocks_on_disk.bv.end(); ++bit )
   {
      int blockIdx = bit->block_idx;
      for (std::vector<int>::iterator chunkIt = bit->arr.begin(); chunkIt != bit->arr.end(); ++chunkIt)
      {
         int chunkIdx = *chunkIt;

         long long off;     // offset in user buffer
         long long blk_off;    // offset in block
         long long size;    // size to copy

         TRACEF(Dump, "VReadFromDisk block= " << blockIdx <<" chunk=" << chunkIdx);

         overlap(blockIdx, m_cfi.GetBufferSize(), readV[chunkIdx].offset, readV[chunkIdx].size, off, blk_off, size);

         int rs = m_data_file->Read(readV[chunkIdx].data + off,  blockIdx*m_cfi.GetBufferSize() + blk_off - m_offset, size);

         if (rs < 0)
         {
            TRACEF(Error, "VReadFromDisk FAILED rs=" << rs << " block=" << blockIdx << " chunk=" << chunkIdx << " off=" << off  <<
                          " blk_off=" <<  blk_off << " size=" << size <<  " chunkOff=" << readV[chunkIdx].offset);
            return rs;
         }

         if (rs != size)
         {
            TRACEF(Error, "VReadFromDisk FAILED incomplete read rs=" << rs << " block=" << blockIdx << " chunk=" << chunkIdx << " off=" << off  <<
                          " blk_off=" <<  blk_off << " size=" << size <<  " chunkOff=" << readV[chunkIdx].offset);
            return -EIO;
         }

         bytes_read += rs;
      }
   }

   return bytes_read;
}

//------------------------------------------------------------------------------

int File::VReadProcessBlocks(IO *io, const XrdOucIOVec *readV, int n,
                             std::vector<ReadVChunkListRAM>& blocks_to_process,
                             std::vector<ReadVChunkListRAM>& blocks_processed,
                             long long& bytes_hit,
                             long long& bytes_missed)
{
   long long bytes_read = 0;
   int       error_cond = 0; // to be set to -errno

   while ( ! blocks_to_process.empty())
   {
      std::vector<ReadVChunkListRAM> finished;
      BlockList_t                    to_reissue;
      {
         XrdSysCondVarHelper _lck(m_state_cond);

         std::vector<ReadVChunkListRAM>::iterator bi = blocks_to_process.begin();
         while (bi != blocks_to_process.end())
         {
            if (bi->block->is_failed() && bi->block->get_io() != io)
            {
               TRACEF(Info, "VReadProcessBlocks() requested block " << bi->block << " failed with another io " <<
                      bi->block->get_io() << " - reissuing request with my io " << io);

               bi->block->reset_error_and_set_io(io);
               to_reissue.push_back(bi->block);
               ++bi;
            }
            else if (bi->block->is_finished())
            {
               finished.push_back(ReadVChunkListRAM(bi->block, bi->arr, bi->req));
               // Here we rely on the fact that std::vector does not reallocate on erase!
               blocks_to_process.erase(bi);
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

      std::vector<ReadVChunkListRAM>::iterator bi = finished.begin();
      while (bi != finished.end())
      {
         if (bi->block->is_ok())
         {
            long long b_read = 0;
            for (std::vector<int>::iterator chunkIt = bi->arr->begin(); chunkIt < bi->arr->end(); ++chunkIt)
            {
               long long off;      // offset in user buffer
               long long blk_off;  // offset in block
               long long size;     // size to copy

               int block_idx = bi->block->m_offset/m_cfi.GetBufferSize();
               overlap(block_idx, m_cfi.GetBufferSize(), readV[*chunkIt].offset, readV[*chunkIt].size, off, blk_off, size);

               memcpy(readV[*chunkIt].data + off,  &(bi->block->m_buff[blk_off]), size);

               b_read += size;
            }

            bytes_read += b_read;
            if (bi->req)
               bytes_missed += b_read;
            else
               bytes_hit    += b_read;
         }
         else
         {
            // It has failed ... report only the first error.
            if ( ! error_cond)
            {
               error_cond = bi->block->m_errno;
               TRACEF(Error, "VReadProcessBlocks() io " << io << ", block "<< bi->block <<
                     " finished with error " << -error_cond << " " << XrdSysE2T(-error_cond));
               break;
            }
         }

         ++bi;
      }

      // add finished to processed list
      std::copy(finished.begin(), finished.end(), std::back_inserter(blocks_processed));
      finished.clear();
   }

   TRACEF(Dump, "VReadProcessBlocks status " << error_cond << ", total read " <<  bytes_read);

   return error_cond ? error_cond : bytes_read;
}
