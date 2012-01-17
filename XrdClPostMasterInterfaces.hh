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

namespace XrdClient
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
      enum Action
      {
        Take          = 0x01,
        Ignore        = 0x02,
        RemoveHandler = 0x04,
      };

      //------------------------------------------------------------------------
      //! Examine the message
      //!
      //! @param msg the message
      //! @return    action type that needs to be take wrt the message and
      //!            the handler
      //------------------------------------------------------------------------
      virtual uint8_t HandleMessage( Message *msg ) = 0;
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
    HandShakeData( const URL *addr, uint16_t stream ):
      step(0), out(0), in(0), url(addr), streamId(stream), startTime( time(0) )
    {}
    uint16_t   step;
    Message   *out;
    Message   *in;
    const URL *url;
    uint16_t   streamId;
    time_t     startTime;
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
  };
}

#endif // __XRD_CL_POST_MASTER_INTERFACES_HH__
