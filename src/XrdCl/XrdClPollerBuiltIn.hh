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

#ifndef __XRD_CL_POLLER_BUILT_IN_HH__
#define __XRD_CL_POLLER_BUILT_IN_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClPoller.hh"
#include <map>
#include <vector>


namespace XrdSys { namespace IOEvents
{
  class Poller;
}; };

namespace XrdCl
{
  class AnyObject;

  //----------------------------------------------------------------------------
  //! A poller implementation using the build-in XRootD poller
  //----------------------------------------------------------------------------
  class PollerBuiltIn: public Poller
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      PollerBuiltIn() : pNbPoller( GetNbPollerInit() ){}

      ~PollerBuiltIn() {}

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
        return !pPollerPool.empty();
      }

    private:

      //------------------------------------------------------------------------
      //! Goes over poller threads in round robin fashion
      //------------------------------------------------------------------------
      XrdSys::IOEvents::Poller* GetNextPoller();

      //------------------------------------------------------------------------
      //! Registers given socket as a poller user and returns the poller object
      //------------------------------------------------------------------------
      XrdSys::IOEvents::Poller* RegisterAndGetPoller(const Socket *socket);

      //------------------------------------------------------------------------
      //! Unregisters given socket from poller object
      //------------------------------------------------------------------------
      void UnregisterFromPoller( const Socket *socket);

      //------------------------------------------------------------------------
      //! Returns the poller object associated with the given socket
      //------------------------------------------------------------------------
      XrdSys::IOEvents::Poller* GetPoller(const Socket *socket);

      //------------------------------------------------------------------------
      //! Gets the initial value for 'pNbPoller'
      //------------------------------------------------------------------------
      static int GetNbPollerInit();

      // associates channel ID to a pair: poller and count (how many sockets where mapped to this poller)
      typedef std::map<const AnyObject *, std::pair<XrdSys::IOEvents::Poller *, size_t> > PollerMap;

      typedef std::map<Socket *, void *>              SocketMap;
      typedef std::vector<XrdSys::IOEvents::Poller *> PollerPool;

      SocketMap            pSocketMap;
      PollerMap            pPollerMap;
      PollerPool           pPollerPool;
      PollerPool::iterator pNext;
      const int            pNbPoller;
      XrdSysMutex          pMutex;
  };
}

#endif // __XRD_CL_POLLER_BUILT_IN_HH__
