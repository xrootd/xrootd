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

#ifndef SRC_XRDZIP_XRDZIPLFH_HH_
#define SRC_XRDZIP_XRDZIPLFH_HH_

#include "XrdZip/XrdZipUtils.hh"
#include "XrdZip/XrdZipExtra.hh"

#include <string>
#include <memory>
#include <algorithm>
#include <iterator>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  //! A data structure representing ZIP Local File Header
  //---------------------------------------------------------------------------
  struct LFH
  {
    //-------------------------------------------------------------------------
    //! Convenience function for initializing compressed/uncompressed size
    //-------------------------------------------------------------------------
    inline static uint32_t initSize( const off_t &fileSize )
    {
      return fileSize >= ovrflw<uint32_t>::value ?
             ovrflw<uint32_t>::value : fileSize;
    }

    //-------------------------------------------------------------------------
    //! Constructor
    //-------------------------------------------------------------------------
    LFH( const std::string &filename, uint32_t crc, off_t fileSize, time_t time ) :
      generalBitFlag( 0 ), compressionMethod( 0 ), timestmp( time ), ZCRC32( crc ),
      compressedSize( initSize( fileSize ) ), uncompressedSize( initSize( fileSize ) ),
      filenameLength( filename.size() ), filename( filename ), extra( new Extra( fileSize ) )
    {
      extraLength = extra->totalSize;
      if ( extraLength == 0 )
        minZipVersion = 10;
      else
        minZipVersion = 45;
      lfhSize = lfhBaseSize + filenameLength + extraLength;
    }

    //-------------------------------------------------------------------------
    //! Constructor from buffer
    //-------------------------------------------------------------------------
    LFH( const char *buffer, const uint64_t bufferSize = 0 )
    {
    	if(bufferSize > 0 && bufferSize < (uint64_t)lfhBaseSize)
    		throw bad_data();
      // check if the buffer contains a LFH record
      uint32_t signature = 0;
      from_buffer( signature, buffer );
      if( signature != lfhSign ) throw bad_data();
      // parse LFH filds
      from_buffer( minZipVersion, buffer );
      from_buffer( generalBitFlag, buffer );
      from_buffer( compressionMethod, buffer );
      from_buffer( timestmp.time, buffer );
      from_buffer( timestmp.date, buffer );
      from_buffer( ZCRC32, buffer );
      from_buffer( compressedSize, buffer );
      from_buffer( uncompressedSize, buffer );
      from_buffer( filenameLength, buffer );
      from_buffer( extraLength, buffer );

      if(bufferSize > 0 && (uint64_t)(lfhBaseSize + filenameLength + extraLength) > bufferSize)
    	  throw bad_data();
      // parse the filename
      filename.assign( buffer, filenameLength );
      buffer += filenameLength;
      // parse the extra record
      if( extraLength > 0 )
        ParseExtra( buffer, extraLength );

      lfhSize = lfhBaseSize + filenameLength + extraLength;
    }

    //-------------------------------------------------------------------------
    //! Serialize the object into a buffer
    //-------------------------------------------------------------------------
    void Serialize( buffer_t &buffer )
    {
      copy_bytes( lfhSign, buffer );
      copy_bytes( minZipVersion, buffer );
      copy_bytes( generalBitFlag, buffer );
      copy_bytes( compressionMethod, buffer );
      copy_bytes( timestmp.time, buffer );
      copy_bytes( timestmp.date, buffer );
      copy_bytes( ZCRC32, buffer );
      copy_bytes( compressedSize, buffer );
      copy_bytes( uncompressedSize, buffer );
      copy_bytes( filenameLength, buffer );
      copy_bytes( extraLength , buffer );
      std::copy( filename.begin(), filename.end(), std::back_inserter( buffer ) );
      extra->Serialize( buffer );
    }

    //-------------------------------------------------------------------------
    // Parse the extensible data fields
    //-------------------------------------------------------------------------
    void ParseExtra( const char *buffer, uint16_t length)
    {
      uint8_t ovrflws = Extra::NONE;
      uint16_t exsize = 0;

      // check if compressed size is overflown
      if( compressedSize == ovrflw<uint32_t>::value)
      {
        ovrflws |= Extra::CPMSIZE;
        exsize  += sizeof( uint64_t );
      }

      // check if original size is overflown
      if( uncompressedSize == ovrflw<uint32_t>::value )
      {
        ovrflws |= Extra::UCMPSIZE;
        exsize  += sizeof( uint64_t );
      }

      // if the expected size of ZIP64 extension is 0 we
      // can skip parsing of 'extra'
      if( exsize == 0 ) return;

      extra.reset( new Extra() );

      // Parse the extra part
      buffer = Extra::Find( buffer, length );
      if( buffer )
        extra->FromBuffer( buffer, exsize, ovrflws );
    }

    uint16_t               minZipVersion;      //< minimum ZIP version required to read the file
    uint16_t               generalBitFlag;     //< flags
    uint16_t               compressionMethod;  //< compression method
    dos_timestmp           timestmp;           //< DOS time stamp
    uint32_t               ZCRC32;             //< crc32 value
    uint32_t               compressedSize;     //< compressed data size
    uint32_t               uncompressedSize;   //< uncompressed data size
    uint16_t               filenameLength;     //< file name length
    uint16_t               extraLength;        //< size of the ZIP64 extra field
    std::string            filename;           //< file name
    std::unique_ptr<Extra> extra;              //< the ZIP64 extra field
    uint16_t               lfhSize;            //< size of the Local File Header

    //-------------------------------------------------------------------------
    //! Local File Header signature
    //-------------------------------------------------------------------------
    static const uint32_t lfhSign     = 0x04034b50;
    static const uint16_t lfhBaseSize = 30;
  };
}

#endif /* SRC_XRDZIP_XRDZIPLFH_HH_ */
