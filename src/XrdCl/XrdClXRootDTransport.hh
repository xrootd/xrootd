//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_XROOTD_TRANSPORT_HH__
#define __XRD_CL_XROOTD_TRANSPORT_HH__

#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdOuc/XrdOucEnv.hh"

class XrdSysPlugin;
class XrdSecProtect;

namespace XrdCl
{
  class Tls;
  class Socket;
  struct XRootDChannelInfo;
  struct PluginUnloadHandler;

  //----------------------------------------------------------------------------
  //! XRootD transport handler
  //----------------------------------------------------------------------------
  class XRootDTransport: public TransportHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDTransport();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~XRootDTransport();

      //------------------------------------------------------------------------
      //! Read a message header from the socket, the socket is non-blocking,
      //! so if there is not enough data the function should return suRetry
      //! in which case it will be called again when more data arrives, with
      //! the data previously read stored in the message buffer
      //!
      //! @param message the message buffer
      //! @param socket  the socket
      //! @return        stOK & suDone if the whole message has been processed
      //!                stOK & suRetry if more data is needed
      //!                stError on failure
      //------------------------------------------------------------------------
      virtual XRootDStatus GetHeader( Message &message, Socket *socket );

      //------------------------------------------------------------------------
      //! Read the message body from the socket, the socket is non-blocking,
      //! the method may be called multiple times - see GetHeader for details
      //!
      //! @param message the message buffer containing the header
      //! @param socket  the socket
      //! @return        stOK & suDone if the whole message has been processed
      //!                stOK & suRetry if more data is needed
      //!                stError on failure
      //------------------------------------------------------------------------
      virtual XRootDStatus GetBody( Message &message, Socket *socket );

      //------------------------------------------------------------------------
      //! Read more of the message body from the socket, the socket is
      //! non-blocking the method may be called multiple times - see GetHeader
      //! for details
      //!
      //! @param message the message buffer containing the header
      //! @param socket  the socket
      //! @return        stOK & suDone if the whole message has been processed
      //!                stOK & suRetry if more data is needed
      //!                stError on failure
      //------------------------------------------------------------------------
      virtual XRootDStatus GetMore( Message &message, Socket *socket );

      //------------------------------------------------------------------------
      //! Initialize channel
      //------------------------------------------------------------------------
      virtual void InitializeChannel( const URL  &url,
                                      AnyObject  &channelData );

      //------------------------------------------------------------------------
      //! Finalize channel
      //------------------------------------------------------------------------
      virtual void FinalizeChannel( AnyObject &channelData );

      //------------------------------------------------------------------------
      //! HandShake
      //------------------------------------------------------------------------
      virtual XRootDStatus HandShake( HandShakeData *handShakeData,
                                      AnyObject     &channelData );

      //------------------------------------------------------------------------
      // @return true if handshake has been done and stream is connected,
      //         false otherwise
      //------------------------------------------------------------------------
      virtual bool HandShakeDone( HandShakeData *handShakeData,
                                  AnyObject     &channelData );

      //------------------------------------------------------------------------
      //! Check if the stream should be disconnected
      //------------------------------------------------------------------------
      virtual bool IsStreamTTLElapsed( time_t     time,
                                       AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Check the stream is broken - ie. TCP connection got broken and
      //! went undetected by the TCP stack
      //------------------------------------------------------------------------
      virtual Status IsStreamBroken( time_t     inactiveTime,
                                     AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Return the ID for the up stream this message should be sent by
      //! and the down stream which the answer should be expected at.
      //! Modify the message itself if necessary.
      //! If hint is non-zero then the message should be modified such that
      //! the answer will be returned via the hinted stream.
      //------------------------------------------------------------------------
      virtual PathID Multiplex( Message   *msg,
                                AnyObject &channelData,
                                PathID    *hint = 0 );

      //------------------------------------------------------------------------
      //! Return the ID for the up substream this message should be sent by
      //! and the down substream which the answer should be expected at.
      //! Modify the message itself if necessary.
      //! If hint is non-zero then the message should be modified such that
      //! the answer will be returned via the hinted stream.
      //------------------------------------------------------------------------
      virtual PathID MultiplexSubStream( Message   *msg,
                                         AnyObject &channelData,
                                         PathID    *hint = 0 );

      //------------------------------------------------------------------------
      //! Return a number of substreams per stream that should be created
      //------------------------------------------------------------------------
      virtual uint16_t SubStreamNumber( AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Return the information whether a control connection needs to be
      //! valid before establishing other connections
      //------------------------------------------------------------------------
      virtual bool NeedControlConnection()
      {
        return true;
      }

      //------------------------------------------------------------------------
      //! Marshal the outgoing message
      //------------------------------------------------------------------------
      inline static XRootDStatus MarshallRequest( Message *msg )
      {
        MarshallRequest( msg->GetBuffer() );
        msg->SetIsMarshalled( true );
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Marshal the outgoing message
      //------------------------------------------------------------------------
      static XRootDStatus MarshallRequest( char *msg );

      //------------------------------------------------------------------------
      //! Unmarshall the request - sometimes the requests need to be rewritten,
      //! so we need to unmarshall them
      //------------------------------------------------------------------------
      static XRootDStatus UnMarshallRequest( Message *msg );

      //------------------------------------------------------------------------
      //! Unmarshall the body of the incoming message
      //------------------------------------------------------------------------
      static XRootDStatus UnMarshallBody( Message *msg, uint16_t reqType );

      //------------------------------------------------------------------------
      //! Unmarshall the body of the status response
      //------------------------------------------------------------------------
      static XRootDStatus UnMarshalStatusBody( Message &msg, uint16_t reqType );

      //------------------------------------------------------------------------
      //! Unmarshall the correction-segment of the status response for pgwrite
      //------------------------------------------------------------------------
      static XRootDStatus UnMarchalStatusMore( Message &msg );

      //------------------------------------------------------------------------
      //! Unmarshall the header incoming message
      //------------------------------------------------------------------------
      static void UnMarshallHeader( Message &msg );

      //------------------------------------------------------------------------
      //! Log server error response
      //------------------------------------------------------------------------
      static void LogErrorResponse( const Message &msg );

      //------------------------------------------------------------------------
      //! Number of currently connected data streams
      //------------------------------------------------------------------------
      static uint16_t NbConnectedStrm( AnyObject &channelData );

      //------------------------------------------------------------------------
      //! The stream has been disconnected, do the cleanups
      //------------------------------------------------------------------------
      virtual void Disconnect( AnyObject &channelData,
                               uint16_t   subStreamId );

      //------------------------------------------------------------------------
      //! Query the channel
      //------------------------------------------------------------------------
      virtual Status Query( uint16_t   query,
                            AnyObject &result,
                            AnyObject &channelData );


      //------------------------------------------------------------------------
      //! Get the description of a message
      //------------------------------------------------------------------------
      static void GenerateDescription( char *msg, std::ostringstream &o );

      //------------------------------------------------------------------------
      //! Get the description of a message
      //------------------------------------------------------------------------
      inline static void SetDescription( Message *msg )
      {
        std::ostringstream o;
        GenerateDescription( msg->GetBuffer(), o );
        msg->SetDescription( o.str() );
      }

      //------------------------------------------------------------------------
      //! Check if the message invokes a stream action
      //------------------------------------------------------------------------
      virtual uint32_t MessageReceived( Message   &msg,
                                        uint16_t   subStream,
                                        AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Notify the transport about a message having been sent
      //------------------------------------------------------------------------
      virtual void MessageSent( Message   *msg,
                                uint16_t   subStream,
                                uint32_t   bytesSent,
                                AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Get signature for given message
      //------------------------------------------------------------------------
      virtual Status GetSignature( Message *toSign, Message *&sign,
                                   AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Get signature for given message
      //------------------------------------------------------------------------
      virtual Status GetSignature( Message *toSign, Message *&sign,
                                   XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      //! Decrement file object instance count bound to this channel
      //------------------------------------------------------------------------
      virtual void DecFileInstCnt( AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Wait until the program can safely exit
      //------------------------------------------------------------------------
      virtual void WaitBeforeExit();

      //------------------------------------------------------------------------
      //! @return : true if encryption should be turned on, false otherwise
      //------------------------------------------------------------------------
      virtual bool NeedEncryption( HandShakeData  *handShakeData,
                                  AnyObject      &channelData );

      //------------------------------------------------------------------------
      //! Get bind preference for the next data stream
      //------------------------------------------------------------------------
      virtual URL GetBindPreference( const URL  &url,
                                     AnyObject  &channelData );

    private:

      //------------------------------------------------------------------------
      // Hand shake the main stream
      //------------------------------------------------------------------------
      XRootDStatus HandShakeMain( HandShakeData *handShakeData,
                            AnyObject     &channelData );

      //------------------------------------------------------------------------
      // Hand shake a parallel stream
      //------------------------------------------------------------------------
      XRootDStatus HandShakeParallel( HandShakeData *handShakeData,
                                AnyObject     &channelData );

      //------------------------------------------------------------------------
      // Generate the message to be sent as an initial handshake
      // (handshake + kXR_protocol)
      //------------------------------------------------------------------------
      Message *GenerateInitialHSProtocol( HandShakeData     *hsData,
                                          XRootDChannelInfo *info,
                                          kXR_char           expect );

      //------------------------------------------------------------------------
      // Generate the protocol message
      //------------------------------------------------------------------------
      Message *GenerateProtocol( HandShakeData     *hsData,
                                 XRootDChannelInfo *info,
                                 kXR_char           expect );

      //------------------------------------------------------------------------
      // Initialize protocol request
      //------------------------------------------------------------------------
      void InitProtocolReq( ClientProtocolRequest *request,
                            XRootDChannelInfo     *info,
                            kXR_char               expect );

      //------------------------------------------------------------------------
      // Process the server initial handshake response
      //------------------------------------------------------------------------
      XRootDStatus ProcessServerHS( HandShakeData     *hsData,
                                    XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Process the protocol response
      //------------------------------------------------------------------------
      XRootDStatus ProcessProtocolResp( HandShakeData     *hsData,
                                        XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Process the protocol body:
      //  * 'B' : bind preferences
      //  * 'S' : security requirements
      //------------------------------------------------------------------------
      XRootDStatus ProcessProtocolBody( char              *bodybuff,
                                        size_t             bodysize,
                                        XRootDChannelInfo *info  );

      //------------------------------------------------------------------------
      // Generate the bind message
      //------------------------------------------------------------------------
      Message *GenerateBind( HandShakeData     *hsData,
                             XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Generate the bind message
      //------------------------------------------------------------------------
      XRootDStatus ProcessBindResp( HandShakeData     *hsData,
                                    XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Generate the login  message
      //------------------------------------------------------------------------
      Message *GenerateLogIn( HandShakeData     *hsData,
                              XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Process the login response
      //------------------------------------------------------------------------
      XRootDStatus ProcessLogInResp( HandShakeData     *hsData,
                               XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Do the authentication
      //------------------------------------------------------------------------
      XRootDStatus DoAuthentication( HandShakeData     *hsData,
                               XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Get the initial credentials using one of the protocols
      //------------------------------------------------------------------------
      XRootDStatus GetCredentials( XrdSecCredentials *&credentials,
                                   HandShakeData      *hsData,
                                   XRootDChannelInfo  *info );

      //------------------------------------------------------------------------
      // Clean up the data structures created for the authentication process
      //------------------------------------------------------------------------
      Status CleanUpAuthentication( XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Clean up the data structures created for the protection purposes
      //------------------------------------------------------------------------
      Status CleanUpProtection( XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Get the authentication function handle
      //------------------------------------------------------------------------
      XrdSecGetProt_t GetAuthHandler();

      //------------------------------------------------------------------------
      // Generate the end session message
      //------------------------------------------------------------------------
      Message *GenerateEndSession( HandShakeData     *hsData,
                                   XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Process the end session response
      //------------------------------------------------------------------------
      Status ProcessEndSessionResp( HandShakeData     *hsData,
                                    XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Get a string representation of the server flags
      //------------------------------------------------------------------------
      static std::string ServerFlagsToStr( uint32_t flags );

      //------------------------------------------------------------------------
      // Get a string representation of file handle
      //------------------------------------------------------------------------
      static std::string FileHandleToStr( const unsigned char handle[4] );

      friend struct PluginUnloadHandler;
      PluginUnloadHandler *pSecUnloadHandler;
  };
}

#endif // __XRD_CL_XROOTD_TRANSPORT_HANDLER_HH__
