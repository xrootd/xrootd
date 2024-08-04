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

#ifndef __XRD_CL_POST_CHANNEL_HH__
#define __XRD_CL_POST_CHANNEL_HH__

#include <cstdint>
#include <vector>
#include <ctime>
#include <functional>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClTaskManager.hh"

#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{
  class Stream;
  class JobManager;
  class VirtualRedirector;
  class TickGeneratorTask;
  class Job;

  //----------------------------------------------------------------------------
  //! A communication channel between the client and the server
  //----------------------------------------------------------------------------
  class Channel
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url         address of the server to connect to
      //! @param poller      poller object to be used for non-blocking IO
      //! @param transport   protocol specific transport handler
      //! @param taskManager async task handler to be used by the channel
      //! @param jobManager  worker thread handler to be used by the channel
      //------------------------------------------------------------------------
      Channel( const URL        &url,
               Poller           *poller,
               TransportHandler *transport,
               TaskManager      *taskManager,
               JobManager       *jobManager,
               const URL        &prefurl = URL() );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Channel();

      //------------------------------------------------------------------------
      //! Get the URL
      //------------------------------------------------------------------------
      const URL &GetURL() const
      {
        return pUrl;
      }

      //------------------------------------------------------------------------
      //! Send the message asynchronously - the message is inserted into the
      //! send queue and a listener is called when the message is successfully
      //! pushed through the wire or when the timeout elapses
      //!
      //! @param msg     message to be sent
      //! @param handler handler to be notified about the status
      //! @param stateful physical stream disconnection causes an error
      //! @param expires unix timestamp after which a failure is reported
      //!                to the listener
      //! @return        success if the message was successfully inserted
      //!                into the send queues, failure otherwise
      //------------------------------------------------------------------------
      XRootDStatus Send( Message              *msg,
                         MsgHandler           *handler,
                         bool                  stateful,
                         time_t                expires );

      //------------------------------------------------------------------------
      //! Query the transport handler
      //!
      //! @param query  the query as defined in the TransportQuery struct or
      //!               others that may be recognized by the protocol transport
      //! @param result the result of the query
      //! @return       status of the query
      //------------------------------------------------------------------------
      Status QueryTransport( uint16_t query, AnyObject &result );

      //------------------------------------------------------------------------
      //! Register channel event handler
      //------------------------------------------------------------------------
      void RegisterEventHandler( ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Remove a channel event handler
      //------------------------------------------------------------------------
      void RemoveEventHandler( ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Handle a time event
      //------------------------------------------------------------------------
      void Tick( time_t now );

      //------------------------------------------------------------------------
      //! Force disconnect of all streams
      //------------------------------------------------------------------------
      Status ForceDisconnect();

      //------------------------------------------------------------------------
      //! Force reconnect
      //------------------------------------------------------------------------
      Status ForceReconnect();

      //------------------------------------------------------------------------
      //! Get the number of connected data streams
      //------------------------------------------------------------------------
      uint16_t NbConnectedStrm();

      //------------------------------------------------------------------------
      //! Set the on-connect handler for data streams
      //------------------------------------------------------------------------
      void SetOnDataConnectHandler( std::shared_ptr<Job> &onConnJob );

      //------------------------------------------------------------------------
      //! @return : true if this channel can be collapsed using this URL, false
      //!           otherwise
      //------------------------------------------------------------------------
      bool CanCollapse( const URL &url );

      //------------------------------------------------------------------------
      //! Decrement file object instance count bound to this channel
      //------------------------------------------------------------------------
      void DecFileInstCnt();

    private:

      URL                    pUrl;
      Poller                *pPoller;
      TransportHandler      *pTransport;
      TaskManager           *pTaskManager;
      Stream                *pStream;
      XrdSysMutex            pMutex;
      AnyObject              pChannelData;
      InQueue                pIncoming;
      TickGeneratorTask     *pTickGenerator;
      JobManager            *pJobManager;
  };
}

#endif // __XRD_CL_POST_CHANNEL_HH__
