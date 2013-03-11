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
    uint8_t                action  = 0;
    IncomingMsgHandler    *handler = 0;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
    {
      handler = it->first;
      action  = handler->Examine( msg );

      if( action & IncomingMsgHandler::RemoveHandler )
      {
        it = pHandlers.erase( it );
        --it;
      }

      if( action & IncomingMsgHandler::Take )
        break;

      handler = 0;
    }

    if( !(action & IncomingMsgHandler::Take) )
      pMessages.push_front( msg );

    pMutex.UnLock();
    if( handler )
      handler->Process( msg );

    return true;
  }

  //----------------------------------------------------------------------------
  // Add a listener that should be notified about incomming messages
  //----------------------------------------------------------------------------
  void InQueue::AddMessageHandler( IncomingMsgHandler *handler, time_t expires )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    std::list<Message *>::iterator it;
    uint8_t                        action = 0;
    for( it = pMessages.begin(); it != pMessages.end(); ++it )
    {
      action = handler->Examine( *it );

      if( action & IncomingMsgHandler::Take )
      {
        handler->Process( *it );
        it = pMessages.erase( it );
        --it;
      }

      if( action & IncomingMsgHandler::RemoveHandler )
        break;
    }

    if( !(action & IncomingMsgHandler::RemoveHandler) )
      pHandlers.push_back( HandlerAndExpire( handler, expires ) );
  }

  //----------------------------------------------------------------------------
  // Remove a listener
  //----------------------------------------------------------------------------
  void InQueue::RemoveMessageHandler( IncomingMsgHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
      if( it->first == handler )
        pHandlers.erase( it );
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
