//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_IN_QUEUE_HH__
#define __XRD_CL_IN_QUEUE_HH__

#include <XrdSys/XrdSysPthread.hh>
#include <list>
#include <utility>
#include "XrdCl/XrdClStatus.hh"

namespace XrdCl
{
  class Message;
  class MessageHandler;

  //----------------------------------------------------------------------------
  //! A synchronize queue for incomming data
  //----------------------------------------------------------------------------
  class InQueue
  {
    public:
      //------------------------------------------------------------------------
      //! Add a message to the queue
      //------------------------------------------------------------------------
      bool AddMessage( Message *msg );

      //------------------------------------------------------------------------
      //! Add a listener that should be notified about incomming messages
      //!
      //! @param handler message handler
      //! @param expires time when the message handler expires
      //------------------------------------------------------------------------
      void AddMessageHandler( MessageHandler *handler, time_t expires );

      //------------------------------------------------------------------------
      //! Remove a listener
      //------------------------------------------------------------------------
      void RemoveMessageHandler( MessageHandler *handler );

      //------------------------------------------------------------------------
      //! Fail and remove all the message handlers with a given status code
      //------------------------------------------------------------------------
      void FailAllHandlers( Status status );

      //------------------------------------------------------------------------
      //! Timeout handlers
      //------------------------------------------------------------------------
      void TimeoutHandlers( time_t now = 0 );

    private:
      typedef std::pair<MessageHandler *, time_t> HandlerAndExpire;
      typedef std::list<HandlerAndExpire> HandlerList;
      std::list<Message *> pMessages;
      HandlerList          pHandlers;
      XrdSysMutex          pMutex;
  };
}

#endif // __XRD_CL_IN_QUEUE_HH__
