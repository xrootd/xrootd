//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_IN_QUEUE_HH__
#define __XRD_CL_IN_QUEUE_HH__

#include <XrdSys/XrdSysPthread.hh>
#include <list>

namespace XrdClient
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
      //------------------------------------------------------------------------
      void AddMessageHandler( MessageHandler *handler );

      //------------------------------------------------------------------------
      //! Remove a listener
      //------------------------------------------------------------------------
      void RemoveMessageHandler( MessageHandler *handler );

    private:
      std::list<Message *>        pMessages;
      std::list<MessageHandler *> pHandlers;
      XrdSysMutex                 pMutex;
  };
}

#endif // __XRD_CL_IN_QUEUE_HH__
