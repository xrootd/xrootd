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

namespace XrdCl
{
  class Message;
  class IncomingMsgHandler;

  //----------------------------------------------------------------------------
  //! A synchronize queue for incomming data
  //----------------------------------------------------------------------------
  class InQueue
  {
    public:
      //------------------------------------------------------------------------
      //! Add a message to the queue
      //------------------------------------------------------------------------
      bool AddMessage( Message *msg );

      //------------------------------------------------------------------------
      //! Add a listener that should be notified about incomming messages
      //!
      //! @param handler message handler
      //! @param expires time when the message handler expires
      //------------------------------------------------------------------------
      void AddMessageHandler( IncomingMsgHandler *handler, time_t expires );

      //------------------------------------------------------------------------
      //! Remove a listener
      //------------------------------------------------------------------------
      void RemoveMessageHandler( IncomingMsgHandler *handler );

      //------------------------------------------------------------------------
      //! Fail and remove all the message handlers with a given status code
      //------------------------------------------------------------------------
      void FailAllHandlers( Status status, uint16_t streamNum );

      //------------------------------------------------------------------------
      //! Timeout handlers
      //------------------------------------------------------------------------
      void TimeoutHandlers( time_t now = 0 );

    private:
      typedef std::pair<IncomingMsgHandler *, time_t> HandlerAndExpire;
      typedef std::list<HandlerAndExpire> HandlerList;
      std::list<Message *> pMessages;
      HandlerList          pHandlers;
      XrdSysMutex          pMutex;
  };
}

#endif // __XRD_CL_IN_QUEUE_HH__
