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

#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClPollerLibEvent.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessage.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  PostMaster::PostMaster()
  {
    pPoller           = new PollerLibEvent();
    pTransportHandler = new XRootDTransport();
    pTaskManager      = new TaskManager();
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  PostMaster::~PostMaster()
  {
    delete pPoller;
    delete pTransportHandler;
    delete pTaskManager;
  }

  //----------------------------------------------------------------------------
  // Initializer
  //----------------------------------------------------------------------------
  bool PostMaster::Initialize()
  {
    return pPoller->Initialize();
  }

  //----------------------------------------------------------------------------
  // Finalizer
  //----------------------------------------------------------------------------
  bool PostMaster::Finalize()
  {
    //--------------------------------------------------------------------------
    // Clean up the channels
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( pChannelMapMutex );
    ChannelMap::iterator it;
    for( it = pChannelMap.begin(); it != pChannelMap.end(); ++it )
      delete it->second;
    pChannelMap.clear();
    return pPoller->Finalize();
  }

  //----------------------------------------------------------------------------
  // Start the post master
  //----------------------------------------------------------------------------
  bool PostMaster::Start()
  {
    if( !pPoller->Start() )
      return false;

    if( !pTaskManager->Start() )
    {
      pPoller->Stop();
      return false;
    }
    return true;
  }

  //----------------------------------------------------------------------------
  // Stop the postmaster
  //----------------------------------------------------------------------------
  bool PostMaster::Stop()
  {
    if( !pPoller->Stop() )
      return false;
    if( !pTaskManager->Stop() )
      return false;
    return true;
  }

  //----------------------------------------------------------------------------
  // Reinitialize after fork
  //----------------------------------------------------------------------------
  bool PostMaster::Reinitialize()
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Send a message synchronously
  //----------------------------------------------------------------------------
  Status PostMaster::Send( const URL &url, Message *msg, uint16_t timeout )
  {
    Channel *channel = GetChannel( url );
    return channel->Send( msg, timeout );
  }

  //----------------------------------------------------------------------------
  // Send the message asynchronously
  //----------------------------------------------------------------------------
  Status PostMaster::Send( const URL            &url,
                           Message              *msg,
                           MessageStatusHandler *handler,
                           uint16_t              timeout )
  {
    Channel *channel = GetChannel( url );
    return channel->Send( msg, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Synchronously receive a message
  //----------------------------------------------------------------------------
  Status PostMaster::Receive( const URL      &url,
                              Message       *&msg,
                              MessageFilter  *filter,
                              uint16_t        timeout )
  {
    Channel *channel = GetChannel( url );
    return channel->Receive( msg, filter, timeout );
  }

  //----------------------------------------------------------------------------
  // Listen to incomming messages
  //----------------------------------------------------------------------------
  Status PostMaster::Receive( const URL      &url,
                              MessageHandler *handler,
                              uint16_t        timeout )
  {
    Channel *channel = GetChannel( url );
    return channel->Receive( handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Query the transport handler
  //----------------------------------------------------------------------------
  Status PostMaster::QueryTransport( const URL &url,
                                     uint16_t   query,
                                     AnyObject &result )
  {
    Channel *channel = GetChannel( url );
    return channel->QueryTransport( query, result );
  }


  //----------------------------------------------------------------------------
  // Get the channel
  //----------------------------------------------------------------------------
  Channel *PostMaster::GetChannel( const URL &url )
  {
    XrdSysMutexHelper scopedLock( pChannelMapMutex );
    Channel *channel = 0;
    ChannelMap::iterator it = pChannelMap.find( url.GetHostId() );
    if( it == pChannelMap.end() )
    {
      channel = new Channel( url, pPoller, pTransportHandler, pTaskManager );
      pChannelMap[url.GetHostId()] = channel;
    }
    else
      channel = it->second;
    return channel;
  }
}
