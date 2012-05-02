//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POST_MASTER_INTERFACES_HH__
#define __XRD_CL_POST_MASTER_INTERFACES_HH__

#include <stdint.h>
#include <ctime>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClURL.hh"

namespace XrdCl
{
  class Channel;
  class Message;
  class Socket;
  class URL;

  //----------------------------------------------------------------------------
  //! Message filter
  //----------------------------------------------------------------------------
  class MessageFilter
  {
    public:
      //------------------------------------------------------------------------
      //! Examine the message and return true if the message should be picked
      //! up (usually removed from the queue and to the caller)
      //------------------------------------------------------------------------
      virtual bool Filter( const Message *msg ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Message handler
  //----------------------------------------------------------------------------
  class MessageHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Actions to be taken after a message is processed by the handler
      //------------------------------------------------------------------------
      enum Action
      {
        Take          = 0x01,     //!< Take ownership over the message
        Ignore        = 0x02,     //!< Ignore the message
        RemoveHandler = 0x04      //!< Remove the handler from the notification
                                  //!< list
      };

      //------------------------------------------------------------------------
      //! Examine an incomming message, and decide on the action to be taken
      //!
      //! @param msg    the message, may be zero if receive failed
      //! @return       action type that needs to be take wrt the message and
      //!               the handler
      //------------------------------------------------------------------------
      virtual uint8_t HandleMessage( Message *msg  ) = 0;

      //------------------------------------------------------------------------
      //! Handle an event other that a message arrival - may be timeout
      //! or stream failure
      //!
      //! @param status info about the fault that occured
      //------------------------------------------------------------------------
      virtual void HandleFault( Status status ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Message status handler
  //----------------------------------------------------------------------------
  class MessageStatusHandler
  {
    public:
      //------------------------------------------------------------------------
      //! The requested action has been performed and the status is available
      //------------------------------------------------------------------------
      virtual void HandleStatus( const Message *message,
                                 Status         status ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Data structure that carries the handshake information
  //----------------------------------------------------------------------------
  struct HandShakeData
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    HandShakeData( const URL *addr, uint16_t stream ):
      step(0), out(0), in(0), url(addr), streamId(stream), startTime( time(0) ),
      serverAddr(0)
    {}
    uint16_t     step;           //!< Handshake step
    Message     *out;            //!< Message to be sent out
    Message     *in;             //!< Message that has been received
    const URL   *url;            //!< Destination URL
    uint16_t     streamId;       //!< Stream number
    time_t       startTime;      //!< Timestamp of when the handshake started
    const void  *serverAddr;     //!< Server address in the form of sockaddr
    std::string  clientName;     //!< Client name (an IPv6 representation)
  };

  //----------------------------------------------------------------------------
  //! Transport query definitions
  //! The transports may support other queries, with ids > 1000
  //----------------------------------------------------------------------------
  struct TransportQuery
  {
    static const uint16_t Name = 1; //!< Transport name, returns const char *
  };

  //----------------------------------------------------------------------------
  //! Perform the handshake and the authentication for each physical stream
  //----------------------------------------------------------------------------
  class TransportHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Read a message from the socket, the socket is non blocking, so if
      //! there is not enough data the function should retutn errRetry in which
      //! case it will be called again when more data arrives with the data
      //! previousely read stored in the message buffer
      //!
      //! @param message the message
      //! @param socket  the socket
      //! @return        stOK if the message has been processed properly,
      //!                stError & errRetry when the method needs to be called
      //!                again to finish reading the message
      //!                stError on faiure
      //------------------------------------------------------------------------
      virtual Status GetMessage( Message *message, Socket *socket ) = 0;

      //------------------------------------------------------------------------
      //! Initialize channel
      //------------------------------------------------------------------------
      virtual void InitializeChannel( AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Finalize channel
      //------------------------------------------------------------------------
      virtual void FinalizeChannel( AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! HandHake
      //------------------------------------------------------------------------
      virtual Status HandShake( HandShakeData *handShakeData,
                                AnyObject     &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Check if the stream should be disconnected
      //------------------------------------------------------------------------
      virtual bool IsStreamTTLElapsed( time_t     inactiveTime,
                                       AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Return a stream number by which the message should be sent and/or
      //! alter the message to include the info by which stream the response
      //! should be sent back
      //------------------------------------------------------------------------
      virtual uint16_t Multiplex( Message *msg, AnyObject &channelData ) = 0;

      //------------------------------------------------------------------------
      //! Return the information whether a control connection needs to be
      //! valid before establishing other connections
      //------------------------------------------------------------------------
      virtual bool NeedControlConnection() = 0;

      //------------------------------------------------------------------------
      //! The stream has been disconnected, do the cleanups
      //------------------------------------------------------------------------
      virtual void Disconnect( AnyObject &channelData, uint16_t streamId ) = 0;

      //------------------------------------------------------------------------
      //! Query the channel
      //------------------------------------------------------------------------
      virtual Status Query( uint16_t   query,
                            AnyObject &result,
                            AnyObject &channelData ) = 0;
  };
}

#endif // __XRD_CL_POST_MASTER_INTERFACES_HH__
