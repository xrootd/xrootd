//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef UTILS_HH
#define UTILS_HH

#include <unistd.h>
#include <stdint.h>
#include <zlib.h>

//------------------------------------------------------------------------------
//! A bunch of useful functions
//------------------------------------------------------------------------------
class Utils
{
  public:
    //--------------------------------------------------------------------------
    //! Fill the buffer with random data
    //!
    //! @param  buffer the buffer to be filled
    //! @param  size   size of the buffer
    //! @return        number of ranom bytes actually generated, -1 on error
    //--------------------------------------------------------------------------
    static ssize_t GetRandomBytes( char *buffer, size_t size );

    //--------------------------------------------------------------------------
    //! Compute crc32 checksum out of a buffer
    //!
    //! @param buffer data buffer
    //! @param len    size of the data buffer
    //--------------------------------------------------------------------------
    static uint32_t ComputeCRC32( const void *buffer, uint32_t len )
    {
      return crc32( crc32( 0L, Z_NULL, 0 ), (const Bytef*)buffer, len );
    }

    //--------------------------------------------------------------------------
    //! Update a crc32 checksum
    //!
    //! @param crc    old checksum
    //! @param buffer data buffer
    //! @param len    size of the data buffer
    //--------------------------------------------------------------------------
    static uint32_t UpdateCRC32( uint32_t crc, const void *buffer, uint32_t len )
    {
      return crc32( crc, (const Bytef*)buffer, len );
    }
};

#endif // UTILS_HH
