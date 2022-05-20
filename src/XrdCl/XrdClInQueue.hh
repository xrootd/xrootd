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
#include <map>
#include <memory>
#include <utility>
#include "XrdCl/XrdClXRootDResponses.hh"
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
      //! Add a listener that should be notified about incoming messages
      //!
      //! @param handler message handler
      //! @param expires time when the message handler expires
      //! @param rmMsg   will be set to true if a left over message matching the
      //!                request has been removed from the queue
      //------------------------------------------------------------------------
      void AddMessageHandler( MsgHandler *handler, time_t expires, bool &rmMsg );

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
      MsgHandler *GetHandlerForMessage( std::shared_ptr<Message> &msg,
                                                time_t                   &expires,
                                                uint16_t                 &action );

      //------------------------------------------------------------------------
      //! Re-insert the handler without scanning the cached messages
      //------------------------------------------------------------------------
      void ReAddMessageHandler( MsgHandler *handler, time_t expires );

      //------------------------------------------------------------------------
      //! Remove a listener
      //------------------------------------------------------------------------
      void RemoveMessageHandler( MsgHandler *handler );

      //------------------------------------------------------------------------
      //! Report an event to the handlers
      //------------------------------------------------------------------------
      void ReportStreamEvent( MsgHandler::StreamEvent event,
                              XRootDStatus                    status );

      //------------------------------------------------------------------------
      //! Timeout handlers
      //------------------------------------------------------------------------
      void ReportTimeout( time_t now = 0 );

    private:

      //------------------------------------------------------------------------
      //! Discard messages that don't meet basic criteria and extract the
      //! message sid
      //!
      //! @param msg message object
      //! @param sid extracted message sid used later for matching with the
      //!        handler
      //!
      //! @return true if message discarded, otherwise false
      //------------------------------------------------------------------------
      bool DiscardMessage(Message& msg, uint16_t& sid) const;

      typedef std::pair<MsgHandler *, time_t> HandlerAndExpire;
      typedef std::map<uint16_t, HandlerAndExpire> HandlerMap;
      HandlerMap pHandlers;
      XrdSysRecMutex pMutex;
  };
}

#endif // __XRD_CL_IN_QUEUE_HH__
