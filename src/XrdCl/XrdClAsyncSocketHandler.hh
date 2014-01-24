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

#ifndef __XRD_CL_ASYNC_SOCKET_HANDLER_HH__
#define __XRD_CL_ASYNC_SOCKET_HANDLER_HH__

#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

#include <sys/types.h>
#include <sys/socket.h>

namespace XrdCl
{
  class Stream;

  //----------------------------------------------------------------------------
  //! Utility class handling asynchronous socket interactions and forwarding
  //! events to the parent stream.
  //----------------------------------------------------------------------------
  class AsyncSocketHandler: public SocketHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AsyncSocketHandler( Poller           *poller,
                          TransportHandler *transport,
                          AnyObject        *channelData,
                          uint16_t          subStreamNum );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~AsyncSocketHandler();

      //------------------------------------------------------------------------
      //! Set address
      //------------------------------------------------------------------------
      void SetAddress( const XrdNetAddr &address )
      {
        pSockAddr = address;
      }

      //------------------------------------------------------------------------
      //! Get the address that the socket is connected to
      //------------------------------------------------------------------------
      const XrdNetAddr &GetAddress() const
      {
        return pSockAddr;
      }

      //------------------------------------------------------------------------
      //! Connect to the currently set address
      //------------------------------------------------------------------------
      Status Connect( time_t timeout );

      //------------------------------------------------------------------------
      //! Close the connection
      //------------------------------------------------------------------------
      Status Close();

      //------------------------------------------------------------------------
      //! Set a stream object to be notified about the status of the operations
      //------------------------------------------------------------------------
      void SetStream( Stream *stream );

      //------------------------------------------------------------------------
      //! Handle a socket event
      //------------------------------------------------------------------------
      virtual void Event( uint8_t type, XrdCl::Socket */*socket*/ );

      //------------------------------------------------------------------------
      //! Enable uplink
      //------------------------------------------------------------------------
      Status EnableUplink()
      {
        if( !pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ) )
          return Status( stFatal, errPollerError );
        return Status();
      }

      //------------------------------------------------------------------------
      //! Disable uplink
      //------------------------------------------------------------------------
      Status DisableUplink()
      {
        if( !pPoller->EnableWriteNotification( pSocket, false ) )
          return Status( stFatal, errPollerError );
        return Status();
      }

      //------------------------------------------------------------------------
      //! Get stream name
      //------------------------------------------------------------------------
      const std::string &GetStreamName()
      {
        return pStreamName;
      }

      //------------------------------------------------------------------------
      //! Set socket domain
      //------------------------------------------------------------------------
      void SetSocketDomain( int domain )
      {
        pSocketDomain = domain;
      }

      //------------------------------------------------------------------------
      //! Get timestamp of last registered socket activity
      //------------------------------------------------------------------------
      time_t GetLastActivity()
      {
        return pLastActivity;
      }

    private:

      //------------------------------------------------------------------------
      // Connect returned
      //------------------------------------------------------------------------
      void OnConnectionReturn();

      //------------------------------------------------------------------------
      // Got a write readiness event
      //------------------------------------------------------------------------
      void OnWrite();

      //------------------------------------------------------------------------
      // Got a write readiness event while handshaking
      //------------------------------------------------------------------------
      void OnWriteWhileHandshaking();

      //------------------------------------------------------------------------
      // Write the current message
      //------------------------------------------------------------------------
      Status WriteCurrentMessage();

      //------------------------------------------------------------------------
      // Got a read readiness event
      //------------------------------------------------------------------------
      void OnRead();

      //------------------------------------------------------------------------
      // Got a read readiness event while handshaking
      //------------------------------------------------------------------------
      void OnReadWhileHandshaking();

      //------------------------------------------------------------------------
      // Read a message
      //------------------------------------------------------------------------
      Status ReadMessage();

      //------------------------------------------------------------------------
      // Handle fault
      //------------------------------------------------------------------------
      void OnFault( Status st );

      //------------------------------------------------------------------------
      // Handle fault while handshaking
      //------------------------------------------------------------------------
      void OnFaultWhileHandshaking( Status st );

      //------------------------------------------------------------------------
      // Handle write timeout event
      //------------------------------------------------------------------------
      void OnWriteTimeout();

      //------------------------------------------------------------------------
      // Handle read timeout event
      //------------------------------------------------------------------------
      void OnReadTimeout();

      //------------------------------------------------------------------------
      // Handle timeout event while handshaking
      //------------------------------------------------------------------------
      void OnTimeoutWhileHandshaking();

      //------------------------------------------------------------------------
      // Data members
      //------------------------------------------------------------------------
      Poller                        *pPoller;
      TransportHandler              *pTransport;
      AnyObject                     *pChannelData;
      uint16_t                       pSubStreamNum;
      Stream                        *pStream;
      std::string                    pStreamName;
      Socket                        *pSocket;
      Message                       *pIncoming;
      Message                       *pOutgoing;
      XrdNetAddr                     pSockAddr;
      HandShakeData                 *pHandShakeData;
      bool                           pHandShakeDone;
      uint16_t                       pTimeoutResolution;
      time_t                         pConnectionStarted;
      time_t                         pConnectionTimeout;
      bool                           pHeaderDone;
      std::pair<IncomingMsgHandler*, bool> pIncHandler;
      bool                           pOutMsgDone;
      OutgoingMsgHandler            *pOutHandler;
      uint32_t                       pIncMsgSize;
      uint32_t                       pOutMsgSize;
      int                            pSocketDomain;
      time_t                         pLastActivity;
  };
}

#endif // __XRD_CL_ASYNC_SOCKET_HANDLER_HH__
