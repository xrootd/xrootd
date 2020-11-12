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

#ifndef SRC_XRDZIP_XRDZIPEOCD_HH_
#define SRC_XRDZIP_XRDZIPEOCD_HH_

#include "XrdZip/XrdZipUtils.hh"
#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include <string>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  // A data structure representing the End of Central Directory record
  //---------------------------------------------------------------------------
  struct EOCD
  {
    inline static const char* Find( const char *buffer, uint64_t size )
    {
      for( ssize_t offset = size - eocdBaseSize; offset >= 0; --offset )
      {
        uint32_t signature = to<uint32_t>( buffer + offset );
        if( signature == eocdSign ) return buffer + offset;
      }
      return 0;
    }

    //-------------------------------------------------------------------------
    //! Constructor from buffer
    //-------------------------------------------------------------------------
    EOCD( const char *buffer )
    {
      nbDisk        = *reinterpret_cast<const uint16_t*>( buffer + 4 );
      nbDiskCd      = *reinterpret_cast<const uint16_t*>( buffer + 6 );
      nbCdRecD      = *reinterpret_cast<const uint16_t*>( buffer + 8 );
      nbCdRec       = *reinterpret_cast<const uint16_t*>( buffer + 10 );
      cdSize        = *reinterpret_cast<const uint32_t*>( buffer + 12 );
      cdOffset      = *reinterpret_cast<const uint32_t*>( buffer + 16 );
      commentLength = *reinterpret_cast<const uint16_t*>( buffer + 20 );
      comment       = std::string( buffer + 22, commentLength );

      eocdSize = eocdBaseSize + commentLength;
      useZip64= false;
    }

    //-------------------------------------------------------------------------
    //! Constructor from last LFH + CDFH
    //-------------------------------------------------------------------------
    EOCD( LFH *lfh, CDFH *cdfh ):
      useZip64( false ),
      nbDisk( 0 ),
      nbDiskCd( 0 ),
      nbCdRecD( 1 ),
      nbCdRec( 1 ),
      commentLength( 0 )
    {
      if( lfh->compressedSize == ovrflw<uint32_t>::value ||
          lfh->lfhSize + lfh->compressedSize >= ovrflw<uint32_t>::value )
      {
        cdOffset = ovrflw<uint32_t>::value;
        cdSize   = ovrflw<uint32_t>::value;
        useZip64 = true;
      }
      else
      {
        cdOffset = lfh->lfhSize + lfh->compressedSize;
        cdSize   = cdfh->cdfhSize;
      }

      eocdSize = eocdBaseSize + commentLength;
    }

    //-------------------------------------------------------------------------
    //! Serialize the object into a buffer
    //-------------------------------------------------------------------------
    void Serialize( buffer_t &buffer )
    {
      copy_bytes( eocdSign, buffer );
      copy_bytes( nbDisk,   buffer );
      copy_bytes( nbDiskCd, buffer );
      copy_bytes( nbCdRecD, buffer );
      copy_bytes( nbCdRec,  buffer );
      copy_bytes( cdSize,   buffer );
      copy_bytes( cdOffset, buffer );
      copy_bytes( commentLength, buffer );

      std::copy( comment.begin(), comment.end(), std::back_inserter( buffer ) );
    }

    uint16_t    nbDisk;        //< number of this disk
    uint16_t    nbDiskCd;      //< number of the disk with the start of the central directory
    uint16_t    nbCdRecD;      //< total number of entries in the central directory on this disk
    uint16_t    nbCdRec;       //< total number of entries in the central directory
    uint32_t    cdSize;        //< size of the central directory
    uint32_t    cdOffset;      //< offset of start of central directory
    uint16_t    commentLength; //< comment length
    std::string comment;       //< user comment
    uint16_t    eocdSize;      //< size of the record
    bool        useZip64;      //< true if ZIP64 format is to be used, false otherwise

    //-------------------------------------------------------------------------
    // the End of Central Directory signature
    //-------------------------------------------------------------------------
    static const uint32_t eocdSign = 0x06054b50;
    static const uint16_t eocdBaseSize = 22;
    static const uint16_t maxCommentLength = 65535;
  };

}

#endif /* SRC_XRDZIP_XRDZIPEOCD_HH_ */
