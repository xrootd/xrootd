//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <simonm@cern.ch>
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

#ifndef __XRD_CL_ASYNC_TLS_SOCKET_HANDLER_HH__
#define __XRD_CL_ASYNC_TLS_SOCKET_HANDLER_HH__


#include "XrdCl/XrdClAsyncSocketHandler.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include <memory>

namespace XrdCl
{
  class Tls;
  class XRootDTransport;
  class XRootDMsgHandler;

  //----------------------------------------------------------------------------
  //! Utility class handling asynchronous TLS socket interactions and forwarding
  //! events to the parent stream.
  //----------------------------------------------------------------------------
  class AsyncTlsSocketHandler: public AsyncSocketHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AsyncTlsSocketHandler( Poller           *poller,
                             TransportHandler *transport,
                             AnyObject        *channelData,
                             uint16_t          subStreamNum );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~AsyncTlsSocketHandler();

    private:

      //------------------------------------------------------------------------
      //! Flags to indicate what is the TLS hand-shake revert state
      //!
      //! - None        : there is no revert state
      //! - ReadOnWrite : OnRead routines will be called on write event due to
      //!                 TLS handshake
      //! - WriteOnRead : OnWrite routines will be called on read event due to
      //!                 TLS handshake
      //------------------------------------------------------------------------
      enum TlsHSRevert{ None, ReadOnWrite, WriteOnRead };

      //------------------------------------------------------------------------
      //! Handle a socket event
      //------------------------------------------------------------------------
      virtual void Event( uint8_t type, XrdCl::Socket *socket );

      //------------------------------------------------------------------------
      // Connect returned
      //------------------------------------------------------------------------
      void OnConnectionReturn();

      //------------------------------------------------------------------------
      // Got a write readiness event
      //------------------------------------------------------------------------
      void OnWrite();

      //------------------------------------------------------------------------
      // Got a write readiness event while handshaking
      //------------------------------------------------------------------------
      void OnWriteWhileHandshaking();

      //------------------------------------------------------------------------
      // Write the current message
      //------------------------------------------------------------------------
      Status WriteCurrentMessage( Message *toWrite );

      //------------------------------------------------------------------------
      // Write the current body chunk
      //------------------------------------------------------------------------
      Status WriteCurrentChunk( ChunkInfo &toWrite );

      //------------------------------------------------------------------------
      // Got a read readiness event
      //------------------------------------------------------------------------
      void OnRead();

      //------------------------------------------------------------------------
      // Got a read readiness event while handshaking
      //------------------------------------------------------------------------
      void OnReadWhileHandshaking();

      //------------------------------------------------------------------------
      // Read a message
      //------------------------------------------------------------------------
      Status ReadMessage( Message *&toRead );

      //------------------------------------------------------------------------
      // Cork the underlying socket
      //
      // As there is no way to do vector writes with SSL/TLS we need to cork
      // the socket and then flash it when appropriate
      //------------------------------------------------------------------------
      Status Cork();

      //------------------------------------------------------------------------
      // Uncork the underlying socket
      //------------------------------------------------------------------------
      Status Uncork();

      //------------------------------------------------------------------------
      // Flash the underlying socket
      //------------------------------------------------------------------------
      Status Flash();

      //------------------------------------------------------------------------
      // Process the status of an operation that issues a TLS write
      //
      // - If the operation failed there is nothing to be done.
      //
      // - If the operation succeeded with suRetry it might be that TLS layer
      //   asked to reissue the operation but on a read event (SSL_ERROR_WANT_READ)
      //   due to hand-shake re-negotiations, in this case we need to set a
      //   respective TLS revert state (pTlsHSRevert=WriteOnRead).
      //
      // - Otherwise we need to clear the revert state (pTlsHSRevert=None)
      //
      //------------------------------------------------------------------------
      inline void OnTlsWrite( Status& st );

      //------------------------------------------------------------------------
      // Process the status of an operation that issues a TLS read
      //
      // - If the operation failed there is nothing to be done.
      //
      // - If the operation succeeded with suRetry it might be that TLS layer
      //   asked to reissue the operation but on a write event (SSL_ERROR_WANT_WRITE)
      //   due to hand-shake re-negotiations, in this case we need to set a
      //   respective TLS revert state (pTlsHSRevert=ReadOnWrite).
      //
      // - Otherwise we need to clear the revert state (pTlsHSRevert=None)
      //
      //------------------------------------------------------------------------
      inline void OnTlsRead( Status& st );

      //------------------------------------------------------------------------
      // Data members
      //------------------------------------------------------------------------
      XRootDTransport               *pXrdTransport;
      XRootDMsgHandler              *pXrdHandler;
      std::unique_ptr<Tls>           pTls;
      bool                           pCorked;
      bool                           pWrtHdrDone;
      //------------------------------------------------------------------------
      // In case during TLS hand-shake WantRead has been returned on write or
      // WantWrite has been returned on read we need to flip the following events.
      //
      // None        : all events should be processed normally
      // ReadOnWrite : on write event the OnRead routines should be called
      // WriteOnRead : on read event the OnWrite routines should be called
      //------------------------------------------------------------------------
      TlsHSRevert                    pTlsHSRevert;
      ChunkList::iterator            pCurrentChunk;
      ChunkList                     *pWrtBody;
  };
}

#endif // __XRD_CL_ASYNC_TLS_SOCKET_HANDLER_HH__
