//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef SRC_XRDEC_XRDECREADER_HH_
#define SRC_XRDEC_XRDECREADER_HH_

#include "XrdEc/XrdEcObjCfg.hh"

#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdEc/XrdEcUtilities.hh"
#include "XrdEc/XrdEcConfig.hh"
#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcThreadPool.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipLFH.hh"

#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClZipOperations.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClFinalOperation.hh"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <tuple>

class MicroTest;

namespace XrdEc
{
  //---------------------------------------------------------------------------
  // Forward declaration for the internal cache
  //---------------------------------------------------------------------------
  struct block_t;
  //---------------------------------------------------------------------------
  // Buffer for a single chunk of data
  //---------------------------------------------------------------------------
  typedef std::vector<char> buffer_t;
  //---------------------------------------------------------------------------
  // Read callback, to be called with status and number of bytes read
  //---------------------------------------------------------------------------
  typedef std::function<void( const XrdCl::XRootDStatus&, uint32_t )> callback_t;

  //---------------------------------------------------------------------------
  // Reader object for reading erasure coded and striped data
  //---------------------------------------------------------------------------
  class Reader
  {
    friend class ::MicroTest;
    friend class XrdEc::RepairTool;
    friend struct block_t;

    public:
      //-----------------------------------------------------------------------
      //! Constructor
      //!
      //! @param objcfg : configuration for the data object (e.g. number of
      //!                 data and parity stripes)
      //-----------------------------------------------------------------------
      Reader( ObjCfg &objcfg ) : objcfg( objcfg ), lstblk( 0 ), filesize( 0 )
      {
      }

      //-----------------------------------------------------------------------
      // Destructor
      //-----------------------------------------------------------------------
      virtual ~Reader();

      //-----------------------------------------------------------------------
      //! Open the erasure coded / striped object
      //!
      //! @param handler : user callback
      //-----------------------------------------------------------------------
      void Open( XrdCl::ResponseHandler *handler, uint16_t timeout = 0 );

      //-----------------------------------------------------------------------
      //! Read data from the data object
      //!
      //! @param offset  : offset of the data to be read
      //! @param length  : length of the data to be read
      //! @param buffer  : buffer for the data to be read
      //! @param handler : user callback
      //-----------------------------------------------------------------------
      void Read( uint64_t                offset,
                 uint32_t                length,
                 void                   *buffer,
                 XrdCl::ResponseHandler *handler,
                 uint16_t                timeout = 0);

      /*
       * Read multiple locations and lengths of data
       * internally remapped to stripes
       *
       * @param chunks	: list of offsets, lengths and separate buffers
       * @param buffer 	: optional full buffer for all data
       */
      void VectorRead( 	const XrdCl::ChunkList 	&chunks,
    		  	  	  	void 					*buffer,
					    XrdCl::ResponseHandler 	*handler,
						uint16_t 				timeout);

      //-----------------------------------------------------------------------
      //! Close the data object
      //-----------------------------------------------------------------------
      void Close( XrdCl::ResponseHandler *handler, uint16_t timeout = 0 );

      //-----------------------------------------------------------------------
      //! @return : get file size
      //-----------------------------------------------------------------------
      inline uint64_t GetSize()
      {
        return filesize;
      }

    private:

      //-----------------------------------------------------------------------
      //! Read data from given stripes from given block
      //!
      //! @param blknb   : number of the block
      //! @param strpnb  : number of stripe in the block
      //! @param buffer  : buffer for the data
      //! @param cb      : callback
      //! @param timeout : operation timeout
      //-----------------------------------------------------------------------
      void Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb, uint16_t timeout = 0 );

      //-----------------------------------------------------------------------
      //! Read metadata for the object
      //!
      //! @param index : placement's index
      //-----------------------------------------------------------------------
      XrdCl::Pipeline ReadMetadata( size_t index );

      //-----------------------------------------------------------------------
      //! Read size from xattr
      //!
      //! @param index : placement's index
      //-----------------------------------------------------------------------
      XrdCl::Pipeline ReadSize( size_t index );

      XrdCl::Pipeline ReadHealth( size_t index );

      XrdCl::Pipeline CheckHealthExists( size_t index );

      //-----------------------------------------------------------------------
      //! Parse metadata from chunk info object
      //!
      //! @param ch : chunk info object returned by a read operation
      //-----------------------------------------------------------------------
      bool ParseMetadata( XrdCl::ChunkInfo &ch );

      //-----------------------------------------------------------------------
      //! Add all the entries from given Central Directory to missing
      //!
      //! @param cdbuff : buffer containing central directory
      //-----------------------------------------------------------------------
      void AddMissing( const buffer_t &cdbuff );

      //-----------------------------------------------------------------------
      //! Check if chunk file name is missing
      //-----------------------------------------------------------------------
      bool IsMissing( const std::string &fn );

      inline static callback_t ErrorCorrected(Reader *reader, std::shared_ptr<block_t> &self, size_t blkid, size_t strpid);

      void MissingVectorRead(std::shared_ptr<block_t> &block, size_t blkid, size_t strpid, uint16_t timeout = 0);

      typedef std::unordered_map<std::string, std::shared_ptr<XrdCl::ZipArchive>> dataarchs_t;
      typedef std::unordered_map<std::string, buffer_t> metadata_t;
      typedef std::unordered_map<std::string, std::string> urlmap_t;
      typedef std::unordered_set<std::string> missing_t;

      ObjCfg                   &objcfg;
      dataarchs_t               dataarchs; //> map URL to ZipArchive object
      metadata_t                metadata;  //> map URL to CD metadata
      urlmap_t                  urlmap;    //> map blknb/strpnb (data chunk) to URL
      missing_t                 missing;   //> set of missing stripes
      std::shared_ptr<block_t>  block;     //> cache for the block we are reading from
      std::mutex                blkmtx;    //> mutex guarding the block from parallel access
      size_t                    lstblk;    //> last block number
      uint64_t                  filesize;  //> file size (obtained from xattr)
      std::map<std::string, size_t>  archiveIndices;

      std::mutex	missingChunksMutex;
      std::vector<std::tuple<size_t, size_t>> missingChunksVectorRead;
      std::condition_variable waitMissing;
  };

  //---------------------------------------------------------------------------
  // OpenOnly operation (@see ZipOperation) - a private ZIP operation
  //---------------------------------------------------------------------------
  template<bool HasHndl>
  class OpenOnlyImpl: public XrdCl::ZipOperation<OpenOnlyImpl, HasHndl,
      XrdCl::Resp<void>, XrdCl::Arg<std::string>, XrdCl::Arg<bool>>

  {
    public:

      //-----------------------------------------------------------------------
      // Inherit constructors from FileOperation (@see FileOperation)
      //-----------------------------------------------------------------------
      using XrdCl::ZipOperation<OpenOnlyImpl, HasHndl, XrdCl::Resp<void>,
          XrdCl::Arg<std::string>, XrdCl::Arg<bool>>::ZipOperation;

      //-----------------------------------------------------------------------
      // Argument indexes in the args tuple
      //-----------------------------------------------------------------------
      enum { UrlArg, UpdtArg };

      //-----------------------------------------------------------------------
      // @return : name of the operation (@see Operation)
      //-----------------------------------------------------------------------
      std::string ToString()
      {
        return "OpenOnly";
      }

    protected:

      //-----------------------------------------------------------------------
      // RunImpl operation (@see Operation)
      //
      // @param params :  container with parameters forwarded from
      //                  previous operation
      // @return       :  status of the operation
      //-----------------------------------------------------------------------
      XrdCl::XRootDStatus RunImpl( XrdCl::PipelineHandler *handler,
                                   uint16_t                pipelineTimeout )
      {
        std::string      url     = std::get<UrlArg>( this->args ).Get();
        bool             updt    = std::get<UpdtArg>( this->args ).Get();
        uint16_t         timeout = pipelineTimeout < this->timeout ?
                                   pipelineTimeout : this->timeout;
        return this->zip->OpenOnly( url, updt, handler, timeout );
      }
  };

  //---------------------------------------------------------------------------
  // Factory for creating OpenArchiveImpl objects
  //---------------------------------------------------------------------------
  inline OpenOnlyImpl<false> OpenOnly( XrdCl::Ctx<XrdCl::ZipArchive> zip,
                                       XrdCl::Arg<std::string>       fn,
                                       XrdCl::Arg<bool>              updt,
                                       uint16_t                      timeout = 0 )
  {
    return OpenOnlyImpl<false>( std::move( zip ), std::move( fn ),
                                std::move( updt ) ).Timeout( timeout );
  }

  struct block_t {
	  friend class XrdEc::Reader;
  	typedef std::tuple<uint64_t, uint32_t, char*, callback_t> args_t;
  	typedef std::vector<args_t> pending_t;
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
  	                                                              recovering( 0 ),
																  redirectionIndex( 0)
  	    {
  	    }
  	    //-----------------------------------------------------------------------
  	    // Read data from stripe
  	    //
  	    // @param self     : the block_t object
  	    // @param strpid   : stripe ID
  	    // @param offset   : relative offset within the stripe
  	    // @param size     : number of bytes to be read from the stripe
  	    // @param usrbuff  : user buffer for the data
  	    // @param usrcb    : user callback to be notified when the read operation
  	    //                   has been resolved
  	    // @param timeout  : operation timeout
  	    //-----------------------------------------------------------------------
  	    static void read( std::shared_ptr<block_t> &self,
  	                      size_t                    strpid,
  	                      uint64_t                  offset,
  	                      uint32_t                  size,
  	                      char                     *usrbuff,
  	                      callback_t                usrcb,
  	                      uint16_t                  timeout)
  	    {
  	    	std::unique_lock<std::mutex> lck( self->mtx );

  	      //---------------------------------------------------------------------
  	      // The cache is empty, we need to load the data
  	      //---------------------------------------------------------------------
  	      if( self->state[strpid] == Empty )
  	      {
  	    	  self->reader.Read( self->blkid, strpid, self->stripes[strpid],
  	                           read_callback( self, strpid ), timeout );
  	        self->state[strpid] = Loading;
  	      }
  	      //---------------------------------------------------------------------
  	      // The stripe is either corrupted or unreachable
  	      //---------------------------------------------------------------------
  	      if( self->state[strpid] == Missing )
  	      {
  	        if(!error_correction( self ) )
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
  	      if( self->state[strpid] == Loading || self->state[strpid] == Recovering )
  	      {
  	    	  self->pending[strpid].emplace_back( offset, size, usrbuff, usrcb );
  	        return;
  	      }
  	      //---------------------------------------------------------------------
  	      // We do have the data so we can serve the user right away
  	      //---------------------------------------------------------------------
  	      if( self->state[strpid] == Valid )
  	      {
  	        if( offset + size > self->stripes[strpid].size() )
  	          size = self->stripes[strpid].size() - offset;
  	        memcpy( usrbuff, self->stripes[strpid].data() + offset, size );
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
  	    // If neccessary trigger error correction procedure
  	    // @param self : the block_t object
  	    // @return     : false if the block is corrupted and cannot be recovered,
  	    //               true otherwise
  	    //-----------------------------------------------------------------------
  	    static bool error_correction( std::shared_ptr<block_t> &self )
  	    {
  	      //---------------------------------------------------------------------
  	      // Do the accounting for our stripes
  	      //---------------------------------------------------------------------
  	      size_t missingcnt = 0, validcnt = 0, loadingcnt = 0, recoveringcnt = 0;
  	      std::for_each( self->state.begin(), self->state.end(), [&]( state_t &s )
  	        {
  	          switch( s )
  	          {
  	            case Missing:    ++missingcnt;    break;
  	            case Valid:      ++validcnt;      break;
  	            case Loading:    ++loadingcnt;    break;
  	            case Recovering: ++recoveringcnt; break;
  	            default: ;
  	          }
  	        } );
  	      //---------------------------------------------------------------------
  	      // If there are no missing stripes all is good ...
  	      //---------------------------------------------------------------------
  	      if( missingcnt + recoveringcnt == 0 ) return true;
  	      //---------------------------------------------------------------------
  	      // Check if we can do the recovery at all (if too many stripes are
  	      // missing it won't be possible)
  	      //---------------------------------------------------------------------
  	      if( missingcnt + recoveringcnt > self->objcfg.nbparity )
  	      {
  	        std::for_each( self->state.begin(), self->state.end(),
  	                       []( state_t &s ){ if( s == Recovering ) s = Missing; } );
  	        return false;
  	      }
  	      //---------------------------------------------------------------------
  	      // Check if we can do the recovery right away
  	      //---------------------------------------------------------------------
  	      if( validcnt >= self->objcfg.nbdata )
  	      {
  	        Config &cfg = Config::Instance();
  	        stripes_t strps( self->get_stripes() );
  	        try
  	        {
  	          cfg.GetRedundancy( self->objcfg ).compute( strps );
  	        }
  	        catch( const IOError &ex )
  	        {
  	          std::for_each( self->state.begin(), self->state.end(),
  	                         []( state_t &s ){ if( s == Recovering ) s = Missing; } );
  	          return false;
  	        }
  	        //-------------------------------------------------------------------
  	        // Now when we recovered the data we need to mark every stripe as
  	        // valid and execute the pending reads
  	        //-------------------------------------------------------------------
  	        for( size_t strpid = 0; strpid < self->objcfg.nbchunks; ++strpid )
  	        {
  	          if( self->state[strpid] != Recovering ) continue;
  	          self->state[strpid] = Valid;
  	          self->carryout( self->pending[strpid], self->stripes[strpid] );
  	        }
  	        return true;
  	      }
  	      //---------------------------------------------------------------------
  	      // Try loading the data and only then attempt recovery
  	      //---------------------------------------------------------------------
  	      size_t i = 0;
  	      while( loadingcnt + validcnt < self->objcfg.nbdata && i < self->objcfg.nbchunks )
  	      {
  	        size_t strpid = i++;
  	        if( self->state[strpid] != Empty ) continue;
  	        self->reader.Read( self->blkid, strpid, self->stripes[strpid],
  	                           read_callback( self, strpid ) );
  	        self->state[strpid] = Loading;
  	        ++loadingcnt;
  	      }

  	      //-------------------------------------------------------------------
  	      // Now that we triggered the recovery procedure mark every missing
  	      // stripe as recovering.
  	      //-------------------------------------------------------------------
  	      std::for_each( self->state.begin(), self->state.end(),
  	                     []( state_t &s ){ if( s == Missing ) s = Recovering; } );
  	      return true;
  	    }

  	    //-----------------------------------------------------------------------
  	    // Get a callback for read operation
  	    //-----------------------------------------------------------------------
  	    inline static
  	    callback_t read_callback( std::shared_ptr<block_t> &self, size_t strpid, bool allowRepair = true )
  	    {
  	      return [self, strpid, allowRepair]( const XrdCl::XRootDStatus &st, uint32_t) mutable
  	             {
  	    	std::unique_lock<std::mutex> lck( self->mtx );
  	               self->state[strpid] = st.IsOK() ? Valid : Missing;
  	               if(allowRepair){
  	               //------------------------------------------------------------
  	               // Check if we need to do any error correction (either for
  	               // the current stripe, or any other stripe)
  	               //------------------------------------------------------------
  	               bool recoverable = error_correction( self );
  	               //------------------------------------------------------------
  	               // Carry out the pending read requests if we got the data
  	               //------------------------------------------------------------
  	               if( st.IsOK() )
  	                 self->carryout( self->pending[strpid], self->stripes[strpid], st );

  	               //------------------------------------------------------------
  	               // Carry out the pending read requests if there was an error
  	               // and we cannot recover
  	               //------------------------------------------------------------
  	               if( !recoverable )
  	                 self->fail_missing();
  	               }
  	               else {
  	            	   if (!st.IsOK()) self->fail_missing();
  	            	   else self->carryout( self->pending[strpid], self->stripes[strpid], st );
  	               }
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

  	    //-------------------------------------------------------------------------
  	    // Execute the pending read requests
  	    //-------------------------------------------------------------------------
  	    inline void carryout( pending_t                 &pending,
  	                          const buffer_t            &stripe,
  	                          const XrdCl::XRootDStatus &st = XrdCl::XRootDStatus() )
  	    {
  	      //-----------------------------------------------------------------------
  	      // Iterate over all pending read operations for given stripe
  	      //-----------------------------------------------------------------------
  	      auto itr = pending.begin();
  	      for( ; itr != pending.end() ; ++itr )
  	      {
  	        auto       &args     = *itr;
  	        callback_t &callback = std::get<3>( args );
  	        uint32_t    nbrd  = 0; // number of bytes read
  	        //---------------------------------------------------------------------
  	        // If the read was successful, copy the data to user buffer
  	        //---------------------------------------------------------------------
  	        if( st.IsOK() )
  	        {
  	          uint64_t  offset  = std::get<0>( args );
  	          uint32_t  size    = std::get<1>( args );
  	          char     *usrbuff = std::get<2>( args );
  	          // are we reading past the end of file?
  	          if( offset > stripe.size() )
  	            size = 0;
  	          // are partially reading past the end of the file?
  	          else if( offset + size > stripe.size() )
  	            size = stripe.size() - offset;
  	          memcpy( usrbuff, stripe.data() + offset, size );
  	          nbrd = size;
  	        }
  	        //---------------------------------------------------------------------
  	        // Call the user callback
  	        //---------------------------------------------------------------------
  	        callback( st, nbrd );
  	      }
  	      //-----------------------------------------------------------------------
  	      // Now we can clear the pending reads
  	      //-----------------------------------------------------------------------
  	      pending.clear();
  	    }

  	    //-------------------------------------------------------------------------
  	    // Execute pending read requests for missing stripes
  	    //-------------------------------------------------------------------------
  	    inline void fail_missing()
  	    {
  	      size_t size = objcfg.nbchunks;
  	      for( size_t i = 0; i < size; ++i )
  	      {
  	        if( state[i] != Missing ) continue;
  	        carryout( pending[i], stripes[i],
  	                  XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError ) );
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
  	    uint32_t redirectionIndex; 			// tracks to which replacement archives this block was written to so far
  };
} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECREADER_HH_ */
