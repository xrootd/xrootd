/*
 * XrdEcObjCfg.hh
 *
 *  Created on: Nov 25, 2019
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECOBJCFG_HH_
#define SRC_XRDEC_XRDECOBJCFG_HH_

#include "XrdOuc/XrdOucCRC32C.hh"

#include "isa-l/crc.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  //! ISAL crc32 implementation
  //---------------------------------------------------------------------------
  inline static uint32_t isal_crc32(uint32_t crc, void const *buf, size_t len)
  {
    const unsigned char* buffer = reinterpret_cast<const unsigned char*>( buf );
    return crc32_gzip_refl( crc, buffer, len );
  }

  struct ObjCfg
  {
      ObjCfg() = delete;

      ObjCfg( const std::string &obj, uint8_t nbdata, uint8_t nbparity, uint64_t chunksize, bool usecrc32c, bool nomtfile = false ) :
        obj( obj ),
        nbchunks( nbdata + nbparity ),
        nbparity( nbparity ),
        nbdata( nbdata ),
        datasize( nbdata * chunksize ),
        chunksize( chunksize ),
        paritysize( nbparity * chunksize ),
        blksize( datasize + paritysize ),
        nomtfile( nomtfile )
      {
        digest = usecrc32c ? crc32c : isal_crc32;
      }

      ObjCfg( const ObjCfg &objcfg ) : obj( objcfg.obj ),
                                       nbchunks( objcfg.nbchunks ),
                                       nbparity( objcfg.nbparity ),
                                       nbdata( objcfg.nbdata ),
                                       datasize( objcfg.datasize ),
                                       chunksize( objcfg.chunksize ),
                                       paritysize( objcfg.paritysize ),
                                       blksize( objcfg.blksize ),
                                       plgr( objcfg.plgr ),
                                       digest( objcfg.digest ),
                                       nomtfile( objcfg.nomtfile )
      {
      }

      inline std::string GetDataUrl( size_t i ) const
      {
        std::string url = plgr[i] + '/' + obj;
        if( !dtacgi.empty() ) url += '?' + dtacgi[i];
        return url;
      }

      inline std::string GetMetadataUrl( size_t i ) const
      {
        std::string url = plgr[i] + '/' + obj + ".mt";
        if( !mdtacgi.empty() ) url += '?' + mdtacgi[i];
        return url;
      }

      inline std::string GetFileName( size_t blknb, size_t strpnb ) const
      {
        return obj + '.' + std::to_string( blknb ) + '.' + std::to_string( strpnb );
      }

      const std::string obj;
      const uint8_t     nbchunks;   // number of chunks in block
      const uint8_t     nbparity;   // number of chunks in parity
      const uint8_t     nbdata;     // number of chunks in data
      const uint64_t    datasize;   // size of the data in the block
      const uint64_t    chunksize;  // size of single chunk (nbchunks * chunksize = blksize)
      const uint64_t    paritysize; // size of the parity in the block
      const uint64_t    blksize;    // the whole block size (data + parity) in MB

      std::vector<std::string> plgr;
      std::vector<std::string> dtacgi;
      std::vector<std::string> mdtacgi;

      uint32_t (*digest)(uint32_t, void const*, size_t);

      bool nomtfile;
  };
}


#endif /* SRC_XRDEC_XRDECOBJCFG_HH_ */
