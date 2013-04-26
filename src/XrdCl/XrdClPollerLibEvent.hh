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

#ifndef __XRD_CL_POLLER_LIBEVENT_HH__
#define __XRD_CL_POLLER_LIBEVENT_HH__

#include "XrdSys/XrdSysPthread.hh"

#include "XrdCl/XrdClPoller.hh"
#include <pthread.h>
#include <map>

struct event_base;
struct event;

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! A poller implementation using libEvent underneath
  //----------------------------------------------------------------------------
  class PollerLibEvent: public Poller
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      PollerLibEvent(): pEventBase( 0 ), pPollerThreadRunning( false ) {}

      //------------------------------------------------------------------------
      //! Initialize the poller
      //------------------------------------------------------------------------
      virtual bool Initialize();

      //------------------------------------------------------------------------
      //! Finalize the poller
      //------------------------------------------------------------------------
      virtual bool Finalize();

      //------------------------------------------------------------------------
      //! Start polling
      //------------------------------------------------------------------------
      virtual bool Start();

      //------------------------------------------------------------------------
      //! Stop polling
      //------------------------------------------------------------------------
      virtual bool Stop();

      //------------------------------------------------------------------------
      //! Add socket to the polling loop
      //!
      //! @param socket  the socket
      //! @param handler object handling the events
      //------------------------------------------------------------------------
      virtual bool AddSocket( Socket        *socket,
                              SocketHandler *handler );


      //------------------------------------------------------------------------
      //! Remove the socket
      //------------------------------------------------------------------------
      virtual bool RemoveSocket( Socket *socket );

      //------------------------------------------------------------------------
      //! Notify the handler about read events
      //!
      //! @param socket  the socket
      //! @param notify  specify if the handler should be notified
      //! @param timeout if no read event occurred after this time a timeout
      //!                event will be generated
      //------------------------------------------------------------------------
      virtual bool EnableReadNotification( Socket  *socket,
                                           bool     notify,
                                           uint16_t timeout = 60 );

      //------------------------------------------------------------------------
      //! Notify the handler about write events
      //!
      //! @param socket  the socket
      //! @param notify  specify if the handler should be notified
      //! @param timeout if no write event occurred after this time a timeout
      //!                event will be generated
      //------------------------------------------------------------------------
      virtual bool EnableWriteNotification( Socket  *socket,
                                            bool     notify,
                                            uint16_t timeout = 60);

      //------------------------------------------------------------------------
      //! Check whether the socket is registered with the poller
      //------------------------------------------------------------------------
      virtual bool IsRegistered( Socket *socket );

      //------------------------------------------------------------------------
      //! Is the event loop running?
      //------------------------------------------------------------------------
      virtual bool IsRunning() const
      {
        return pPollerThreadRunning;
      }

      //------------------------------------------------------------------------
      //! Run the libevent event loop
      //------------------------------------------------------------------------
      int RunEventLoop();

    private:
      typedef std::map<const Socket *, void *> SocketMap;
      SocketMap   pSocketMap;
      XrdSysMutex pMutex;
      event_base *pEventBase;
      pthread_t   pPollerThread;
      bool        pPollerThreadRunning;
  };
}

#endif // __XRD_CL_POLLER_LIBEVENT_HH__
