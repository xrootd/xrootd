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

#ifndef __XRD_CL_POST_MASTER_HH__
#define __XRD_CL_POST_MASTER_HH__

#include <stdint.h>
#include <map>
#include <vector>
#include <functional>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{
  class Poller;
  class TaskManager;
  class Channel;
  class JobManager;
  class Job;

  //----------------------------------------------------------------------------
  //! A hub for dispatching and receiving messages
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
      //! DEADLOCK WARNING: no lock should be taken while calling this method
      //! that are used in the callback as well.
      //!
      //! @param url     recipient of the message
      //! @param msg     message to be sent
      //! @param stateful physical stream disconnection causes an error
      //! @param expires unix timestamp after which a failure should be
      //!                reported if sending was unsuccessful
      //! @return        success if the message has been pushed through the wire,
      //!                failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL &url,
                   Message   *msg,
                   bool       stateful,
                   time_t     expires );

      //------------------------------------------------------------------------
      //! Send the message asynchronously - the message is inserted into the
      //! send queue and a listener is called when the message is succesfsully
      //! pushed through the wire or when the timeout elapses
      //!
      //! DEADLOCK WARNING: no lock should be taken while calling this method
      //! that are used in the callback as well.
      //!
      //! @param url           recipient of the message
      //! @param msg           message to be sent
      //! @param expires       unix timestamp after which a failure is reported
      //!                      to the handler
      //! @param handler       handler will be notified about the status
      //! @param stateful      physical stream disconnection causes an error
      //! @return              success if the message was successfully inserted
      //!                      into the send queues, failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL            &url,
                   Message              *msg,
                   OutgoingMsgHandler   *handler,
                   bool                  stateful,
                   time_t                expires );

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      Status Redirect( const URL          &url,
                       Message            *msg,
                       IncomingMsgHandler *handler);

      //------------------------------------------------------------------------
      //! Synchronously receive a message - blocks until a message matching
      //! a filter is found in the incoming queue or the timeout passes
      //!
      //! @param url     sender of the message
      //! @param msg     reference to a message pointer, the pointer will
      //!                point to the received message
      //! @param filter  filter object defining what to look for
      //! @param expires expiration timestamp
      //! @return        success when the message has been received
      //!                successfully, failure otherwise
      //------------------------------------------------------------------------
      Status Receive( const URL      &url,
                      Message       *&msg,
                      MessageFilter *filter,
                      time_t         expires );

      //------------------------------------------------------------------------
      //! Listen to incoming messages, the listener is notified when a new
      //! message arrives and when the timeout passes
      //!
      //! @param url     sender of the message
      //! @param handler handler to be notified about new messages
      //! @param expires expiration timestamp
      //! @return        success when the listener has been inserted correctly
      //------------------------------------------------------------------------
      Status Receive( const URL          &url,
                      IncomingMsgHandler *handler,
                      time_t              expires );

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
      //! Register channel event handler
      //------------------------------------------------------------------------
      Status RegisterEventHandler( const URL           &url,
                                   ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Remove a channel event handler
      //------------------------------------------------------------------------
      Status RemoveEventHandler( const URL           &url,
                                 ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Get the task manager object user by the post master
      //------------------------------------------------------------------------
      TaskManager *GetTaskManager()
      {
        return pTaskManager;
      }

      //------------------------------------------------------------------------
      //! Get the job manager object user by the post master
      //------------------------------------------------------------------------
      JobManager *GetJobManager()
      {
        return pJobManager;
      }

      //------------------------------------------------------------------------
      //! Shut down a channel
      //------------------------------------------------------------------------
      Status ForceDisconnect( const URL &url );

      //------------------------------------------------------------------------
      //! Get the number of connected data streams
      //------------------------------------------------------------------------
      uint16_t NbConnectedStrm( const URL &url );

      //------------------------------------------------------------------------
      //! Set the on-connect handler for data streams
      //------------------------------------------------------------------------
      void SetOnConnectHandler( const URL &url,
                                Job       *onConnJob );

    private:
      Channel *GetChannel( const URL &url );

      typedef std::map<std::string, Channel*> ChannelMap;
      Poller           *pPoller;
      TaskManager      *pTaskManager;
      ChannelMap        pChannelMap;
      XrdSysMutex       pChannelMapMutex;
      bool              pInitialized;
      JobManager       *pJobManager;
  };
}

#endif // __XRD_CL_POST_MASTER_HH__
