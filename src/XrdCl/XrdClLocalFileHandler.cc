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
#include <stdexcept>
#include <iostream>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <aio.h>

namespace
{

  class AioCtx
  {
    public:

      enum Opcode
      {
        None,
        Read,
        Write,
        Sync
      };

      AioCtx( const XrdCl::HostList &hostList, XrdCl::ResponseHandler *handler ) :
        opcode( None ), hosts( new XrdCl::HostList( hostList ) ), handler( handler )
      {
        aiocb *ptr = new aiocb();
        memset( ptr, 0, sizeof( aiocb ) );

        XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
        int useSignals = XrdCl::DefaultAioSignal;
        env->GetInt( "AioSignal", useSignals );

        if( useSignals )
        {
          static SignalHandlerRegistrator registrator; // registers the signal handler

          ptr->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
          ptr->aio_sigevent.sigev_signo  = SIGUSR1;
        }
        else
        {
          ptr->aio_sigevent.sigev_notify = SIGEV_THREAD;
          ptr->aio_sigevent.sigev_notify_function = ThreadHandler;
        }

        ptr->aio_sigevent.sigev_value.sival_ptr = this;

        cb.reset( ptr );
      }


      void SetWrite( int fd, size_t offset, size_t size, const void *buffer )
      {
        cb->aio_fildes = fd;
        cb->aio_offset = offset;
        cb->aio_buf    = const_cast<void*>( buffer );
        cb->aio_nbytes = size;
        opcode = Opcode::Write;
      }

      void SetRead( int fd, size_t offset, size_t size, void *buffer )
      {
        cb->aio_fildes = fd;
        cb->aio_offset = offset;
        cb->aio_buf    = buffer;
        cb->aio_nbytes = size;
        opcode = Opcode::Read;
      }

      void SetFsync( int fd )
      {
        cb->aio_fildes = fd;
        opcode = Opcode::Sync;
      }

      static void ThreadHandler( sigval arg )
      {
        std::unique_ptr<AioCtx> me( reinterpret_cast<AioCtx*>( arg.sival_ptr ) );
        Handler( std::move( me ) );
      }

      static void SignalHandler( int sig, siginfo_t *info, void *ucontext )
      {
        std::unique_ptr<AioCtx> me( reinterpret_cast<AioCtx*>( info->si_value.sival_ptr ) );
        Handler( std::move( me ) );
      }

      operator aiocb*()
      {
        return cb.get();
      }

    private:

      struct SignalHandlerRegistrator
      {
        SignalHandlerRegistrator()
        {
          struct sigaction newact, oldact;
          newact.sa_sigaction = SignalHandler;
          sigemptyset( &newact.sa_mask );
          newact.sa_flags = SA_SIGINFO;
          int rc = sigaction( SIGUSR1, &newact, &oldact );
          if( rc < 0 )
            throw std::runtime_error( strerror( errno ) );
        }
      };

      static void Handler( std::unique_ptr<AioCtx> me )
      {
        if( me->opcode == Opcode::None )
          return;

        using namespace XrdCl;

        int rc = aio_return( me->cb.get() );
        if( rc < 0 )
        {
          Log *log = DefaultEnv::GetLog();
          log->Error( FileMsg, GetErrMsg( me->opcode ), strerror( errno ) );
          XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                  XProtocol::mapError( errno ),
                                                  strerror( errno ) );
          QueueTask( error, 0, me->hosts, me->handler );
        }
        else
        {
          AnyObject *resp = 0;

          if( me->opcode == Opcode::Read )
          {
            ChunkInfo *chunk = new ChunkInfo( me->cb->aio_offset,
                                              rc,
                                              const_cast<void*>( me->cb->aio_buf ) );
            resp = new AnyObject();
            resp->Set( chunk );
          }

          QueueTask( new XRootDStatus(), resp, me->hosts, me->handler );
        }
      }

      static const char* GetErrMsg( Opcode opcode )
      {
        static const char readmsg[]  = "Read:  failed %s";
        static const char writemsg[] = "Write: failed %s";
        static const char syncmsg[]  = "Sync:  failed %s";

        switch( opcode )
        {
          case Opcode::Read:  return readmsg;

          case Opcode::Write: return writemsg;

          case Opcode::Sync:  return syncmsg;

          default:            return 0;
        }
      }

      static void QueueTask( XrdCl::XRootDStatus *status, XrdCl::AnyObject *resp,
                             XrdCl::HostList *hosts, XrdCl::ResponseHandler *handler )
      {
        using namespace XrdCl;

        // if it is simply the sync handler we can release the semaphore
        // and return there is no need to execute this in the thread-pool
        SyncResponseHandler *syncHandler =
            dynamic_cast<SyncResponseHandler*>( handler );
        if( syncHandler )
        {
          syncHandler->HandleResponse( status, resp );
        }
        else
        {
          JobManager *jmngr = DefaultEnv::GetPostMaster()->GetJobManager();
          LocalFileTask *task = new LocalFileTask( status, resp, hosts, handler );
          jmngr->QueueJob( task );
        }
      }
      
      std::unique_ptr<aiocb>  cb;
      Opcode                  opcode;
      XrdCl::HostList        *hosts;
      XrdCl::ResponseHandler *handler;
  };

};

namespace XrdCl
{

  //------------------------------------------------------------------------
  // Constructor
  //------------------------------------------------------------------------
  LocalFileHandler::LocalFileHandler() :
      fd( -1 )
  {
    jmngr = DefaultEnv::GetPostMaster()->GetJobManager();
  }

  //------------------------------------------------------------------------
  // Destructor
  //------------------------------------------------------------------------
  LocalFileHandler::~LocalFileHandler()
  {

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
#if defined(__APPLE__)
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
    ChunkInfo *chunk = new ChunkInfo( offset, read, buffer );
    AnyObject *resp = new AnyObject();
    resp->Set( chunk );
    return QueueTask( new XRootDStatus(), resp, handler );
#else
    AioCtx *ctx = new AioCtx( pHostList, handler );
    ctx->SetRead( fd, offset, size, buffer );

    int rc = aio_read( *ctx );

    if( rc < 0 )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Read: failed %s", strerror( errno ) );
      return XRootDStatus( stError, errOSError, XProtocol::mapError( rc ),
                           strerror( errno ) );
    }

    return XRootDStatus();
#endif
  }

  //------------------------------------------------------------------------
  // Write
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Write( uint64_t offset, uint32_t size,
      const void* buffer, ResponseHandler* handler, uint16_t timeout )
  {
#if defined(__APPLE__)
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
#else
    AioCtx *ctx = new AioCtx( pHostList, handler );
    ctx->SetWrite( fd, offset, size, buffer );

    int rc = aio_write( *ctx );

    if( rc < 0 )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Write: failed %s", strerror( errno ) );
      return XRootDStatus( stError, errOSError, XProtocol::mapError( rc ),
                           strerror( errno ) );
    }

    return XRootDStatus();
#endif
  }

  //------------------------------------------------------------------------
  // Sync
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::Sync( ResponseHandler* handler,
      uint16_t timeout )
  {
#if defined(__APPLE__)
    if( fsync( fd ) )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Sync: failed %s", strerror( errno ) );
      XRootDStatus *error = new XRootDStatus( stError, errOSError,
                                              XProtocol::mapError( errno ),
                                              strerror( errno ) );
      return QueueTask( error, 0, handler );
    }
#else
    AioCtx *ctx = new AioCtx( pHostList, handler );
    ctx->SetFsync( fd );
    int rc = aio_fsync( O_SYNC, *ctx );
    if( rc < 0 )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( FileMsg, "Sync: failed %s", strerror( errno ) );
      return XRootDStatus( stError, errOSError, XProtocol::mapError( rc ),
                           strerror( errno ) );
    }
    return XRootDStatus();
#endif
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
        log->Error( FileMsg, "VectorRead: failed, file descriptor: %i, %s",
                    fd, strerror( errno ) );
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
  // VectorWrite
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::VectorWrite( const ChunkList &chunks,
      ResponseHandler *handler, uint16_t timeout )
  {

    for( auto itr = chunks.begin(); itr != chunks.end(); ++itr )
    {
      auto &chunk = *itr;
      ssize_t bytesWritten = pwrite( fd, chunk.buffer, chunk.length,
                                     chunk.offset );
      if( bytesWritten < 0 )
      {
        Log *log = DefaultEnv::GetLog();
        log->Error( FileMsg, "VectorWrite: failed, file descriptor: %i, %s",
                    fd, strerror( errno ) );
        XRootDStatus *error = new XRootDStatus( stError, errErrorResponse,
                                                XProtocol::mapError( errno ),
                                                strerror( errno ) );
        return QueueTask( error, 0, handler );
      }
    }

    return QueueTask( new XRootDStatus(), 0, handler );
  }

  //------------------------------------------------------------------------
  // WriteV
  //------------------------------------------------------------------------
  XRootDStatus LocalFileHandler::WriteV( uint64_t            offset,
                                         ChunkList          *chunks,
                                         ResponseHandler    *handler,
                                         uint16_t            timeout )
  {
    size_t iovcnt = chunks->size();
    iovec iovcp[iovcnt];
    ssize_t size = 0;
    for( size_t i = 0; i < iovcnt; ++i )
    {
      iovcp[i].iov_base = (*chunks)[i].buffer;
      iovcp[i].iov_len  = (*chunks)[i].length;
      size += (*chunks)[i].length;
    }
    iovec *iovptr = iovcp;

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

    HostList *hosts = pHostList.empty() ? 0 : new HostList( pHostList );
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

    // add the URL to hosts list
    pHostList.push_back( HostInfo( pUrl, false ) );

    //All went well
    uint32_t ufd = fd;
    OpenInfo *openInfo = new OpenInfo( (uint8_t*)&ufd, 1, statInfo );
    resp = new AnyObject();
    resp->Set( openInfo );
    return XRootDStatus();
  }

  XRootDStatus LocalFileHandler::ExecRequest( const URL         &url,
                                              Message           *msg,
                                              ResponseHandler   *handler,
                                              MessageSendParams &sendParams )
  {
    ClientRequest *req = reinterpret_cast<ClientRequest*>( msg->GetBuffer() );

    switch( req->header.requestid )
    {
      case kXR_open:
      {
        XRootDStatus st = Open( url.GetURL(), req->open.options,
                                req->open.mode, handler, sendParams.timeout );
        delete msg; // in case of other operations msg is owned by the handler
        return st;
      }

      case kXR_close:
      {
        return Close( handler, sendParams.timeout );
      }

      case kXR_stat:
      {
        return Stat( handler, sendParams.timeout );
      }

      case kXR_read:
      {
        return Read( req->read.offset, req->read.rlen,
                     sendParams.chunkList->front().buffer,
                     handler, sendParams.timeout );
      }

      case kXR_write:
      {
        ChunkList *chunks = sendParams.chunkList;
        if( chunks->size() == 1 )
        {
          // it's an ordinary write
          return Write( req->write.offset, req->write.dlen,
                        chunks->front().buffer, handler,
                        sendParams.timeout );
        }
        // it's WriteV call
        return WriteV( req->write.offset, sendParams.chunkList,
                       handler, sendParams.timeout );
      }

      case kXR_sync:
      {
        return Sync( handler, sendParams.timeout );
      }

      case kXR_truncate:
      {
        return Truncate( req->truncate.offset, handler, sendParams.timeout );
      }

      case kXR_writev:
      {
        return VectorWrite( *sendParams.chunkList, handler,
                            sendParams.timeout );
      }

      case kXR_readv:
      {
        return VectorRead( *sendParams.chunkList, 0,
                           handler, sendParams.timeout );
      }

      default:
      {
        return XRootDStatus( stError, errNotSupported );
      }
    }
  }
}
