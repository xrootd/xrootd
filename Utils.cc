//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "Utils.hh"
#include "XrdCl/XrdClUtils.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <cstdlib>

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
