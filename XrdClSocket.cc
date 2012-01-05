//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdSys/XrdSysDNS.hh"
#include "XrdNet/XrdNetConnect.hh"

#include <iostream>

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Status Socket::Initialize()
  {
    if( pStatus != Uninitialized )
      return Status( stError, errInvalidOp );

    pSocket = ::socket( PF_INET, SOCK_STREAM, 0 );
    if( pSocket < 0 )
    {
      pSocket = -1;
      return Status( stError, errSocketError );
    }

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

    pStatus = Initialized;
    return Status();
  }

  //----------------------------------------------------------------------------
  // Set the socket flags
  //----------------------------------------------------------------------------
  Status Socket::SetFlags( int flags )
  {
    if( pStatus == Uninitialized )
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
    if( pStatus == Uninitialized )
      return Status( stError, errInvalidOp );

    int st = ::fcntl( pSocket, F_GETFL, 0 );
    if( st == -1 )
      return Status( stError, errSocketError, errno );
    flags = st;
  }

  //----------------------------------------------------------------------------
  // Connect to the given URL
  //----------------------------------------------------------------------------
  Status Socket::Connect( const std::string &host,
                          uint16_t           port,
                          uint16_t           timeout )
  {
    if( pStatus != Initialized )
      return Status( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Set up the network connection structures
    //--------------------------------------------------------------------------
    sockaddr_in inetAddr;
    if( XrdSysDNS::getHostAddr( host.c_str(), (sockaddr&)inetAddr ) == 0 )
      return Status( stError, errInvalidAddr );

    inetAddr.sin_port = htons( (unsigned short) port );

    //--------------------------------------------------------------------------
    // Connect
    //--------------------------------------------------------------------------
    int status = XrdNetConnect::Connect( pSocket, (sockaddr *)&inetAddr,
                                         sizeof(inetAddr), timeout );

    if( status != 0 )
    {
      Status st( stError );
      if( status == ETIMEDOUT )
        st.errorType = errSocketTimeout;
      else
        st.errorType = errSocketError;
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
    if( pStatus != Uninitialized )
    {
      close( pSocket );
      pStatus      = Uninitialized;
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
    // Some usefull variables
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
  Status Socket::WriteRaw( void *buffer, uint32_t size, uint16_t timeout,
                           uint32_t &bytesWritten )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( pStatus != Connected )
      return Status( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Some usefull variables
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

  //----------------------------------------------------------------------------
  // Poll the descriptor
  //----------------------------------------------------------------------------
  Status Socket::Poll( bool readyForReading, bool readyForWriting,
                       uint32_t timeout )
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
    // has occured, these may be:
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
    sockaddr  sockAddr;
    socklen_t sockAddrLen = sizeof( sockAddr );
    if( getsockname( pSocket, &sockAddr, &sockAddrLen ) )
      return "";

    XrdSysDNS::IPFormat( &sockAddr, nameBuff, sizeof(nameBuff) );
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
    sockaddr  sockAddr;
    socklen_t sockAddrLen = sizeof( sockAddr );
    if( getpeername( pSocket, &sockAddr, &sockAddrLen ) )
      return "";

    XrdSysDNS::IPFormat( &sockAddr, nameBuff, sizeof(nameBuff) );
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
