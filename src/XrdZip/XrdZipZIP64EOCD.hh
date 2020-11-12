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
    ZIP64_EOCD( EOCD *eocd,
               LFH  *lfh,
               CDFH *cdfh,
               uint16_t prevNbCdRecD = 0,
               uint16_t prevNbCdRec = 0,
               uint32_t prevCdSize = 0,
               uint32_t prevCdOffset = 0 ) :
                  zipVersion( ( 3 << 8 ) | 63 ),
                  minZipVersion( 45 ),
                  nbDisk( eocd->nbDisk ),
                  nbDiskCd( eocd->nbDiskCd ),
                  extensibleDataLength( 0 )
    {
      if ( eocd->nbCdRecD == ovrflw<uint16_t>::value )
        nbCdRecD = prevNbCdRecD + 1;
      else
        nbCdRecD = eocd->nbCdRecD;
      if ( eocd->nbCdRec == ovrflw<uint16_t>::value )
        nbCdRec = prevNbCdRec + 1;
      else
        nbCdRec = eocd->nbCdRec;
      if ( eocd->cdSize == ovrflw<uint32_t>::value )
        cdSize = prevCdSize + cdfh->cdfhSize;
      else
        cdSize = eocd->cdSize;
      if ( eocd->cdOffset == ovrflw<uint32_t>::value )
      {
        if ( lfh->compressedSize == ovrflw<uint32_t>::value )
          cdOffset = prevCdOffset + lfh->lfhSize + lfh->extra->compressedSize;
        else
          cdOffset = prevCdOffset + lfh->lfhSize + lfh->compressedSize;
      }
      else
        cdOffset = eocd->cdOffset;
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
