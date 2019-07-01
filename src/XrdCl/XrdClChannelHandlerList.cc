//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClChannelHandlerList.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Add a channel event handler
  //----------------------------------------------------------------------------
  void ChannelHandlerList::AddHandler( ChannelEventHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    pHandlers.push_back( handler );
  }

  //----------------------------------------------------------------------------
  // Remove the channel event handler
  //----------------------------------------------------------------------------
  void ChannelHandlerList::RemoveHandler( ChannelEventHandler *handler )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    std::list<ChannelEventHandler*>::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
    {
      if( *it == handler )
      {
        pHandlers.erase( it );
        return;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Report an event to the channel event handlers
  //----------------------------------------------------------------------------
  void ChannelHandlerList::ReportEvent(
                        ChannelEventHandler::ChannelEvent event,
                        Status                            status )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    std::list<ChannelEventHandler*>::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); )
    {
      bool st = (*it)->OnChannelEvent( event, status );
      if( !st )
        it = pHandlers.erase( it );
      else
        ++it;
    }
  }
}

