/*
 * XrdZipZIP64EOCD.hh
 *
 *  Created on: 9 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPZIP64EOCD_HH_
#define SRC_XRDZIP_XRDZIPZIP64EOCD_HH_

#include "XrdZip/XrdZipUtils.hh"
#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include <string>
#include <sstream>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  //! A data structure representing the ZIP64 extension to End of Central
  //! Directory record
  //---------------------------------------------------------------------------
  struct ZIP64_EOCD
  {
    //-------------------------------------------------------------------------
    //! Constructor from a buffer
    //-------------------------------------------------------------------------
    ZIP64_EOCD( const char* buffer ):
      extensibleDataLength( 0 )
    {
      zip64EocdSize = *reinterpret_cast<const uint64_t*>( buffer + 4 );
      zipVersion    = *reinterpret_cast<const uint16_t*>( buffer + 12 );
      minZipVersion = *reinterpret_cast<const uint16_t*>( buffer + 14 );
      nbDisk        = *reinterpret_cast<const uint32_t*>( buffer + 16 );
      nbDiskCd      = *reinterpret_cast<const uint32_t*>( buffer + 20 );
      nbCdRecD      = *reinterpret_cast<const uint64_t*>( buffer + 24 );
      nbCdRec       = *reinterpret_cast<const uint64_t*>( buffer + 32 );
      cdSize        = *reinterpret_cast<const uint64_t*>( buffer + 40 );
      cdOffset      = *reinterpret_cast<const uint64_t*>( buffer + 48 );

      zip64EocdTotalSize = zip64EocdBaseSize + extensibleDataLength;
    }

    //-------------------------------------------------------------------------
    //! Constructor from last LFH + CDFH
    //-------------------------------------------------------------------------
    ZIP64_EOCD( uint64_t cdoff, uint32_t cdcnt, uint32_t cdsize ) :
                  zipVersion( ( 3 << 8 ) | 63 ),
                  minZipVersion( 45 ),
                  nbDisk( 0 ),
                  nbDiskCd( 0 ),
                  extensibleDataLength( 0 )
    {
      nbCdRec  = cdcnt;
      nbCdRecD = cdcnt;
      cdSize   = cdsize;
      cdOffset = cdoff;

      zip64EocdSize = zip64EocdBaseSize + extensibleDataLength - 12;
      zip64EocdTotalSize = zip64EocdBaseSize + extensibleDataLength;
    }

    //-------------------------------------------------------------------------
    //! Serialize the object into a buffer
    //-------------------------------------------------------------------------
    void Serialize( buffer_t &buffer )
    {
      copy_bytes( zip64EocdSign, buffer );
      copy_bytes( zip64EocdSize, buffer );
      copy_bytes( zipVersion,    buffer );
      copy_bytes( minZipVersion, buffer );
      copy_bytes( nbDisk,        buffer );
      copy_bytes( nbDiskCd,      buffer );
      copy_bytes( nbCdRecD,      buffer );
      copy_bytes( nbCdRec,       buffer );
      copy_bytes( cdSize,        buffer );
      copy_bytes( cdOffset,      buffer );

      std::copy( extensibleData.begin(), extensibleData.end(), std::back_inserter( buffer ) );
    }

    //-------------------------------------------------------------------------
    //! Convert the ZIP64EOCD into a string for logging purposes
    //-------------------------------------------------------------------------
    std::string ToString()
    {
      std::stringstream ss;
      ss << "{zip64EocdSize="       << zip64EocdSize;
      ss << ";zipVersion="          << zipVersion;
      ss << ";minZipVersion="       << minZipVersion;
      ss << ";nbDisk="              << nbDisk;
      ss << ";nbDiskCd="            << nbDiskCd;
      ss << ";nbCdRecD="            << nbCdRecD;
      ss << ";nbCdRec="             << nbCdRec;
      ss << ";cdSize="              << cdSize;
      ss << ";cdOffset="            << cdOffset;
      ss << ";extensibleData="      << extensibleData;
      ss << ";extensibleDataLength" << extensibleDataLength << "}";
      return ss.str();
    }

    uint64_t    zip64EocdSize;        //< size of zip64 end of central directory record
    uint16_t    zipVersion;           //< version made by
    uint16_t    minZipVersion;        //< version needed to extract
    uint32_t    nbDisk;               //< number of this disk
    uint32_t    nbDiskCd;             //< number of the disk with the  start of the central directory
    uint64_t    nbCdRecD;             //< total number of entries in the central directory on this disk
    uint64_t    nbCdRec;              //< total number of entries in the central directory
    uint64_t    cdSize;               //< size of the central directory
    uint64_t    cdOffset;             //< offset of start of central directory
    std::string extensibleData;       //< zip64 extensible data sector
    uint64_t    extensibleDataLength; //< extensible data length
    uint64_t    zip64EocdTotalSize;   //< size of the record

    //-------------------------------------------------------------------------
    // the End of Central Directory signature
    //-------------------------------------------------------------------------
    static const uint32_t zip64EocdSign = 0x06064b50;
    static const uint16_t zip64EocdBaseSize = 56;
  };
}

#endif /* SRC_XRDZIP_XRDZIPZIP64EOCD_HH_ */
