//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Add a message to the queue
  //----------------------------------------------------------------------------
  bool InQueue::AddMessage( Message *msg )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    std::list<MessageHandler*>::iterator it;
    uint8_t                              action = 0;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
    {
      action = (*it)->HandleMessage( msg );

      if( action & MessageHandler::RemoveHandler )
      {
        it = pHandlers.erase( it );
        --it;
      }

      if( action & MessageHandler::Take )
        break;
    }

    if( !(action & MessageHandler::Take) )
      pMessages.push_front( msg );

    return true;
  }

  //----------------------------------------------------------------------------
  // Add a listener that should be notified about incomming messages
  //----------------------------------------------------------------------------
  void InQueue::AddMessageHandler( MessageHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    std::list<Message *>::iterator it;
    uint8_t                        action = 0;
    for( it = pMessages.begin(); it != pMessages.end(); ++it )
    {
      action = handler->HandleMessage( *it );

      if( action & MessageHandler::Take )
      {
        it = pMessages.erase( it );
        --it;
      }

      if( action & MessageHandler::RemoveHandler )
        break;
    }

    if( !(action & MessageHandler::RemoveHandler) )
      pHandlers.push_back( handler );
  }

  //----------------------------------------------------------------------------
  // Remove a listener
  //----------------------------------------------------------------------------
  void InQueue::RemoveMessageHandler( MessageHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    std::list<MessageHandler *>::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
      if( *it == handler )
        pHandlers.erase( it );
  }
}
