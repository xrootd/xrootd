//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_STREAM_HH__
#define __XRD_CL_STREAM_HH__

#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClStatus.hh"

#include "XrdSys/XrdSysPthread.hh"
#include <list>

namespace XrdClient
{
  class  Message;
  class  MessageStatusHandler;
  class  Channel;
  class  TransportHandler;
  class  InQueue;
  struct OutMessageHelper;

  //----------------------------------------------------------------------------
  //! Stream
  //----------------------------------------------------------------------------
  class Stream: public SocketHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Stream( Channel          *channel,
              uint16_t          streamNum,
              TransportHandler *transport,
              Socket           *socket,
              Poller           *poller,
              InQueue          *incomming );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Stream();

      //------------------------------------------------------------------------
      //! Handle a socket event
      //------------------------------------------------------------------------
      virtual void Event( uint8_t  type, Socket *socket );

      //------------------------------------------------------------------------
      //! Queue the message for sending
      //------------------------------------------------------------------------
      Status QueueOut( Message              *msg,
                       MessageStatusHandler *handler,
                       uint32_t              timeout );

      //------------------------------------------------------------------------
      //! Lock the stream
      //------------------------------------------------------------------------
      void Lock() { pMutex.Lock(); }

      //------------------------------------------------------------------------
      //! UnLock the stream
      //------------------------------------------------------------------------
      void UnLock() { pMutex.UnLock(); }

      //------------------------------------------------------------------------
      //! Get the socket
      //------------------------------------------------------------------------
      Socket *GetSocket()
      {
        return pSocket;
      }

    private:
      //------------------------------------------------------------------------
      // Write a message to a socket
      //------------------------------------------------------------------------
      void WriteMessage();

      //------------------------------------------------------------------------
      // Get a message from a socket
      //------------------------------------------------------------------------
      void ReadMessage();

      //------------------------------------------------------------------------
      // Get a message from a socket
      //------------------------------------------------------------------------
//      void HandleStreamFault();

      Channel                       *pChannel;
      uint16_t                       pStreamNum;
      TransportHandler              *pTransport;
      Socket                        *pSocket;
      Poller                        *pPoller;
      XrdSysMutex                    pMutex;
      std::list<OutMessageHelper *>  pOutQueue;
      OutMessageHelper              *pCurrentOut;
      InQueue                       *pIncomingQueue;
      Message                       *pIncoming;
  };
}

#endif // __XRD_CL_STREAM_HH__
