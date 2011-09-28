//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POLLER_HH__
#define __XRD_CL_POLLER_HH__

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Interface
  //----------------------------------------------------------------------------
  class SocketEventListener
  {
    public:
      enum EventType
      {
        ReadyToRead  = 0,  //!< New data has arrived
        ReadyToWrite = 1,  //!< A write won't block
        Error        = 2,  //!< An error occured
        RemoteClose  = 3   //!< The remote side has closed the socket
      };

      virtual void Event( EventType type, int socket ) = 0;
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
      //! Add socket to the polling queue
      //!
      //! @param socket    the socket
      //! @param listeners the listener that need to be added, if you don't add
      //!                  any at this point you may miss some events
      //------------------------------------------------------------------------
      virtual bool AddSocket( int socket, SocketEventListener *listener = 0 ) = 0;

      //------------------------------------------------------------------------
      // Remove the socket
      //------------------------------------------------------------------------
      virtual bool RemoveSocket( int socket ) = 0;

      //------------------------------------------------------------------------
      // Add a new listener to the socket
      //------------------------------------------------------------------------
      virtual bool AddSocketListener( int socket, SocketEventListener *listener ) = 0;
  };
}

#endif // __XRD_CL_POLLER_HH__
