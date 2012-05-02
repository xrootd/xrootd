//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POLLER_HH__
#define __XRD_CL_POLLER_HH__

#include <stdint.h>
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
      //! Initializer
      //------------------------------------------------------------------------
      virtual void Initialize( Poller * ) {}

      //------------------------------------------------------------------------
      //! Finalizer
      //------------------------------------------------------------------------
      virtual void Finalize() {};

      //------------------------------------------------------------------------
      //! Called when an event occured on a given socket
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
      //! @param timeout if no read event occured after this time a timeout
      //!                event will be generated
      //------------------------------------------------------------------------
      virtual bool EnableReadNotification( Socket  *socket,
                                           bool     notify,
                                           uint16_t timeout = 60 ) = 0;

      //------------------------------------------------------------------------
      //! Notify the handler about write events
      //! @param socket  the socket
      //! @param notify  specify if the handler should be notified
      //! @param timeout if no write event occured after this time a timeout
      //!                event will be generated
      //------------------------------------------------------------------------
      virtual bool EnableWriteNotification( Socket  *socket,
                                            bool     notify,
                                            uint16_t timeout = 60 ) = 0;

      //------------------------------------------------------------------------
      //! Check whether the socket is registered with the poller
      //------------------------------------------------------------------------
      virtual bool IsRegistered( const Socket *socket )  = 0;

      //------------------------------------------------------------------------
      //! Is the event loop running?
      //------------------------------------------------------------------------
      virtual bool IsRunning() const = 0;
  };
}

#endif // __XRD_CL_POLLER_HH__
