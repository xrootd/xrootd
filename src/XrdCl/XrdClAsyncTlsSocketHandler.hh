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
#include "XrdCl/XrdClTlsSocket.hh"
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
      // Connect returned
      //------------------------------------------------------------------------
      void OnConnectionReturn();
  };
}

#endif // __XRD_CL_ASYNC_TLS_SOCKET_HANDLER_HH__
