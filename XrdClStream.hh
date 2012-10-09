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

#include "XrdSys/XrdSysPthread.hh"
#include <list>
#include <vector>
#include <netinet/in.h>

namespace XrdCl
{
  class  Message;
  class  Channel;
  class  TransportHandler;
  class  InQueue;
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
      Stream( const URL *url, uint16_t streamNum );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Stream();

      //------------------------------------------------------------------------
      //! Initializer
      //------------------------------------------------------------------------
      Status Initialize();

      //------------------------------------------------------------------------
      //! Queue the message for sending
      //------------------------------------------------------------------------
      Status Send( Message              *msg,
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
      //! Connect if needed, otherwise make sure that the underlying socket
      //! handler gets write readiness events, it will update the path with
      //! what it has actually enabled
      //------------------------------------------------------------------------
      Status EnableLink( PathID &path );

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
      //! Get the stream number
      //------------------------------------------------------------------------
      uint16_t GetStreamNumber() const
      {
        return pStreamNum;
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
      //! Call back when a message has been reconstructed
      //------------------------------------------------------------------------
      void OnIncoming( uint16_t subStream, Message *msg );

      //------------------------------------------------------------------------
      // Call when one of the sockets is ready to accept a new message
      //------------------------------------------------------------------------
      Message *OnReadyToWrite( uint16_t subStream );

      //------------------------------------------------------------------------
      // Call when a message is written to the socket
      //------------------------------------------------------------------------
      void OnMessageSent( uint16_t subStream, Message *msg );

      //------------------------------------------------------------------------
      //! Call back when a message has been reconstructed
      //------------------------------------------------------------------------
      void OnConnect( uint16_t subStream );

      //------------------------------------------------------------------------
      //! On connect error
      //------------------------------------------------------------------------
      void OnConnectError( uint16_t subStream, Status status );

      //------------------------------------------------------------------------
      //! On error
      //------------------------------------------------------------------------
      void OnError( uint16_t subStream, Status status );

      //------------------------------------------------------------------------
      //! On read timeout
      //------------------------------------------------------------------------
      void OnReadTimeout( uint16_t subStream );

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

    private:
      //------------------------------------------------------------------------
      //! On fatal error - unlocks the stream
      //------------------------------------------------------------------------
      void OnFatalError( uint16_t           subStream,
                         Status             status,
                         XrdSysMutexHelper &lock );

      //------------------------------------------------------------------------
      //! Inform the monitoring about disconnection
      //------------------------------------------------------------------------
      void MonitorDisconnection( Status status );

      typedef std::vector<SubStreamData*> SubStreamList;

      //------------------------------------------------------------------------
      // Data members
      //------------------------------------------------------------------------
      const URL                     *pUrl;
      uint16_t                       pStreamNum;
      std::string                    pStreamName;
      TransportHandler              *pTransport;
      Poller                        *pPoller;
      TaskManager                   *pTaskManager;
      XrdSysRecMutex                 pMutex;
      InQueue                       *pIncomingQueue;
      AnyObject                     *pChannelData;
      uint16_t                       pLastStreamError;
      uint16_t                       pStreamErrorWindow;
      uint16_t                       pConnectionCount;
      uint16_t                       pConnectionRetry;
      time_t                         pConnectionInitTime;
      uint16_t                       pConnectionWindow;
      SubStreamList                  pSubStreams;
      std::vector<sockaddr_in>       pAddresses;
      ChannelHandlerList             pChannelEvHandlers;
      uint64_t                       pSessionId;

      //------------------------------------------------------------------------
      // Monitoring info
      //------------------------------------------------------------------------
      timeval                        pConnectionStarted;
      timeval                        pConnectionDone;
      uint64_t                       pBytesSent;
      uint64_t                       pBytesReceived;
  };
}

#endif // __XRD_CL_STREAM_HH__
