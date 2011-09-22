//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Testing utils
//------------------------------------------------------------------------------

#include "Utils.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
