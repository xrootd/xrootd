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

#ifndef __XRD_CL_CHANNEL_HANDLER_LIST_HH__
#define __XRD_CL_CHANNEL_HANDLER_LIST_HH__

#include <list>
#include <utility>
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClStatus.hh"
#include <XrdSys/XrdSysPthread.hh>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! A helper for handling channel event handlers
  //----------------------------------------------------------------------------
  class ChannelHandlerList
  {
    public:
      //------------------------------------------------------------------------
      //! Add a channel event handler
      //------------------------------------------------------------------------
      void AddHandler( ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Remove the channel event handler
      //------------------------------------------------------------------------
      void RemoveHandler( ChannelEventHandler *handler );

      //------------------------------------------------------------------------
      //! Report an event to the channel event handlers
      //------------------------------------------------------------------------
      void ReportEvent( ChannelEventHandler::ChannelEvent event,
                        Status                            status,
                        uint16_t                          stream );

    private:
      std::list<ChannelEventHandler*> pHandlers;
      XrdSysMutex                     pMutex;
  };
}

#endif // __XRD_CL_CHANNEL_HANDLER_LIST_HH__
