//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POST_MASTER_INTERFACES_HH__
#define __XRD_CL_POST_MASTER_INTERFACES_HH__

#include <stdint.h>

#include "XrdCl/XrdClStatus.hh"

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
      virtual void InitializeChannel( void *&channelData ) = 0;

      //------------------------------------------------------------------------
      //! Finalize channel
      //------------------------------------------------------------------------
      virtual void FinalizeChannel( void *&channelData ) = 0;

      //------------------------------------------------------------------------
      //! HandHake
      //------------------------------------------------------------------------
      virtual Status HandShake( Socket    *socket,
                                const URL &url, 
                                uint16_t   streamNumber,
                                void      *channelData ) = 0;

      //------------------------------------------------------------------------
      //! Disconnect
      //------------------------------------------------------------------------
      virtual Status Disconnect( Socket   *socket,
                                 uint16_t  streamNumber,
                                 void     *channelData )  = 0;

      //------------------------------------------------------------------------
      //! Return a stream number by which the message should be sent and/or
      //! alter the message to include the info by which stream the response
      //! should be sent back
      //------------------------------------------------------------------------
      virtual uint16_t Multiplex( Message *msg, void *channelData ) = 0;

      //------------------------------------------------------------------------
      //! Return the information whether a control connection needs to be
      //! valid before establishing other connections
      //------------------------------------------------------------------------
      virtual bool NeedControlConnection() = 0;
  };
}

#endif // __XRD_CL_POST_MASTER_INTERFACES_HH__
