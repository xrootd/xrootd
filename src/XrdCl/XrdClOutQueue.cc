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
  // Report status to all handlers
  //----------------------------------------------------------------------------
  void OutQueue::Report( XRootDStatus status )
  {
    MessageList::iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); ++it )
      it->handler->OnStatusReady( it->msg, status );
  }

  //------------------------------------------------------------------------
  // Return the size of the queue counting only the stateless messages
  //------------------------------------------------------------------------
  uint64_t OutQueue::GetSizeStateless() const
  {
    uint64_t size = 0;
    MessageList::const_iterator it;
    for( it = pMessages.begin(); it != pMessages.end(); ++it )
      if( !it->stateful )
        ++size;
    return size;
  }

  //----------------------------------------------------------------------------
  // Remove all the expired messages from the queue and put them in
  // this one
  //----------------------------------------------------------------------------
  void OutQueue::GrabExpired( OutQueue &queue, time_t exp )
  {
    MessageList::iterator it;
    for( it = queue.pMessages.begin(); it != queue.pMessages.end(); )
    {
      if( it->expires > exp )
      {
        ++it;
        continue;
      }
      pMessages.push_back( *it );
      it = queue.pMessages.erase( it );
    }
  }

  //----------------------------------------------------------------------------
  // Remove all the stateful messages from the queue and put them in this
  // one
  //----------------------------------------------------------------------------
  void OutQueue::GrabStateful( OutQueue &queue )
  {
    MessageList::iterator it;
    for( it = queue.pMessages.begin(); it != queue.pMessages.end(); )
    {
      if( !it->stateful )
      {
        ++it;
        continue;
      }
      pMessages.push_back( *it );
      it = queue.pMessages.erase( it );
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

