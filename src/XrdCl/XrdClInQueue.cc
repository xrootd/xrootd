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

#include "XProtocol/XProtocol.hh"
#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClMessage.hh"

#include <arpa/inet.h>              // for network unmarshalling stuff

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Filter messages
  //----------------------------------------------------------------------------
  bool InQueue::DiscardMessage( Message& msg, uint16_t& sid) const
  {
    if( msg.GetSize() < 8 )
      return true;

    ServerResponse *rsp = (ServerResponse *)msg.GetBuffer();

    // We only care about async responses, but those are extracted now
    // in the SocketHandler
    if( rsp->hdr.status == kXR_attn )
      return true;
    else
      sid = ((uint16_t)rsp->hdr.streamid[1] << 8) | (uint16_t)rsp->hdr.streamid[0];

    return false;
  }

  //----------------------------------------------------------------------------
  // Add a listener that should be notified about incoming messages
  //----------------------------------------------------------------------------
  void InQueue::AddMessageHandler( MsgHandler *handler, time_t expires, bool &rmMsg )
  {
    uint16_t handlerSid = handler->GetSid();
    XrdSysMutexHelper scopedLock( pMutex );
    MessageMap::iterator it = pMessages.find(handlerSid);

    //--------------------------------------------------------------------------
    // If there is a leftover message in the in-queue simply remove it, there's
    // no way this is the actual message we are waiting for as we just send it
    // over the wire and we are still occupying the event-loop thread
    //--------------------------------------------------------------------------
    if( ( rmMsg = it != pMessages.end() ) )
      pMessages.erase( it );

    pHandlers[handlerSid] = HandlerAndExpire( handler, expires );
  }

  //----------------------------------------------------------------------------
  // Get a message handler interested in receiving message whose header
  // is stored in msg
  //
  // If the handler is found it is always removed from the queue for the time
  // of processing the response. In case of a t/o, stream error, if the response
  // has been chunked the handler should be readded using `ReAddMessageHandler`.
  //----------------------------------------------------------------------------
  MsgHandler *InQueue::GetHandlerForMessage( std::shared_ptr<Message> &msg,
						                                 time_t                   &expires,
						                                 uint16_t                 &action )
  {
    time_t   exp = 0;
    uint16_t act = 0;
    uint16_t msgSid = 0;
    MsgHandler* handler = 0;

    if (DiscardMessage(*msg, msgSid))
    {
      return handler;
    }

    XrdSysMutexHelper scopedLock( pMutex );
    HandlerMap::iterator it = pHandlers.find(msgSid);

    if (it != pHandlers.end())
    {
      handler = it->second.first;
      act     = handler->Examine( msg );
      exp     = it->second.second;
      pHandlers.erase( it );
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
  void InQueue::ReAddMessageHandler( MsgHandler *handler,
				                             time_t      expires )
  {
    uint16_t handlerSid = handler->GetSid();
    XrdSysMutexHelper scopedLock( pMutex );
    pHandlers[handlerSid] = HandlerAndExpire( handler, expires );
  }

  //----------------------------------------------------------------------------
  // Report an event to the handlers
  //----------------------------------------------------------------------------
  void InQueue::ReportStreamEvent( MsgHandler::StreamEvent event,
                                   XRootDStatus            status )
  {
    uint8_t action = 0;
    XrdSysMutexHelper scopedLock( pMutex );
    for( HandlerMap::iterator it = pHandlers.begin(); it != pHandlers.end(); )
    {
      it->second.first->OnStreamEvent( event, status );
      if( event != MsgHandler::Ready )
      {
        auto next = it; ++next;
        pHandlers.erase( it );
        it = next;
      }
      else
        ++it;
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
    HandlerMap::iterator it = pHandlers.begin();
    while( it != pHandlers.end() )
    {
      if( it->second.second <= now )
      {
        it->second.first->OnStreamEvent( MsgHandler::Timeout,
                                         Status( stError, errOperationExpired ) );
        auto next = it; ++next;
        pHandlers.erase( it );
        it = next;
      }
      else
        ++it;
    }
  }
}
