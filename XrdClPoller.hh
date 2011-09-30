//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POLLER_HH__
#define __XRD_CL_POLLER_HH__

#include <stdint.h>

namespace XrdClient
{
  class Socket;
  class Poller;

  //----------------------------------------------------------------------------
  //! Interface
  //----------------------------------------------------------------------------
  class SocketEventListener
  {
    public:
      enum EventType
      {
        TimeOut      = 0x01,  //!< Timeout
        ReadyToRead  = 0x02,  //!< New data has arrived
      };

      virtual void Event( uint8_t  type,
                          Socket  *socket,
                          Poller  *poller ) = 0;
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
      //! @param socket   the socket
      //! @param listener a listener obhect that will handle
      //! @param timeout  signal a timeout after this many seconds of inactivity
      //------------------------------------------------------------------------
      virtual bool AddSocket( Socket              *socket,
                              SocketEventListener *listener,
                              uint16_t             timeout ) = 0;

      //------------------------------------------------------------------------
      //! Remove the socket
      //------------------------------------------------------------------------
      virtual bool RemoveSocket( Socket *socket ) = 0;

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
