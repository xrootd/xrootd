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

#ifndef __XRD_CL_POLLER_HH__
#define __XRD_CL_POLLER_HH__

#include <cstdint>
#include <ctime>
#include <string>

namespace XrdCl
{
  class Socket;
  class Poller;

  //----------------------------------------------------------------------------
  //! Interface
  //----------------------------------------------------------------------------
  class SocketHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Event type
      //------------------------------------------------------------------------
      enum EventType
      {
        ReadyToRead  = 0x01,  //!< New data has arrived
        ReadTimeOut  = 0x02,  //!< Read timeout
        ReadyToWrite = 0x04,  //!< Writing won't block
        WriteTimeOut = 0x08   //!< Write timeout
      };

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~SocketHandler() {}

      //------------------------------------------------------------------------
      //! Initializer
      //------------------------------------------------------------------------
      virtual void Initialize( Poller * ) {}

      //------------------------------------------------------------------------
      //! Finalizer
      //------------------------------------------------------------------------
      virtual void Finalize() {};

      //------------------------------------------------------------------------
      //! Called when an event occurred on a given socket
      //------------------------------------------------------------------------
      virtual void Event( uint8_t  type,
                          Socket  *socket ) = 0;

      //------------------------------------------------------------------------
      //! Translate the event type to a string
      //------------------------------------------------------------------------
      static std::string EventTypeToString( uint8_t event )
      {
        std::string ev;
        if( event & ReadyToRead )  ev += "ReadyToRead|";
        if( event & ReadTimeOut )  ev += "ReadTimeOut|";
        if( event & ReadyToWrite ) ev += "ReadyToWrite|";
        if( event & WriteTimeOut ) ev += "WriteTimeOut|";
        ev.erase( ev.length()-1, 1) ;
        return ev;
      }
  };

  //----------------------------------------------------------------------------
  //! Interface for socket pollers
  //----------------------------------------------------------------------------
  class Poller
  {
    public:
      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Poller() {}

      //------------------------------------------------------------------------
      //! Initialize the poller
      //------------------------------------------------------------------------
      virtual bool Initialize() = 0;

      //------------------------------------------------------------------------
      //! Finalize the poller
      //------------------------------------------------------------------------
      virtual bool Finalize() = 0;

      //------------------------------------------------------------------------
      //! Start polling
      //------------------------------------------------------------------------
      virtual bool Start() = 0;

      //------------------------------------------------------------------------
      //! Stop polling
      //------------------------------------------------------------------------
      virtual bool Stop() = 0;

      //------------------------------------------------------------------------
      //! Add socket to the polling loop
      //!
      //! @param socket  the socket
      //! @param handler object handling the events
      //------------------------------------------------------------------------
      virtual bool AddSocket( Socket        *socket,
                              SocketHandler *handler ) = 0;

      //------------------------------------------------------------------------
      //! Remove the socket
      //------------------------------------------------------------------------
      virtual bool RemoveSocket( Socket *socket ) = 0;

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
                                           time_t   timeout = 60 ) = 0;

      //------------------------------------------------------------------------
      //! Notify the handler about write events
      //! @param socket  the socket
      //! @param notify  specify if the handler should be notified
      //! @param timeout if no write event occurred after this time a timeout
      //!                event will be generated
      //------------------------------------------------------------------------
      virtual bool EnableWriteNotification( Socket  *socket,
                                            bool     notify,
                                            time_t   timeout = 60 ) = 0;

      //------------------------------------------------------------------------
      //! Check whether the socket is registered with the poller
      //------------------------------------------------------------------------
      virtual bool IsRegistered( Socket *socket )  = 0;

      //------------------------------------------------------------------------
      //! Is the event loop running?
      //------------------------------------------------------------------------
      virtual bool IsRunning() const = 0;
  };
}

#endif // __XRD_CL_POLLER_HH__
