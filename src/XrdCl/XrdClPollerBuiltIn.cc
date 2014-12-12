//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClPollerBuiltIn.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClOptimizers.hh"
#include "XrdSys/XrdSysIOEvents.hh"

namespace
{
  //----------------------------------------------------------------------------
  // A helper struct passed to the callback as a custom arg
  //----------------------------------------------------------------------------
  struct PollerHelper
  {
    PollerHelper():
      channel(0), callBack(0), readEnabled(false), writeEnabled(false),
      readTimeout(0), writeTimeout(0)
    {}
    XrdSys::IOEvents::Channel  *channel;
    XrdSys::IOEvents::CallBack *callBack;
    bool                        readEnabled;
    bool                        writeEnabled;
    uint16_t                    readTimeout;
    uint16_t                    writeTimeout;
  };

  //----------------------------------------------------------------------------
  // Call back implementation
  //----------------------------------------------------------------------------
  class SocketCallBack: public XrdSys::IOEvents::CallBack
  {
    public:
      SocketCallBack( XrdCl::Socket *sock, XrdCl::SocketHandler *sh ):
        pSocket( sock ), pHandler( sh ) {}
      virtual ~SocketCallBack() {};

      virtual bool Event( XrdSys::IOEvents::Channel *chP,
                          void                      *cbArg,
                          int                        evFlags )
      {
        using namespace XrdCl;
        uint8_t ev      = 0;

        if( evFlags & ReadyToRead )  ev |= SocketHandler::ReadyToRead;
        if( evFlags & ReadTimeOut )  ev |= SocketHandler::ReadTimeOut;
        if( evFlags & ReadyToWrite ) ev |= SocketHandler::ReadyToWrite;
        if( evFlags & WriteTimeOut ) ev |= SocketHandler::WriteTimeOut;

        Log *log = DefaultEnv::GetLog();
        if( unlikely(log->GetLevel() >= Log::DumpMsg) )
        {
          log->Dump( PollerMsg, "%s Got an event: %s",
                                pSocket->GetName().c_str(),
                                SocketHandler::EventTypeToString( ev ).c_str() );
        }

        pHandler->Event( ev, pSocket );
        return true;
      }
    private:
      XrdCl::Socket        *pSocket;
      XrdCl::SocketHandler *pHandler;
  };
}


namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Initialize the poller
  //----------------------------------------------------------------------------
  bool PollerBuiltIn::Initialize()
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Finalize the poller
  //----------------------------------------------------------------------------
  bool PollerBuiltIn::Finalize()
  {
    //--------------------------------------------------------------------------
    // Clean up the channels
    //--------------------------------------------------------------------------
    SocketMap::iterator it;
    for( it = pSocketMap.begin(); it != pSocketMap.end(); ++it )
    {
      PollerHelper *helper = (PollerHelper*)it->second;
             helper->channel->Delete();
      delete helper->callBack;
      delete helper;
    }
    pSocketMap.clear();

    return true;
  }

  //------------------------------------------------------------------------
  // Start polling
  //------------------------------------------------------------------------
  bool PollerBuiltIn::Start()
  {
    //--------------------------------------------------------------------------
    // Start the poller
    //--------------------------------------------------------------------------
    using namespace XrdSys;

    Log *log = DefaultEnv::GetLog();
    log->Debug( PollerMsg, "Creating and starting the built-in poller..." );
    XrdSysMutexHelper scopedLock( pMutex );
    int         errNum = 0;
    const char *errMsg = 0;
    pPoller = IOEvents::Poller::Create( errNum, &errMsg );
    if( !pPoller )
    {
      log->Error( PollerMsg, "Unable to create the internal poller object: ",
                             "%s (%s)", strerror( errno ), errMsg );
      return false;
    }

    //--------------------------------------------------------------------------
    // Check if we have any descriptors to reinsert from the last time we
    // were started
    //--------------------------------------------------------------------------
    SocketMap::iterator it;
    for( it = pSocketMap.begin(); it != pSocketMap.end(); ++it )
    {
      PollerHelper *helper = (PollerHelper*)it->second;
      Socket       *socket = it->first;
      helper->channel = new IOEvents::Channel( pPoller, socket->GetFD(),
                                               helper->callBack );
      if( helper->readEnabled )
      {
        bool status = helper->channel->Enable( IOEvents::Channel::readEvents,
                                               helper->readTimeout, &errMsg );
        if( !status )
        {
          log->Error( PollerMsg, "Unable to enable read notifications ",
                      "while re-starting %s (%s)", strerror( errno ), errMsg );

          return false;
        }
      }

      if( helper->writeEnabled )
      {
        bool status = helper->channel->Enable( IOEvents::Channel::writeEvents,
                                               helper->writeTimeout, &errMsg );
        if( !status )
        {
          log->Error( PollerMsg, "Unable to enable write notifications ",
                      "while re-starting %s (%s)", strerror( errno ), errMsg );

          return false;
        }
      }
    }
    return true;
  }

  //------------------------------------------------------------------------
  // Stop polling
  //------------------------------------------------------------------------
  bool PollerBuiltIn::Stop()
  {
    using namespace XrdSys::IOEvents;

    Log *log = DefaultEnv::GetLog();
    log->Debug( PollerMsg, "Stopping the poller..." );

    XrdSysMutexHelper scopedLock( pMutex );
    if( !pPoller )
    {
      log->Debug( PollerMsg, "Stopping a poller that has not been started" );
      return true;
    }

    XrdSys::IOEvents::Poller *poller = pPoller;
    pPoller = 0;

    scopedLock.UnLock();
    poller->Stop();
    delete poller;
    scopedLock.Lock( &pMutex );

    SocketMap::iterator  it;
    const char          *errMsg = 0;

    for( it = pSocketMap.begin(); it != pSocketMap.end(); ++it )
    {
      PollerHelper *helper = (PollerHelper*)it->second;
      Socket       *socket = it->first;
      bool status = helper->channel->Disable( Channel::allEvents, &errMsg );
      if( !status )
      {
        log->Error( PollerMsg, "%s Unable to disable write notifications: %s",
                    socket->GetName().c_str(), errMsg );
      }
      helper->channel->Delete();
      helper->channel = 0;
    }

    return true;
  }

  //------------------------------------------------------------------------
  // Add socket to the polling queue
  //------------------------------------------------------------------------
  bool PollerBuiltIn::AddSocket( Socket        *socket,
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
    PollerHelper *helper = new PollerHelper();
    helper->callBack = new ::SocketCallBack( socket, handler );

    if( pPoller )
    {
      helper->channel  = new XrdSys::IOEvents::Channel( pPoller,
                                                        socket->GetFD(),
                                                        helper->callBack );
    }

    handler->Initialize( this );
    pSocketMap[socket] = helper;
    return true;
  }

  //------------------------------------------------------------------------
  // Remove the socket
  //------------------------------------------------------------------------
  bool PollerBuiltIn::RemoveSocket( Socket *socket )
  {
    using namespace XrdSys::IOEvents;
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Find the right socket
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pMutex );
    SocketMap::iterator it = pSocketMap.find( socket );
    if( it == pSocketMap.end() )
      return true;

    log->Debug( PollerMsg, "%s Removing socket from the poller",
                           socket->GetName().c_str() );

    //--------------------------------------------------------------------------
    // Remove the socket
    //--------------------------------------------------------------------------
    PollerHelper *helper = (PollerHelper*)it->second;
    if( pPoller )
    {
      const char *errMsg;
      bool status = helper->channel->Disable( Channel::allEvents, &errMsg );
      if( !status )
      {
        log->Error( PollerMsg, "%s Unable to disable write notifications: %s",
                    socket->GetName().c_str(), errMsg );
        return false;
      }
      helper->channel->Delete();
    }
    delete helper->callBack;
    delete helper;
    pSocketMap.erase( it );
    return true;
  }

  //----------------------------------------------------------------------------
  // Notify the handler about read events
  //----------------------------------------------------------------------------
  bool PollerBuiltIn::EnableReadNotification( Socket  *socket,
                                              bool     notify,
                                              uint16_t timeout )
  {
    using namespace XrdSys::IOEvents;
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
      helper->readTimeout = timeout;

      log->Dump( PollerMsg, "%s Enable read notifications, timeout: %d",
                            socket->GetName().c_str(), timeout );

      if( pPoller )
      {
        const char *errMsg;
        bool status = helper->channel->Enable( Channel::readEvents, timeout,
                                               &errMsg );
        if( !status )
        {
          log->Error( PollerMsg, "%s Unable to enable read notifications: %s",
                      socket->GetName().c_str(), errMsg );
          return false;
        }
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

      if( pPoller )
      {
        const char *errMsg;
        bool status = helper->channel->Disable( Channel::readEvents, &errMsg );
        if( !status )
        {
          log->Error( PollerMsg, "%s Unable to disable read notifications: %s",
                      socket->GetName().c_str(), errMsg );
          return false;
        }
      }
      helper->readEnabled = false;
    }
    return true;
  }

  //----------------------------------------------------------------------------
  // Notify the handler about write events
  //----------------------------------------------------------------------------
  bool PollerBuiltIn::EnableWriteNotification( Socket  *socket,
                                               bool     notify,
                                               uint16_t timeout )
  {
    using namespace XrdSys::IOEvents;
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

      helper->writeTimeout = timeout;

      log->Dump( PollerMsg, "%s Enable write notifications, timeout: %d",
                            socket->GetName().c_str(), timeout );

      if( pPoller )
      {
        const char *errMsg;
        bool status = helper->channel->Enable( Channel::writeEvents, timeout,
                                               &errMsg );
        if( !status )
        {
          log->Error( PollerMsg, "%s Unable to enable write notifications: %s",
                      socket->GetName().c_str(), errMsg );
          return false;
        }
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

      log->Dump( PollerMsg, "%s Disable write notifications",
                            socket->GetName().c_str() );
      if( pPoller )
      {
        const char *errMsg;
        bool status = helper->channel->Disable( Channel::writeEvents, &errMsg );
        if( !status )
        {
          log->Error( PollerMsg, "%s Unable to disable write notifications: %s",
                      socket->GetName().c_str(), errMsg );
          return false;
        }
      }
      helper->writeEnabled = false;
    }
    return true;
  }

  //----------------------------------------------------------------------------
  // Check whether the socket is registered with the poller
  //----------------------------------------------------------------------------
  bool PollerBuiltIn::IsRegistered( Socket *socket )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    SocketMap::iterator it = pSocketMap.find( socket );
    return it != pSocketMap.end();
  }
}
