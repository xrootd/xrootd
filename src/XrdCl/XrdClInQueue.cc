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
  bool InQueue::DiscardMessage(Message* msg, uint16_t& sid) const
  {
    if( msg->GetSize() < 8 )
      return true;

    ServerResponse *rsp = (ServerResponse *)msg->GetBuffer();

    // We got an async message
    if( rsp->hdr.status == kXR_attn )
    {
      if( msg->GetSize() < 12 )
	return true;

      // We only care about async responses
      if( rsp->body.attn.actnum != (int32_t)htonl(kXR_asynresp) )
	return true;

      if( msg->GetSize() < 24 )
	return true;

      ServerResponse *embRsp = (ServerResponse*)msg->GetBuffer(16);
      sid = ((uint16_t)embRsp->hdr.streamid[1] << 8) | (uint16_t)embRsp->hdr.streamid[0];
    }
    else
    {
      sid = ((uint16_t)rsp->hdr.streamid[1] << 8) | (uint16_t)rsp->hdr.streamid[0];
    }

    return false;
  }

  //----------------------------------------------------------------------------
  // Add a message to the queue
  //----------------------------------------------------------------------------
  bool InQueue::AddMessage( Message *msg )
  {
    uint16_t            action  = 0;
    IncomingMsgHandler* handler = 0;
    uint16_t msgSid = 0;

    if (DiscardMessage(msg, msgSid))
    {
      return true;
    }

    // Lookup the sid in the map of handlers
    pMutex.Lock();
    HandlerMap::iterator it = pHandlers.find(msgSid);

    if (it != pHandlers.end())
    {
      handler = it->second.first;
      action  = handler->Examine( msg );

      if( action & IncomingMsgHandler::RemoveHandler )
        pHandlers.erase( it );
    }

    if( !(action & IncomingMsgHandler::Take) )
      pMessages[msgSid] = msg;

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
    uint16_t action = 0;
    uint16_t handlerSid = handler->GetSid();
    XrdSysMutexHelper scopedLock( pMutex );
    MessageMap::iterator it = pMessages.find(handlerSid);

    if (it != pMessages.end())
    {
      action = handler->Examine( it->second );

      if( action & IncomingMsgHandler::Take )
      {
        if( !(action & IncomingMsgHandler::NoProcess ) )
          handler->Process( it->second );

        pMessages.erase( it );
      }
    }

    if( !(action & IncomingMsgHandler::RemoveHandler) )
      pHandlers[handlerSid] = HandlerAndExpire( handler, expires );
  }

  //----------------------------------------------------------------------------
  // Get a message handler interested in receiving message whose header
  // is stored in msg
  //----------------------------------------------------------------------------
  IncomingMsgHandler *InQueue::GetHandlerForMessage( Message  *msg,
						     time_t   &expires,
						     uint16_t &action )
  {
    time_t   exp = 0;
    uint16_t act = 0;
    uint16_t msgSid = 0;
    IncomingMsgHandler* handler = 0;

    if (DiscardMessage(msg, msgSid))
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

      if( act & IncomingMsgHandler::RemoveHandler )
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
  void InQueue::ReAddMessageHandler( IncomingMsgHandler *handler,
				     time_t              expires )
  {
    uint16_t handlerSid = handler->GetSid();
    XrdSysMutexHelper scopedLock( pMutex );
    pHandlers[handlerSid] = HandlerAndExpire( handler, expires );
  }

  //----------------------------------------------------------------------------
  // Remove a listener
  //----------------------------------------------------------------------------
  void InQueue::RemoveMessageHandler( IncomingMsgHandler *handler )
  {
    uint16_t handlerSid = handler->GetSid();
    XrdSysMutexHelper scopedLock( pMutex );
    pHandlers.erase(handlerSid);
  }

  //----------------------------------------------------------------------------
  // Report an event to the handlers
  //----------------------------------------------------------------------------
  void InQueue::ReportStreamEvent( IncomingMsgHandler::StreamEvent event,
				   uint16_t                        streamNum,
				   Status                          status )
  {
    uint8_t action = 0;
    XrdSysMutexHelper scopedLock( pMutex );
    for( HandlerMap::iterator it = pHandlers.begin(); it != pHandlers.end(); )
    {
      action = it->second.first->OnStreamEvent( event, streamNum, status );

      if( action & IncomingMsgHandler::RemoveHandler )
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
        it->second.first->OnStreamEvent( IncomingMsgHandler::Timeout, 0,
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
