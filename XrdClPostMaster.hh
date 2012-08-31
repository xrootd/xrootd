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

#ifndef __XRD_CL_POST_MASTER_HH__
#define __XRD_CL_POST_MASTER_HH__

#include <stdint.h>
#include <map>
#include <vector>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{
  class Poller;
  class TaskManager;
  class Channel;

  //----------------------------------------------------------------------------
  //! A hub for dispaching and receiving messages
  //----------------------------------------------------------------------------
  class PostMaster
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      PostMaster();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~PostMaster();

      //------------------------------------------------------------------------
      //! Initializer
      //------------------------------------------------------------------------
      bool Initialize();

      //------------------------------------------------------------------------
      //! Finalizer
      //------------------------------------------------------------------------
      bool Finalize();

      //------------------------------------------------------------------------
      //! Start the post master
      //------------------------------------------------------------------------
      bool Start();

      //------------------------------------------------------------------------
      //! Stop the postmaster
      //------------------------------------------------------------------------
      bool Stop();

      //------------------------------------------------------------------------
      //! Reinitialize after fork
      //------------------------------------------------------------------------
      bool Reinitialize();

      //------------------------------------------------------------------------
      //! Send a message synchronously - synchronously means that
      //! it will block until the message is written to a socket
      //!
      //! @param url     recipient of the message
      //! @param msg     message to be sent
      //! @param statful physical stream disconnection causes an error
      //! @param timeout timout after which a failure should be reported if
      //!                sending was unsuccessful
      //! @return        success if the message has been pushed through the wire,
      //!                failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL &url,
                   Message   *msg,
                   bool       stateful,
                   uint16_t   timeout );

      //------------------------------------------------------------------------
      //! Send the message asynchronously - the message is inserted into the
      //! send queue and a listener is called when the message is successuly
      //! pushed through the wire or when the timeout elapses
      //!
      //! @param url           recipient of the message
      //! @param msg           message to be sent
      //! @param timeout       timeout after which a failure is reported to the
      //!                      handler
      //! @param handler       handler will be notified about the status
      //! @param stateful      physical stream disconnection causes an error
      //! @return              success if the message was successfuly inserted
      //!                      into the send quees, failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL            &url,
                   Message              *msg,
                   OutgoingMsgHandler   *handler,
                   bool                  stateful,
                   uint16_t              timeout );

      //------------------------------------------------------------------------
      //! Synchronously receive a message - blocks until a message maching
      //! a filter is found in the incomming queue or the timout passes
      //!
      //! @param url     sender of the message
      //! @param msg     reference to a message pointer, the pointer will
      //!                point to the received message
      //! @param filter  filter object defining what to look for
      //! @param timeout timeout
      //! @return        success when the message has been received
      //!                successfuly, failure otherwise
      //------------------------------------------------------------------------
      Status Receive( const URL      &url,
                      Message       *&msg,
                      MessageFilter *filter,
                      uint16_t       timeout );

      //------------------------------------------------------------------------
      //! Listen to incomming messages, the listener is notified when a new
      //! message arrives and when the timeout passes
      //!
      //! @param url     sender of the message
      //! @param handler handler to be notified about new messages
      //! @param timeout timout
      //! @return        success when the listener has been inserted correctly
      //------------------------------------------------------------------------
      Status Receive( const URL          &url,
                      IncomingMsgHandler *handler,
                      uint16_t            timeout );

      //------------------------------------------------------------------------
      //! Query the transport handler for a given URL
      //!
      //! @param url    the channel to be queried
      //! @param query  the query as defined in the TransportQuery struct or
      //!               others that may be recognized by the protocol transport
      //! @param result the result of the query
      //! @return       status of the query
      //------------------------------------------------------------------------
      Status QueryTransport( const URL &url,
                             uint16_t   query,
                             AnyObject &result );

      //------------------------------------------------------------------------
      //! Get the task manager object user by the post master
      //------------------------------------------------------------------------
      TaskManager *GetTaskManager()
      {
        return pTaskManager;
      }

    private:
      Channel *GetChannel( const URL &url );

      typedef std::map<std::string, Channel*> ChannelMap;
      Poller           *pPoller;
      TaskManager      *pTaskManager;
      ChannelMap        pChannelMap;
      XrdSysMutex       pChannelMapMutex;
      TransportHandler *pTransportHandler; // to be removed when protocol
                                           // factory is implemented
      bool              pInitialized;
  };
}

#endif // __XRD_CL_POST_MASTER_HH__
