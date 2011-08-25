//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

//------------------------------------------------------------------------------
// Let the show begin
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
  using namespace XrdClient;
  Log *log = Utils::GetDefaultLog();

  //----------------------------------------------------------------------------
  // Parse commandline
  //----------------------------------------------------------------------------
  if( argc != 3 )
  {
    log->Error( AppMsg, "Usage: %s %s %s", argv[0], "localFile",
                "root://remoteHost//remoteFile" );
    return 1;
  }

  log->Info( AppMsg, "Starting to copy %s to %s", argv[1], argv[2] );

  //----------------------------------------------------------------------------
  // Open the local file
  //----------------------------------------------------------------------------
  int localFile = open( argv[1], O_RDONLY );
  if( localFile == -1 )
  {
    log->Error( AppMsg, "Unable to open the input file: %s: %s",
                argv[1], strerror( errno ) );
    return 2;
  }

  //----------------------------------------------------------------------------
  // Open the remote file
  //----------------------------------------------------------------------------
  File *remoteFile = new File();
  if( !remoteFile->Open( argv[2], File::OpMkPath|File::OpNew|File::OpAppend ) )
  {
    log->Error( AppMsg, "Remote file opening failed: %s. Aborting.", argv[2] );
    delete remoteFile;
    return 3;
  }

  //----------------------------------------------------------------------------
  // Copy the data
  //----------------------------------------------------------------------------
  char    *buffer = new char[4*1024*1024];
  ssize_t  readData = 0;
  off_t    offset   = 0;
  do
  {
    log->Dump( AppMsg, "Reading %s, offset: %ld", argv[1], offset );
    readData = read( localFile, buffer, 4*1024*1024 );
    if( readData < 0 )
    {
      log->Error( AppMsg, "Unable to read data from %s, offset: %ld",
                  argv[1], offset );
      break;
    }

    if( !remoteFile->Write( offset, readData, buffer ) )
    {
      log->Error( AppMsg, "Failed to write %d bytes to %s at offset %ld",
                  readData, argv[2], offset );
      break;
    }
    offset += readData;
  }
  while( readData );

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  close( localFile );
  delete remoteFile;
  delete buffer;
  return 0;
}
