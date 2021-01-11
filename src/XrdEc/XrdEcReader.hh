/*
 * XrdEcReader.hh
 *
 *  Created on: 18 Dec 2020
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECREADER_HH_
#define SRC_XRDEC_XRDECREADER_HH_

#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcScheduleHandler.hh"

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClZipArchive.hh"

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
    friend struct block_t;

    public:
      //-----------------------------------------------------------------------
      //! Constructor
      //!
      //! @param objcfg : configuration for the data object (e.g. number of
      //!                 data and parity stripes)
      //-----------------------------------------------------------------------
      Reader( ObjCfg &objcfg ) : objcfg( objcfg )
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
      void Open( XrdCl::ResponseHandler *handler );

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
                 XrdCl::ResponseHandler *handler );

    private:

      //-----------------------------------------------------------------------
      //! Read data from given stripes from given block
      //!
      //! @param blknb  : number of the block
      //! @param strpnb : number of stripe in the block
      //! @param buffer : buffer for the data
      //! @param cb     : callback
      //-----------------------------------------------------------------------
      void Read( size_t blknb, size_t strpnb, buffer_t &buffer, callback_t cb );

      //-----------------------------------------------------------------------
      //! Read metadata for the object
      //!
      //! @param index : placement's index
      //-----------------------------------------------------------------------
      XrdCl::Pipeline ReadMetadata( size_t index );

      //-----------------------------------------------------------------------
      //! Parse metadata from chunk info object
      //!
      //! @param ch : chunk info object returned by a read operation
      //-----------------------------------------------------------------------
      bool ParseMetadata( XrdCl::ChunkInfo &ch );

      typedef std::unordered_map<std::string, std::shared_ptr<XrdCl::ZipArchive>> dataarchs_t;
      typedef std::unordered_map<std::string, buffer_t> metadata_t;
      typedef std::unordered_map<std::string, std::string> urlmap_t;

      ObjCfg                   &objcfg;
      dataarchs_t               dataarchs; //> map URL to ZipArchive object
      metadata_t                metadata;  //> map URL to CD metadata
      urlmap_t                  urlmap;    //> map blknb/strpnb (data chunk) to URL
      std::shared_ptr<block_t>  block;     //> cache for the block we are reading from
  };

} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECREADER_HH_ */
