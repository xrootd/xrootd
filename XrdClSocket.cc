//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Connect to the given URL
  //----------------------------------------------------------------------------
  Status Socket::Connect( const URL &url, uint16_t timeout )
  {
    //--------------------------------------------------------------------------
    // Sanity checks
    //--------------------------------------------------------------------------
    if( pIsConnected )
      return Status( stError, errInvalidOp );

    if( !url.IsValid() )
      return Status( stFatal, errInvalidAddr );

    //--------------------------------------------------------------------------
    // Initialize the socket
    //--------------------------------------------------------------------------
    int sock = ::socket( PF_INET, SOCK_STREAM, 0 );
    if( sock < 0 )
      return Status( stError, errSocketError );
    ScopedDescriptor scopedSock( sock );

    //--------------------------------------------------------------------------
    // Set up the network connection structures
    //--------------------------------------------------------------------------
    sockaddr_in inetAddr;
    if( XrdSysDNS::getHostAddr( url.GetHostName().c_str(),
                                (sockaddr&)inetAddr ) == 0 )
      return Status( stError, errInvalidAddr );

    inetAddr.sin_port = htons( (unsigned short) url.GetPort() );

    //--------------------------------------------------------------------------
    // Connect
    //--------------------------------------------------------------------------
    int status = XrdNetConnect::Connect( sock, (sockaddr *)&inetAddr,
                                         sizeof(inetAddr),
                                         timeout );

    if( status != 0 )
    {
      Status st( stError );
      if( status == ETIMEDOUT )
        st.errorType = errSocketTimeout;
      else
        st.errorType = errSocketError;
      st.errNo = status;
      return st;
    }

    //--------------------------------------------------------------------------
    // Make the socket non blocking
    //--------------------------------------------------------------------------
    int flags;
    if( (flags = ::fcntl( sock, F_GETFL, 0 )) == -1 )
      flags = 0;
    if( ::fcntl( sock, F_SETFL, flags | O_NONBLOCK ) == -1 )
      return Status( stError, errFcntl, errno );

    //--------------------------------------------------------------------------
    // Connected
    //--------------------------------------------------------------------------
    pIsConnected = true;
    pSocket      = scopedSock.Release();

    return Status();
  }

  //----------------------------------------------------------------------------
  // Disconnect
  //----------------------------------------------------------------------------
  void Socket::Disconnect()
  {
    if( pIsConnected )
    {
      close( pSocket );
      pIsConnected = false;
      pSocket      = -1;
    }
  }

  //----------------------------------------------------------------------------
  //! Read raw bytes from the socket
  //----------------------------------------------------------------------------
  Status Socket::ReadRaw( char *buffer, uint32_t size, int32_t timeout,
                          uint32_t &bytesRead )
  {
    //--------------------------------------------------------------------------
    // Check if we're connected
    //--------------------------------------------------------------------------
    if( !pIsConnected )
      return Status( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // Some usefull variables
    //--------------------------------------------------------------------------
    bytesRead = 0;

    pollfd   pollDesc;
    int      pollRet;
    char    *current    = buffer;
    bool     useTimeout = (timeout!=-1);
    time_t   now        = 0;
    time_t   newNow     = 0;

    if( useTimeout )
      now = ::time(0);

    pollDesc.fd     = pSocket;
    pollDesc.events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL | POLLRDHUP;

    //--------------------------------------------------------------------------
    // Repeat the following until we have read all the requested data
    //--------------------------------------------------------------------------
    while ( bytesRead < size )
    {
      //------------------------------------------------------------------------
      // We loop on poll because it may return -1 even thought no fatal error
      // has occured, these may be:
      // * a signal interrupting the execution (errno == EINTR)
      // * a failure to initialize some internal structures (Solaris only)
      //   (errno EAGAIN)
      //------------------------------------------------------------------------
      do
      {
        pollRet = poll( &pollDesc, 1, (useTimeout ? timeout*1000 : -1) );
        if( (pollRet < 0) && (errno != EINTR) && (errno != EAGAIN) )
          return Status( stError, errPoll, errno );

        //----------------------------------------------------------------------
        // Check if we did not time out in the case where we are not supposed
        // to wait indefinitely
        //----------------------------------------------------------------------
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

      //------------------------------------------------------------------------
      // Check if we have timed out
      //------------------------------------------------------------------------
      if( pollRet == 0 )
        return Status( stError, errSocketTimeout );

      //------------------------------------------------------------------------
      // It looks like we've got an event. Let's check if we can read something.
      //------------------------------------------------------------------------
      if( pollDesc.revents & (POLLIN | POLLPRI) )
      {
        int n = ::read( pSocket, current, (size-bytesRead) );

        //----------------------------------------------------------------------
        // We got a close here - this means that there is no more data in
        // the buffer so we disconnect
        //----------------------------------------------------------------------
        if( n == 0 )
        {
          Disconnect();
          return Status( stError, errSocketDisconnected );
        }

        //----------------------------------------------------------------------
        // Error
        //----------------------------------------------------------------------
        if( (n < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) )
        {
          Disconnect();
          return Status( stError, errSocketError, errno );
        }

        if( n > 0 )
        {
          bytesRead += n;
          current   += n;
        }
      }

      //------------------------------------------------------------------------
      // Nothing to read this time, let's check for errors
      //------------------------------------------------------------------------
      else
      {
        //----------------------------------------------------------------------
        // We've been hang up on
        //----------------------------------------------------------------------
        if( pollDesc.revents & (POLLHUP|POLLRDHUP) )
        {
          Disconnect();
          return Status( stError, errSocketDisconnected );
        }

        //----------------------------------------------------------------------
        // We're messed up, either because we messed up ourselves (POLLNVAL) or
        // got messed up by the network (POLLERR)
        //----------------------------------------------------------------------
        if( pollDesc.revents & (POLLNVAL|POLLERR) )
        {
          Disconnect();
          return Status( stError, errSocketError );
        }
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
  Status Socket::WriteRaw( char *buffer, uint32_t size, uint16_t timeout,
                           uint32_t &bytesWritten )
  {
    return Status( stOK );
  }
}
