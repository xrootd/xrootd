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

#ifndef __XRD_CL_IN_QUEUE_HH__
#define __XRD_CL_IN_QUEUE_HH__

#include <XrdSys/XrdSysPthread.hh>
#include <list>
#include <utility>
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

namespace XrdCl
{
  class Message;

  //----------------------------------------------------------------------------
  //! A synchronize queue for incoming data
  //----------------------------------------------------------------------------
  class InQueue
  {
    public:
      //------------------------------------------------------------------------
      //! Add a fully reconstructed message to the queue
      //------------------------------------------------------------------------
      bool AddMessage( Message *msg );

      //------------------------------------------------------------------------
      //! Add a listener that should be notified about incoming messages
      //!
      //! @param handler message handler
      //! @param expires time when the message handler expires
      //------------------------------------------------------------------------
      void AddMessageHandler( IncomingMsgHandler *handler, time_t expires );

      //------------------------------------------------------------------------
      //! Get a message handler interested in receiving message whose header
      //! is stored in msg
      //!
      //! @param msg     message header
      //! @param expires handle's expiration timestamp
      //! @param action  the action declared by the handler
      //!
      //! @return handler or 0 if none is interested
      //------------------------------------------------------------------------
      IncomingMsgHandler *GetHandlerForMessage( Message  *msg,
                                                time_t   &expires,
                                                uint16_t &action );

      //------------------------------------------------------------------------
      //! Re-insert the handler without scanning the cached messages
      //------------------------------------------------------------------------
      void ReAddMessageHandler( IncomingMsgHandler *handler, time_t expires );

      //------------------------------------------------------------------------
      //! Remove a listener
      //------------------------------------------------------------------------
      void RemoveMessageHandler( IncomingMsgHandler *handler );

      //------------------------------------------------------------------------
      //! Report an event to the handlers
      //------------------------------------------------------------------------
      void ReportStreamEvent( IncomingMsgHandler::StreamEvent event,
                              uint16_t                        streamNum,
                              Status                          status );

      //------------------------------------------------------------------------
      //! Timeout handlers
      //------------------------------------------------------------------------
      void ReportTimeout( time_t now = 0 );

    private:
      typedef std::pair<IncomingMsgHandler *, time_t> HandlerAndExpire;
      typedef std::list<HandlerAndExpire> HandlerList;
      std::list<Message *> pMessages;
      HandlerList          pHandlers;
      XrdSysMutex          pMutex;
  };
}

#endif // __XRD_CL_IN_QUEUE_HH__
