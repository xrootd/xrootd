//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_SOCKET_HH__
#define __XRD_CL_SOCKET_HH__

#include <stdint.h>
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClStatus.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! A network socket
  //----------------------------------------------------------------------------
  class Socket
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param socket already connected socket if available, -1 otherwise
      //------------------------------------------------------------------------
      Socket( int socket = -1 ): pSocket(-1)
      {
        if( pSocket != -1 )
          pIsConnected = true;
        else
          pIsConnected = false;
      };

      //------------------------------------------------------------------------
      //! Desctuctor
      //------------------------------------------------------------------------
      virtual ~Socket() { Disconnect(); };

      //------------------------------------------------------------------------
      //! Connect to the given URL
      //!
      //! @param url    the address of the host
      //! @param timout timeout in seconds, 0 for no timeout
      //------------------------------------------------------------------------
      Status Connect( const URL &url, uint16_t timout = 10 );

      //------------------------------------------------------------------------
      //! Disconnect
      //------------------------------------------------------------------------
      void Disconnect();

      //------------------------------------------------------------------------
      //! Check whether the socket is connected
      //------------------------------------------------------------------------
      bool IsConnected() const
      {
        return pIsConnected;
      }

      //------------------------------------------------------------------------
      //! Read raw bytes from the socket
      //!
      //! @param buffer    data to be sent
      //! @param size      size of the data buffer
      //! @param timout    timout value in seconds, -1 to wait indefinitely
      //! @param bytesRead the amount of data actually read
      //------------------------------------------------------------------------
      Status ReadRaw( void *buffer, uint32_t size, int32_t timeout,
                      uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Write raw bytes to the socket
      //!
      //! @param buffer       data to be written
      //! @param size         size of the data buffer
      //! @param timout       timeout value in seconds, -1 to wait indefinitely
      //! @param bytedWritten the amount of data actually written
      //------------------------------------------------------------------------
      Status WriteRaw( void *buffer, uint32_t size, uint16_t timeout,
                       uint32_t &bytesWritten );

      //------------------------------------------------------------------------
      //! Get the file descriptor
      //------------------------------------------------------------------------
      int GetFD()
      {
        return pSocket;
      }

      //------------------------------------------------------------------------
      //! Get the name of the socket
      //------------------------------------------------------------------------
      std::string GetSockName() const;

    private:
      //------------------------------------------------------------------------
      //! Poll the socket to see whether it is ready for IO
      //!
      //! @param  readyForReading poll for readiness to read
      //! @param  readyForWriting poll for readiness to write
      //! @param  timeout         timeout in seconds, -1 to wait indefinitely
      //! @return stOK                  - ready for IO
      //!         errSocketDisconnected - on disconnection
      //!         errSocketError        - on socket error
      //!         errSocketTimeout      - on socket timeout
      //!         errInvalidOp          - when called on a non connected socket
      //------------------------------------------------------------------------
      Status Poll( bool readyForReading, bool readyForWriting,
                   uint32_t timeout );

      int                 pSocket;
      bool                pIsConnected;
      mutable std::string pSockName;     // mutable because it's for caching
  };
}

#endif // __XRD_CL_SOCKET_HH__
 
