//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
    pMutex.Lock();

    HandlerList::iterator  it;
    uint16_t               action  = 0;
    IncomingMsgHandler    *handler = 0;
    for( it = pHandlers.begin(); it != pHandlers.end(); )
    {
      handler = it->first;
      action  = handler->Examine( msg );

      if( action & IncomingMsgHandler::RemoveHandler )
        it = pHandlers.erase( it );
      else
        ++it;

      if( action & IncomingMsgHandler::Take )
        break;

      handler = 0;
    }

    if( !(action & IncomingMsgHandler::Take) )
      pMessages.push_front( msg );

    pMutex.UnLock();
    if( handler && !(action & IncomingMsgHandler::NoProcess) )
      handler->Process( msg );

    return true;
  }

  //----------------------------------------------------------------------------
  // Add a listener that should be notified about incoming messages
  //----------------------------------------------------------------------------
  void InQueue::AddMessageHandler( IncomingMsgHandler *handler, time_t expires )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    std::list<Message *>::iterator it;
    uint16_t                       action = 0;
    for( it = pMessages.begin(); it != pMessages.end(); )
    {
      action = handler->Examine( *it );

      if( action & IncomingMsgHandler::Take )
      {
        if( !(action & IncomingMsgHandler::NoProcess ) )
          handler->Process( *it );
        it = pMessages.erase( it );
      }
      else
        ++it;

      if( action & IncomingMsgHandler::RemoveHandler )
        break;
    }

    if( !(action & IncomingMsgHandler::RemoveHandler) )
      pHandlers.push_back( HandlerAndExpire( handler, expires ) );
  }

  //----------------------------------------------------------------------------
  // Get a message handler interested in receiving message whose header
  // is stored in msg
  //----------------------------------------------------------------------------
  IncomingMsgHandler *InQueue::GetHandlerForMessage( Message  *msg,
                                                     time_t   &expires,
                                                     uint16_t &action )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator  it;
    IncomingMsgHandler    *handler = 0;
    time_t   exp = 0;
    uint16_t act = 0;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
    {
      handler = it->first;
      act     = handler->Examine( msg );
      exp     = it->second;

      if( act & IncomingMsgHandler::Take )
      {
        pHandlers.erase( it );
        break;
      }

      handler = 0;
    }

    if( handler )
    {
      expires = exp;
      action  = act;
    }
    return handler;
  }

  //----------------------------------------------------------------------------
  // Re-insert the handler without scanning the cached messages
  //----------------------------------------------------------------------------
  void InQueue::ReAddMessageHandler( IncomingMsgHandler *handler,
                                     time_t              expires )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    pHandlers.push_front( HandlerAndExpire( handler, expires ) );
  }

  //----------------------------------------------------------------------------
  // Remove a listener
  //----------------------------------------------------------------------------
  void InQueue::RemoveMessageHandler( IncomingMsgHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); )
      if( it->first == handler )
        it = pHandlers.erase( it );
      else
        ++it;
  }

  //----------------------------------------------------------------------------
  // Report an event to the handlers
  //----------------------------------------------------------------------------
  void InQueue::ReportStreamEvent( IncomingMsgHandler::StreamEvent event,
                                   uint16_t                        streamNum,
                                   Status                          status )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it;
    uint8_t               action = 0;
    for( it = pHandlers.begin(); it != pHandlers.end(); )
    {
      action = it->first->OnStreamEvent( event, streamNum, status );

      if( action & IncomingMsgHandler::RemoveHandler )
        it = pHandlers.erase( it );
      else ++it;
    }
  }

  //----------------------------------------------------------------------------
  // Timeout handlers
  //----------------------------------------------------------------------------
  void InQueue::ReportTimeout( time_t now )
  {
    if( !now )
      now = ::time(0);

    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it = pHandlers.begin();
    while( it != pHandlers.end() )
    {
      if( it->second <= now )
      {
        it->first->OnStreamEvent( IncomingMsgHandler::Timeout, 0,
                                  Status( stError, errOperationExpired ) );
        it = pHandlers.erase( it );
      }
      else
        ++it;
    }
  }
}
