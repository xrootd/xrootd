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

#include <string>
#include <unordered_map>
#include <unordered_set>

class MicroTest;
class XrdEcTests;

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
    friend class ::XrdEcTests;
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
      void Open( XrdCl::ResponseHandler *handler, time_t timeout = 0 );

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
                 time_t                  timeout );

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
						time_t   				timeout);

      //-----------------------------------------------------------------------
      //! Close the data object
      //-----------------------------------------------------------------------
      void Close( XrdCl::ResponseHandler *handler, time_t timeout = 0 );

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
      void Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb, time_t timeout = 0 );

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

      void MissingVectorRead(std::shared_ptr<block_t> &block, size_t blkid, size_t strpid, time_t timeout = 0);

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

} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECREADER_HH_ */
