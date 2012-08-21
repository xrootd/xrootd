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

#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClOutQueue.hh"
#include "XrdCl/XrdClAsyncSocketHandler.hh"
#include "XrdSys/XrdSysDNS.hh"

#include <sys/types.h>
#include <sys/socket.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Message helper
  //----------------------------------------------------------------------------
  struct OutMessageHelper
  {
    OutMessageHelper( Message              *message = 0,
                      MessageStatusHandler *hndlr   = 0,
                      time_t                expir   = 0,
                      bool                  statefu = 0 ):
      msg( message ), handler( hndlr ), expires( expir ), stateful( statefu ) {}
    void Reset()
    {
      msg = 0; handler = 0; expires = 0; stateful = 0;
    }
    Message              *msg;
    MessageStatusHandler *handler;
    time_t                expires;
    bool                  stateful;
  };

  //----------------------------------------------------------------------------
  // Sub stream helper
  //----------------------------------------------------------------------------
  struct SubStreamData
  {
    SubStreamData(): socket( 0 )
    {
      outQueue = new OutQueue();
    }
    ~SubStreamData()
    {
      delete socket;
      delete outQueue;
    }
    AsyncSocketHandler *socket;
    OutQueue           *outQueue;
    OutMessageHelper    msgHelper;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Stream::Stream( const URL *url, uint16_t streamNum ):
    pUrl( url ),
    pStreamNum( streamNum ),
    pTransport( 0 ),
    pPoller( 0 ),
    pTaskManager( 0 ),
    pIncomingQueue( 0 ),
    pChannelData( 0 ),
    pLastStreamError( 0 ),
    pConnectionCount( 0 ),
    pConnectionInitTime( 0 )
  {
    std::ostringstream o;
    o << pUrl->GetHostId() << " #" << pStreamNum;
    pStreamName = o.str();

    Env *env = DefaultEnv::GetEnv();
    int connectionWindow = DefaultConnectionWindow;
    env->GetInt( "ConnectionWindow", connectionWindow );
    pConnectionWindow = connectionWindow;

    int connectionRetry = DefaultConnectionRetry;
    env->GetInt( "ConnectionRetry", connectionRetry );
    pConnectionRetry = connectionRetry;

    int streamErrorWindow = DefaultStreamErrorWindow;
    env->GetInt( "StreamErrorWindow", streamErrorWindow );
    pStreamErrorWindow = streamErrorWindow;
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Stream::~Stream()
  {
    Disconnect( true );
    SubStreamList::iterator it;
    for( it = pSubStreams.begin(); it != pSubStreams.end(); ++it )
      delete *it;
  }

  //----------------------------------------------------------------------------
  // Initializer
  //----------------------------------------------------------------------------
  Status Stream::Initialize()
  {
    if( !pTransport || !pPoller || !pChannelData )
      return Status( stError, errUninitialized );

    AsyncSocketHandler *s = new AsyncSocketHandler( pPoller,
                                                    pTransport,
                                                    pChannelData,
                                                    0 );
    s->SetStream( this );
    pSubStreams.push_back( new SubStreamData() );
    pSubStreams[0]->socket = s;
    return Status();
  }

  //------------------------------------------------------------------------
  // Make sure that the underlying socket handler gets write readiness
  // events
  //------------------------------------------------------------------------
  Status Stream::EnableLink( PathID &path )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // We are in the process of connecting the main stream, so we do nothing
    // because when the main stream connection is established it will connect
    // all the other streams
    //--------------------------------------------------------------------------
    if( pSubStreams[0]->socket->GetStatus() == Socket::Connecting )
      return Status();

    //--------------------------------------------------------------------------
    // The main stream is connected, so we can verify whether we have
    // the up and the down stream connected and ready to handle data.
    // If anything is not right we fall back to stream 0.
    //--------------------------------------------------------------------------
    if( pSubStreams[0]->socket->GetStatus() == Socket::Connected )
    {
      if( pSubStreams[path.down]->socket->GetStatus() != Socket::Connected )
        path.down = 0;

      if( pSubStreams[path.up]->socket->GetStatus() == Socket::Disconnected )
      {
        path.up = 0;
        return pSubStreams[0]->socket->EnableUplink();
      }

      if( pSubStreams[path.up]->socket->GetStatus() == Socket::Connected )
        return pSubStreams[path.up]->socket->EnableUplink();

      return Status();
    }

    //--------------------------------------------------------------------------
    // The main stream is not connected, we need to check whether enough time
    // has passed since we last encoutnered an error (if any) so that we could
    // reattempt the connection
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    time_t now = ::time(0);

    if( now-pLastStreamError < pStreamErrorWindow )
      return Status( stFatal, errConnectionError );

    pConnectionInitTime = now;
    ++pConnectionCount;

    //--------------------------------------------------------------------------
    // Resolve all the addresses of the host we're supposed to connect to
    //--------------------------------------------------------------------------
    Status st = Utils::GetHostAddresses( pAddresses, *pUrl );
    if( !st.IsOK() )
    {
      log->Error( PostMasterMsg, "[%s] Unable to resolve IP address for "
                                 "the host", pStreamName.c_str() );
      pLastStreamError = now;
      st.status = stFatal;
      return st;
    }

    Utils::LogHostAddresses( log, PostMasterMsg, pUrl->GetHostId(),
                             pAddresses );

    //--------------------------------------------------------------------------
    // Initiate the connection process to the first one on the list
    //--------------------------------------------------------------------------
    sockaddr_in addr;
    memcpy( &addr, &pAddresses.back(), sizeof( sockaddr_in ) );
    pAddresses.pop_back();
    pSubStreams[0]->socket->SetAddress( addr );
    return pSubStreams[0]->socket->Connect( pConnectionWindow );
  }

  //----------------------------------------------------------------------------
  // Queue the message for sending
  //----------------------------------------------------------------------------
  Status Stream::Send( Message              *msg,
                       MessageStatusHandler *handler,
                       uint32_t              timeout )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Decide on the path to send the message
    //--------------------------------------------------------------------------
    PathID path = pTransport->MultiplexSubStream( msg, *pChannelData );
    if( pSubStreams.size() <= path.up )
    {
      log->Warning( PostMasterMsg, "[%s] Unable to send message 0x%x through "
                                   "substream %d using 0 instead",
                                   pStreamName.c_str(), msg, path.up );
      path.up = 0;
    }

    log->Dump( PostMasterMsg, "[%s] Sending message %x through substream %d "
                              "expecting answer at %d",
                              pStreamName.c_str(), msg, path.up, path.down );

    //--------------------------------------------------------------------------
    // See if we can enable this path and, if no, whether there is another
    // path available. If not, there is nothing else we can do here.
    //--------------------------------------------------------------------------
    Status st = EnableLink( path );
    if( st.IsOK() )
    {
      pTransport->MultiplexSubStream( msg, *pChannelData, &path );
      pSubStreams[path.up]->outQueue->PushBack( msg, handler,
                                                time(0)+timeout, false );
    }
    return st;
  }

  //----------------------------------------------------------------------------
  // Disconnect the stream
  //----------------------------------------------------------------------------
  void Stream::Disconnect( bool /*force*/ )
  {
  }

  //----------------------------------------------------------------------------
  // Handle a clock event
  //----------------------------------------------------------------------------
  void Stream::Tick( time_t now )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    if( pStreamNum == 0 )
      pIncomingQueue->TimeoutHandlers( now );
    SubStreamList::iterator it;
    for( it = pSubStreams.begin(); it != pSubStreams.end(); ++it )
      (*it)->outQueue->ReportTimeout( now );
  }
}

//------------------------------------------------------------------------------
// Handle message timeouts and reconnection in the future
//------------------------------------------------------------------------------
namespace
{
  class StreamConnectorTask: public XrdCl::Task
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      StreamConnectorTask( XrdCl::Stream *stream ):
        pStream( stream )
      {
        std::string name = "StreamConnectorTask for";
        name += stream->GetName();
        SetName( name );
      }

      //------------------------------------------------------------------------
      // Run the task
      //------------------------------------------------------------------------
      time_t Run( time_t )
      {
        XrdCl::PathID path( 0, 0 );
        XrdCl::Status st = pStream->EnableLink( path );
        if( !st.IsOK() )
          pStream->OnConnectError( 0, st );
        return 0;
      }

    private:
      XrdCl::Stream *pStream;
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Call back when a message has been reconstructed
  //----------------------------------------------------------------------------
  void Stream::OnIncoming( uint16_t /*subStream*/, Message *msg )
  {
    pIncomingQueue->AddMessage( msg );
  }

  //----------------------------------------------------------------------------
  // Call when one of the sockets is ready to accept a new message
  //----------------------------------------------------------------------------
  Message *Stream::OnReadyToWrite( uint16_t subStream )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();
    if( pSubStreams[subStream]->outQueue->IsEmpty() )
    {
      log->Dump( PostMasterMsg, "[%s] Nothing to write, disable uplink",
                 pSubStreams[subStream]->socket->GetStreamName().c_str() );

      pSubStreams[subStream]->socket->DisableUplink();
      return 0;
    }

    OutMessageHelper &h = pSubStreams[subStream]->msgHelper;
    h.msg = pSubStreams[subStream]->outQueue->PopMessage( h.handler,
                                                          h.expires,
                                                          h.stateful );
    return h.msg;
  }

  //----------------------------------------------------------------------------
  // Call when a message is written to the socket
  //----------------------------------------------------------------------------
  void Stream::OnMessageSent( uint16_t subStream, Message *msg )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    OutMessageHelper &h = pSubStreams[subStream]->msgHelper;
    if( h.handler )
      h.handler->HandleStatus( msg, Status() );
    pSubStreams[subStream]->msgHelper.Reset();
  }

  //----------------------------------------------------------------------------
  // Call back when a message has been reconstructed
  //----------------------------------------------------------------------------
  void Stream::OnConnect( uint16_t subStream )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    pSubStreams[subStream]->socket->SetStatus( Socket::Connected );
    Log *log = DefaultEnv::GetLog();
    log->Debug( PostMasterMsg, "[%s] Stream %d connected.",
                               pStreamName.c_str(), subStream );

    if( subStream == 0 )
    {
      uint16_t numSub = pTransport->SubStreamNumber( *pChannelData );

      //------------------------------------------------------------------------
      // Create the streams if they don't exist yet
      //------------------------------------------------------------------------
      if( pSubStreams.size() == 1 && numSub > 1 )
      {
        for( uint16_t i = 1; i < numSub; ++i )
        {
          AsyncSocketHandler *s = new AsyncSocketHandler( pPoller, pTransport,
                                                          pChannelData, i );
          s->SetStream( this );
          pSubStreams.push_back( new SubStreamData() );
          pSubStreams[i]->socket = s;
        }
      }

      //------------------------------------------------------------------------
      // Connect the extra streams, if we fail we move all the outgoing items
      // to stream 0, we don't need to enable the uplink here, because it
      // should be already enabled after the handshaking process is completed.
      //------------------------------------------------------------------------
      if( pSubStreams.size() > 1 )
      {
        log->Debug( PostMasterMsg, "[%s] Attempting to connect %d additional "
                                   "streams.",
                                   pStreamName.c_str(), pSubStreams.size()-1 );
        for( size_t i = 1; i < pSubStreams.size(); ++i )
        {
          pSubStreams[i]->socket->SetAddress( pSubStreams[0]->socket->GetAddress() );
          Status st = pSubStreams[i]->socket->Connect( pConnectionWindow );
          if( !st.IsOK() )
          {
            pSubStreams[0]->outQueue->GrabItems( *pSubStreams[i]->outQueue );
            pSubStreams[i]->socket->Close();
          }
        }
      }
    }
  }

  //----------------------------------------------------------------------------
  // On connect error
  //----------------------------------------------------------------------------
  void Stream::OnConnectError( uint16_t subStream, Status st )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();
    pSubStreams[subStream]->socket->Close();
    time_t now = ::time(0);

    //--------------------------------------------------------------------------
    // If we connected subStream == 0 and cannot connect >0 then we just give
    // up and move the outgoing messages to another queue
    //--------------------------------------------------------------------------
    if( subStream > 0 )
    {
      pSubStreams[0]->outQueue->GrabItems( *pSubStreams[subStream]->outQueue );
      if( pSubStreams[0]->socket->GetStatus() == Socket::Connected )
      {
        Status st = pSubStreams[0]->socket->EnableUplink().IsOK();
        if( !st.IsOK() )
          OnFatalError( 0, st );
        return;
      }

      if( pSubStreams[0]->socket->GetStatus() == Socket::Connecting )
        return;

      OnFatalError( subStream, st );
      return;
    }

    //--------------------------------------------------------------------------
    // Check if we still have time to try and do somethig in the current window
    //--------------------------------------------------------------------------
    time_t elapsed = now-pConnectionInitTime;
    if( elapsed < pConnectionWindow )
    {
      //------------------------------------------------------------------------
      // If we have some IP addresses left we try them
      //------------------------------------------------------------------------
      if( !pAddresses.empty() )
      {
        sockaddr_in addr;
        memcpy( &addr, &pAddresses.back(), sizeof( sockaddr_in ) );
        pAddresses.pop_back();
        pSubStreams[0]->socket->SetAddress( addr );

        Status st = pSubStreams[0]->socket->Connect( pConnectionWindow-elapsed );
        if( !st.IsOK() )
          OnFatalError( subStream, st );
        return;
      }

      //------------------------------------------------------------------------
      // If we still can retry with the same host name, we sleep until the end
      // of the connection window and try
      //------------------------------------------------------------------------
      else if( pConnectionCount < pConnectionRetry )
      {
        log->Info( PostMasterMsg, "[%s] Attempting reconnection in %d "
                                  "seconds.", pStreamName.c_str(),
                                  pConnectionWindow-elapsed );

        Task *task = new ::StreamConnectorTask( this );
        pTaskManager->RegisterTask( task, pConnectionInitTime+pConnectionWindow );
        return;
      }

      //------------------------------------------------------------------------
      // Nothing can be done, we declare a failure
      //------------------------------------------------------------------------
      OnFatalError( subStream, Status( stFatal, errConnectionError ) );
    }

    //--------------------------------------------------------------------------
    // We are out of the connection window, the only thing we can do here
    // is re-resolving the host name and retrying if we still can
    //--------------------------------------------------------------------------
    if( pConnectionCount < pConnectionRetry )
    {
      pAddresses.clear();
      Status st = Utils::GetHostAddresses( pAddresses, *pUrl );
      if( !st.IsOK() )
      {
        log->Error( PostMasterMsg, "[%s] Unable to resolve IP address for "
                                   "the host", pStreamName.c_str() );
        OnFatalError( subStream, Status( stFatal, errConnectionError ) );
        return;
      }

      Utils::LogHostAddresses( log, PostMasterMsg, pUrl->GetHostId(),
                               pAddresses );

      //----------------------------------------------------------------------
      // Initiate the connection process to the first one on the list
      //----------------------------------------------------------------------
      sockaddr_in addr;
      memcpy( &addr, &pAddresses.back(), sizeof( sockaddr_in ) );
      pAddresses.pop_back();
      pSubStreams[0]->socket->SetAddress( addr );
      st = pSubStreams[0]->socket->Connect( pConnectionWindow );
      if( !st.IsOK() )
        OnFatalError( subStream, Status( stFatal, errConnectionError ) );
      return;
    }

    //--------------------------------------------------------------------------
    // Else, we fail
    //--------------------------------------------------------------------------
    OnFatalError( subStream, Status( stFatal, errConnectionError ) );
  }

  //----------------------------------------------------------------------------
  // Call back when an error has occured
  //----------------------------------------------------------------------------
  void Stream::OnError( uint16_t subStream, Status st )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();
    pSubStreams[subStream]->socket->Close();

    log->Debug( PostMasterMsg, "[%s] Recovering error for stream #%d: %s.",
                               pStreamName.c_str(), subStream,
                               st.ToString().c_str() );

    //--------------------------------------------------------------------------
    // Reinsert the stuff that we have failed to sent
    //--------------------------------------------------------------------------
    if( pSubStreams[subStream]->msgHelper.msg )
    {
      OutMessageHelper &h = pSubStreams[subStream]->msgHelper;
      pSubStreams[subStream]->outQueue->PushFront( h.msg, h.handler, h.expires,
                                                   h.stateful );
      pSubStreams[subStream]->msgHelper.Reset();
    }

    //--------------------------------------------------------------------------
    // If we lost the stream 0 we have lost the session, we re-enable the
    // stream if we still have things in one of the outgoing queues
    //--------------------------------------------------------------------------
    if( subStream == 0 )
    {
      SubStreamList::iterator it;
      size_t outstanding = 0;
      for( it = pSubStreams.begin(); it != pSubStreams.end(); ++it )
      {
        (*it)->outQueue->ReportDisconnection();
        outstanding += (*it)->outQueue->GetSize();
      }

      if( outstanding )
      {
        PathID path( 0, 0 );
        Status st = EnableLink( path );
        if( !st.IsOK() )
        {
          OnFatalError( 0, st );
          return;
        }
      }

      return;
    }

    //--------------------------------------------------------------------------
    // We are now dealing with an error of a peripheral stream. If we don't have
    // anything to send don't bother recovering. Otherwise move the requests
    // to stream 0 if possible.
    //--------------------------------------------------------------------------
    if( pSubStreams[subStream]->outQueue->IsEmpty() )
      return;

    if( pSubStreams[0]->socket->GetStatus() != Socket::Disconnected )
    {
      pSubStreams[0]->outQueue->GrabItems( *pSubStreams[subStream]->outQueue );
      if( pSubStreams[0]->socket->GetStatus() == Socket::Connected )
      {
        Status st = pSubStreams[0]->socket->EnableUplink();
        if( !st.IsOK() )
          OnFatalError( 0, st );
        return;
      }
      return;
    }

    OnFatalError( subStream, st );
  }

  //----------------------------------------------------------------------------
  // On fatal error
  //----------------------------------------------------------------------------
  void Stream::OnFatalError( uint16_t /*subStream*/, Status st )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log    *log = DefaultEnv::GetLog();
    log->Error( PostMasterMsg, "[%s] Unable to recover: %s.",
                               pStreamName.c_str(), st.ToString().c_str() );

    pConnectionCount = 0;
    pIncomingQueue->FailAllHandlers( st );
    SubStreamList::iterator it;
    for( it = pSubStreams.begin(); it != pSubStreams.end(); ++it )
      (*it)->outQueue->ReportError( st );
    pLastStreamError = ::time(0);
  }

  //----------------------------------------------------------------------------
  // Call back when a message has been reconstructed
  //----------------------------------------------------------------------------
  void Stream::OnReadTimeout( uint16_t /*substream*/ )
  {
  }

  //----------------------------------------------------------------------------
  // Call back when a message has been reconstru
  //----------------------------------------------------------------------------
  void Stream::OnWriteTimeout( uint16_t /*substream*/ )
  {
  }
}
