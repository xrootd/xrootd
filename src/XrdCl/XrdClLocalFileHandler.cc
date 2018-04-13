//------------------------------------------------------------------------------
// Copyright (c) 2017 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH 
// Author: Paul-Niklas Kramp <p.n.kramp@gsi.de>
//         Michal Simon <michal.simon@cern.ch>
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
#include "XrdCl/XrdClLocalFileHandler.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XProtocol/XProtocol.hh"

#include <string>
#include <memory>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>

namespace XrdCl
{

  //------------------------------------------------------------------------
  // Constructor
  //------------------------------------------------------------------------
  LocalFileHandler::LocalFileHandler() :
      fd( -1 ), pHostList( 0 )
  {
    jmngr = DefaultEnv::GetPostMaster()->GetJobManager();
  }

  //------------------------------------------------------------------------
  // Destructor
  //------------------------------------------------------------------------
  LocalFileHandler::~LocalFileHandler()
  {
    delete pHostList;
  }

  //------------------------------------------------------------------------
  // Open
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Open( const std::string& url, uint16_t flags,
      uint16_t mode, ResponseHandler* handler, uint16_t timeout )
  {
    AnyObject *resp = 0;
    XRootDStatus st = OpenImpl( url, flags, mode, resp );
    if( !st.IsOK() && st.code != errErrorResponse )
      return st;

    return QueueTask( new XRootDStatus( st ), resp, handler );
  }

  XRootDStatus LocalFileHandler::Open( const URL *url, const Message *req, AnyObject *&resp )
  {
    const ClientOpenRequest* request =
        reinterpret_cast<const ClientOpenRequest*>( req->GetBuffer() );
    uint16_t flags = ntohs( request->options );
    uint16_t mode  = ntohs( request->mode );
    return OpenImpl( url->GetURL(), flags, mode, resp );
  }

  //------------------------------------------------------------------------
  // Close
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Close( ResponseHandler* handler,
      uint16_t timeout )
  {
    if( close( fd ) == -1 )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Close: file fd: %i %s", fd, strerror( errno ) );
      XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                              XProtocol::mapError( errno ),
                                              strerror( errno ) );
      return QueueTask( error, 0, handler );
    }

    return QueueTask( new XRootDStatus(), 0, handler );
  }

  //------------------------------------------------------------------------
  // Stat
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Stat( ResponseHandler* handler,
      uint16_t timeout )
  {
    Log *log = DefaultEnv::GetLog();

    struct stat ssp;
    if( fstat( fd, &ssp ) == -1 )
    {
      log->Error( FileMsg, "Stat: failed fd: %i %s", fd, strerror( errno ) );
      XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                              XProtocol::mapError( errno ),
                                              strerror( errno ) );
      return QueueTask( error, 0, handler );
    }
    std::ostringstream data;
    data << ssp.st_dev << " " << ssp.st_size << " " << ssp.st_mode << " "
        << ssp.st_mtime;
    log->Debug( FileMsg, data.str().c_str() );

    StatInfo *statInfo = new StatInfo();
    if( !statInfo->ParseServerResponse( data.str().c_str() ) )
    {
      log->Error( FileMsg, "Stat: ParseServerResponse failed." );
      delete statInfo;
      return QueueTask( new XRootDStatus( stError, errErrorResponse, kXR_FSError ),
                        0, handler );
    }

    AnyObject *resp = new AnyObject();
    resp->Set( statInfo );
    return QueueTask( new XRootDStatus(), resp, handler );
  }

  //------------------------------------------------------------------------
  // Read
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Read( uint64_t offset, uint32_t size,
      void* buffer, ResponseHandler* handler, uint16_t timeout )
  {
    Log *log = DefaultEnv::GetLog();
    int read = 0;
    if( ( read = pread( fd, buffer, size, offset ) ) == -1 )
    {
      log->Error( FileMsg, "Read: failed %s", strerror( errno ) );
      XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                              XProtocol::mapError( errno ),
                                              strerror( errno ) );
      return QueueTask( error, 0, handler );
    }

    ChunkInfo *chunk;
    chunk = new ChunkInfo( offset, read, buffer );
    AnyObject *resp = new AnyObject();
    resp->Set( chunk );
    return QueueTask( new XRootDStatus(), resp, handler );
  }

  //------------------------------------------------------------------------
  // Write
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Write( uint64_t offset, uint32_t size,
      const void* buffer, ResponseHandler* handler, uint16_t timeout )
  {
    const char *buff = reinterpret_cast<const char*>( buffer );
    size_t bytesWritten = 0;
    while( bytesWritten < size )
    {
      ssize_t ret = pwrite( fd, buff, size, offset );
      if( ret < 0 )
      {
        Log *log = DefaultEnv::GetLog();
        log->Error( FileMsg, "Write: failed %s", strerror( errno ) );
        XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                XProtocol::mapError( errno ),
                                                strerror( errno ) );
        return QueueTask( error, 0, handler );
      }
      offset += ret;
      buff += ret;
      bytesWritten += ret;
    }

    return QueueTask( new XRootDStatus(), 0, handler );
  }

  //------------------------------------------------------------------------
  // Sync
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Sync( ResponseHandler* handler,
      uint16_t timeout )
  {
    if( fsync( fd ) )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Sync: failed, file descriptor: %i, %s", fd,
                  strerror( errno ) );
      XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                              XProtocol::mapError( errno ),
                                              strerror( errno ) );
      return QueueTask( error, 0, handler );
    }

    return QueueTask( new XRootDStatus(), 0, handler );
  }

  //------------------------------------------------------------------------
  // Truncate
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Truncate( uint64_t size,
      ResponseHandler* handler, uint16_t timeout )
  {
    if( ftruncate( fd, size ) )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Truncate: failed, file descriptor: %i, %s", fd,
                  strerror( errno ) );
      XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                              XProtocol::mapError( errno ),
                                              strerror( errno ) );
      return QueueTask( error, 0, handler );
    }

    return QueueTask( new XRootDStatus( stOK ), 0, handler );
  }

  //------------------------------------------------------------------------
  // VectorRead
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::VectorRead( const ChunkList& chunks,
      void* buffer, ResponseHandler* handler, uint16_t timeout )
  {
    std::unique_ptr<VectorReadInfo> info( new VectorReadInfo() );
    size_t totalSize = 0;
    bool useBuffer( buffer );

    for( auto itr = chunks.begin(); itr != chunks.end(); ++itr )
    {
      auto &chunk = *itr;
      if( !useBuffer )
        buffer = chunk.buffer;
      ssize_t bytesRead = pread( fd, buffer, chunk.length,
                                 chunk.offset );
      if( bytesRead < 0 )
      {
        Log *log = DefaultEnv::GetLog();
        log->Error( FileMsg, "VectorRead: failed, file descriptor: %i, %s", fd,
                    strerror( errno ) );
        XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                XProtocol::mapError( errno ),
                                                strerror( errno ) );
        return QueueTask( error, 0, handler );
      }
      totalSize += bytesRead;
      info->GetChunks().push_back( ChunkInfo( chunk.offset, bytesRead, buffer ) );
      if( useBuffer )
        buffer = reinterpret_cast<char*>( buffer ) + bytesRead;
    }

    info->SetSize( totalSize );
    AnyObject *resp = new AnyObject();
    resp->Set( info.release() );
    return QueueTask( new XRootDStatus(), resp, handler );
  }

  //------------------------------------------------------------------------
  // WriteV
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::WriteV( uint64_t            offset,
                                              const struct iovec *iov,
                                              int                 iovcnt,
                                              ResponseHandler    *handler,
                                              uint16_t            timeout )
  {
    iovec iovcp[iovcnt];
    memcpy( iovcp, iov, sizeof( iovcp ) );
    iovec *iovptr = iovcp;

    ssize_t size = 0;
    for( int i = 0; i < iovcnt; ++i )
      size += iovptr[i].iov_len;

    ssize_t bytesWritten = 0;
    while( bytesWritten < size )
    {
#ifdef __APPLE__
      ssize_t ret = lseek( fd, offset, SEEK_SET );
      if( ret >= 0 )
        ret = writev( fd, iovptr, iovcnt );
#else
      ssize_t ret = pwritev( fd, iovptr, iovcnt, offset );
#endif
      if( ret < 0 )
      {
        Log *log = DefaultEnv::GetLog();
        log->Error( FileMsg, "WriteV: failed %s", strerror( errno ) );
        XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                XProtocol::mapError( errno ),
                                                strerror( errno ) );
        return QueueTask( error, 0, handler );
      }

      bytesWritten += ret;
      while( ret )
      {
        if( size_t( ret ) > iovptr[0].iov_len )
        {
          ret -= iovptr[0].iov_len;
          --iovcnt;
          ++iovptr;
        }
        else
        {
          iovptr[0].iov_len -= ret;
          iovptr[0].iov_base = reinterpret_cast<char*>( iovptr[0].iov_base ) + ret;
          ret = 0;
        }
      }
    }

    return QueueTask( new XRootDStatus(), 0, handler );
  }

  //------------------------------------------------------------------------
  // Fcntl
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Fcntl( const Buffer &arg,
      ResponseHandler *handler, uint16_t timeout )
  {
    return XRootDStatus( stError, errNotSupported );
  }

  //------------------------------------------------------------------------
  // Visa
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Visa( ResponseHandler *handler,
      uint16_t timeout )
  {
    return XRootDStatus( stError, errNotSupported );
  }

  //------------------------------------------------------------------------
  // QueueTask - queues error/success tasks for all operations.
  // Must always return stOK.
  // Is always creating the same HostList containing only localhost.
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::QueueTask( XRootDStatus *st, AnyObject *resp,
      ResponseHandler *handler )
  {
    // if it is simply the sync handler we can release the semaphore
    // and return there is no need to execute this in the thread-pool
    SyncResponseHandler *syncHandler =
        dynamic_cast<SyncResponseHandler*>( handler );
    if( syncHandler )
    {
      syncHandler->HandleResponse( st, resp );
      return XRootDStatus();
    }

    HostList *hosts = pHostList ? new HostList( *pHostList ) : new HostList();
    hosts->push_back( HostInfo( pUrl, false ) );
    LocalFileTask *task = new LocalFileTask( st, resp, hosts, handler );
    jmngr->QueueJob( task );
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // MkdirPath - creates the folders specified in file_path
  // called if kXR_mkdir flag is set
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::MkdirPath( const std::string &path )
  {
    // first find the most up-front component that exists
    size_t pos = path.rfind( '/' );
    while( pos != std::string::npos && pos != 0 )
    {
      std::string tmp = path.substr( 0, pos );
      struct stat st;
      int rc = lstat( tmp.c_str(), &st );
      if( rc == 0 ) break;
      if( errno != ENOENT )
        return XRootDStatus( stError, errErrorResponse,
                             XProtocol::mapError( errno ),
                             strerror( errno ) );
      pos = path.rfind( '/', pos - 1 );
    }

    pos = path.find( '/', pos + 1 );
    while( pos != std::string::npos && pos != 0 )
    {
      std::string tmp = path.substr( 0, pos );
      if( mkdir( tmp.c_str(), 0755 ) )
      {
        if( errno != EEXIST )
          return XRootDStatus( stError, errErrorResponse,
                               XProtocol::mapError( errno ),
                               strerror( errno ) );
      }
      pos = path.find( '/', pos + 1 );
    }
    return XRootDStatus();
  }

  XRootDStatus LocalFileHandler::OpenImpl( const std::string &url, uint16_t flags,
                                           uint16_t mode, AnyObject *&resp)
  {
    Log *log = DefaultEnv::GetLog();

    // safe the file URL for the HostList for later
    pUrl = url;

    URL fileUrl( url );
    if( !fileUrl.IsValid() )
      return XRootDStatus( stError, errInvalidArgs );

    if( fileUrl.GetHostName() != "localhost" )
      return XRootDStatus( stError, errNotSupported );

    std::string path = fileUrl.GetPath();

    //---------------------------------------------------------------------
    // Prepare Flags
    //---------------------------------------------------------------------
    uint16_t openflags = 0;
    if( flags & kXR_new )
      openflags |= O_CREAT | O_EXCL;
    if( flags & kXR_open_wrto )
      openflags |= O_WRONLY;
    else if( flags & kXR_open_updt )
      openflags |= O_RDWR;
    else
      openflags |= O_RDONLY;
    if( flags & kXR_delete )
      openflags |= O_CREAT | O_TRUNC;

    if( flags & kXR_mkdir )
    {
      XRootDStatus st = MkdirPath( path );
      if( !st.IsOK() )
      {
        log->Error( FileMsg, "Open MkdirPath failed %s: %s", path.c_str(),
                    strerror( st.errNo ) );
        return st;
      }

    }
    //---------------------------------------------------------------------
    // Open File
    //---------------------------------------------------------------------
    if( mode == Access::Mode::None)
      mode = 0600;
    fd = open( path.c_str(), openflags, mode );
    if( fd == -1 )
    {
      log->Error( FileMsg, "Open: open failed: %s: %s", path.c_str(),
                  strerror( errno ) );

      return XRootDStatus( stError, errErrorResponse,
                           XProtocol::mapError( errno ),
                           strerror( errno ) );
    }
    //---------------------------------------------------------------------
    // Stat File and cache statInfo in openInfo
    //---------------------------------------------------------------------
    struct stat ssp;
    if( fstat( fd, &ssp ) == -1 )
    {
      log->Error( FileMsg, "Open: stat failed." );
      return XRootDStatus( stError, errErrorResponse,
                           XProtocol::mapError( errno ),
                           strerror( errno ) );
    }

    std::ostringstream data;
    data << ssp.st_dev << " " << ssp.st_size << " " << ssp.st_mode << " "
        << ssp.st_mtime;

    StatInfo *statInfo = new StatInfo();
    if( !statInfo->ParseServerResponse( data.str().c_str() ) )
    {
      log->Error( FileMsg, "Open: ParseServerResponse failed." );
      delete statInfo;
      return XRootDStatus( stError, errErrorResponse, kXR_FSError );
    }
    //All went well
    uint32_t ufd = fd;
    OpenInfo *openInfo = new OpenInfo( (uint8_t*)&ufd, 1, statInfo );
    resp = new AnyObject();
    resp->Set( openInfo );
    return XRootDStatus();
  }
}
