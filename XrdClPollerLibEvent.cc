//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
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
// A helper struct
//------------------------------------------------------------------------------
namespace
{
  struct PollerHelper
  {
    PollerHelper( XrdClient::SocketEventListener *lst,
                  XrdClient::Socket              *sock,
                  XrdClient::Poller              *pol,
                  event                          *ev = 0 ):
      listener( lst ), socket( sock ), poller( pol ), pollerEvent( ev ) {}
    XrdClient::SocketEventListener *listener;
    XrdClient::Socket              *socket;
    XrdClient::Poller              *poller;
    event                          *pollerEvent;
  };
}

//------------------------------------------------------------------------------
// The stuff that needs to stay demangled
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
  void DummyCallback( evutil_socket_t fd, short what, void *arg )
  {
  }

  //----------------------------------------------------------------------------
  // Event callback
  //----------------------------------------------------------------------------
  void EventCallback( evutil_socket_t fd, short what, void *arg )
  {
    using namespace XrdClient;
    PollerHelper *helper = (PollerHelper *)arg;
    uint8_t       ev     = 0;
    if( what | EV_TIMEOUT ) ev |= SocketEventListener::TimeOut;
    if( what | EV_READ    ) ev |= SocketEventListener::ReadyToRead;
    helper->listener->Event( ev, helper->socket, helper->poller );
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
      ::event_del( helper->pollerEvent );
      ::event_free( helper->pollerEvent );
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
  bool PollerLibEvent::AddSocket( Socket              *socket,
                                  SocketEventListener *listener,
                                  uint16_t             timeout )
  {
    Log *log = Utils::GetDefaultLog();
    timeval tOut = { timeout, 0 };
    XrdSysMutexHelper( pMutex );

    log->Debug( PollerMsg, "Adding socket: <%s><--><%s>",
                           socket->GetSockName().c_str(),
                           socket->GetPeerName().c_str() );

    //--------------------------------------------------------------------------
    // Check if the socket is already registered
    //--------------------------------------------------------------------------
    SocketMap::const_iterator it = pSocketMap.find( socket );
    if( it != pSocketMap.end() )
    {
      log->Warning( PollerMsg, "The socket is already registered" );
      return false;
    }

    //--------------------------------------------------------------------------
    // Create the socket event
    //--------------------------------------------------------------------------
    PollerHelper *helper = new PollerHelper( listener, socket, this );
    event *ev = ::event_new( pEventBase, socket->GetFD(),
                             EV_TIMEOUT|EV_READ|EV_PERSIST,
                             EventCallback, helper );

    if( !ev )
    {
      log->Error( PollerMsg, "Unable to create an event." );
      delete helper;
      return false;
    }

    helper->pollerEvent = ev;

    //--------------------------------------------------------------------------
    // Add the event to the loop
    //--------------------------------------------------------------------------
    if( ::event_add( ev, &tOut ) != 0 )
    {
      log->Error( PollerMsg, "Unable to add the event." );
      delete helper;
      ::event_free( ev );
      return false;
    }

    pSocketMap[socket] = helper;

    return true;
  }

  //------------------------------------------------------------------------
  // Remove the socket
  //------------------------------------------------------------------------
  bool PollerLibEvent::RemoveSocket( Socket *socket )
  {
    Log *log = Utils::GetDefaultLog();

    log->Debug( PollerMsg, "Removing socket: <%s><--><%s>",
                           socket->GetSockName().c_str(),
                           socket->GetPeerName().c_str() );

    //--------------------------------------------------------------------------
    // Find the right event
    //--------------------------------------------------------------------------
    XrdSysMutexHelper( pMutex );
    SocketMap::iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
    {
      log->Warning( PollerMsg, "Attempted to remove socket that has not "
                               "been registered" );
      return false;
    }

    //--------------------------------------------------------------------------
    // Remove the event
    //--------------------------------------------------------------------------
    PollerHelper *helper = (PollerHelper*)it->second;

    if( ::event_del( helper->pollerEvent ) != 0 )
    {
      log->Error( PollerMsg, "Failed to remove the socket from the event "
                             "loop" );
      return false;
    }

    ::event_free( helper->pollerEvent );
    delete helper;
    pSocketMap.erase( it );
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
