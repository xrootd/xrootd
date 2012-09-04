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

#include "XrdCl/XrdClOutQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

#include <iostream>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Add a message to the back of the queue
  //----------------------------------------------------------------------------
  void OutQueue::PushBack( Message              *msg,
                           OutgoingMsgHandler   *handler,
                           time_t                expires,
                           bool                  stateful )
  {
    pMessages.push_back( MsgHelper( msg, handler, expires, stateful ) );
  }

  //----------------------------------------------------------------------------
  // Add a message to the front the queue
  //----------------------------------------------------------------------------
  void OutQueue::PushFront( Message              *msg,
                            OutgoingMsgHandler   *handler,
                            time_t                expires,
                            bool                  stateful )
  {
    pMessages.push_front( MsgHelper( msg, handler, expires, stateful ) );
  }

  //----------------------------------------------------------------------------
  //! Get a message from the front of the queue
  //----------------------------------------------------------------------------
  Message *OutQueue::PopMessage( OutgoingMsgHandler   *&handler,
                                 time_t                &expires,
                                 bool                  &stateful )
  {
    if( pMessages.empty() )
      return 0;

    MsgHelper  m = pMessages.front();
    handler  = m.handler;
    expires  = m.expires;
    stateful = m.stateful;
    pMessages.pop_front();
    return m.msg;
  }

  //----------------------------------------------------------------------------
  // Remove a message from the front
  //----------------------------------------------------------------------------
  void OutQueue::PopFront()
  {
    pMessages.pop_front();
  }

  //----------------------------------------------------------------------------
  // Report disconection to the handlers of stateful messages and remove
  // the messages from the queue
  //----------------------------------------------------------------------------
  void OutQueue::ReportDisconnection()
  {
    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); )
    {
      if( !it->stateful )
      {
        ++it;
        continue;
      }
      it->handler->OnStatusReady( it->msg,
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
    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); ++it )
      it->handler->OnStatusReady( it->msg, status );
    pMessages.clear();
  }

  //----------------------------------------------------------------------------
  // Report timout to the handlers of the expired messages and remove
  // these messages from the queue
  //----------------------------------------------------------------------------
  void OutQueue::ReportTimeout( time_t exp )
  {
    if( !exp )
      exp = time(0);

    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); )
    {
      if( it->expires > exp )
      {
        ++it;
        continue;
      }
      it->handler->OnStatusReady( it->msg,
                                  Status( stError, errSocketTimeout ) );
      it = pMessages.erase( it );
    }
  }

  //----------------------------------------------------------------------------
  // Take all the items from the queue and put them in this one
  //----------------------------------------------------------------------------
  void OutQueue::GrabItems( OutQueue &queue )
  {
    MessageList::iterator it;
    for( it = queue.pMessages.begin(); it != queue.pMessages.end(); ++it )
      pMessages.push_back( *it );
    queue.pMessages.clear();
  }
}
