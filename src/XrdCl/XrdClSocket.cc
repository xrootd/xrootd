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

#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClTls.hh"
#include "XrdNet/XrdNetConnect.hh"
#include "XrdSys/XrdSysFD.hh"

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include <stdexcept>

namespace XrdCl
{
  Socket::Socket( int socket, SocketStatus status ):
    pSocket(socket), pStatus( status ),
    pProtocolFamily( AF_INET ),
    pChannelID( 0 ),
    pCorked( false )
  {
  };

  //------------------------------------------------------------------------
  // Desctuctor
  //------------------------------------------------------------------------
  Socket::~Socket()
  {
    Close();
  };

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  XRootDStatus Socket::Initialize( int family )
  {
    if( pSocket != -1 )
      return XRootDStatus( stError, errInvalidOp );

    pSocket = XrdSysFD_Socket( family, SOCK_STREAM, 0 );
    if( pSocket < 0 )
    {
      pSocket = -1;
      return XRootDStatus( stError, errSocketError );
    }

    pProtocolFamily = family;

    //--------------------------------------------------------------------------
    // Make the socket non blocking and disable the Nagle algorithm since
    // we will be using this for transmitting messages not handling streams
    //--------------------------------------------------------------------------
    int flags;
    if( (flags = ::fcntl( pSocket, F_GETFL, 0 )) == -1 )
      flags = 0;
    if( ::fcntl( pSocket, F_SETFL, flags | O_NONBLOCK | O_NDELAY ) == -1 )
    {
      Close();
      return XRootDStatus( stError, errFcntl, errno );
    }

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    flags = DefaultNoDelay;
    env->GetInt( "NoDelay", flags );
    if( setsockopt( pSocket, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof( int ) ) < 0 )
    {
      Close();
      return XRootDStatus( stError, errFcntl, errno );
    }

    //--------------------------------------------------------------------------
    // We use send with MSG_NOSIGNAL to avoid SIGPIPEs on Linux, on MacOSX
    // we set SO_NOSIGPIPE option, on Solaris we ignore the SIGPIPE
    //--------------------------------------------------------------------------
#ifdef __APPLE__
    int set = 1;
    XRootDStatus st = SetSockOpt( SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(int) );
    if( !st.IsOK() )
    {
      Close();
      return st;
    }
#elif __solaris__
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigaction( SIGPIPE, &act, NULL );
#endif

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Set the socket flags
  //----------------------------------------------------------------------------
  XRootDStatus Socket::SetFlags( int flags )
  {
    if( pSocket == -1 )
      return XRootDStatus( stError, errInvalidOp );

    int st = ::fcntl( pSocket, F_SETFL, flags );
    if( st == -1 )
      return XRootDStatus( stError, errSocketError, errno );
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Get the socket flags
  //----------------------------------------------------------------------------
  XRootDStatus Socket::GetFlags( int &flags )
  {
    if( pSocket == -1 )
      return XRootDStatus( stError, errInvalidOp );

    int st = ::fcntl( pSocket, F_GETFL, 0 );
    if( st == -1 )
      return XRootDStatus( stError, errSocketError, errno );
    flags = st;
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Get socket options
  //----------------------------------------------------------------------------
  XRootDStatus Socket::GetSockOpt( int level, int optname, void *optval,
                                   socklen_t *optlen )
  {
    if( pSocket == -1 )
      return XRootDStatus( stError, errInvalidOp );

    if( ::getsockopt( pSocket, level, optname, optval, optlen ) != 0 )
      return XRootDStatus( stError, errSocketOptError, errno );

    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Set socket options
  //------------------------------------------------------------------------
  XRootDStatus Socket::SetSockOpt( int level, int optname, const void *optval,
                                   socklen_t optlen )
  {
    if( pSocket == -1 )
      return XRootDStatus( stError, errInvalidOp );

    if( ::setsockopt( pSocket, level, optname, optval, optlen ) != 0 )
      return XRootDStatus( stError, errSocketOptError, errno );

    return XRootDStatus();
  }


  //----------------------------------------------------------------------------
  // Connect to the given host name
  //----------------------------------------------------------------------------
  XRootDStatus Socket::Connect( const std::string &host,
                                uint16_t           port,
                                uint16_t           timeout )
  {
    if( pSocket == -1 || pStatus == Connected || pStatus == Connecting )
      return XRootDStatus( stError, errInvalidOp );

    std::vector<XrdNetAddr> addrs;
    std::ostringstream o; o << host << ":" << port;
    XRootDStatus st;

    if( pProtocolFamily == AF_INET6 )
      st = Utils::GetHostAddresses( addrs, URL( o.str() ), Utils::IPAll );
    else
      st = Utils::GetHostAddresses( addrs, URL( o.str() ), Utils::IPv4 );

    if( !st.IsOK() )
      return st;

    Utils::LogHostAddresses( DefaultEnv::GetLog(), PostMasterMsg, o.str(),
                             addrs );


    return ConnectToAddress( addrs[0], timeout );
  }

  //----------------------------------------------------------------------------
  // Connect to the given host
  //----------------------------------------------------------------------------
  XRootDStatus Socket::ConnectToAddress( const XrdNetAddr &addr,
                                         uint16_t          timeout )
  {
    if( pSocket == -1 || pStatus == Connected || pStatus == Connecting )
      return XRootDStatus( stError, errInvalidOp );

    pServerAddr.reset( new XrdNetAddr( addr ) );;

    //--------------------------------------------------------------------------
    // Make sure TLS is off when the physical connection is newly established
    //--------------------------------------------------------------------------
    pTls.reset();

    //--------------------------------------------------------------------------
    // Connect
    //--------------------------------------------------------------------------
    int status = XrdNetConnect::Connect( pSocket, pServerAddr->SockAddr(),
                                         pServerAddr->SockSize(), timeout );
    if( status != 0 )
    {
      XRootDStatus st( stError );

      //------------------------------------------------------------------------
      // If we connect asynchronously this is not really an error
      //------------------------------------------------------------------------
      if( !timeout && status == EINPROGRESS )
      {
        pStatus = Connecting;
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      // Errors
      //------------------------------------------------------------------------
      else if( status == ETIMEDOUT )
        st.code = errSocketTimeout;
      else
        st.code = errSocketError;
      st.errNo = status;

      Close();
      return st;
    }
    pStatus = Connected;
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Disconnect
  //----------------------------------------------------------------------------
  void Socket::Close()
  {
    if( pTls ) pTls->Shutdown();

    if( pSocket != -1 )
    {
      close( pSocket );
      pStatus      = Disconnected;
      pSocket      = -1;
      pSockName    = "";
      pPeerName    = "";
      pName        = "";
    }
  }

  //----------------------------------------------------------------------------
  //! Read raw bytes from the socket
  //----------------------------------------------------------------------------
  XRootDStatus Socket::ReadRaw( void *buffer, uint32_t size, int32_t timeout,
                                uint32_t &bytesRead )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Some useful variables
    //--------------------------------------------------------------------------
    bytesRead = 0;

    char          *current    = (char *)buffer;
    bool           useTimeout = (timeout!=-1);
    time_t         now        = 0;
    time_t         newNow     = 0;
    XRootDStatus   sc;

    if( useTimeout )
      now = ::time(0);

    //--------------------------------------------------------------------------
    // Repeat the following until we have read all the requested data
    //--------------------------------------------------------------------------
    while ( bytesRead < size )
    {
      //------------------------------------------------------------------------
      // Check if we can read something
      //------------------------------------------------------------------------
      sc = Poll( true, false, useTimeout ? timeout : -1 );

      //------------------------------------------------------------------------
      // It looks like we've got an event. Let's check if we can read something.
      //------------------------------------------------------------------------
      if( sc.status == stOK )
      {
        ssize_t n = ::read( pSocket, current, (size-bytesRead) );

        if( n > 0 )
        {
          bytesRead += n;
          current   += n;
        }

        //----------------------------------------------------------------------
        // We got a close here - this means that there is no more data in
        // the buffer so we disconnect
        //----------------------------------------------------------------------
        if( n == 0 )
        {
          Close();
          return XRootDStatus( stError, errSocketDisconnected );
        }

        //----------------------------------------------------------------------
        // Error
        //----------------------------------------------------------------------
        if( (n < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) )
        {
          Close();
          return XRootDStatus( stError, errSocketError, errno );
        }
      }
      else
      {
        Close();
        return sc;
      }

      //------------------------------------------------------------------------
      // Do we still have time to wait for data?
      //------------------------------------------------------------------------
      if( useTimeout )
      {
        newNow    = ::time(0);
        timeout -= (newNow-now);
        now       = newNow;
        if( timeout < 0 )
          break;
      }
    }

    //--------------------------------------------------------------------------
    // Have we managed to read everything?
    //--------------------------------------------------------------------------
    if( bytesRead < size )
      return XRootDStatus( stError, errSocketTimeout );
    return XRootDStatus( stOK );
  }

  //----------------------------------------------------------------------------
  // Write raw bytes to the socket
  //----------------------------------------------------------------------------
  XRootDStatus Socket::WriteRaw( void *buffer, uint32_t size, int32_t timeout,
                                 uint32_t &bytesWritten )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Some useful variables
    //--------------------------------------------------------------------------
    bytesWritten = 0;

    char          *current    = (char *)buffer;
    bool           useTimeout = (timeout!=-1);
    time_t         now        = 0;
    time_t         newNow     = 0;
    XRootDStatus   sc;

    if( useTimeout )
      now = ::time(0);

    //--------------------------------------------------------------------------
    // Repeat the following until we have written everything
    //--------------------------------------------------------------------------
    while ( bytesWritten < size )
    {
      //------------------------------------------------------------------------
      // Check if we can read something
      //------------------------------------------------------------------------
      sc = Poll( false, true, useTimeout ? timeout : -1 );

      //------------------------------------------------------------------------
      // Let's write
      //------------------------------------------------------------------------
      if( sc.status == stOK )
      {
        ssize_t n = ::write( pSocket, current, (size-bytesWritten) );

        if( n > 0 )
        {
          bytesWritten += n;
          current   += n;
        }

        //----------------------------------------------------------------------
        // Error
        //----------------------------------------------------------------------
        if( (n <= 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) )
        {
          Close();
          return XRootDStatus( stError, errSocketError, errno );
        }
      }
      else
      {
        Close();
        return sc;
      }

      //------------------------------------------------------------------------
      // Do we still have time to wait for data?
      //------------------------------------------------------------------------
      if( useTimeout )
      {
        newNow    = ::time(0);
        timeout -= (newNow-now);
        now       = newNow;
        if( timeout < 0 )
          break;
      }
    }

    //--------------------------------------------------------------------------
    // Have we managed to read everything?
    //--------------------------------------------------------------------------
    if( bytesWritten < size )
      return XRootDStatus( stError, errSocketTimeout );

    return XRootDStatus( stOK );
  }

  //------------------------------------------------------------------------
  // Portable wrapper around SIGPIPE free send
  //----------------------------------------------------------------------------
  XRootDStatus Socket::Send( const char *buffer, size_t size, int &bytesWritten )
  {
    if( pTls ) return pTls->Send( buffer, size, bytesWritten );

    //--------------------------------------------------------------------------
    // We use send with MSG_NOSIGNAL to avoid SIGPIPEs on Linux
    //--------------------------------------------------------------------------
#if defined(__linux__) || defined(__GNU__) || (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
    int status = ::send( pSocket, buffer, size, MSG_NOSIGNAL );
#else
    int status = ::write( pSocket, buffer, size );
#endif

    if( status <= 0 )
      return ClassifyErrno( errno );

    bytesWritten = status;
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  //! Write data from a kernel buffer to the socket
  //!
  //! @param kbuff : data to be written
  //! @return      : the amount of data actually written
  //------------------------------------------------------------------------
  XRootDStatus Socket::Send( XrdSys::KernelBuffer &kbuff, int &bytesWritten )
  {
    if( pTls ) return XRootDStatus( stError, errNotSupported, 0,
                                     "Cannot send a kernel-buffer over TLS." );

    ssize_t status = XrdSys::Send( pSocket, kbuff );

    if( status <= 0 )
      return ClassifyErrno( errno );
    bytesWritten += status;
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Poll the descriptor
  //----------------------------------------------------------------------------
  XRootDStatus Socket::Poll( bool readyForReading, bool readyForWriting,
                             int32_t timeout )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Prepare the stuff
    //--------------------------------------------------------------------------
    pollfd   pollDesc;
    int      pollRet;
    bool     useTimeout = (timeout!=-1);
    time_t   now        = 0;
    time_t   newNow     = 0;
    short    hupEvents  = POLLHUP;

#ifdef __linux__
    hupEvents |= POLLRDHUP;
#endif

    if( useTimeout )
      now = ::time(0);

    pollDesc.fd     = pSocket;
    pollDesc.events = POLLERR | POLLNVAL | hupEvents;

    if( readyForReading )
      pollDesc.events |= (POLLIN | POLLPRI);

    if( readyForWriting )
      pollDesc.events |= POLLOUT;

    //--------------------------------------------------------------------------
    // We loop on poll because it may return -1 even thought no fatal error
    // has occurred, these may be:
    // * a signal interrupting the execution (errno == EINTR)
    // * a failure to initialize some internal structures (Solaris only)
    //   (errno == EAGAIN)
    //--------------------------------------------------------------------------
    do
    {
      pollRet = poll( &pollDesc, 1, (useTimeout ? timeout*1000 : -1) );
      if( (pollRet < 0) && (errno != EINTR) && (errno != EAGAIN) )
        return XRootDStatus( stError, errPoll, errno );

      //------------------------------------------------------------------------
      // Check if we did not time out in the case where we are not supposed
      // to wait indefinitely
      //------------------------------------------------------------------------
      if( useTimeout )
      {
        newNow   = time(0);
        timeout -= (newNow-now);
        now      = newNow;
        if( timeout < 0 )
          return XRootDStatus( stError, errSocketTimeout );
      }
    }
    while( pollRet == -1 );

    //--------------------------------------------------------------------------
    // Check if we have timed out
    //--------------------------------------------------------------------------
    if( pollRet == 0 )
      return XRootDStatus( stError, errSocketTimeout );

    //--------------------------------------------------------------------------
    // We have some events
    //--------------------------------------------------------------------------
    if( pollDesc.revents & (POLLIN | POLLPRI | POLLOUT) )
      return XRootDStatus( stOK );

    //--------------------------------------------------------------------------
    // We've been hang up on
    //--------------------------------------------------------------------------
    if( pollDesc.revents & hupEvents )
      return XRootDStatus( stError, errSocketDisconnected );

    //--------------------------------------------------------------------------
    // We're messed up, either because we messed up ourselves (POLLNVAL) or
    // got messed up by the network (POLLERR)
    //--------------------------------------------------------------------------
    return XRootDStatus( stError, errSocketError );
  }

  //----------------------------------------------------------------------------
  // Get the name of the socket
  //----------------------------------------------------------------------------
  std::string Socket::GetSockName() const
  {
    if( pStatus != Connected )
      return "";

    if( pSockName.length() )
      return pSockName;

    char      nameBuff[256];
    int len = XrdNetUtils::IPFormat( -pSocket, nameBuff, sizeof(nameBuff) );
    if( len == 0 )
      return "";

    pSockName = nameBuff;
    return pSockName;
  }

  //----------------------------------------------------------------------------
  // Get the name of the remote peer
  //----------------------------------------------------------------------------
  std::string Socket::GetPeerName() const
  {
    if( pStatus != Connected )
      return "";

    if( pPeerName.length() )
      return pPeerName;

    char      nameBuff[256];
    int len = XrdNetUtils::IPFormat( pSocket, nameBuff, sizeof(nameBuff) );
    if( len == 0 )
      return "";

    pPeerName = nameBuff;
    return pPeerName;
  }

  //----------------------------------------------------------------------------
  // Get the string representation of the socket
  //----------------------------------------------------------------------------
  std::string Socket::GetName() const
  {
    if( pStatus != Connected )
      return "<x><--><x>";

    if( pName.length() )
      return pName;

    pName = "<";
    pName += GetSockName();
    pName += "><--><";
    pName += GetPeerName();
    pName += ">";
    return pName;
  }


  //------------------------------------------------------------------------
  // Classify errno while reading/writing
  //------------------------------------------------------------------------
  XRootDStatus Socket::ClassifyErrno( int error )
  {
    switch( errno )
    {

      case EAGAIN:
#if EAGAIN != EWOULDBLOCK
      case EWOULDBLOCK:
#endif
      {
        //------------------------------------------------------------------
        // Reading/writing operation would block! So we are done for now,
        // but we will be back ;-)
        //------------------------------------------------------------------
        return XRootDStatus( stOK, suRetry );
      }
      case ECONNRESET:
      case EDESTADDRREQ:
      case EMSGSIZE:
      case ENOTCONN:
      case ENOTSOCK:
      {
        //------------------------------------------------------------------
        // Actual socket error error!
        //------------------------------------------------------------------
        return XRootDStatus( stError, errSocketError, errno );
      }
      case EFAULT:
      {
        //------------------------------------------------------------------
        // The buffer provided by the user for reading/writing is invalid
        //------------------------------------------------------------------
        return XRootDStatus( stError, errInvalidArgs );
      }
      default:
      {
        //------------------------------------------------------------------
        // Not a socket error
        //------------------------------------------------------------------
        return XRootDStatus( stError, errInternal, errno );
      }
    }
  }


  //----------------------------------------------------------------------------
  // Read helper from raw socket helper
  //----------------------------------------------------------------------------
  XRootDStatus Socket::Read( char *buffer, size_t size, int &bytesRead )
  {
    if( pTls ) return pTls->Read( buffer, size, bytesRead );

    int status = ::read( pSocket, buffer, size );

    // if the server shut down the socket declare a socket error (it
    // will trigger a re-connect)
    if( status == 0 )
      return XRootDStatus( stError, errSocketError, errno );

    if( status < 0 )
      return ClassifyErrno( errno );

    bytesRead = status;
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // ReadV helper for raw socket
  //----------------------------------------------------------------------------
  XRootDStatus Socket::ReadV( iovec *iov, int iovcnt, int &bytesRead )
  {
    if( pTls ) return pTls->ReadV( iov, iovcnt, bytesRead );

    int status = ::readv( pSocket, iov, iovcnt );

    // if the server shut down the socket declare a socket error (it
    // will trigger a re-connect)
    if( status == 0 )
      return XRootDStatus( stError, errSocketError, errno );

    if( status < 0 )
      return ClassifyErrno( errno );

    bytesRead = status;
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Cork the underlying socket
  //------------------------------------------------------------------------
  XRootDStatus Socket::Cork()
  {
#if defined(TCP_CORK) // it's not defined on mac, we might want explore the possibility of using TCP_NOPUSH
    if( pCorked ) return XRootDStatus();

    int state = 1;
    int rc = setsockopt( pSocket, IPPROTO_TCP, TCP_CORK, &state, sizeof( state ) );
    if( rc != 0 )
      return XRootDStatus( stFatal, errSocketOptError, errno );
#endif
    pCorked = true;
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Uncork the underlying socket
  //------------------------------------------------------------------------
  XRootDStatus Socket::Uncork()
  {
#if defined(TCP_CORK) // it's not defined on mac, we might want explore the possibility of using TCP_NOPUSH
    if( !pCorked ) return XRootDStatus();

    int state = 0;
    int rc = setsockopt( pSocket, IPPROTO_TCP, TCP_CORK, &state, sizeof( state ) );
    if( rc != 0 )
      return XRootDStatus( stFatal, errSocketOptError, errno );
#endif
    pCorked = false;
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // Flash the underlying socket
  //------------------------------------------------------------------------
  XRootDStatus Socket::Flash()
  {
    //----------------------------------------------------------------------
    // Uncork the socket in order to flash the socket
    //----------------------------------------------------------------------
    XRootDStatus st = Uncork();
    if( !st.IsOK() ) return st;

    //----------------------------------------------------------------------
    // Once the data has been flashed we can cork the socket back
    //----------------------------------------------------------------------
    return Cork();
  }

  //------------------------------------------------------------------------
  // Do special event mapping if applicable
  //------------------------------------------------------------------------
  uint8_t Socket::MapEvent( uint8_t event )
  {
    if( pTls ) return pTls->MapEvent( event );
    return event;
  }

  //------------------------------------------------------------------------
  // Enable encryption
  //------------------------------------------------------------------------
  XRootDStatus Socket::TlsHandShake( AsyncSocketHandler *socketHandler,
                                     const std::string  &thehost )
  {
    try
    {
      if( !pServerAddr ) return XRootDStatus( stError, errInvalidOp );
      if( !pTls ) pTls.reset( new Tls( this, socketHandler ) );
      return pTls->Connect( thehost, pServerAddr.get() );
    }
    catch( std::exception& ex )
    {
      // the exception has been thrown when we tried to create
      // the TLS context
      return XRootDStatus( stFatal, errTlsError, 0, ex.what() );
    }

    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // @return : true if socket is using TLS layer for encryption,
  //           false otherwise
  //------------------------------------------------------------------------
  bool Socket::IsEncrypted()
  {
    return bool( pTls.get() );
  }

}


