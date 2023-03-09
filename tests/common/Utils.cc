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

#include "Utils.hh"
#include "XrdCl/XrdClUtils.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <cstdlib>

namespace XrdClTests {

//------------------------------------------------------------------------------
// Convert text CRC32 to int
//------------------------------------------------------------------------------
bool Utils::CRC32TextToInt( uint32_t &result, const std::string &text  )
{
  std::vector<std::string> res;
  XrdCl::Utils::splitString( res, text, " " );
  if( res.size() != 2 )
    return false;

  char *cnvCursor;
  result = ::strtoll( res[1].c_str(), &cnvCursor, 0 );
  if( *cnvCursor != 0 )
    return false;

  return true;
}

//------------------------------------------------------------------------------
// Fill the buffer with random data
//------------------------------------------------------------------------------
ssize_t Utils::GetRandomBytes( char *buffer, size_t size )
{
  int fileFD = open( "/dev/urandom", O_RDONLY );
  if( fileFD < 0 )
    return -1;

  char    *current   = buffer;
  ssize_t  toRead    = size;
  ssize_t  currRead  = 0;
  while( toRead > 0 && (currRead = read( fileFD, current, toRead )) > 0 )
  {
    toRead  -= currRead;
    current += currRead;
  }
  close( fileFD );

  return size-toRead;;
}

//------------------------------------------------------------------------------
// Write buffer to a socket
//------------------------------------------------------------------------------
ssize_t Utils::Write( int fd, const void *buf, size_t count )
{
  ssize_t ret;
  size_t remain = count;
  const uint8_t *p = (uint8_t*)buf;
  while( remain )
  {
    do
    {
      ret = ::write( fd, p, remain );
    } while( ret<0 && errno == EINTR );
    if( ret <= 0 )
      return ret;
    p += ret;
    remain -= ret;
  }
  return count;
}

//------------------------------------------------------------------------------
// Read buffer from a socket
//------------------------------------------------------------------------------
ssize_t Utils::Read( int fd, void *buf, size_t count )
{
  ssize_t ret;
  size_t remain = count, totRead = 0;
  uint8_t *p = (uint8_t*)buf;
  while( remain )
  {
    do
    {
      ret = ::read( fd, p, remain );
    } while( ret<0 && errno == EINTR );
    if( ret == 0 )
      break;
    if( ret < 0 )
      return ret;
    p += ret;
    remain -= ret;
    totRead += ret;
  }
  return totRead;
}
}
