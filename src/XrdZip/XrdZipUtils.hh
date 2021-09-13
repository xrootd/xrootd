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

#ifndef SRC_XRDZIP_XRDZIPUTILS_HH_
#define SRC_XRDZIP_XRDZIPUTILS_HH_

#include <cstring>

#include <ctime>
#include <vector>
#include <algorithm>
#include <iterator>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  // Exception indicating corrupted data
  //---------------------------------------------------------------------------
  struct bad_data : public std::exception { };

  //---------------------------------------------------------------------------
  // Provides overflow value for unsigned int types
  //---------------------------------------------------------------------------
  template<typename UINT>
  struct ovrflw
  {
    static const UINT value = -1;
  };

  //---------------------------------------------------------------------------
  // Buffer type (a typedef for std::vector<char>)
  //---------------------------------------------------------------------------
  typedef std::vector<char> buffer_t;

  //---------------------------------------------------------------------------
  // Copies integer byte by byte into a buffer
  //---------------------------------------------------------------------------
  template<typename INT>
  inline static void copy_bytes( const INT value, buffer_t &buffer)
  {
    const char *begin = reinterpret_cast<const char*>( &value );
    const char *end   = begin + sizeof( INT );
    std::copy( begin, end, std::back_inserter( buffer ) );
  }

  //---------------------------------------------------------------------------
  // Copies bytes into an integer type and advances the buffer by the number
  // of bytes read.
  //---------------------------------------------------------------------------
  template<typename INT>
  inline static void from_buffer( INT &var, const char *&buffer )
  {
    memcpy( &var, buffer, sizeof( INT ) );
    buffer += sizeof( INT );
  }

  //---------------------------------------------------------------------------
  // Converts bytes into an integer type
  //---------------------------------------------------------------------------
  template<typename INT>
  inline static INT to( const char *buffer )
  {
    INT value;
    memcpy( &value, buffer, sizeof( INT) );
    return value;
  }

  //---------------------------------------------------------------------------
  // Generate a DOS timestamp (time/date)
  //---------------------------------------------------------------------------
  struct dos_timestmp
  {
    //-------------------------------------------------------------------------
    // Default constructor (creates a timestamp for current time)
    //-------------------------------------------------------------------------
    inline dos_timestmp() : time( 0 ), date( 0 )
    {
      const std::time_t now = std::time( nullptr );
      const std::tm calendar_time = *std::localtime( std::addressof( now ) );

      time |= ( hour_mask & uint16_t( calendar_time.tm_hour    ) ) << hour_shift;
      time |= ( min_mask  & uint16_t( calendar_time.tm_min     ) ) << min_shift;
      time |= ( sec_mask  & uint16_t( calendar_time.tm_sec / 2 ) ) << sec_shift;

      date |= ( year_mask & uint16_t( calendar_time.tm_year - 1980 ) ) << year_shift;
      date |= ( mon_mask  & uint16_t( calendar_time.tm_mon         ) ) << mon_shift;
      date |= ( day_mask  & uint16_t( calendar_time.tm_mday        ) ) << day_shift;
    }

    //-------------------------------------------------------------------------
    // Constructs a DOS timestamp from time_t value
    //-------------------------------------------------------------------------
    inline dos_timestmp( time_t timestmp ) : time( 0 ), date( 0 )
    {
      const std::tm calendar_time = *std::localtime( std::addressof( timestmp ) );

      time |= ( hour_mask & uint16_t( calendar_time.tm_hour    ) ) << hour_shift;
      time |= ( min_mask  & uint16_t( calendar_time.tm_min     ) ) << min_shift;
      time |= ( sec_mask  & uint16_t( calendar_time.tm_sec / 2 ) ) << sec_shift;

      date |= ( year_mask & uint16_t( calendar_time.tm_year - 1980 ) ) << year_shift;
      date |= ( mon_mask  & uint16_t( calendar_time.tm_mon         ) ) << mon_shift;
      date |= ( day_mask  & uint16_t( calendar_time.tm_mday        ) ) << day_shift;
    }

    //-------------------------------------------------------------------------
    // The time part of the DOS timestamp
    //-------------------------------------------------------------------------
    uint16_t time;

    static const uint16_t sec_mask  = 0x1f; //< seconds mask
    static const uint16_t min_mask  = 0x3f; //< minutes mask
    static const uint16_t hour_mask = 0x1f; //< hour mask

    static const uint8_t sec_shift  = 0; //< seconds shift
    static const uint8_t min_shift  = 5; //< minutes shift
    static const uint8_t hour_shift = 11; //< hour shift

    //-------------------------------------------------------------------------
    // The date part of the DOS timestamp
    //-------------------------------------------------------------------------
    uint16_t date;

    static const uint16_t day_mask  = 0x1f; //< day mask
    static const uint16_t mon_mask  = 0x0f; //< month mask
    static const uint16_t year_mask = 0x7f; //< year mask

    static const uint8_t day_shift  = 0; //< day shift
    static const uint8_t mon_shift  = 5; //< month shift
    static const uint8_t year_shift = 9; //< year shift
  };
}

#endif /* SRC_XRDZIP_XRDZIPUTILS_HH_ */
