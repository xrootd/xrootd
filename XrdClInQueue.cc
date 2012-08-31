//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
      action = it->first->OnIncoming( msg );

      if( action & IncomingMsgHandler::RemoveHandler )
      {
        it = pHandlers.erase( it );
        --it;
      }

      if( action & IncomingMsgHandler::Take )
        break;
    }

    if( !(action & IncomingMsgHandler::Take) )
      pMessages.push_front( msg );

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
      action = handler->OnIncoming( *it );

      if( action & IncomingMsgHandler::Take )
      {
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
  // Fail and remove all the message handlers with a given status code
  //----------------------------------------------------------------------------
  void InQueue::FailAllHandlers( Status status, uint16_t streamNum )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    HandlerList::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
      it->first->OnStreamEvent( IncomingMsgHandler::Broken, streamNum, status );
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
        it->first->OnStreamEvent( IncomingMsgHandler::Timeout, 0,
                                  Status( stError, errSocketTimeout ) );
        it = pHandlers.erase( it );
      }
      else
        ++it;
    }
  }
}
