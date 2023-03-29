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

#include <cstdint>
#include <map>
#include <vector>
#include <functional>
#include <memory>

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

  struct PostMasterImpl;

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
      XRootDStatus Send( const URL    &url,
                         Message      *msg,
                         MsgHandler   *handler,
                         bool          stateful,
                         time_t        expires );

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      Status Redirect( const URL  &url,
                       Message    *msg,
                       MsgHandler *handler);

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
      TaskManager *GetTaskManager();

      //------------------------------------------------------------------------
      //! Get the job manager object user by the post master
      //------------------------------------------------------------------------
      JobManager *GetJobManager();

      //------------------------------------------------------------------------
      //! Shut down a channel
      //------------------------------------------------------------------------
      Status ForceDisconnect( const URL &url );

      //------------------------------------------------------------------------
      //! Reconnect the channel
      //------------------------------------------------------------------------
      Status ForceReconnect( const URL &url );

      //------------------------------------------------------------------------
      //! Get the number of connected data streams
      //------------------------------------------------------------------------
      uint16_t NbConnectedStrm( const URL &url );

      //------------------------------------------------------------------------
      //! Set the on-connect handler for data streams
      //------------------------------------------------------------------------
      void SetOnDataConnectHandler( const URL            &url,
                                    std::shared_ptr<Job>  onConnJob );

      //------------------------------------------------------------------------
      //! Set the global connection error handler
      //------------------------------------------------------------------------
      void SetOnConnectHandler( std::unique_ptr<Job> onConnJob );

      //------------------------------------------------------------------------
      //! Set the global on-error on-connect handler for control streams
      //------------------------------------------------------------------------
      void SetConnectionErrorHandler( std::function<void( const URL&, const XRootDStatus& )> handler );

      //------------------------------------------------------------------------
      //! Notify the global on-connect handler
      //------------------------------------------------------------------------
      void NotifyConnectHandler( const URL &url );

      //------------------------------------------------------------------------
      //! Notify the global error connection handler
      //------------------------------------------------------------------------
      void NotifyConnErrHandler( const URL &url, const XRootDStatus &status );

      //------------------------------------------------------------------------
      //! Collapse channel URL - replace the URL of the channel
      //------------------------------------------------------------------------
      void CollapseRedirect( const URL &oldurl, const URL &newURL );

      //------------------------------------------------------------------------
      //! Decrement file object instance count bound to this channel
      //------------------------------------------------------------------------
      void DecFileInstCnt( const URL &url );

      //------------------------------------------------------------------------
      //! @return : true if underlying threads are running, false otherwise
      //------------------------------------------------------------------------
      bool IsRunning();

    private:
      Channel *GetChannel( const URL &url );

      std::unique_ptr<PostMasterImpl> pImpl;
  };
}

#endif // __XRD_CL_POST_MASTER_HH__
