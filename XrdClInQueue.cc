//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Add a message to the queue
  //----------------------------------------------------------------------------
  bool InQueue::AddMessage( Message *msg )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    HandlerList::iterator it;
    uint8_t               action = 0;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
    {
      action = it->first->HandleMessage( msg );

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
  void InQueue::AddMessageHandler( MessageHandler *handler, time_t expires )
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
      pHandlers.push_back( HandlerAndExpire( handler, expires ) );
  }

  //----------------------------------------------------------------------------
  // Remove a listener
  //----------------------------------------------------------------------------
  void InQueue::RemoveMessageHandler( MessageHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
      if( it->first == handler )
        pHandlers.erase( it );
  }

  //----------------------------------------------------------------------------
  // Fail and remove all the message handlers with a given status code
  //----------------------------------------------------------------------------
  void InQueue::FailAllHandlers( Status status )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
      it->first->HandleFault( status );
     pHandlers.clear();
  }

  //----------------------------------------------------------------------------
  // Timeout handlers
  //----------------------------------------------------------------------------
  void InQueue::TimeoutHandlers( time_t now )
  {
    if( !now )
      now = ::time(0);

    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it = pHandlers.begin();
    while( it != pHandlers.end() )
    {
      if( it->second <= now )
      {
        it->first->HandleFault( Status( stError, errSocketTimeout ) );
        it = pHandlers.erase( it );
      }
      else
        ++it;
    }
  }
}
