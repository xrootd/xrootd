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

#ifndef __XRD_CL_STREAM_HH__
#define __XRD_CL_STREAM_HH__

#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClChannelHandlerList.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClUtils.hh"

#include "XrdSys/XrdSysPthread.hh"
#include "XrdNet/XrdNetAddr.hh"
#include <list>
#include <vector>
#include <functional>

namespace XrdCl
{
  class  Message;
  class  Channel;
  class  TransportHandler;
  class  TaskManager;
  struct SubStreamData;

  //----------------------------------------------------------------------------
  //! Stream
  //----------------------------------------------------------------------------
  class Stream
  {
    public:
      //------------------------------------------------------------------------
      //! Status of the stream
      //------------------------------------------------------------------------
      enum StreamStatus
      {
        Disconnected    = 0,    //!< Not connected
        Connected       = 1,    //!< Connected
        Connecting      = 2,    //!< In the process of being connected
        Error           = 3     //!< Broken
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Stream( const URL *url );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Stream();

      //------------------------------------------------------------------------
      //! Initializer
      //------------------------------------------------------------------------
      XRootDStatus Initialize();

      //------------------------------------------------------------------------
      //! Queue the message for sending
      //------------------------------------------------------------------------
      XRootDStatus Send( Message              *msg,
                         OutgoingMsgHandler   *handler,
                         bool                  stateful,
                         time_t                expires );

      //------------------------------------------------------------------------
      //! Set the transport
      //------------------------------------------------------------------------
      void SetTransport( TransportHandler *transport )
      {
        pTransport = transport;
      }

      //------------------------------------------------------------------------
      //! Set the poller
      //------------------------------------------------------------------------
      void SetPoller( Poller *poller )
      {
        pPoller = poller;
      }

      //------------------------------------------------------------------------
      //! Set the incoming queue
      //------------------------------------------------------------------------
      void SetIncomingQueue( InQueue *incomingQueue )
      {
        pIncomingQueue = incomingQueue;
        delete pQueueIncMsgJob;
        pQueueIncMsgJob = new QueueIncMsgJob( incomingQueue );
      }

      //------------------------------------------------------------------------
      //! Set the channel data
      //------------------------------------------------------------------------
      void SetChannelData( AnyObject *channelData )
      {
        pChannelData = channelData;
      }

      //------------------------------------------------------------------------
      //! Set task manager
      //------------------------------------------------------------------------
      void SetTaskManager( TaskManager *taskManager )
      {
        pTaskManager = taskManager;
      }

      //------------------------------------------------------------------------
      //! Set job manager
      //------------------------------------------------------------------------
      void SetJobManager( JobManager *jobManager )
      {
        pJobManager = jobManager;
      }

      //------------------------------------------------------------------------
      //! Connect if needed, otherwise make sure that the underlying socket
      //! handler gets write readiness events, it will update the path with
      //! what it has actually enabled
      //------------------------------------------------------------------------
      XRootDStatus EnableLink( PathID &path );

      //------------------------------------------------------------------------
      //! Disconnect the stream
      //------------------------------------------------------------------------
      void Disconnect( bool force = false );

      //------------------------------------------------------------------------
      //! Handle a clock event generated either by socket timeout, or by
      //! the task manager event
      //------------------------------------------------------------------------
      void Tick( time_t now );

      //------------------------------------------------------------------------
      //! Get the URL
      //------------------------------------------------------------------------
      const URL *GetURL() const
      {
        return pUrl;
      }

      //------------------------------------------------------------------------
      //! Force connection
      //------------------------------------------------------------------------
      void ForceConnect();

      //------------------------------------------------------------------------
      //! Return stream name
      //------------------------------------------------------------------------
      const std::string &GetName() const
      {
        return pStreamName;
      }

      //------------------------------------------------------------------------
      //! Disables respective uplink if empty
      //------------------------------------------------------------------------
      void DisableIfEmpty( uint16_t subStream );

      //------------------------------------------------------------------------
      //! Call back when a message has been reconstructed
      //------------------------------------------------------------------------
      void OnIncoming( uint16_t  subStream,
                       Message  *msg,
                       uint32_t  bytesReceived );

      //------------------------------------------------------------------------
      // Call when one of the sockets is ready to accept a new message
      //------------------------------------------------------------------------
      std::pair<Message *, OutgoingMsgHandler *>
        OnReadyToWrite( uint16_t subStream );

      //------------------------------------------------------------------------
      // Call when a message is written to the socket
      //------------------------------------------------------------------------
      void OnMessageSent( uint16_t  subStream,
                          Message  *msg,
                          uint32_t  bytesSent );

      //------------------------------------------------------------------------
      //! Call back when a message has been reconstructed
      //------------------------------------------------------------------------
      void OnConnect( uint16_t subStream );

      //------------------------------------------------------------------------
      //! On connect error
      //------------------------------------------------------------------------
      void OnConnectError( uint16_t subStream, XRootDStatus status );

      //------------------------------------------------------------------------
      //! On error
      //------------------------------------------------------------------------
      void OnError( uint16_t subStream, XRootDStatus status );

      //------------------------------------------------------------------------
      //! Force error
      //------------------------------------------------------------------------
      void ForceError( XRootDStatus status );

      //------------------------------------------------------------------------
      //! On read timeout
      //------------------------------------------------------------------------
      void OnReadTimeout( uint16_t subStream, bool &isBroken );

      //------------------------------------------------------------------------
      //! On write timeout
      //------------------------------------------------------------------------
      void OnWriteTimeout( uint16_t subStream );

      //------------------------------------------------------------------------
      //! Register channel event handler
      //------------------------------------------------------------------------
      void RegisterEventHandler( ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Remove a channel event handler
      //------------------------------------------------------------------------
      void RemoveEventHandler( ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Install a message handler for the given message if there is one
      //! available, if the handler want's to be called in the raw mode
      //! it will be returned, the message ownership flag is returned
      //! in any case
      //!
      //! @param msg    message header
      //! @param stream stream concerned
      //! @return       a pair containing the handler and ownership flag
      //------------------------------------------------------------------------
      std::pair<IncomingMsgHandler *, bool>
        InstallIncHandler( Message *msg, uint16_t stream );

      //------------------------------------------------------------------------
      //! Set the on-connect handler for data streams
      //------------------------------------------------------------------------
      void SetOnDataConnectHandler( std::shared_ptr<Job> &onConnJob )
      {
        pOnDataConnJob = onConnJob;
      }

    private:

      //------------------------------------------------------------------------
      // Job queuing the incoming messages
      //------------------------------------------------------------------------
      class QueueIncMsgJob: public Job
      {
        public:
          QueueIncMsgJob( InQueue *queue ): pQueue( queue ) {};
          virtual ~QueueIncMsgJob() {};
          virtual void Run( void *arg )
          {
            Message *msg = (Message *)arg;
            pQueue->AddMessage( msg );
          }
        private:
          InQueue *pQueue;
      };

      //------------------------------------------------------------------------
      // Job handling the incoming messages
      //------------------------------------------------------------------------
      class HandleIncMsgJob: public Job
      {
        public:
          HandleIncMsgJob( IncomingMsgHandler *handler ): pHandler( handler ) {};
          virtual ~HandleIncMsgJob() {};
          virtual void Run( void *arg )
          {
            Message *msg = (Message *)arg;
            pHandler->Process( msg );
            delete this;
          }
        private:
          IncomingMsgHandler *pHandler;
      };

      //------------------------------------------------------------------------
      //! On fatal error - unlocks the stream
      //------------------------------------------------------------------------
      void OnFatalError( uint16_t           subStream,
                         XRootDStatus       status,
                         XrdSysMutexHelper &lock );

      //------------------------------------------------------------------------
      //! Inform the monitoring about disconnection
      //------------------------------------------------------------------------
      void MonitorDisconnection( XRootDStatus status );

      //------------------------------------------------------------------------
      //! Send close after an open request timed out
      //------------------------------------------------------------------------
      XRootDStatus RequestClose( Message  *resp );

      typedef std::vector<SubStreamData*> SubStreamList;

      //------------------------------------------------------------------------
      // Data members
      //------------------------------------------------------------------------
      const URL                     *pUrl;
      std::string                    pStreamName;
      TransportHandler              *pTransport;
      Poller                        *pPoller;
      TaskManager                   *pTaskManager;
      JobManager                    *pJobManager;
      XrdSysRecMutex                 pMutex;
      InQueue                       *pIncomingQueue;
      AnyObject                     *pChannelData;
      uint32_t                       pLastStreamError;
      XRootDStatus                   pLastFatalError;
      uint16_t                       pStreamErrorWindow;
      uint16_t                       pConnectionCount;
      uint16_t                       pConnectionRetry;
      time_t                         pConnectionInitTime;
      uint16_t                       pConnectionWindow;
      SubStreamList                  pSubStreams;
      std::vector<XrdNetAddr>        pAddresses;
      Utils::AddressType             pAddressType;
      ChannelHandlerList             pChannelEvHandlers;
      uint64_t                       pSessionId;

      //------------------------------------------------------------------------
      // Jobs
      //------------------------------------------------------------------------
      QueueIncMsgJob                *pQueueIncMsgJob;

      //------------------------------------------------------------------------
      // Monitoring info
      //------------------------------------------------------------------------
      timeval                        pConnectionStarted;
      timeval                        pConnectionDone;
      uint64_t                       pBytesSent;
      uint64_t                       pBytesReceived;

      //------------------------------------------------------------------------
      // Data stream on-connect handler
      //------------------------------------------------------------------------
      std::shared_ptr<Job>           pOnDataConnJob;
  };
}

#endif // __XRD_CL_STREAM_HH__
