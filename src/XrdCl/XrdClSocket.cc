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
#include "XrdNet/XrdNetConnect.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

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

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Status Socket::Initialize( int family )
  {
    if( pSocket != -1 )
      return Status( stError, errInvalidOp );

    pSocket = ::socket( family, SOCK_STREAM, 0 );
    if( pSocket < 0 )
    {
      pSocket = -1;
      return Status( stError, errSocketError );
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
      return Status( stError, errFcntl, errno );
    }

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    flags = DefaultNoDelay;
    env->GetInt( "NoDelay", flags );
    if( setsockopt( pSocket, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof( int ) ) < 0 )
    {
      Close();
      return Status( stError, errFcntl, errno );
    }

    //--------------------------------------------------------------------------
    // We use send with MSG_NOSIGNAL to avoid SIGPIPEs on Linux, on MacOSX
    // we set SO_NOSIGPIPE option, on Solaris we ignore the SIGPIPE
    //--------------------------------------------------------------------------
#ifdef __APPLE__
    int set = 1;
    Status st = SetSockOpt( SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(int) );
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

    return Status();
  }

  //----------------------------------------------------------------------------
  // Set the socket flags
  //----------------------------------------------------------------------------
  Status Socket::SetFlags( int flags )
  {
    if( pSocket == -1 )
      return Status( stError, errInvalidOp );

    int st = ::fcntl( pSocket, F_SETFL, flags );
    if( st == -1 )
      return Status( stError, errSocketError, errno );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Get the socket flags
  //----------------------------------------------------------------------------
  Status Socket::GetFlags( int &flags )
  {
    if( pSocket == -1 )
      return Status( stError, errInvalidOp );

    int st = ::fcntl( pSocket, F_GETFL, 0 );
    if( st == -1 )
      return Status( stError, errSocketError, errno );
    flags = st;
    return Status();
  }

  //----------------------------------------------------------------------------
  // Get socket options
  //----------------------------------------------------------------------------
  Status Socket::GetSockOpt( int level, int optname, void *optval,
                             socklen_t *optlen )
  {
    if( pSocket == -1 )
      return Status( stError, errInvalidOp );

    if( ::getsockopt( pSocket, level, optname, optval, optlen ) != 0 )
      return Status( stError, errSocketOptError, errno );

    return Status();
  }

  //------------------------------------------------------------------------
  // Set socket options
  //------------------------------------------------------------------------
  Status Socket::SetSockOpt( int level, int optname, const void *optval,
                             socklen_t optlen )
  {
    if( pSocket == -1 )
      return Status( stError, errInvalidOp );

    if( ::setsockopt( pSocket, level, optname, optval, optlen ) != 0 )
      return Status( stError, errSocketOptError, errno );

    return Status();
  }


  //----------------------------------------------------------------------------
  // Connect to the given host name
  //----------------------------------------------------------------------------
  Status Socket::Connect( const std::string &host,
                          uint16_t           port,
                          uint16_t           timeout )
  {
    if( pSocket == -1 || pStatus == Connected || pStatus == Connecting )
      return Status( stError, errInvalidOp );

    std::vector<XrdNetAddr> addrs;
    std::ostringstream o; o << host << ":" << port;
    Status st;

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
  Status Socket::ConnectToAddress( const XrdNetAddr &addr,
                                   uint16_t          timeout )
  {
    if( pSocket == -1 || pStatus == Connected || pStatus == Connecting )
      return Status( stError, errInvalidOp );

    pServerAddr = addr;

    //--------------------------------------------------------------------------
    // Connect
    //--------------------------------------------------------------------------
    int status = XrdNetConnect::Connect( pSocket, pServerAddr.SockAddr(),
                                         pServerAddr.SockSize(), timeout );
    if( status != 0 )
    {
      Status st( stError );

      //------------------------------------------------------------------------
      // If we connect asynchronously this is not really an error
      //------------------------------------------------------------------------
      if( !timeout && status == EINPROGRESS )
      {
        pStatus = Connecting;
        return Status();
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
    return Status();
  }

  //----------------------------------------------------------------------------
  // Disconnect
  //----------------------------------------------------------------------------
  void Socket::Close()
  {
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
  Status Socket::ReadRaw( void *buffer, uint32_t size, int32_t timeout,
                          uint32_t &bytesRead )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return Status( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Some useful variables
    //--------------------------------------------------------------------------
    bytesRead = 0;

    char    *current    = (char *)buffer;
    bool     useTimeout = (timeout!=-1);
    time_t   now        = 0;
    time_t   newNow     = 0;
    Status   sc;

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
          return Status( stError, errSocketDisconnected );
        }

        //----------------------------------------------------------------------
        // Error
        //----------------------------------------------------------------------
        if( (n < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) )
        {
          Close();
          return Status( stError, errSocketError, errno );
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
      return Status( stError, errSocketTimeout );
    return Status( stOK );
  }

  //----------------------------------------------------------------------------
  // Write raw bytes to the socket
  //----------------------------------------------------------------------------
  Status Socket::WriteRaw( void *buffer, uint32_t size, int32_t timeout,
                           uint32_t &bytesWritten )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return Status( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Some useful variables
    //--------------------------------------------------------------------------
    bytesWritten = 0;

    char    *current    = (char *)buffer;
    bool     useTimeout = (timeout!=-1);
    time_t   now        = 0;
    time_t   newNow     = 0;
    Status   sc;

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
          return Status( stError, errSocketError, errno );
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
      return Status( stError, errSocketTimeout );

    return Status( stOK );
  }

  //------------------------------------------------------------------------
  // Portable wrapper around SIGPIPE free send
  //----------------------------------------------------------------------------
  ssize_t Socket::Send( void *buffer, uint32_t size )
  {
    //--------------------------------------------------------------------------
    // We use send with MSG_NOSIGNAL to avoid SIGPIPEs on Linux
    //--------------------------------------------------------------------------
#ifdef __linux__
    return ::send( pSocket, buffer, size, MSG_NOSIGNAL );
#else
    return ::write( pSocket, buffer, size );
#endif
  }

  //------------------------------------------------------------------------
  // Wrapper around writev
  //------------------------------------------------------------------------
  ssize_t Socket::WriteV( iovec *iov, int iovcnt )
  {
    return ::writev( pSocket, iov, iovcnt );
  }

  //----------------------------------------------------------------------------
  // Poll the descriptor
  //----------------------------------------------------------------------------
  Status Socket::Poll( bool readyForReading, bool readyForWriting,
                       int32_t timeout )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return Status( stError, errInvalidOp );

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
        return Status( stError, errPoll, errno );

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
          return Status( stError, errSocketTimeout );
      }
    }
    while( pollRet == -1 );

    //--------------------------------------------------------------------------
    // Check if we have timed out
    //--------------------------------------------------------------------------
    if( pollRet == 0 )
      return Status( stError, errSocketTimeout );

    //--------------------------------------------------------------------------
    // We have some events
    //--------------------------------------------------------------------------
    if( pollDesc.revents & (POLLIN | POLLPRI | POLLOUT) )
      return Status( stOK );

    //--------------------------------------------------------------------------
    // We've been hang up on
    //--------------------------------------------------------------------------
    if( pollDesc.revents & hupEvents )
      return Status( stError, errSocketDisconnected );

    //--------------------------------------------------------------------------
    // We're messed up, either because we messed up ourselves (POLLNVAL) or
    // got messed up by the network (POLLERR)
    //--------------------------------------------------------------------------
    return Status( stError, errSocketError );
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
}
