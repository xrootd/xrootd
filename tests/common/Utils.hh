//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef UTILS_HH
#define UTILS_HH

#include <unistd.h>
#include <cstdint>
#include <zlib.h>
#include <string>

namespace XrdClTests {

//------------------------------------------------------------------------------
//! A bunch of useful functions
//------------------------------------------------------------------------------
class Utils
{
  public:
    //--------------------------------------------------------------------------
    //! Convert string representation of a crc checksum to int
    //!
    //! @param  result the resulting integer
    //! @param  text   input sting
    //! @return status of the conversion
    //--------------------------------------------------------------------------
    static bool CRC32TextToInt( uint32_t &result, const std::string &text  );

    //--------------------------------------------------------------------------
    //! Fill the buffer with random data
    //!
    //! @param  buffer the buffer to be filled
    //! @param  size   size of the buffer
    //! @return        number of ranom bytes actually generated, -1 on error
    //--------------------------------------------------------------------------
    static ssize_t GetRandomBytes( char *buffer, size_t size );

    //--------------------------------------------------------------------------
    //! Get initial CRC32 value
    //--------------------------------------------------------------------------
    static uint32_t GetInitialCRC32()
    {
      return crc32( 0L, Z_NULL, 0 );
    }

    //--------------------------------------------------------------------------
    //! Compute crc32 checksum out of a buffer
    //!
    //! @param buffer data buffer
    //! @param len    size of the data buffer
    //--------------------------------------------------------------------------
    static uint32_t ComputeCRC32( const void *buffer, uint64_t len )
    {
      return crc32( GetInitialCRC32(), (const Bytef*)buffer, len );
    }

    //--------------------------------------------------------------------------
    //! Update a crc32 checksum
    //!
    //! @param crc    old checksum
    //! @param buffer data buffer
    //! @param len    size of the data buffer
    //--------------------------------------------------------------------------
    static uint32_t UpdateCRC32( uint32_t crc, const void *buffer, uint64_t len )
    {
      return crc32( crc, (const Bytef*)buffer, len );
    }

    //--------------------------------------------------------------------------
    //! Combine two crc32 checksums
    //!
    //! @param crc1 checksum of the first data block
    //! @param crc2 checksum of the second data block
    //! @param len2 size of the second data block
    //--------------------------------------------------------------------------
    static uint32_t CombineCRC32( uint32_t crc1, uint32_t crc2, uint64_t len2 )
    {
      return crc32_combine( crc1, crc2, len2 );
    }
};
};

#endif // UTILS_HH
