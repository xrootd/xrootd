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
#include <numeric>

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
      typedef std::tuple<uint64_t, uint32_t, char*, callback_t> args_t;
      typedef std::vector<args_t> pending_t;

      //-----------------------------------------------------------------------
      // Stripe state: empty / loading / valid
      //-----------------------------------------------------------------------
      enum state_t { Empty = 0, Loading, Valid, Missing, Recovering };

      //-----------------------------------------------------------------------
      // Constructor
      //-----------------------------------------------------------------------
      block_t( size_t blkid, Reader &reader, ObjCfg &objcfg ) : reader( reader ),
                                                                objcfg( objcfg ),
                                                                stripes( objcfg.nbchunks ),
                                                                state( objcfg.nbchunks, Empty ),
                                                                pending( objcfg.nbchunks ),
                                                                blkid( blkid ),
                                                                recovering( 0 )
      {
      }

      //-----------------------------------------------------------------------
      // Read data from stripe
      //
      // @param strpid   : stripe ID
      // @param offset   : relative offset within the stripe
      // @param size     : number of bytes to be read from the stripe
      // @param usrbuff  : user buffer for the data
      // @param usrcb    : user callback to be notified when the read operation
      //                   has been resolved
      //-----------------------------------------------------------------------
      void read( size_t      strpid,
                 uint64_t    offset,
                 uint32_t    size,
                 char       *usrbuff,
                 callback_t  usrcb )
      {
        std::unique_lock<std::mutex> lck( mtx );

        //---------------------------------------------------------------------
        // The cache is empty, we need to load the data
        //---------------------------------------------------------------------
        if( state[strpid] == Empty )
        {
          reader.Read( blkid, strpid, stripes[strpid], read_callback( strpid ) );
          state[strpid] = Loading;
        }
        //---------------------------------------------------------------------
        // The stripe is either corrupted or unreachable
        //---------------------------------------------------------------------
        if( state[strpid] == Missing )
        {
          if( !error_correction() )
          {
            //-----------------------------------------------------------------
            // Recovery was not possible, notify the user of the error
            //-----------------------------------------------------------------
            usrcb( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError ), 0 );
            return;
          }
          //-------------------------------------------------------------------
          // we fall through to the following if-statements that will handle
          // Recovering / Valid state
          //-------------------------------------------------------------------
        }
        //---------------------------------------------------------------------
        // The cache is loading or recovering, we don't have the data yet
        //---------------------------------------------------------------------
        if( state[strpid] == Loading || state[strpid] == Recovering )
        {
          pending[strpid].emplace_back( offset, size, usrbuff, usrcb );
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
          usrcb( XrdCl::XRootDStatus(), size );
          return;
        }
        //---------------------------------------------------------------------
        // In principle we should never end up here, nevertheless if this
        // happens it is clearly an error ...
        //---------------------------------------------------------------------
        usrcb( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidOp ), 0 );
      }

      //-----------------------------------------------------------------------
      //
      //-----------------------------------------------------------------------
      bool error_correction()
      {
        //---------------------------------------------------------------------
        // Check if we can do the recovery at all (if too many stripes are
        // missing it wont be possible)
        //---------------------------------------------------------------------
        size_t missingcnt = std::accumulate( state.begin(), state.end(), 0,
                                             []( size_t cnt, state_t st )
                                             {
                                               if( st == Missing ) return cnt + 1;
                                               return cnt;
                                             } );
        if( missingcnt > objcfg.nbparity ) return false;
        //---------------------------------------------------------------------
        // If there are no missing stripes all is good ...
        //---------------------------------------------------------------------
        if( missingcnt == 0 ) return true;

        //---------------------------------------------------------------------
        // Check if we can do the recovery right away
        //---------------------------------------------------------------------
        size_t validcnt = std::accumulate( state.begin(), state.end(), 0,
                                           []( size_t cnt, state_t st )
                                           {
                                             if( st == Valid ) return cnt + 1;
                                             return cnt;
                                           } );
        if( validcnt >= objcfg.nbdata )
        {
          Config &cfg = Config::Instance();
          stripes_t strps( get_stripes() );
          try
          {
            cfg.GetRedundancy( objcfg ).compute( strps );
          }
          catch( const IOError &ex )
          {
            return false;
          }
          //-------------------------------------------------------------------
          // Now when we recovered the data we need to mark every stripe as
          // valid.
          //-------------------------------------------------------------------
          std::for_each( state.begin(), state.end(), []( state_t &s ){ s = Valid; } );
          return true;
        }

        //---------------------------------------------------------------------
        // Try loading the data and only then attempt recovery
        //---------------------------------------------------------------------
        size_t loadingcnt = std::accumulate( state.begin(), state.end(), 0,
                                             []( size_t cnt, state_t st )
                                             {
                                               if( st == Loading ) return cnt + 1;
                                               return cnt;
                                             } );
        size_t i = 0;
        while( loadingcnt + validcnt < objcfg.nbdata )
        {
          size_t strpid = i++;
          if( state[strpid] != Empty ) continue;
          reader.Read( blkid, strpid, stripes[strpid], read_callback( strpid ) );
          state[strpid] = Loading;
          ++loadingcnt;
        }

        //-------------------------------------------------------------------
        // Now that we triggered the recovery procedure mark every missing
        // stripe as recovering.
        //-------------------------------------------------------------------
        std::for_each( state.begin(), state.end(),
                       []( state_t &s ){ if( s == Missing ) s = Recovering; } );
        return true;
      }

      //-----------------------------------------------------------------------
      // Get a callback for read operation
      //-----------------------------------------------------------------------
      inline
      callback_t read_callback( size_t strpid )
      {
        return [=]( const XrdCl::XRootDStatus &st, uint32_t )
               {
                 std::unique_lock<std::mutex> lck( mtx );
                 state[strpid] = st.IsOK() ? Valid : Missing;
                 //------------------------------------------------------------
                 // Check if we need to do any error correction (either for
                 // the current stripe, or any other stripe)
                 //------------------------------------------------------------
                 bool recoverable = error_correction();
                 //------------------------------------------------------------
                 // Carry out the pending read requests if we got the data or
                 // if there was an error and we cannot recover
                 //------------------------------------------------------------
                 if( st.IsOK() || !recoverable )
                   carryout( pending[strpid], stripes[strpid], st );
               };
      }

      //-----------------------------------------------------------------------
      // Get stripes_t data structure used for error recovery
      //-----------------------------------------------------------------------
      inline stripes_t get_stripes()
      {
        stripes_t ret;
        ret.reserve( objcfg.nbchunks );
        for( size_t i = 0; i < objcfg.nbchunks; ++i )
        {
          if( state[i] == Valid )
            ret.emplace_back( stripes[i].data(), true );
          else
          {
            stripes[i].resize( objcfg.chunksize, 0 );
            ret.emplace_back( stripes[i].data(), false );
          }
        }
        return ret;
      }

      //-----------------------------------------------------------------------
      // Execute the pending read requests
      //-----------------------------------------------------------------------
      inline static
      void carryout( pending_t                 &pending,
                     const buffer_t            &stripe,
                     const XrdCl::XRootDStatus &st = XrdCl::XRootDStatus() )
      {
        auto itr = pending.begin();
        //---------------------------------------------------------------
        // Iterate over all pending read operations for given stripe
        //---------------------------------------------------------------
        for( ; itr != pending.end() ; ++itr )
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
            if( offset + size > stripe.size() )
              size = stripe.size() - offset;
            memcpy( usrbuff, stripe.data() + offset, size );
            nbrd = size;
          }
          //-------------------------------------------------------------
          // Call the user callback
          //-------------------------------------------------------------
          callback( st, nbrd );
        }
      }

      Reader                 &reader;
      ObjCfg                 &objcfg;
      std::vector<buffer_t>   stripes;    //< data buffer for every stripe
      std::vector<state_t>    state;      //< state of every data buffer (empty/loading/valid)
      std::vector<pending_t>  pending;    //< pending reads per stripe
      size_t                  blkid;      //< block ID
      bool                    recovering; //< true if we are in the process of recovering data, false otherwise
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
        typedef std::tuple<uint64_t, uint32_t, void*, uint32_t, XrdCl::ResponseHandler*> rdctx_t;
        auto rdctx = std::make_shared<rdctx_t>( offset, 0, buffer, length, handler );
        auto rdmtx = std::make_shared<std::mutex>();

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
          auto callback = [blk, rdctx, rdsize, rdmtx]( const XrdCl::XRootDStatus &st, uint32_t nbrd )
          {
            std::unique_lock<std::mutex> lck( *rdmtx );
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
                        [zipptr, fn, cb]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch )
                        {
                          //---------------------------------------------------
                          // If read failed there's nothing to do, just pass the
                          // status to user callback
                          //---------------------------------------------------
                          if( !st.IsOK() )
                          {
                            cb( st, 0 );
                            return;
                          }
                          //---------------------------------------------------
                          // Get the checksum for the read data
                          //---------------------------------------------------
                          uint32_t orgcksum = 0;
                          auto s = zipptr->GetCRC32( fn, orgcksum );
                          //---------------------------------------------------
                          // If we cannot extract the checksum assume the data
                          // are corrupted
                          //---------------------------------------------------
                          if( !st.IsOK() )
                          {
                            cb( st, 0 );
                            return;
                          }
                          //---------------------------------------------------
                          // Verify data integrity
                          //---------------------------------------------------
                          uint32_t cksum = crc32c( 0, ch.buffer, ch.length );
                          if( orgcksum != cksum )
                          {
                            cb( XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError ), 0 );
                            return;
                          }
                          //---------------------------------------------------
                          // All is good, we can call now the user callback
                          //---------------------------------------------------
                          cb( XrdCl::XRootDStatus(), ch.length );
                        } );
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
