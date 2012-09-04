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

#ifndef __XRD_CL_OUT_QUEUE_HH__
#define __XRD_CL_OUT_QUEUE_HH__

#include <list>
#include <utility>
#include "XrdCl/XrdClStatus.hh"

namespace XrdCl
{
  class Message;
  class OutgoingMsgHandler;

  //----------------------------------------------------------------------------
  //! A synchronized queue for the outgoind data
  //----------------------------------------------------------------------------
  class OutQueue
  {
    public:
      //------------------------------------------------------------------------
      //! Add a message to the back the queue
      //!
      //! @param msg      message to be sent
      //! @param handler  handler to be notified about the status of the
      //!                 operation
      //! @param expires  timeout
      //! @param stateful if true a disconnection will cause an error and
      //!                 removing from the queue, otherwise sending
      //!                 wil be retattempted
      //------------------------------------------------------------------------
      void PushBack( Message              *msg,
                     OutgoingMsgHandler   *handler,
                     time_t                expires,
                     bool                  stateful );

      //------------------------------------------------------------------------
      //! Add a message to the front the queue
      //!
      //! @param msg      message to be sent
      //! @param handler  handler to be notified about the status of the
      //!                 operation
      //! @param expires  timeout
      //! @param stateful if true a disconnection will cause an error and
      //!                 removing from the queue, otherwise sending
      //!                 wil be retattempted
      //------------------------------------------------------------------------
      void PushFront( Message              *msg,
                      OutgoingMsgHandler   *handler,
                      time_t                expires,
                      bool                  stateful );

      //------------------------------------------------------------------------
      //! Pop a message from the front of the queue
      //!
      //! @return 0 if there is no message message
      //------------------------------------------------------------------------
      Message *PopMessage( OutgoingMsgHandler   *&handler,
                           time_t                &expires,
                           bool                  &stateful );

      //------------------------------------------------------------------------
      //! Remove a message from the front
      //------------------------------------------------------------------------
      void PopFront();

      //------------------------------------------------------------------------
      //! Report disconection to the handlers of stateful messages and remove
      //! the messages from the queue
      //------------------------------------------------------------------------
      void ReportDisconnection();

      //------------------------------------------------------------------------
      //! Report error to the message handlers and remove all the messages
      //! from the queue
      //------------------------------------------------------------------------
      void ReportError( Status status );

      //------------------------------------------------------------------------
      //! Report timout to the handlers of the expired messages and remove
      //! these messages from the queue
      //!
      //! @param exp handlers having lower expiry date than "exp" will be
      //!            notified about timeout, 0 indicates now
      //------------------------------------------------------------------------
      void ReportTimeout( time_t exp = 0 );

      //------------------------------------------------------------------------
      //! Check if the queue is empty
      //------------------------------------------------------------------------
      bool IsEmpty() const
      {
        return pMessages.empty();
      }

      //------------------------------------------------------------------------
      // Return the size of the queue
      //------------------------------------------------------------------------
      uint64_t GetSize() const
      {
        return pMessages.size();
      }

      //------------------------------------------------------------------------
      //! Take all the items from the queue and put them in this one
      //------------------------------------------------------------------------
      void GrabItems( OutQueue &queue );

    private:
      //------------------------------------------------------------------------
      // Helper struct holding all the message data
      //------------------------------------------------------------------------
      struct MsgHelper
      {
        MsgHelper( Message *m, OutgoingMsgHandler *h, time_t r, bool s ):
          msg( m ), handler( h ), expires( r ), stateful( s ) {}

        Message              *msg;
        OutgoingMsgHandler   *handler;
        time_t                expires;
        bool                  stateful;
      };

      typedef std::list<MsgHelper> MessageList;
      MessageList pMessages;
  };
}

#endif // __XRD_CL_OUT_QUEUE_HH__
