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
#include <sstream>

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
    EOCD( const char *buffer, uint32_t maxSize = 0 )
    {
      nbDisk        = to<uint16_t>(buffer + 4);
      nbDiskCd      = to<uint16_t>(buffer + 6);
      nbCdRecD      = to<uint16_t>(buffer + 8);
      nbCdRec       = to<uint16_t>(buffer + 10);
      cdSize        = to<uint32_t>(buffer + 12);
      cdOffset      = to<uint32_t>(buffer + 16);
      commentLength = to<uint16_t>(buffer + 20);
      if(maxSize > 0 && (uint32_t)(eocdBaseSize + commentLength) > maxSize)
    	  throw bad_data();
      comment       = std::string( buffer + 22, commentLength );

      eocdSize = eocdBaseSize + commentLength;
      useZip64= false;
    }

    //-------------------------------------------------------------------------
    //! Constructor from last LFH + CDFH
    //-------------------------------------------------------------------------
    EOCD( uint64_t cdoff, uint32_t cdcnt, uint32_t cdsize ):
      nbDisk( 0 ),
      nbDiskCd( 0 ),
      commentLength( 0 ),
      useZip64( false )
    {
      if( cdcnt >= ovrflw<uint16_t>::value )
      {
        nbCdRecD = ovrflw<uint16_t>::value;
        nbCdRec  = ovrflw<uint16_t>::value;
      }
      else
      {
        nbCdRecD = cdcnt;
        nbCdRec  = cdcnt;
      }

      cdSize = cdsize;

      if( cdoff >= ovrflw<uint32_t>::value )
      {
        cdOffset = ovrflw<uint32_t>::value;
        useZip64 = true;
      }
      else
        cdOffset = cdoff;

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

    //-------------------------------------------------------------------------
    //! Convert the EOCD into a string for logging purposes
    //-------------------------------------------------------------------------
    std::string ToString()
    {
      std::stringstream ss;
      ss << "{nbDisk="        << nbDisk;
      ss << ";nbDiskCd="      << nbDiskCd;
      ss << ";nbCdRecD="      << nbCdRecD;
      ss << ";nbCdRec="       << nbCdRec;
      ss << ";cdSize"         << cdSize;
      ss << ";cdOffset="      << cdOffset;
      ss << ";commentLength=" << commentLength;
      ss << ";comment="       << comment << '}';
      return ss.str();
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
