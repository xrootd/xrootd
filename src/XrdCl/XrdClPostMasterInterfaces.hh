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

#ifndef __XRD_CL_POST_MASTER_INTERFACES_HH__
#define __XRD_CL_POST_MASTER_INTERFACES_HH__

#include "XrdCl/XrdClXRootDResponses.hh"

#include <cstdint>
#include <ctime>
#include <memory>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClURL.hh"

class XrdNetAddr;

namespace XrdCl
{
  class Channel;
  class Message;
  class URL;
  class Socket;

  //----------------------------------------------------------------------------
  //! Message handler
  //----------------------------------------------------------------------------
  class MsgHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Actions to be taken after a message is processed by the handler
      //------------------------------------------------------------------------
      enum Action
      {
        None          = 0x0000,
        Nop           = 0x0001,    //!< A place holder
        Ignore        = 0x0002,    //!< Ignore the message
        RemoveHandler = 0x0004,    //!< Remove the handler from the notification
                                   //!< list
        Raw           = 0x0008,    //!< the handler is interested in reading
                                   //!< the message body directly from the
                                   //!< socket
        NoProcess     = 0x0010,    //!< don't call the processing callback
                                   //!< even if the message belongs to this
                                   //!< handler
        Corrupted     = 0x0020,    //!< the handler discovered that the message
                                   //!< header is corrupted, we will have to
                                   //!< tear down the socket
        More          = 0x0040     //!< there are more (non-raw) data to be read
      };

      //------------------------------------------------------------------------
      //! Events that may have occurred to the stream
      //------------------------------------------------------------------------
      enum StreamEvent
      {
        Ready      = 1, //!< The stream has become connected
        Broken     = 2, //!< The stream is broken
        Timeout    = 3, //!< The declared timeout has occurred
        FatalError = 4  //!< Stream has been broken and won't be recovered
      };

      //------------------------------------------------------------------------
      //! Event types that the message handler may receive
      //------------------------------------------------------------------------

      virtual ~MsgHandler() {}

      //------------------------------------------------------------------------
      //! Examine an incoming message, and decide on the action to be taken
      //!
      //! @param msg    the message, may be zero if receive failed
      //! @return       action type that needs to be take wrt the message and
      //!               the handler
      //------------------------------------------------------------------------
      virtual uint16_t Examine( std::shared_ptr<Message> &msg ) = 0;

      //------------------------------------------------------------------------
      //! Reexamine the incoming message, and decide on the action to be taken
      //!
      //! In case of kXR_status the message can be only fully examined after
      //! reading the whole body (without raw data).
      //!
      //! @param msg    the message, may be zero if receive failed
      //! @return       action type that needs to be take wrt the message and
      //!               the handler
      //------------------------------------------------------------------------
      virtual uint16_t InspectStatusRsp() = 0;

      //------------------------------------------------------------------------
      //! Get handler sid
      //!
      //! return sid of the corresponding request, otherwise 0
      //------------------------------------------------------------------------
      virtual uint16_t GetSid() const = 0;

      //------------------------------------------------------------------------
      //! Process the message if it was "taken" by the examine action
      //!
      //! @param msg the message to be processed
      //------------------------------------------------------------------------
      virtual void Process() {};

      //------------------------------------------------------------------------
      //! Read message body directly from a socket - called if Examine returns
      //! Raw flag - only socket related errors may be returned here
      //!
      //! @param msg       the corresponding message header
      //! @param socket    the socket to read from
      //! @param bytesRead number of bytes read by the method
      //! @return          stOK & suDone if the whole body has been processed
      //!                  stOK & suRetry if more data is needed
      //!                  stError on failure
      //------------------------------------------------------------------------
      virtual XRootDStatus ReadMessageBody( Message  *msg,
                                            Socket   *socket,
                                            uint32_t &bytesRead )
      {
        (void)msg; (void)socket; (void)bytesRead;
        return Status( stOK, suDone );
      };

      //------------------------------------------------------------------------
      //! Handle an event other that a message arrival
      //!
      //! @param event     type of the event
      //! @param status    status info
      //! @return          Action::RemoveHandler or 0
      //------------------------------------------------------------------------
      virtual uint8_t OnStreamEvent( StreamEvent   event,
                                     XRootDStatus  status )
      {
        (void)event; (void)status;
        return 0;
      };

      //------------------------------------------------------------------------
      //! The requested action has been performed and the status is available
      //------------------------------------------------------------------------
      virtual void OnStatusReady( const Message *message,
                                  XRootDStatus   status ) = 0;

      //------------------------------------------------------------------------
      //! Called just before the message is going to be sent through
      //! a valid connection, so that the user can still make some
      //! modifications that were impossible before (ie. protocol version
      //! dependent adjustments)
      //!
      //! @param msg       message concerned
      //------------------------------------------------------------------------
      virtual void OnReadyToSend( Message *msg )
      {
        (void)msg;
      };

      //------------------------------------------------------------------------
      //! Determines whether the handler wants to write some data directly
      //! to the socket after the message (or message header) has been sent,
      //! WriteMessageBody will be called
      //------------------------------------------------------------------------
      virtual bool IsRaw() const { return false; }

      //------------------------------------------------------------------------
      //! Write message body directly to a socket - called if IsRaw returns
      //! true - only socket related errors may be returned here
      //!
      //! @param socket    the socket to read from
      //! @param bytesWritten number of bytes written by the method
      //! @return          stOK & suDone if the whole body has been processed
      //!                  stOK & suRetry if more data needs to be written
      //!                  stError on failure
      //------------------------------------------------------------------------
      virtual XRootDStatus WriteMessageBody( Socket   *socket,
                                             uint32_t &bytesWritten )
      {
        (void)socket; (void)bytesWritten;
        return Status();
      }

      virtual time_t GetExpiration() = 0;
  };

  //----------------------------------------------------------------------------
  //! Channel event handler
  //----------------------------------------------------------------------------
  class ChannelEventHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Events that may have occurred to the channel
      //------------------------------------------------------------------------
      enum ChannelEvent
      {
        StreamReady  = 1, //!< The stream has become connected
        StreamBroken = 2, //!< The stream is broken
        FatalError   = 4  //!< Stream has been broken and won't be recovered
      };

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~ChannelEventHandler() {};

      //------------------------------------------------------------------------
      //! Event callback
      //!
      //! @param event   the event that has occurred
      //! @param status  the status info
      //! @return true if the handler should be kept
      //!         false if it should be removed from further consideration
      //------------------------------------------------------------------------
      virtual bool OnChannelEvent( ChannelEvent event,
                                   Status       status ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Data structure that carries the handshake information
  //----------------------------------------------------------------------------

  struct HandShakeData
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    HandShakeData( const URL *addr, uint16_t subStream ):
      step(0), out(0), in(0), url(addr), subStreamId( subStream ),
      startTime( time(0) ), serverAddr(0)
    {}
    uint16_t     step;           //!< Handshake step
    Message     *out;            //!< Message to be sent out
    Message     *in;             //!< Message that has been received
    const URL   *url;            //!< Destination URL
    uint16_t     subStreamId;    //!< Sub-stream id
    time_t       startTime;      //!< Timestamp of when the handshake started
    const
    XrdNetAddr  *serverAddr;     //!< Server address
    std::string  clientName;     //!< Client name (an IPv6 representation)
    std::string  streamName;     //!< Name of the stream
  };

  //----------------------------------------------------------------------------
  //! Path ID - a pair of integers describing the up and down stream
  //! for given interaction
  //----------------------------------------------------------------------------
  struct PathID
  {
    PathID( uint16_t u = 0, uint16_t d = 0 ): up(u), down(d) {}
    uint16_t up;
    uint16_t down;
  };

  //----------------------------------------------------------------------------
  //! Transport query definitions
  //! The transports may support other queries, with ids > 1000
  //----------------------------------------------------------------------------
  struct TransportQuery
  {
    static const uint16_t Name    = 1; //!< Transport name, returns const char *
    static const uint16_t Auth    = 2; //!< Transport name, returns std::string *
  };

  //----------------------------------------------------------------------------
  //! XRootD related protocol queries
  //----------------------------------------------------------------------------
  struct XRootDQuery
  {
    static const uint16_t ServerFlags     = 1002; //!< returns server flags
    static const uint16_t ProtocolVersion = 1003; //!< returns the protocol version
    static const uint16_t IsEncrypted     = 1004; //!< returns true if the channel is encrypted
  };

  //----------------------------------------------------------------------------
  //! Stream query definitions
  //! The transports may support other queries, with ids > 2999 and ids < 2000
  //----------------------------------------------------------------------------
  struct StreamQuery
  {
      static const uint16_t IpAddr   = 2001;
      static const uint16_t IpStack  = 2002;
      static const uint16_t HostName = 2003;
  };

  //----------------------------------------------------------------------------
  //! Perform the handshake and the authentication for each physical stream
  //----------------------------------------------------------------------------
  class TransportHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Stream actions that may be triggered by incoming control messages
      //------------------------------------------------------------------------
      enum StreamAction
      {
        NoAction     = 0x0000, //!< No action
        DigestMsg    = 0x0001, //!< Digest the incoming message so that it won't
                               //!< be passed to the user handlers
        AbortStream  = 0x0002, //!< Disconnect, abort all the on-going
                               //!< operations and mark the stream as
                               //!< permanently broken [not yet implemented]
        CloseStream  = 0x0004, //!< Disconnect and attempt reconnection later
                               //!< [not yet implemented]
        ResumeStream = 0x0008, //!< Resume sending requests
                               //!< [not yet implemented]
        HoldStream   = 0x0010, //!< Stop sending requests [not yet implemented]
        RequestClose = 0x0020  //!< Send a close request
      };


      virtual ~TransportHandler() {}

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
      virtual XRootDStatus GetHeader( Message &message, Socket *socket ) = 0;

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
      virtual XRootDStatus GetBody( Message &message, Socket *socket ) = 0;

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
      virtual XRootDStatus GetMore( Message &message, Socket *socket ) = 0;

      //------------------------------------------------------------------------
      //! Initialize channel
      //------------------------------------------------------------------------
      virtual void InitializeChannel( const URL  &url,
                                      AnyObject  &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Finalize channel
      //------------------------------------------------------------------------
      virtual void FinalizeChannel( AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! HandHake
      //------------------------------------------------------------------------
      virtual XRootDStatus HandShake( HandShakeData *handShakeData,
                                      AnyObject     &channelData ) = 0;

      //------------------------------------------------------------------------
      // @return true if handshake has been done and stream is connected,
      //         false otherwise
      //------------------------------------------------------------------------
      virtual bool HandShakeDone( HandShakeData *handShakeData,
                                  AnyObject     &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Check if the stream should be disconnected
      //------------------------------------------------------------------------
      virtual bool IsStreamTTLElapsed( time_t     inactiveTime,
                                       AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Check the stream is broken - ie. TCP connection got broken and
      //! went undetected by the TCP stack
      //------------------------------------------------------------------------
      virtual Status IsStreamBroken( time_t     inactiveTime,
                                     AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Return the ID for the up stream this message should be sent by
      //! and the down stream which the answer should be expected at.
      //! Modify the message itself if necessary.
      //! If hint is non-zero then the message should be modified such that
      //! the answer will be returned via the hinted stream.
      //------------------------------------------------------------------------
      virtual PathID Multiplex( Message   *msg,
                                AnyObject &channelData,
                                PathID    *hint = 0 ) = 0;

      //------------------------------------------------------------------------
      //! Return the ID for the up substream this message should be sent by
      //! and the down substream which the answer should be expected at.
      //! Modify the message itself if necessary.
      //! If hint is non-zero then the message should be modified such that
      //! the answer will be returned via the hinted stream.
      //------------------------------------------------------------------------
      virtual PathID MultiplexSubStream( Message   *msg,
                                         AnyObject &channelData,
                                         PathID    *hint = 0 ) = 0;

      //------------------------------------------------------------------------
      //! Return a number of substreams per stream that should be created
      //------------------------------------------------------------------------
      virtual uint16_t SubStreamNumber( AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! The stream has been disconnected, do the cleanups
      //------------------------------------------------------------------------
      virtual void Disconnect( AnyObject &channelData,
                               uint16_t   subStreamId ) = 0;

      //------------------------------------------------------------------------
      //! Query the channel
      //------------------------------------------------------------------------
      virtual Status Query( uint16_t   query,
                            AnyObject &result,
                            AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Check if the message invokes a stream action
      //------------------------------------------------------------------------
      virtual uint32_t MessageReceived( Message   &msg,
                                        uint16_t   subStream,
                                        AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Notify the transport about a message having been sent
      //------------------------------------------------------------------------
      virtual void MessageSent( Message   *msg,
                                uint16_t   subStream,
                                uint32_t   bytesSent,
                                AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Wait before exit
      //------------------------------------------------------------------------
      virtual void WaitBeforeExit() = 0;

      //------------------------------------------------------------------------
      //! @return : true if encryption should be turned on, false otherwise
      //------------------------------------------------------------------------
      virtual bool NeedEncryption( HandShakeData  *handShakeData,
                                  AnyObject      &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Get signature for given message
      //------------------------------------------------------------------------
      virtual Status GetSignature( Message *toSign, Message *&sign,
                                   AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Decrement file object instance count bound to this channel
      //------------------------------------------------------------------------
      virtual void DecFileInstCnt( AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Get bind preference for the next data stream
      //------------------------------------------------------------------------
      virtual URL GetBindPreference( const URL  &url,
                                     AnyObject  &channelData ) = 0;
  };
}

#endif // __XRD_CL_POST_MASTER_INTERFACES_HH__
