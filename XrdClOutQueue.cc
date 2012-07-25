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

#include "XrdCl/XrdClOutQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Add a message to the queue
  //----------------------------------------------------------------------------
  void OutQueue::AddMessage( Message              */*msg*/,
                             MessageStatusHandler */*handler*/,
                             time_t                /*expires*/,
                             bool                  /*stateful*/ )
  {
    XrdSysMutexHelper scopedLock( pMutex );
  }

  //----------------------------------------------------------------------------
  // Get a message from the beginning of the queue
  //----------------------------------------------------------------------------
  Message *OutQueue::GetFront()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( !pMessages.empty() )
      return pMessages.front().msg;
    return 0;
  }

  //----------------------------------------------------------------------------
  // Confirm successful sending of the message from the front
  //----------------------------------------------------------------------------
  void OutQueue::ConfirmFront()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( pMessages.empty() )
      return;
    MsgHelper &mh = pMessages.front();
    mh.handler->HandleStatus( mh.msg, Status() );
    pMessages.pop_front();
  }

  //----------------------------------------------------------------------------
  // Report disconection to the handlers of stateful messages and remove
  // the messages from the queue
  //----------------------------------------------------------------------------
  void OutQueue::ReportDisconnection()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); )
    {
      if( !it->stateful )
      {
        ++it;
        continue;
      }
      it->handler->HandleStatus( it->msg,
                                 Status( stError, errStreamDisconnect ) );
      it = pMessages.erase( it );
    }
  }

  //----------------------------------------------------------------------------
  // eport error to the message handlers and remove all the messages
  // from the queue
  //----------------------------------------------------------------------------
  void OutQueue::ReportError( Status status )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); )
      it->handler->HandleStatus( it->msg, status );
    pMessages.clear();
  }

  //----------------------------------------------------------------------------
  // Report timout to the handlers of the expired messages and remove
  // these messages from the queue
  //----------------------------------------------------------------------------
  void OutQueue::ReportTimeout( time_t exp )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( !exp )
      exp = time(0);

    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); )
    {
      if( !it->expires > exp )
      {
        ++it;
        continue;
      }
      it->handler->HandleStatus( it->msg,
                                 Status( stError, errSocketTimeout ) );
      it = pMessages.erase( it );
    }
  }
}
