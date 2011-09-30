//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POLLER_LIBEVENT_HH__
#define __XRD_CL_POLLER_LIBEVENT_HH__

#include "XrdSys/XrdSysPthread.hh"

#include "XrdCl/XrdClPoller.hh"
#include <pthread.h>
#include <map>

struct event_base;
struct event;

namespace XrdClient
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
      //! @param socket   the socket
      //! @param listener a listener obhect that will handle
      //! @param timeout  signal a timeout after this many seconds of inactivity
      //------------------------------------------------------------------------
      virtual bool AddSocket( Socket              *socket,
                              SocketEventListener *listener,
                              uint16_t             timeout );

      //------------------------------------------------------------------------
      //! Remove the socket
      //------------------------------------------------------------------------
      virtual bool RemoveSocket( Socket *socket );

      //------------------------------------------------------------------------
      //! Check whether the socket is registered with the poller
      //------------------------------------------------------------------------
      virtual bool IsRegistered( const Socket *socket );

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
