/*
 * XrdEcReader.hh
 *
 *  Created on: 18 Dec 2020
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECREADER_HH_
#define SRC_XRDEC_XRDECREADER_HH_

#include "XrdEc/XrdEcObjCfg.hh"

#include "XrdCl/XrdClFileOperations.hh"

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipUtils.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include <string>
#include <tuple>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <list>

namespace XrdEc
{

  class Reader
  {
    //----------------------------------------------------------------------------
    //! OpenOnly operation (@see ZipOperation) - a private ZIP operation
    //----------------------------------------------------------------------------
    template<bool HasHndl>
    class OpenOnlyImpl: public XrdCl::ZipOperation<OpenOnlyImpl, HasHndl,
        XrdCl::Resp<void>, XrdCl::Arg<std::string>>
    {
      public:

        //------------------------------------------------------------------------
        //! Inherit constructors from FileOperation (@see FileOperation)
        //------------------------------------------------------------------------
        using XrdCl::ZipOperation<OpenOnlyImpl, HasHndl, XrdCl::Resp<void>,
            XrdCl::Arg<std::string>>::ZipOperation;

        //------------------------------------------------------------------------
        //! Argument indexes in the args tuple
        //------------------------------------------------------------------------
        enum { UrlArg };

        //------------------------------------------------------------------------
        //! @return : name of the operation (@see Operation)
        //------------------------------------------------------------------------
        std::string ToString()
        {
          return "OpenOnly";
        }

      protected:

        //------------------------------------------------------------------------
        //! RunImpl operation (@see Operation)
        //!
        //! @param params :  container with parameters forwarded from
        //!                  previous operation
        //! @return       :  status of the operation
        //------------------------------------------------------------------------
        XrdCl::XRootDStatus RunImpl( XrdCl::PipelineHandler *handler,
                                     uint16_t                pipelineTimeout )
        {
          std::string      url     = std::get<UrlArg>( this->args ).Get();
          uint16_t         timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
          return this->zip->OpenOnly( url, handler, timeout );
        }
    };

    //----------------------------------------------------------------------------
    //! Factory for creating OpenArchiveImpl objects
    //----------------------------------------------------------------------------
    inline OpenOnlyImpl<false> OpenOnly( XrdCl::Ctx<XrdCl::ZipArchive> zip,
                                         XrdCl::Arg<std::string>       fn,
                                         uint16_t                      timeout = 0 )
    {
      return OpenOnlyImpl<false>( std::move( zip ), std::move( fn ) ).Timeout( timeout );
    }

    //-------------------------------------------------------------------------
    // Buffer for a single chunk of data
    //-------------------------------------------------------------------------
    typedef std::vector<char> buffer_t;
    //-------------------------------------------------------------------------
    // Read callback, to be called with status and number of bytes read
    //-------------------------------------------------------------------------
    typedef std::function<void( const XrdCl::XRootDStatus&, uint32_t )> callback_t;

    //-------------------------------------------------------------------------
    // A single data block
    //-------------------------------------------------------------------------
    struct block_t
    {
      //-----------------------------------------------------------------------
      // Stripe state: empty / loading / valid
      //-----------------------------------------------------------------------
      enum state_t { Empty = 0, Loading, Valid };

      //-----------------------------------------------------------------------
      // Constructor
      //-----------------------------------------------------------------------
      block_t( size_t blkid, Reader &reader, ObjCfg &objcfg ) : reader( reader ),
                                                                objcfg( objcfg ),
                                                                stripes( objcfg.nbchunks ),
                                                                state( objcfg.nbchunks, Empty ),
                                                                pending( objcfg.nbchunks ),
                                                                blkid( blkid )
      {
      }

      //-----------------------------------------------------------------------
      // Read data from stripe
      //
      // @param strpid   : stripe ID
      // @param offset   : relative offset within the stripe
      // @param size     : number of bytes to be read from the stripe
      // @param usrbuff  : user buffer for the data
      // @param callback : use callback to be notified when the read operation
      //                   has been resolved
      //-----------------------------------------------------------------------
      void read( size_t      strpid,
                 uint64_t    offset,
                 uint32_t    size,
                 char       *usrbuff,
                 callback_t  callback )
      {
        std::unique_lock<std::mutex> lck( mtx );

        //---------------------------------------------------------------------
        // The cache is empty, we need to load the data
        //---------------------------------------------------------------------
        if( state[strpid] == Empty )
        {
          //-------------------------------------------------------------------
          // Prepare the read callback
          //-------------------------------------------------------------------
          auto cb = [=]( const XrdCl::XRootDStatus &st, uint32_t )
            {
              std::unique_lock<std::mutex> lck( mtx );
              pending_t &p = pending[strpid];
              auto itr = p.begin();
              //---------------------------------------------------------------
              // Iterate over all pending read operations for given stripe
              //---------------------------------------------------------------
              for( ; itr != p.end() ; ++itr )
              {
                auto       &args     = *itr;
                callback_t &callback = std::get<3>( args );
                uint32_t    nbrd  = 0; // number of bytes read
                //-------------------------------------------------------------
                // If the read was successful, copy the data to user buffer
                //-------------------------------------------------------------
                if( st.IsOK() )
                {
                  uint64_t  offset  = std::get<0>( args );
                  uint32_t  size    = std::get<1>( args );
                  char     *usrbuff = std::get<2>( args );
                  if( offset + size > stripes[strpid].size() )
                    size = stripes[strpid].size() - offset;
                  memcpy( usrbuff, stripes[strpid].data() + offset, size );
                  nbrd = size;
                }
                //-------------------------------------------------------------
                // Call the user callback
                //-------------------------------------------------------------
                callback( st, nbrd );
              }
            };

          reader.Read( blkid, strpid, stripes[strpid], cb );
          state[strpid] = Loading;
        }
        //---------------------------------------------------------------------
        // The cache is loading, we don't have the data yet
        //---------------------------------------------------------------------
        if( state[strpid] == Loading )
        {
          pending[strpid].emplace_back( offset, size, usrbuff, callback );
          return;
        }
        //---------------------------------------------------------------------
        // We do have the data so we can serve the user right away
        //---------------------------------------------------------------------
        if( state[strpid] == Valid )
        {
          if( offset + size > stripes[strpid].size() )
            size = stripes[strpid].size() - offset;
          memcpy( usrbuff, stripes[strpid].data() + offset, size );
          callback( XrdCl::XRootDStatus(), size );
          return;
        }
        //---------------------------------------------------------------------
        // In principle we should never end up here, nevertheless if this
        // happens it is clearly an error ...
        //---------------------------------------------------------------------
        callback( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidOp ), 0 );
      }

      typedef std::tuple<uint64_t, uint32_t, char*, callback_t> args_t;
      typedef std::vector<args_t> pending_t;

      Reader                 &reader;
      ObjCfg                 &objcfg;
      std::vector<buffer_t>   stripes; //< data buffer for every stripe
      std::vector<state_t>    state;   //< state of every data buffer (empty/loading/valid)
      std::vector<pending_t>  pending; //< pending reads per stripe
      size_t                  blkid;   //< block ID
      std::mutex              mtx;
    };

    public:
      Reader( ObjCfg &objcfg ) : objcfg( objcfg )
      {
      }

      virtual ~Reader()
      {
      }

      void Open( XrdCl::ResponseHandler *handler )
      {
        const size_t size = objcfg.plgr.size();
        std::vector<XrdCl::Pipeline> opens; opens.reserve( size );
        for( size_t i = 0; i < size; ++i )
        {
          // generate the URL
          std::string url = objcfg.plgr[i] + objcfg.obj + ".zip";
          // create the file object
          dataarchs.emplace( url, std::make_shared<XrdCl::ZipArchive>() );
          // open the archive
          opens.emplace_back( OpenOnly( *dataarchs[url], url ) );
        }
        // in parallel open the data files and read the metadata
        XrdCl::Pipeline p = XrdCl::Parallel( ReadMetadata( 0 ), XrdCl::Parallel( opens ) ) >>
                              [=]( XrdCl::XRootDStatus &st )
                              { // set the central directories in ZIP archives
                                auto itr = dataarchs.begin();
                                for( ; itr != dataarchs.end() ; ++itr )
                                {
                                  const std::string &url    = itr->first;
                                  auto              &zipptr = itr->second;
                                  if( zipptr->openstage == XrdCl::ZipArchive::NotParsed )
                                    zipptr->SetCD( metadata[url] );
                                  auto itr = zipptr->cdmap.begin();
                                  for( ; itr != zipptr->cdmap.end() ; ++itr )
                                    urlmap.emplace( itr->first, url );
                                }
                                metadata.clear();
                                // call user handler
                                if( handler )
                                  handler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                              };
        XrdCl::Async( std::move( p ) );
      }

      void Read( uint64_t offset, uint32_t length, void *buffer, XrdCl::ResponseHandler *handler )
      {
        char *usrbuff = reinterpret_cast<char*>( buffer );
        typedef std::tuple<uint64_t, uint32_t, void*, uint32_t, XrdCl::ResponseHandler*, std::mutex> rdctx_t;
        auto rdctx = std::make_shared<rdctx_t>( offset, 0, buffer, length, handler );

        while( length > 0 )
        {
          size_t   blkid  = offset / objcfg.datasize;                                     //< ID of the block from which we will be reading
          size_t   strpid = ( offset % objcfg.datasize ) / objcfg.chunksize;              //< ID of the stripe from which we will be reading
          uint64_t rdoff  = offset - blkid * objcfg.datasize - strpid * objcfg.chunksize; //< relative read offset within the stripe
          uint32_t rdsize = objcfg.chunksize - rdoff;                                     //< read size within the stripe
          if( rdsize > length ) rdsize = length;
          //-------------------------------------------------------------------
          // Make sure we operate on a valid block
          //-------------------------------------------------------------------
          if( !block || block->blkid != blkid )
            block = std::make_shared<block_t>( blkid, *this, objcfg );
          //-------------------------------------------------------------------
          // Prepare the callback for reading from single stripe
          //-------------------------------------------------------------------
          auto blk = block;
          auto callback = [blk, rdctx, rdsize]( const XrdCl::XRootDStatus &st, uint32_t nbrd )
          {
            std::unique_lock<std::mutex> lck( std::get<5>( *rdctx ) );
            //-----------------------------------------------------------------
            // Handle failure ...
            //-----------------------------------------------------------------
            if( !st.IsOK() )
            {
              // TODO first check if we can recover using EC
              XrdCl::ResponseHandler *h = std::get<4>( *rdctx );
              if( h ) h->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
              return;
            }
            //-----------------------------------------------------------------
            // Handle success ...
            //-----------------------------------------------------------------
            std::get<1>( *rdctx ) += nbrd;   // number of bytes read
            std::get<3>( *rdctx ) -= rdsize; // number of bytes requested
            if( std::get<3>( *rdctx ) == 0 )
            {
              XrdCl::ResponseHandler *handler = std::get<4>( *rdctx );
              if( handler )
              {
                XrdCl::ChunkInfo *ch = new XrdCl::ChunkInfo( std::get<0>( *rdctx ),
                                                             std::get<1>( *rdctx ),
                                                             std::get<2>( *rdctx ) );
                XrdCl::AnyObject *rsp = new XrdCl::AnyObject();
                rsp->Set( ch );
                handler->HandleResponse( new XrdCl::XRootDStatus(), rsp );
              }
            }
          };
          //-------------------------------------------------------------------
          // Read data from a stripe
          //-------------------------------------------------------------------
          block->read( strpid, rdoff, rdsize, usrbuff, callback );
          //-------------------------------------------------------------------
          // Update absolute offset, read length, and user buffer
          //-------------------------------------------------------------------
          offset  += rdsize;
          length  -= rdsize;
          usrbuff += rdsize;
        }
      }

    private:

      void Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb )
      {
        // generate the file name (blknb/strpnb)
        std::string fn = objcfg.obj + '.' + std::to_string( blknb ) + '.' + std::to_string( strpnb );
        // if the block/stripe does not exist it means we are reading passed the end of the file
        auto itr = urlmap.find( fn );
        if( itr == urlmap.end() ) return cb( XrdCl::XRootDStatus(), 0 );
        // get the URL of the ZIP archive with the respective data
        const std::string &url = itr->second;
        // get the ZipArchive object
        auto &zipptr = dataarchs[url];
        // check the size of the data to be read
        XrdCl::StatInfo *info = nullptr;
        auto st = zipptr->Stat( fn, info );
        if( !st.IsOK() ) return cb( st, 0 );
        uint32_t rdsize = info->GetSize();
        delete info;
        // create a buffer for the data
        buffer.resize( rdsize );
        // issue the read request
        XrdCl::Async( XrdCl::ReadFrom( *zipptr, fn, 0, buffer.size(), buffer.data() ) >>
                      [cb]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch ) { /*TODO verify integrity!!!*/ cb( st, ch.length ); } );
      }

      XrdCl::Pipeline ReadMetadata( size_t index )
      {
        const size_t size = objcfg.plgr.size();
        // create the File object
        auto file = std::make_shared<XrdCl::File>();
        // prepare the URL for Open operation
        std::string url = objcfg.plgr[index] + objcfg.obj + ".metadata.zip";
        // arguments for the Read operation
        XrdCl::Fwd<uint32_t> rdsize;
        XrdCl::Fwd<void*>    rdbuff;

        return XrdCl::Open( *file, url, XrdCl::OpenFlags::Read ) >>
                 [=]( XrdCl::XRootDStatus &st, XrdCl::StatInfo &info )
                 {
                   if( !st.IsOK() )
                   {
                     if( index + 1 < size )
                       XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                     return;
                   }
                   // prepare the args for the subsequent operation
                   rdsize = info.GetSize();
                   rdbuff = new char[info.GetSize()];
                 }
             | XrdCl::Read( *file, 0, rdsize, rdbuff ) >>
                 [=]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch )
                 {
                   if( !st.IsOK() )
                   {
                     if( index + 1 < size )
                       XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                     return;
                   }
                   // now parse the metadata
                   if( !ParseMetadata( ch ) )
                   {
                     if( index + 1 < size )
                       XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                     return;
                   }
                 }
             | XrdCl::Final(
                 [=]( const XrdCl::XRootDStatus& )
                 {
                   // deallocate the buffer if necessary
                   if( rdbuff.Valid() )
                   {
                     char* buffer = reinterpret_cast<char*>( *rdbuff );
                     delete[] buffer;
                   }
                   // close the file if necessary (we don't really care about the result)
                   if( file->IsOpen() )
                     XrdCl::Async( XrdCl::Close( *file ) >> [file]( XrdCl::XRootDStatus& ){ } );
                 } );
      }

      bool ParseMetadata( XrdCl::ChunkInfo &ch )
      {
        const size_t mincnt = objcfg.nbdata + objcfg.nbparity;
        const size_t maxcnt = objcfg.plgr.size();

        char   *buffer = reinterpret_cast<char*>( ch.buffer );
        size_t  length = ch.length;

        for( size_t i = 0; i < maxcnt; ++i )
        {
          uint32_t signature = XrdZip::to<uint32_t>( buffer );
          if( signature != XrdZip::LFH::lfhSign )
          {
            if( i + 1 < mincnt ) return false;
            break;
          }
          XrdZip::LFH lfh( buffer );
          // check if we are not reading passed the end of the buffer
          if( lfh.lfhSize + lfh.uncompressedSize > length ) return false;
          buffer += lfh.lfhSize;
          length -= lfh.lfhSize;
          // verify the checksum
          uint32_t crc32val = crc32c( 0, buffer, lfh.uncompressedSize );
          if( crc32val != lfh.ZCRC32 ) return false;
          // keep the metadata
          metadata.emplace( lfh.filename, buffer_t( buffer, buffer + lfh.uncompressedSize ) );
          buffer += lfh.uncompressedSize;
          length -= lfh.uncompressedSize;
        }

        return true;
      }

      typedef std::unordered_map<std::string, std::shared_ptr<XrdCl::ZipArchive>> dataarchs_t;
      typedef std::unordered_map<std::string, buffer_t> metadata_t;
      typedef std::unordered_map<std::string, std::string> urlmap_t;

      ObjCfg                   &objcfg;
      dataarchs_t               dataarchs; //> map URL to ZipArchive object
      metadata_t                metadata;  //> map URL to CD metadata
      urlmap_t                  urlmap;    //> map blknb/strpnb (data chunk) to URL
      std::shared_ptr<block_t>  block;     //>
  };

} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECREADER_HH_ */
