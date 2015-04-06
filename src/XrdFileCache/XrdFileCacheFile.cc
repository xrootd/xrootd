#include "XrdFileCacheFile.hh"

using namespace XrdFileCache;

File::File(XrdOucCacheIO &io, std::string &path, long long off, long long size) :
   m_input(io),
   m_output(0),
   m_infoFile(0),
   m_temp_filename(path),
   m_offset(off),
   m_fileSize(size),

   m_block_cond(0),
   m_num_reads(0)
{
   clLog()->Debug(XrdCl::AppMsg, "File::File() %s", m_input.Path());
}

File::~File()
{
   // see if we have to shut down
   clLog()->Info(XrdCl::AppMsg, "File::~File() %p %s", (void*) this, m_input.Path());

   clLog()->Info(XrdCl::AppMsg, "File::~File close data file %p",(void*)this );

   if (m_output)
   {
      m_output->Close();
      delete m_output;
      m_output = 0;
   }
   if (m_infoFile)
   {
      m_cfi.AppendIOStat(&m_stats, m_infoFile);
      m_cfi.WriteHeader(m_infoFile);

      clLog()->Info(XrdCl::AppMsg, "File::~File close info file");

      m_infoFile->Close();
      delete m_infoFile;
      m_infoFile = 0;
   }
}

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

   if (m_cfi.Read(m_infoFile) <= 0)
   {
      int ss = (m_fileSize - 1)/m_cfi.GetBufferSize() + 1;
      clLog()->Info(XrdCl::AppMsg, "Creating new file info with size %lld. Reserve space for %d blocks %s", m_fileSize,  ss, m_input.Path());
      m_cfi.ResizeBits(ss);
      RecordDownloadInfo();
   }
   else
   {
      clLog()->Debug(XrdCl::AppMsg, "Info file read from disk: %s", m_input.Path());
   }

   return true;
}

//==============================================================================

ssize_t File::ReadInBlocks(char *buff, off_t off, size_t size)
{
   long long off0 = off;
   int idx_first = off0 / m_cfi.GetBufferSize();
   int idx_last  = (off0 + size - 1)/ m_cfi.GetBufferSize();

   size_t bytes_read = 0;
   for (int blockIdx = idx_first; blockIdx <= idx_last; ++blockIdx)
   {
      int readBlockSize = size;
      if (idx_first != idx_last)
      {
         if (blockIdx == idx_first)
         {
            readBlockSize = (blockIdx + 1) * m_cfi.GetBufferSize() - off0;
            clLog()->Dump(XrdCl::AppMsg, "Read partially till the end of the block %", m_input.Path());
         }
         else if (blockIdx == idx_last)
         {
            readBlockSize = (off0 + size) - blockIdx * m_cfi.GetBufferSize();
            clLog()->Dump(XrdCl::AppMsg, "Read partially from beginning of block %s", m_input.Path());
         }
         else
         {
            readBlockSize = m_cfi.GetBufferSize();
         }
      }

      if (readBlockSize > m_cfi.GetBufferSize()) {
         clLog()->Error(XrdCl::AppMsg, "block size invalid");
      }

      int retvalBlock = -1;
      // now do per block read at Read(buff, off, readBlockSize)

      m_downloadStatusMutex.Lock();
      bool dsl = m_cfi.TestBit(blockIdx);
      m_downloadStatusMutex.UnLock(); 

      if (dsl)
      {
         retvalBlock = m_output->Read(buff, off, readBlockSize);
         m_stats.m_BytesDisk += retvalBlock;
         clLog()->Dump(XrdCl::AppMsg, "File::ReadInBlocks [%d] disk = %d",blockIdx, retvalBlock);
      }
      else 
      {
         if (ReadFromTask(blockIdx, buff, off, readBlockSize))
         {
            retvalBlock = readBlockSize; // presume since ReadFromTask did not fail, could pass a refrence to ReadFromTask
            m_stats.m_BytesRam += retvalBlock;
            clLog()->Dump(XrdCl::AppMsg, "File::ReadInBlocks [%d]  ram = %d", blockIdx, retvalBlock);
         }
         else
         {
            retvalBlock = m_input.Read(buff, off, readBlockSize);
            clLog()->Dump(XrdCl::AppMsg, "File::ReadInBlocks [%d]  client = %d", blockIdx, retvalBlock);
            m_stats.m_BytesMissed += retvalBlock;
         }
      }

      if (retvalBlock > 0 )
      {
         bytes_read += retvalBlock;
         buff       += retvalBlock;
         off        += retvalBlock;
         if (readBlockSize != retvalBlock)
         {
            clLog()->Warning(XrdCl::AppMsg, "File::ReadInBlocks incomplete , missing = %d", readBlockSize-retvalBlock);
            return bytes_read;
         }
      }
      else
      {
         return bytes_read;
      }
   }
   return bytes_read;
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
                long_long &off,     // offset in user buffer
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

Block* File::RequestBlock(int i)
{
   // Must be called w/ block_map locked.
   // Checks on size etc should be done before.
   //
   // Reference count is 0 so increase it in calling function if you want to
   // catch the block while still in memory.

   XrdCl::XrdClFile &client = ((XrdPosixFile*)m_input).clFile;

   const long long   BS = m_cfi.GetBufferSize();
   const int last_block = m_cfi.GetSizeInBits() - 1;

   long long off     = i * BS;
   long long this_bs = (i == last_block) ? m_input.FSize() - off : BS;

   Block *b = new Block(this, off, this_bs);
   m_block_map[i] = b;

   client.Read(off, this_bs, b->get_buff(), new BlockRH(b));
}

//------------------------------------------------------------------------------

int File::RequestBlocksDirect(DirectRH *handler, std::list<int>& blocks,
                              long long req_buf, long long req_off, long long req_size)
{
   XrdCl::XrdClFile &client = ((XrdPosixFile*)m_input).clFile;

   const long long BS = m_cfi.GetBufferSize();

   // XXX Use readv to load more at the same time.

   long long total = 0;

   for (IntList_i ii = blocks ; ii != blocks.end(); ++ii)
   {
      // overlap and request
      long_long off;     // offset in user buffer
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
                             long long req_buf, long long req_off, long long req_size)
{
   const long long BS = m_cfi.GetBufferSize();

   long long total = 0;

   // XXX Coalesce adjacent reads.

   for (IntList_i ii = blocks ; ii != blocks.end(); ++ii)
   {
      // overlap and read
      long_long off;     // offset in user buffer
      long long blk_off; // offset in block
      long long size;    // size to copy

      overlap(*ii, BS, req_off, req_size, off, blk_off, size);

      long long rs = m_output.Read(req_buf + off, *ii * BS + blk_off, size);

      if (rs < 0)
         return rs;

      total += rs;
   } 

   return total;
}

//------------------------------------------------------------------------------

int File::Read(char* buff, long long offset, int size);
{
   XrdCl::XrdClFile &client = ((XrdPosixFile*)m_input).clFile;

   const long long BS = m_cfi.GetBufferSize();

   // lock
   // loop over reqired blocks:
   //   - if on disk, ok;
   //   - if in ram or incoming, inc ref-count
   //   - if not available, request and inc ref count
   // before requesting the hell and more (esp. for sparse readv) assess if
   //   passing the req to client is actually better.
   // unlock

   const int MaxBlocksForRead = 16; // Should be config var! Or even mutable for low client counts.

   m_block_cond.Lock();

   // XXX Check for blocks to free? Later ...

   inc_num_readers();

   const int idx_first = off / BS;
   const int idx_last  = (off + size - 1) / BS;

   BlockList_t  blks_to_process, blks_processed;
   IntList_t    blks_on_disk,    blks_direct;

   int bmap_size = m_block_map.size();

   for (int i = idx_first; blockIdx <= idx_last; ++blockIdx)
   {
      BlockMap_i bi = m_block_map.find(i);

      // In RAM or incoming?
      if (bi != m_block_map.end())
      {
         // XXXX if failed before -- retry if timestamp sufficient or fail?
         // XXXX Or just push it and handle errors in one place later?

         (*bi)->inc_ref_cont();
         blks_to_process.push_front(*bi);
      }
      // On disk?
      else if (m_cfi.TestBit(i))
      {
         blks_on_disk.push_back(i);
      }
      // Then we have to get it ...
      else
      {
         // Is there room for one more RAM Block?
         if (size < MaxBlocksForRead)
         {
            Block *b = RequestBlock(i);
            b->inc_ref_cont();
            blks_to_process.push_back(b);

            ++size;
         }
         // Nope ... read this directly without caching.
         else
         {
            blks_direct.push_back(i);
         }
      }
   }

   m_block_cond.UnLock();

   // First, send out any direct requests.
   // XXX Could send them all out in a single vector read.
   DirectRH *direct_handler = 0;
   int       direct_size = 0;

   if ( ! blks_direct.empty())
   {
      direct_handler = new DirectRH(blks_direct.size());

      direct_size = RequestBlocksDirect(direct_handler, blks_direct, *ii, buff, offset, size);
   }

   long long bytes_read = 0;

   // Second, read blocks from disk.
   int rc = ReadBlocksFromDisk(blks_on_disk, buff, offset, size);
   if (rc >= 0)
   {
      bytes_read += rc;
   }
   else
   {
      bytes_read = rc;
   }

   // Third, loop over blocks that are available or incoming
   while ( ! blks_to_process.empty() && bytes_read >= 0)
   {
      BlockList_t finished;

      {
         XrdSysConVarHelper _lck(m_block_cond);

         BlockList_i bi = blks_to_process.begin();
         while (bi != blks_to_process.end())
         {
            if ((*bi)->is_finished())
            {
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
            m_block_cond.Wait();
            continue;
         }
      }

      bi = finished.begin();
      while (bi != finished.end())
      {
         if ((*bi)->is_ok())
         {
            // XXXX memcpy !
         }
         else // it has failed ... krap up.
         {
            bytes_read = -1;
            errno = (*bi)->errno;
            break;
         }
      }

      std::copy(finished.begin(), finished.end(), std::back_inserter(blks_processed));
   }

   // Fourth, make sure all direct requests have arrived
   if (m_direct_handler != 0)
   {
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

      delete m_direct_handler; m_direct_handler = 0;
   }

   // Last, stamp and release blocks, release file.
   {
      XrdSysConVarHelper _lck(m_block_cond);

      // XXXX stamp file

      // blks_to_process can be non-empty, if we're exiting with an error.
      std::copy(blks_to_process.begin(), blks_to_process.end(), std::back_inserter(blks_processed));

      for (BlockList_i bi = blks_processed.begin(); bi != blks_processed.end(); ++bi)
      {
         (*bi)->dec_ref_count();
         // XXXX stamp block
      }

      dec_num_readers();
   }

   return bytes_read;
}

//------------------------------------------------------------------------------

void File::ProcessBlockResponse(Block* b, XrdCl::XRootDStatus *status)
{
   m_block_cond.Lock();

   if (status->IsOK()) 
   {
      b->m_downloaded = true;
      b->inc_ref_count();

      // XXX Add to write queue (if needed)
      // write_queue->QueueBlock(b);
   }
   else
   {
      // how long to keep?
      // when to retry?

      XrdPosixMap::Result(*status);

      b->set_error_and_free(errno);
      errno = 0;

      // ???
      b->inc_ref_count();
   }

   m_block_cond.Broadcast();

   m_block_cond.UnLock();
}



//==============================================================================

//==============================================================================

void BlockResponseHandler::HandleResponse(XrdCl::XRootDStatus *status,
                                          XrdCl::AnyObject    *response)
{
   m_block->m_file->ProcessBlockResponse(m_block, status);

   delete status;
   delete response;

   delete this;
}

//------------------------------------------------------------------------------

void DirectResponseHandler::HandleResponse(XrdCl::XRootDStatus *status,
                                           XrdCl::AnyObject    *response)
{
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
