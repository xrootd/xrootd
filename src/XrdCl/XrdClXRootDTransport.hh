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
#include "XProtocol/XProtocol.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdOuc/XrdOucEnv.hh"

namespace XrdCl
{
  struct XRootDChannelInfo;

  //----------------------------------------------------------------------------
  //! XRootD related protocol queries
  //----------------------------------------------------------------------------
  struct XRootDQuery
  {
    static const uint16_t SIDManager      = 1001; //!< returns the SIDManager object
    static const uint16_t ServerFlags     = 1002; //!< returns server flags
    static const uint16_t ProtocolVersion = 1003; //!< returns the protocol version
  };

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
      //! so if there is not enough data the function should return errRetry
      //! in which case it will be called again when more data arrives, with
      //! the data previously read stored in the message buffer
      //!
      //! @param message the message buffer
      //! @param socket  the socket
      //! @return        stOK & suDone if the whole message has been processed
      //!                stOK & suRetry if more data is needed
      //!                stError on failure
      //------------------------------------------------------------------------
      virtual Status GetHeader( Message *message, int socket );

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
      virtual Status GetBody( Message *message, int socket );

      //------------------------------------------------------------------------
      //! Initialize channel
      //------------------------------------------------------------------------
      virtual void InitializeChannel( AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Finalize channel
      //------------------------------------------------------------------------
      virtual void FinalizeChannel( AnyObject &channelData );

      //------------------------------------------------------------------------
      //! HandShake
      //------------------------------------------------------------------------
      virtual Status HandShake( HandShakeData *handShakeData,
                                AnyObject     &channelData );

      //------------------------------------------------------------------------
      //! Check if the stream should be disconnected
      //------------------------------------------------------------------------
      virtual bool IsStreamTTLElapsed( time_t     time,
                                       uint16_t   streamId,
                                       AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Check the stream is broken - ie. TCP connection got broken and
      //! went undetected by the TCP stack
      //------------------------------------------------------------------------
      virtual Status IsStreamBroken( time_t     inactiveTime,
                                     uint16_t   streamId,
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
                                         uint16_t   streamId,
                                         AnyObject &channelData,
                                         PathID    *hint = 0 );

      //------------------------------------------------------------------------
      //! Return a number of streams that should be created
      //------------------------------------------------------------------------
      virtual uint16_t StreamNumber( AnyObject &channelData );

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
      static Status MarshallRequest( Message *msg );

      //------------------------------------------------------------------------
      //! Unmarshall the request - sometimes the requests need to be rewritten,
      //! so we need to unmarshall them
      //------------------------------------------------------------------------
      static Status UnMarshallRequest( Message *msg );

      //------------------------------------------------------------------------
      //! Unmarshall the body of the incoming message
      //------------------------------------------------------------------------
      static Status UnMarshallBody( Message *msg, uint16_t reqType );

      //------------------------------------------------------------------------
      //! Unmarshall the header incoming message
      //------------------------------------------------------------------------
      static void UnMarshallHeader( Message *msg );

      //------------------------------------------------------------------------
      //! Log server error response
      //------------------------------------------------------------------------
      static void LogErrorResponse( const Message &msg );

      //------------------------------------------------------------------------
      //! The stream has been disconnected, do the cleanups
      //------------------------------------------------------------------------
      virtual void Disconnect( AnyObject &channelData,
                               uint16_t   streamId,
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
      static void SetDescription( Message *msg );

      //------------------------------------------------------------------------
      //! Check if the message invokes a stream action
      //------------------------------------------------------------------------
      virtual uint32_t MessageReceived( Message   *msg,
                                        uint16_t   streamId,
                                        uint16_t   subStream,
                                        AnyObject &channelData );

      //------------------------------------------------------------------------
      //! Notify the transport about a message having been sent
      //------------------------------------------------------------------------
      virtual void MessageSent( Message   *msg,
                                uint16_t   streamId,
                                uint16_t   subStream,
                                uint32_t   bytesSent,
                                AnyObject &channelData );

    private:

      //------------------------------------------------------------------------
      // Hand shake the main stream
      //------------------------------------------------------------------------
      Status HandShakeMain( HandShakeData *handShakeData,
                            AnyObject     &channelData );

      //------------------------------------------------------------------------
      // Hand shake a parallel stream
      //------------------------------------------------------------------------
      Status HandShakeParallel( HandShakeData *handShakeData,
                                AnyObject     &channelData );

      //------------------------------------------------------------------------
      // Generate the message to be sent as an initial handshake
      //------------------------------------------------------------------------
      Message *GenerateInitialHS( HandShakeData     *hsData,
                                  XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Generate the message to be sent as an initial handshake
      // (handshake + kXR_protocol)
      //------------------------------------------------------------------------
      Message *GenerateInitialHSProtocol( HandShakeData     *hsData,
                                          XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Process the server initial handshake response
      //------------------------------------------------------------------------
      Status ProcessServerHS( HandShakeData     *hsData,
                              XRootDChannelInfo *info );

      //-----------------------------------------------------------------------
      // Process the protocol response
      //------------------------------------------------------------------------
      Status ProcessProtocolResp( HandShakeData     *hsData,
                                  XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Generate the bind message
      //------------------------------------------------------------------------
      Message *GenerateBind( HandShakeData     *hsData,
                             XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Generate the bind message
      //------------------------------------------------------------------------
      Status ProcessBindResp( HandShakeData     *hsData,
                              XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Generate the login  message
      //------------------------------------------------------------------------
      Message *GenerateLogIn( HandShakeData     *hsData,
                              XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Process the login response
      //------------------------------------------------------------------------
      Status ProcessLogInResp( HandShakeData     *hsData,
                               XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Do the authentication
      //------------------------------------------------------------------------
      Status DoAuthentication( HandShakeData     *hsData,
                               XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Get the initial credentials using one of the protocols
      //------------------------------------------------------------------------
      Status GetCredentials( XrdSecCredentials *&credentials,
                             HandShakeData      *hsData,
                             XRootDChannelInfo  *info );

      //------------------------------------------------------------------------
      // Clean up the data structures created for the authentication process
      //------------------------------------------------------------------------
      Status CleanUpAuthentication( XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      // Get the authentication function handle
      //------------------------------------------------------------------------

      XrdSecGetProt_t GetAuthHandler();

      //------------------------------------------------------------------------
      // Get a string representation of the server flags
      //------------------------------------------------------------------------
      static std::string ServerFlagsToStr( uint32_t flags );

      //------------------------------------------------------------------------
      // Get a string representation of file handle
      //------------------------------------------------------------------------
      static std::string FileHandleToStr( const unsigned char handle[4] );

      void            *pSecLibHandle;
      XrdSecGetProt_t  pAuthHandler;
  };
}

#endif // __XRD_CL_XROOTD_TRANSPORT_HANDLER_HH__
