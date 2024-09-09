/*
 * XrdZipZIP64EOCDL.hh
 *
 *  Created on: 9 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPZIP64EOCDL_HH_
#define SRC_XRDZIP_XRDZIPZIP64EOCDL_HH_

#include "XrdZip/XrdZipUtils.hh"
#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipZIP64EOCD.hh"
#include <string>
#include <sstream>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  //! A data structure representing the ZIP64 end of central directory locator
  //---------------------------------------------------------------------------
  struct ZIP64_EOCDL
  {
    //-------------------------------------------------------------------------
    //! Constructor from a buffer
    //-------------------------------------------------------------------------
    ZIP64_EOCDL( const char *buffer )
    {
      nbDiskZip64Eocd = to<uint32_t>(buffer + 4);
      zip64EocdOffset = to<uint64_t>(buffer + 8);
      totalNbDisks    = to<uint32_t>(buffer + 16);
    }

    //-------------------------------------------------------------------------
    //! Constructor from EOCD and ZIP64 EOCD
    //-------------------------------------------------------------------------
    ZIP64_EOCDL( const EOCD &eocd, const ZIP64_EOCD &zip64Eocd ):
      nbDiskZip64Eocd( 0 ),
      totalNbDisks( 1 )
    {
      if ( eocd.cdOffset == ovrflw<uint32_t>::value )
        zip64EocdOffset = zip64Eocd.cdOffset;
      else
        zip64EocdOffset = eocd.cdOffset;

      if ( eocd.cdSize == ovrflw<uint32_t>::value )
        zip64EocdOffset += zip64Eocd.cdSize;
      else
        zip64EocdOffset += eocd.cdSize;
    }

    //-------------------------------------------------------------------------
    //! Serialize the object into a buffer
    //-------------------------------------------------------------------------
    void Serialize( buffer_t &buffer )
    {
      copy_bytes( zip64EocdlSign,  buffer );
      copy_bytes( nbDiskZip64Eocd, buffer );
      copy_bytes( zip64EocdOffset, buffer );
      copy_bytes( totalNbDisks,    buffer );
    }

    //-------------------------------------------------------------------------
    //! Convert the EOCDL into a string for logging purposes
    //-------------------------------------------------------------------------
    std::string ToString()
    {
      std::stringstream ss;
      ss << "{nbDiskZip64Eocd=" << nbDiskZip64Eocd;
      ss << ";zip64EocdOffset=" << zip64EocdOffset;
      ss << ";totalNbDisks="    << totalNbDisks << "}";
      return ss.str();
    }

    uint32_t nbDiskZip64Eocd; //< number of the disk with the start of the zip64 end of central directory
    uint64_t zip64EocdOffset; //< relative offset of the zip64 end of central directory record
    uint32_t totalNbDisks;    //< total number of disks

    //-------------------------------------------------------------------------
    // the End of Central Directory locator signature
    //-------------------------------------------------------------------------
    static const uint32_t zip64EocdlSign = 0x07064b50;
    static const uint16_t zip64EocdlSize = 20;
  };
}

#endif /* SRC_XRDZIP_XRDZIPZIP64EOCDL_HH_ */
