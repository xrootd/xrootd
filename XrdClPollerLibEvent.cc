//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClPollerLibEvent.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClSocket.hh"

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
    PollerHelper( XrdClient::SocketHandler *hndl,
                  XrdClient::Socket        *sock ):
      handler( hndl ), socket( sock ),
      readEvent( 0 ), writeEvent( 0 ),
      readEnabled( false ), writeEnabled( false ) {}
    XrdClient::SocketHandler *handler;
    XrdClient::Socket        *socket;
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
    using namespace XrdClient;
    Log *log = Utils::GetDefaultLog();
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
    using namespace XrdClient;
    PollerLibEvent *poller = (PollerLibEvent*)arg;
    long result = poller->RunEventLoop();
    return (void*)result;
  }

  //----------------------------------------------------------------------------
  // Dummy callback
  //----------------------------------------------------------------------------
  static void DummyCallback( evutil_socket_t fd, short what, void *arg )
  {
  }

  //----------------------------------------------------------------------------
  // Read event callback
  //----------------------------------------------------------------------------
  static void ReadEventCallback( evutil_socket_t fd, short what, void *arg )
  {
    using namespace XrdClient;

    PollerHelper *helper = (PollerHelper *)arg;
    uint8_t       ev     = 0;

    if( what & EV_TIMEOUT ) ev |= SocketHandler::ReadTimeOut;
    if( what & EV_READ    ) ev |= SocketHandler::ReadyToRead;

    Log *log = Utils::GetDefaultLog();
    log->Dump( PollerMsg, "Got socket read event: %s: %s",
                           helper->socket->GetName().c_str(),
                           SocketHandler::EventTypeToString( ev ).c_str() );

    helper->handler->Event( ev, helper->socket );
  }

  //----------------------------------------------------------------------------
  // Write event callback
  //----------------------------------------------------------------------------
  static void WriteEventCallback( evutil_socket_t fd, short what, void *arg )
  {
    using namespace XrdClient;

    PollerHelper *helper = (PollerHelper *)arg;
    uint8_t       ev     = 0;

    if( what & EV_TIMEOUT ) ev |= SocketHandler::WriteTimeOut;
    if( what & EV_WRITE   ) ev |= SocketHandler::ReadyToWrite;

    Log *log = Utils::GetDefaultLog();
    log->Dump( PollerMsg, "Got socket write event: %s: %s",
                           helper->socket->GetName().c_str(),
                           SocketHandler::EventTypeToString( ev ).c_str() );

    helper->handler->Event( ev, helper->socket );
  }
}

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Initialize the poller
  //----------------------------------------------------------------------------
  bool PollerLibEvent::Initialize()
  {
    Log *log = Utils::GetDefaultLog();

    //--------------------------------------------------------------------------
    // Print some debing info
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
    Log *log = Utils::GetDefaultLog();
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
    Log *log = Utils::GetDefaultLog();
    log->Debug( PollerMsg, "Stopping the poller..." );
    if( !pPollerThreadRunning )
    {
      log->Error( PollerMsg, "The poller is not running" );
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
    return true;
  }

  //------------------------------------------------------------------------
  // Add socket to the polling queue
  //------------------------------------------------------------------------
  bool PollerLibEvent::AddSocket( Socket        *socket,
                                  SocketHandler *handler )
  {
    Log *log = Utils::GetDefaultLog();
    XrdSysMutexHelper( pMutex );

    log->Debug( PollerMsg, "Adding socket: %s", socket->GetName().c_str() );

    //--------------------------------------------------------------------------
    // Check if the socket is already registered
    //--------------------------------------------------------------------------
    if( !socket->IsConnected() )
      return false;

    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it != pSocketMap.end() )
    {
      log->Warning( PollerMsg, "The socket is already registered" );
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
    Log *log = Utils::GetDefaultLog();

    log->Debug( PollerMsg, "Removing socket: %s",
                           socket->GetName().c_str() );

    //--------------------------------------------------------------------------
    // Find the right event
    //--------------------------------------------------------------------------
    XrdSysMutexHelper( pMutex );
    SocketMap::iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
    {
      log->Warning( PollerMsg, "Attempted to remove socket that has not "
                               "been registered: %s",
                               socket->GetName().c_str() );
      return false;
    }

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
        log->Error( PollerMsg, "Failed to unregister a read event for %s" );
        return false;
      }
    }

    if( helper->writeEnabled )
    {
      if( ::event_del( helper->readEvent ) != 0 )
      {
        log->Error( PollerMsg, "Failed to unregister a read event for %s" );
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
                                               bool     enable,
                                               uint16_t timeout )
  {
    Log *log = Utils::GetDefaultLog();

    //--------------------------------------------------------------------------
    // Check if the socket is registered
    //--------------------------------------------------------------------------
    XrdSysMutexHelper( pMutex );
    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
    {
      log->Warning( PollerMsg, "Socket %s is not registered",
                               socket->GetName().c_str() );
      return false;
    }

    PollerHelper *helper = (PollerHelper*)it->second;

    //--------------------------------------------------------------------------
    // Enable read notifications
    //--------------------------------------------------------------------------
    if( enable )
    {
      if( helper->readEnabled )
        return true;

      log->Debug( PollerMsg, "Enable read notifications on: %s",
                             socket->GetName().c_str() );

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
          log->Error( PollerMsg, "Unable to create a read event for %s",
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
        log->Error( PollerMsg, "Unable to register a read event for %s",
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

      log->Debug( PollerMsg, "Disable read notifications on: %s",
                             socket->GetName().c_str() );

      if( ::event_del( helper->readEvent ) != 0 )
      {
        log->Error( PollerMsg, "Failed to unregister a read event for %s" );
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
                                                bool     enable,
                                                uint16_t timeout )
  {
    Log *log = Utils::GetDefaultLog();

    //--------------------------------------------------------------------------
    // Check if the socket is registered
    //--------------------------------------------------------------------------
    XrdSysMutexHelper( pMutex );
    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
    {
      log->Warning( PollerMsg, "Socket %s is not registered",
                               socket->GetName().c_str() );
      return false;
    }

    PollerHelper *helper = (PollerHelper*)it->second;

    //--------------------------------------------------------------------------
    // Enable write notifications
    //--------------------------------------------------------------------------
    if( enable )
    {
      if( helper->writeEnabled )
        return true;

      log->Debug( PollerMsg, "Enable write notifications on: %s",
                             socket->GetName().c_str() );

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
          log->Error( PollerMsg, "Unable to create a write event for %s",
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
        log->Error( PollerMsg, "Unable to register a write event for %s",
                               socket->GetName().c_str() );
        return false;
      }

      helper->writeEnabled = true;
    }

    //--------------------------------------------------------------------------
    // Disable read notifications
    //--------------------------------------------------------------------------
    else
    {
      if( !helper->writeEnabled )
        return true;

      log->Debug( PollerMsg, "Disable write notifications on: %s",
                             socket->GetName().c_str() );

      if( ::event_del( helper->writeEvent ) != 0 )
      {
        log->Error( PollerMsg, "Failed to unregister the write event for %s" );
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
    Log *log = Utils::GetDefaultLog();
    log->Debug( PollerMsg, "Running the event loop..." );

    //--------------------------------------------------------------------------
    // Create a dummy event in order to keep the event loop running
    //--------------------------------------------------------------------------
    event       *dummy;
    int          dummyPipe[2];
    pipe( dummyPipe );

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
    if( ::event_base_dispatch( pEventBase ) != 0 )
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
  bool PollerLibEvent::IsRegistered( const Socket *socket )
  {
    XrdSysMutexHelper( pMutex );
    SocketMap::const_iterator it = pSocketMap.find( socket );
    return it != pSocketMap.end();
  }
}
