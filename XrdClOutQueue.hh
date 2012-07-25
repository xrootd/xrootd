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

#ifndef __XRD_CL_OUT_QUEUE_HH__
#define __XRD_CL_OUT_QUEUE_HH__

#include <XrdSys/XrdSysPthread.hh>
#include <list>
#include <utility>
#include "XrdCl/XrdClStatus.hh"

namespace XrdCl
{
  class Message;
  class MessageStatusHandler;

  //----------------------------------------------------------------------------
  //! A synchronized queue for the outgoind data
  //----------------------------------------------------------------------------
  class OutQueue
  {
    public:
      //------------------------------------------------------------------------
      //! Add a message to the queue
      //!
      //! @param msg      message to be sent
      //! @param handler  handler to be notified about the status of the
      //!                 operation
      //! @param expires  timeout
      //! @param stateful if true a disconnection will cause an error and
      //!                 removing from the queue, otherwise sending
      //!                 wil be retattempted
      //------------------------------------------------------------------------
      void AddMessage( Message              *msg,
                       MessageStatusHandler *handler,
                       time_t                expires,
                       bool                  stateful );

      //------------------------------------------------------------------------
      //! Get a message from the beginning of the queue
      //!
      //! @param return 0 if there is no incomming message
      //------------------------------------------------------------------------
      Message *GetFront();

      //------------------------------------------------------------------------
      //! Confirm successful sending of the message from the front
      //------------------------------------------------------------------------
      void ConfirmFront();

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

    private:
      //------------------------------------------------------------------------
      // Helper struct holding all the message data
      //------------------------------------------------------------------------
      struct MsgHelper
      {
        MsgHelper( Message *m, MessageStatusHandler *h, time_t r, bool s ):
          msg( m ), handler( h ), expires( r ), stateful( s ) {}

        Message              *msg;
        MessageStatusHandler *handler;
        time_t                expires;
        bool                  stateful;
      };

      typedef std::list<MsgHelper> MessageList;
      MessageList pMessages;
      XrdSysMutex pMutex;
  };
}

#endif // __XRD_CL_OUT_QUEUE_HH__
