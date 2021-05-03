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
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClAsyncWriter.hh"

namespace XrdCl
{
  class Stream;

  //----------------------------------------------------------------------------
  //! Utility class handling asynchronous socket interactions and forwarding
  //! events to the parent stream.
  //----------------------------------------------------------------------------
  class AsyncSocketHandler: public SocketHandler
  {
      //------------------------------------------------------------------------
      // We need an extra task for rescheduling of HS request that received
      // a wait response.
      //------------------------------------------------------------------------
      class WaitTask: public XrdCl::Task
      {
        public:
          WaitTask( XrdCl::AsyncSocketHandler *handler ):
            pHandler( handler )
          {
            std::ostringstream o;
            o << "WaitTask for: 0x" << handler->pHandShakeData->out;
            SetName( o.str() );
          }

          virtual time_t Run( time_t now )
          {
            pHandler->SendHSMsg();
            return 0;
          }

        private:
          XrdCl::AsyncSocketHandler *pHandler;
      };

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AsyncSocketHandler( const URL        &url,
                          Poller           *poller,
                          TransportHandler *transport,
                          AnyObject        *channelData,
                          uint16_t          subStreamNum,
                          Stream           *strm );

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
      XRootDStatus Connect( time_t timeout );

      //------------------------------------------------------------------------
      //! Close the connection
      //------------------------------------------------------------------------
      XRootDStatus Close();

      //------------------------------------------------------------------------
      //! Handle a socket event
      //------------------------------------------------------------------------
      virtual void Event( uint8_t type, XrdCl::Socket */*socket*/ );

      //------------------------------------------------------------------------
      //! Enable uplink
      //------------------------------------------------------------------------
      XRootDStatus EnableUplink()
      {
        if( !pPoller->EnableWriteNotification( pSocket, true, pTimeoutResolution ) )
          return XRootDStatus( stFatal, errPollerError );
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Disable uplink
      //------------------------------------------------------------------------
      XRootDStatus DisableUplink()
      {
        if( !pPoller->EnableWriteNotification( pSocket, false ) )
          return XRootDStatus( stFatal, errPollerError );
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get stream name
      //------------------------------------------------------------------------
      const std::string &GetStreamName()
      {
        return pStreamName;
      }

      //------------------------------------------------------------------------
      //! Get timestamp of last registered socket activity
      //------------------------------------------------------------------------
      time_t GetLastActivity()
      {
        return pLastActivity;
      }

    protected:

      //------------------------------------------------------------------------
      //! Convert Stream object and sub-stream number to stream name
      //------------------------------------------------------------------------
      static std::string ToStreamName( Stream *stream, uint16_t strmnb );

      //------------------------------------------------------------------------
      // Connect returned
      //------------------------------------------------------------------------
      virtual void OnConnectionReturn();

      //------------------------------------------------------------------------
      // Got a write readiness event
      //------------------------------------------------------------------------
      void OnWrite();

      //------------------------------------------------------------------------
      // Got a write readiness event while handshaking
      //------------------------------------------------------------------------
      void OnWriteWhileHandshaking();

      //------------------------------------------------------------------------
      // Write the message and it's signature in one go with writev
      //------------------------------------------------------------------------
      XRootDStatus WriteMessageAndRaw( Message *toWrite, Message *&sign );

      //------------------------------------------------------------------------
      // Write the current message
      //------------------------------------------------------------------------
      XRootDStatus WriteCurrentMessage( Message *toWrite );

      //------------------------------------------------------------------------
      // Got a read readiness event
      //------------------------------------------------------------------------
      void OnRead();

      //------------------------------------------------------------------------
      // Got a read readiness event while handshaking
      //------------------------------------------------------------------------
      void OnReadWhileHandshaking();

      //------------------------------------------------------------------------
      // Handle the handshake message
      //------------------------------------------------------------------------
      void HandleHandShake();

      //------------------------------------------------------------------------
      // Prepare the next step of the hand-shake procedure
      //------------------------------------------------------------------------
      void HandShakeNextStep( bool done );

      //------------------------------------------------------------------------
      // Read a message
      //------------------------------------------------------------------------
      XRootDStatus ReadMessage( Message *&toRead );

      //------------------------------------------------------------------------
      // Handle fault
      //------------------------------------------------------------------------
      void OnFault( XRootDStatus st );

      //------------------------------------------------------------------------
      // Handle fault while handshaking
      //------------------------------------------------------------------------
      void OnFaultWhileHandshaking( XRootDStatus st );

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
      // Handle header corruption in case of kXR_status response
      //------------------------------------------------------------------------
      void OnHeaderCorruption();

      //------------------------------------------------------------------------
      // Carry out the TLS hand-shake
      //
      // The TLS hand-shake is being initiated in HandleHandShake() by calling
      // Socket::TlsHandShake(), however it returns suRetry the TLS hand-shake
      // needs to be followed up by OnTlsHandShake().
      //
      // However, once the TLS connection has been established the server may
      // decide to redo the TLS hand-shake at any time, this operation is handled
      // under the hood by read and write requests and facilitated by
      // Socket::MapEvent()
      //------------------------------------------------------------------------
      XRootDStatus DoTlsHandShake();

      //------------------------------------------------------------------------
      // Handle read/write event if we are in the middle of a TLS hand-shake
      //------------------------------------------------------------------------
      // Handle read/write event if we are in the middle of a TLS hand-shake
      void OnTLSHandShake();

      //------------------------------------------------------------------------
      // Retry hand shake message
      //------------------------------------------------------------------------
      void SendHSMsg();

      //------------------------------------------------------------------------
      // Extract the value of a wait response
      //
      // @param rsp : the server response
      // @return    : if rsp is a wait response then its value
      //              otherwise -1
      //------------------------------------------------------------------------
      inline kXR_int32 HandleWaitRsp( Message *rsp );

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
      Message                       *pHSIncoming;
      Message                       *pOutgoing;
      Message                       *pSignature;
      XrdNetAddr                     pSockAddr;
      HandShakeData                 *pHandShakeData;
      bool                           pHandShakeDone;
      uint16_t                       pTimeoutResolution;
      time_t                         pConnectionStarted;
      time_t                         pConnectionTimeout;
      bool                           pHeaderDone;
      // true means the handler owns the server response
      std::pair<IncomingMsgHandler*, bool> pIncHandler;
      bool                           pOutMsgDone;
      OutgoingMsgHandler            *pOutHandler;
      uint32_t                       pIncMsgSize;
      uint32_t                       pOutMsgSize;
      time_t                         pLastActivity;
      URL                            pUrl;
      bool                           pTlsHandShakeOngoing;
      std::unique_ptr<MsgWriter>     hswriter;
  };
}

#endif // __XRD_CL_ASYNC_SOCKET_HANDLER_HH__
