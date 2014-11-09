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

#include "XrdCl/XrdClPollerLibEvent.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClOptimizers.hh"

#include <string>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <event2/event.h>
#include <event2/thread.h>

//------------------------------------------------------------------------------
// Check the version of libevent
//------------------------------------------------------------------------------
#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000d00
#error "This version of Libevent is not supported; Get 2.0.13-stable or later."
#endif

//------------------------------------------------------------------------------
// A helper struct being passed as custom data to libevent callbacks
//------------------------------------------------------------------------------
namespace
{
  struct PollerHelper
  {
    PollerHelper( XrdCl::SocketHandler *hndl,
                  XrdCl::Socket        *sock ):
      handler( hndl ), socket( sock ),
      readEvent( 0 ), writeEvent( 0 ),
      readEnabled( false ), writeEnabled( false ) {}
    XrdCl::SocketHandler *handler;
    XrdCl::Socket        *socket;
    event                    *readEvent;
    event                    *writeEvent;
    bool                      readEnabled;
    bool                      writeEnabled;
  };
}

//------------------------------------------------------------------------------
// The stuff that needs to stay unmangled
//------------------------------------------------------------------------------
extern "C"
{
  //----------------------------------------------------------------------------
  // Handle libevent log messages
  //----------------------------------------------------------------------------
  static void HandleLogMessage( int severity, const char *msg )
  {
    using namespace XrdCl;
    Log *log = DefaultEnv::GetLog();
    switch( severity )
    {
      case _EVENT_LOG_DEBUG:
       log->Debug( PollerMsg, "%s", msg );
       break;

      case _EVENT_LOG_MSG:
       log->Info( PollerMsg, "%s", msg );
       break;

      case _EVENT_LOG_WARN:
       log->Warning( PollerMsg, "%s", msg );
       break;

      case _EVENT_LOG_ERR:
       log->Error( PollerMsg, "%s", msg );
       break;

      default:
       log->Error( PollerMsg, "%s", msg );
       break;
    }
  }

  //----------------------------------------------------------------------------
  // Run the poller thread
  //----------------------------------------------------------------------------
  static void *RunPollerThread( void *arg )
  {
    using namespace XrdCl;
    PollerLibEvent *poller = (PollerLibEvent*)arg;
    long result = poller->RunEventLoop();
    return (void*)result;
  }

  //----------------------------------------------------------------------------
  // Dummy callback
  //----------------------------------------------------------------------------
  static void DummyCallback( evutil_socket_t, short, void * )
  {
  }

  //----------------------------------------------------------------------------
  // Read event callback
  //----------------------------------------------------------------------------
  static void ReadEventCallback( evutil_socket_t, short what, void *arg )
  {
    using namespace XrdCl;

    PollerHelper *helper = (PollerHelper *)arg;
    uint8_t       ev     = 0;

    if( what & EV_TIMEOUT ) ev |= SocketHandler::ReadTimeOut;
    if( what & EV_READ    ) ev |= SocketHandler::ReadyToRead;

    Log *log = DefaultEnv::GetLog();
    if( unlikely(log->GetLevel() >= Log::DumpMsg) )
    {
      log->Dump( PollerMsg, "%s Got read event: %s",
                             helper->socket->GetName().c_str(),
                             SocketHandler::EventTypeToString( ev ).c_str() );
    }

    helper->handler->Event( ev, helper->socket );
  }

  //----------------------------------------------------------------------------
  // Write event callback
  //----------------------------------------------------------------------------
  static void WriteEventCallback( evutil_socket_t, short what, void *arg )
  {
    using namespace XrdCl;

    PollerHelper *helper = (PollerHelper *)arg;
    uint8_t       ev     = 0;

    if( what & EV_TIMEOUT ) ev |= SocketHandler::WriteTimeOut;
    if( what & EV_WRITE   ) ev |= SocketHandler::ReadyToWrite;

    Log *log = DefaultEnv::GetLog();
    if( unlikely(log->GetLevel() >= Log::DumpMsg) )
    {
      log->Dump( PollerMsg, "%s Got write event %s",
                             helper->socket->GetName().c_str(),
                             SocketHandler::EventTypeToString( ev ).c_str() );
    }

    helper->handler->Event( ev, helper->socket );
  }
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Initialize the poller
  //----------------------------------------------------------------------------
  bool PollerLibEvent::Initialize()
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Print some debugging info
    //--------------------------------------------------------------------------
    const char *compiledVersion = LIBEVENT_VERSION;
    const char *runningVersion  = event_get_version();
    log->Debug( PollerMsg, "Poller is using the libevent backend\n"
                           "Compiled against libevent: %s\n"
                           "Running with libevent: %s",
                           compiledVersion, runningVersion );

    const char **methods = event_get_supported_methods();
    std::string methodList;
    for( int i = 0; methods[i] != 0; ++i)
    {
      methodList += methods[i];
      methodList += ", ";
    }
    methodList = methodList.substr( 0, methodList.length()-2 );

    log->Dump( PollerMsg, "Available libevent backends are: %s",
                          methodList.c_str() );

    //--------------------------------------------------------------------------
    // Set up the library
    //--------------------------------------------------------------------------
    ::event_set_log_callback( HandleLogMessage );
    ::evthread_use_pthreads();

    //--------------------------------------------------------------------------
    // Create event base
    //--------------------------------------------------------------------------
    pEventBase = event_base_new();
    if( !pEventBase )
    {
      log->Error( PollerMsg, "Initialization of libevent failed" );
      return false;
    }

    log->Debug( PollerMsg, "Backend used by libevent: %s",
                           event_base_get_method( pEventBase ) );
    return true;
  }

  //----------------------------------------------------------------------------
  // Finalize the poller
  //----------------------------------------------------------------------------
  bool PollerLibEvent::Finalize()
  {
    if( IsRunning() )
      Stop();

    SocketMap::iterator it;
    for( it = pSocketMap.begin(); it != pSocketMap.end(); ++it )
    {
      PollerHelper *helper = (PollerHelper *)it->second;

      if( helper->readEvent )
      {
        ::event_del( helper->readEvent );
        ::event_free( helper->readEvent );
      }

      if( helper->writeEvent )
      {
        ::event_del( helper->writeEvent );
        ::event_free( helper->writeEvent );
      }

      delete helper;
    }
    ::event_base_free( pEventBase );
    pSocketMap.clear();
    return true;
  }

  //------------------------------------------------------------------------
  // Start polling
  //------------------------------------------------------------------------
  bool PollerLibEvent::Start()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( PollerMsg, "Starting the poller..." );
    if( pPollerThreadRunning )
    {
      log->Error( PollerMsg, "The poller is already running" );
      return false;
    }

    int ret = ::pthread_create( &pPollerThread, 0, ::RunPollerThread, this );
    if( ret != 0 )
    {
      log->Error( PollerMsg, "Unable to spawn the poller thread: %s",
                             strerror( errno ) );
      return false;
    }
    pPollerThreadRunning = true;
    return true;
  }

  //------------------------------------------------------------------------
  // Stop polling
  //------------------------------------------------------------------------
  bool PollerLibEvent::Stop()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( PollerMsg, "Stopping the poller..." );
    if( !pPollerThreadRunning )
    {
      log->Dump( PollerMsg, "The poller is not running" );
      return false;
    }

    if( ::event_base_loopexit( pEventBase, 0 ) != 0 )
    {
      log->Error( PollerMsg, "Unable to exit the poller loop" );
      return false;
    }

    void *threadRet;
    int ret = pthread_join( pPollerThread, (void **)&threadRet );
    if( ret != 0 )
    {
      log->Error( PollerMsg, "Failed to join the poller thread: %s",
                             strerror( errno ) );
      return false;
    }

    pPollerThreadRunning = false;
    log->Debug( PollerMsg, "Poller stopped" );
    return true;
  }

  //------------------------------------------------------------------------
  // Add socket to the polling queue
  //------------------------------------------------------------------------
  bool PollerLibEvent::AddSocket( Socket        *socket,
                                  SocketHandler *handler )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    if( !socket )
    {
      log->Error( PollerMsg, "Invalid socket, impossible to poll" );
      return false;
    }

    if( socket->GetStatus() != Socket::Connected &&
        socket->GetStatus() != Socket::Connecting )
    {
      log->Error( PollerMsg, "Socket is not in a state valid for polling" );
      return false;
    }

    log->Debug( PollerMsg, "Adding socket 0x%x to the poller", socket );

    //--------------------------------------------------------------------------
    // Check if the socket is already registered
    //--------------------------------------------------------------------------

    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it != pSocketMap.end() )
    {
      log->Warning( PollerMsg, "%s Already registered with this poller",
                               socket->GetName().c_str() );
      return false;
    }

    //--------------------------------------------------------------------------
    // Create the socket helper
    //--------------------------------------------------------------------------
    PollerHelper *helper = new PollerHelper( handler, socket );
    handler->Initialize( this );
    pSocketMap[socket] = helper;

    return true;
  }

  //------------------------------------------------------------------------
  // Remove the socket
  //------------------------------------------------------------------------
  bool PollerLibEvent::RemoveSocket( Socket *socket )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Find the right event
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );
    SocketMap::iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
      return true;

    log->Debug( PollerMsg, "%s Removing socket from the poller",
                           socket->GetName().c_str() );

    //--------------------------------------------------------------------------
    // Remove the event
    //--------------------------------------------------------------------------
    PollerHelper *helper = (PollerHelper*)it->second;

    //--------------------------------------------------------------------------
    // Disable the events if they are enabled
    //--------------------------------------------------------------------------
    if( helper->readEnabled )
    {
      if( ::event_del( helper->readEvent ) != 0 )
      {
        log->Error( PollerMsg, "%s Failed to unregister a read event",
                               socket->GetName().c_str() );
        return false;
      }
    }

    if( helper->writeEnabled )
    {
      if( ::event_del( helper->writeEvent ) != 0 )
      {
        log->Error( PollerMsg, "%s Failed to unregister a write event",
                                socket->GetName().c_str() );
        return false;
      }
    }

    if( helper->readEvent )
      ::event_free( helper->readEvent );

    if( helper->writeEvent )
      ::event_free( helper->writeEvent );

    delete helper;
    pSocketMap.erase( it );

    return true;
  }

  //----------------------------------------------------------------------------
  // Notify the handler about read events
  //----------------------------------------------------------------------------
  bool PollerLibEvent::EnableReadNotification( Socket  *socket,
                                               bool     notify,
                                               uint16_t timeout )
  {
    Log *log = DefaultEnv::GetLog();

    if( !socket )
    {
      log->Error( PollerMsg, "Invalid socket, read events unavailable" );
      return false;
    }

    //--------------------------------------------------------------------------
    // Check if the socket is registered
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );
    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
    {
      log->Warning( PollerMsg, "%s Socket is not registered",
                               socket->GetName().c_str() );
      return false;
    }

    PollerHelper *helper = (PollerHelper*)it->second;

    //--------------------------------------------------------------------------
    // Enable read notifications
    //--------------------------------------------------------------------------
    if( notify )
    {
      if( helper->readEnabled )
        return true;

      log->Dump( PollerMsg, "%s Enable read notifications, timeout: %d",
                 socket->GetName().c_str(), timeout );

      //------------------------------------------------------------------------
      // Create the read event if it doesn't exist
      //------------------------------------------------------------------------
      if( !helper->readEvent )
      {
        event *ev = ::event_new( pEventBase, socket->GetFD(),
                                 EV_TIMEOUT|EV_READ|EV_PERSIST,
                                 ReadEventCallback, it->second );

        if( !ev )
        {
          log->Error( PollerMsg, "%s Unable to create a read event",
                                 socket->GetName().c_str() );
          return false;
        }

        helper->readEvent = ev;
      }

      //------------------------------------------------------------------------
      // Add the event to the loop
      //------------------------------------------------------------------------
      timeval tOut = { timeout, 0 };
      if( ::event_add( helper->readEvent, &tOut ) != 0 )
      {
        log->Error( PollerMsg, "%s Unable to register a read event",
                               socket->GetName().c_str() );
        return false;
      }

      helper->readEnabled = true;
    }

    //--------------------------------------------------------------------------
    // Disable read notifications
    //--------------------------------------------------------------------------
    else
    {
      if( !helper->readEnabled )
        return true;

      log->Dump( PollerMsg, "%s Disable read notifications",
                            socket->GetName().c_str() );

      if( ::event_del( helper->readEvent ) != 0 )
      {
        log->Error( PollerMsg, "%s Failed to unregister a read event" );
        return false;
      }
      helper->readEnabled = false;
    }
    return true;
  }

  //----------------------------------------------------------------------------
  // Notify the handler about write events
  //----------------------------------------------------------------------------
  bool PollerLibEvent::EnableWriteNotification( Socket  *socket,
                                                bool     notify,
                                                uint16_t timeout )
  {
    Log *log = DefaultEnv::GetLog();

    if( !socket )
    {
      log->Error( PollerMsg, "Invalid socket, write events unavailable" );
      return false;
    }

    //--------------------------------------------------------------------------
    // Check if the socket is registered
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );
    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
    {
      log->Warning( PollerMsg, "%s Socket is not registered",
                               socket->GetName().c_str() );
      return false;
    }

    PollerHelper *helper = (PollerHelper*)it->second;

    //--------------------------------------------------------------------------
    // Enable write notifications
    //--------------------------------------------------------------------------
    if( notify )
    {
      if( helper->writeEnabled )
        return true;

      log->Dump( PollerMsg, "%s Enable write notifications, timeout: %d",
                 socket->GetName().c_str(), timeout );

      //------------------------------------------------------------------------
      // Create the read event if it doesn't exist
      //------------------------------------------------------------------------
      if( !helper->writeEvent )
      {
        event *ev = ::event_new( pEventBase, socket->GetFD(),
                                 EV_TIMEOUT|EV_WRITE|EV_PERSIST,
                                 WriteEventCallback, it->second );

        if( !ev )
        {
          log->Error( PollerMsg, "%s Unable to create a write event",
                                 socket->GetName().c_str() );
          return false;
        }

        helper->writeEvent = ev;
      }

      //------------------------------------------------------------------------
      // Add the event to the loop
      //------------------------------------------------------------------------
      timeval tOut = { timeout, 0 };
      if( ::event_add( helper->writeEvent, &tOut ) != 0 )
      {
        log->Error( PollerMsg, "%s Unable to register a write event",
                               socket->GetName().c_str() );
        return false;
      }

      helper->writeEnabled = true;
    }

    //--------------------------------------------------------------------------
    // Disable write notifications
    //--------------------------------------------------------------------------
    else
    {
      if( !helper->writeEnabled )
        return true;

      log->Dump( PollerMsg, "%s Disable write notifications",
                            socket->GetName().c_str() );

      if( ::event_del( helper->writeEvent ) != 0 )
      {
        log->Error( PollerMsg, "%s Failed to unregister the write event",
                               socket->GetName().c_str() );
        return false;
      }

      helper->writeEnabled = false;
    }
    return true;
  }

  //----------------------------------------------------------------------------
  // Run the event loop
  //----------------------------------------------------------------------------
  int PollerLibEvent::RunEventLoop()
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( PollerMsg, "Running the event loop..." );
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Create a dummy event in order to keep the event loop running
    //--------------------------------------------------------------------------
    event       *dummy;
    int          dummyPipe[2];

    if( pipe( dummyPipe ) != 0 )
    {
      log->Error( PollerMsg, "Unable to create a pipe: %s", strerror( errno ) );
      return -1;
    }

    dummy = ::event_new( pEventBase, dummyPipe[0], EV_READ|EV_PERSIST,
                         DummyCallback, (char*)"Dummy event" );

    if( !dummy )
    {
      log->Error( PollerMsg, "Unable to create the dummy event." );
      return -1;
    }

    if( ::event_add( dummy, 0 ) != 0 )
    {
      log->Error( PollerMsg, "Unable to append the dummy event to the "
                             "event loop" );
      return -1;
    }

    //--------------------------------------------------------------------------
    // Run the show
    //--------------------------------------------------------------------------
    scopedLock.UnLock();
    int dispatchStatus = ::event_base_dispatch( pEventBase );
    scopedLock.Lock( &pMutex );
    if( dispatchStatus != 0 )
    {
      log->Error( PollerMsg, "Unable to dispatch the event loop" );
      return -1;
    }

    //--------------------------------------------------------------------------
    // Cleanup
    //--------------------------------------------------------------------------
    log->Debug( PollerMsg, "The event loop has been interrupted" );
    if( ::event_del( dummy ) != 0 )
    {
      log->Error( PollerMsg, "Unable to remove the dummy event" );
      return -1;
    }

    ::event_free( dummy );
    close( dummyPipe[0] );
    close( dummyPipe[1] );
    return 0;
  }

  //----------------------------------------------------------------------------
  // Check whether the socket is registered with the poller
  //----------------------------------------------------------------------------
  bool PollerLibEvent::IsRegistered( Socket *socket )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    SocketMap::const_iterator it = pSocketMap.find( socket );
    return it != pSocketMap.end();
  }
}
