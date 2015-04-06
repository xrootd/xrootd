#ifndef __XRDFILECACHE_FILE_HH__
#define __XRDFILECACHE_FILE_HH__

#include "XrdCl/XrdClXRootDResponses.hh"

#include <string>
#include <map>


namespace XrdFileCache
{
   class RefCounted
   {
      int m_refcnt;

      RefCounted() : m_refcnt(0) {}
   };

   class File;

   class Block
   {
   public:
      std::vector<char>   m_buff;
      long long           m_offset;
      File               *m_file;
      int                 m_refcnt;
      int                 m_errno;
      bool                m_downloaded;
      bool                m_on_disk;

      Block(File *f, long long off, int size) :
         m_offset(off), m_file(f), m_refcnt(0),
         m_errno(0), m_downloaded(false), m_on_disk(false)
      {
         mbuff.resize(size);
      }

      char* get_buff(long long pos = 0) const { return &m_buff[pos]; }

      bool is_finished() { return m_downloaded || m_errno != 0; }
      bool is_ok()       { return m_downloaded; }
      bool is_failed()   { return m_errno != 0; }

      void set_error_and_free(int err)
      {
         m_errno = err;
         m_buff.resize(0);
      }
   };

   class File
   {
      XrdOucCacheIO  *m_input;          //!< original data source
      XrdOssDF       *m_output;         //!< file handle for data file on disk
      XrdOssDF       *m_infoFile;       //!< file handle for data-info file on disk
      Info            m_cfi;            //!< download status of file blocks and access statistics

      std::string     m_temp_filename;  //!< filename of data file on disk
      long long       m_offset;         //!< offset of cached file for block-based operation
      long long       m_fileSize;       //!< size of cached disk file for block-based operation

      typedef std::list<int>         IntList_t;
      typedef IntList_t::iterator    IntList_i;

      typedef std::list<Block*>      BlockList_t;
      typedef BlockList_t::iterator  BlockList_i;

      typedef std::map<int, Block*>  BlockMap_t;
      typedef BlockMap_t::iterator   BlockMap_i;


      BlockMap_t      m_block_map;

      XrdSysCondVar     m_block_cond;

      int             m_num_reads;

      Stats            m_stats;      //!< cache statistics, used in IO detach

   public:

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      File(XrdOucCacheIO &io, std::string &path,
           long long offset, long long fileSize);

      //------------------------------------------------------------------------
      //! Destructor.
      //------------------------------------------------------------------------
      ~File();

      //! Open file handle for data file and info file on local disk.
      bool Open();

      int Read(char* buff, off_t offset, size_t size);

   private:
      Block* RequestBlock(int i);

      int    RequestBlocksDirect(DirectRH *handler, IntList_t& blocks,
                                long long req_buf, long long req_off, long long req_size);

      int    ReadBlocksFromDisk(IntList_t& blocks,
                                long long req_buf, long long req_off, long long req_size);


      void ProcessBlockResponse(Block* b, XrdCl::XRootDStatus *status);

   };


   // ================================================================

   class BlockResponseHandler : public XrdCl::ResponseHandler
   {
   public:
      Block *m_block;

      BlockResponseHandler(Block *b) : m_block(b) {}

      void HandleResponse(XrdCl::XRootDStatus *status,
                          XrdCl::AnyObject    *response);
   };

   class DirectResponseHandler : public XrdCl::ResponseHandler
   {
   public:
      XrdSysCondVar  m_cond;
      int            m_to_wait;
      int            m_errno;

      DirectResponseHandler(int to_wait) : m_cond(0), m_to_wait(to_wait), m_errno(0) {}

      bool is_finished() { XrdSysCondVarHelper _lck(m_cond); return m_to_wait == 0; }
      bool is_ok()       { XrdSysCondVarHelper _lck(m_cond); return m_to_wait == 0 && m_errno == 0; }
      bool is_failed()   { XrdSysCondVarHelper _lck(m_cond); return m_errno != 0; }

      void HandleResponse(XrdCl::XRootDStatus *status,
                          XrdCl::AnyObject    *response);
   };

}

#endif
