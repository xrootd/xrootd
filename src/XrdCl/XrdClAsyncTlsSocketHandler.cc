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

#include "XrdCl/XrdClAsyncTlsSocketHandler.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClTls.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClTlsSocket.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  AsyncTlsSocketHandler::AsyncTlsSocketHandler( Poller           *poller,
                                                TransportHandler *transport,
                                                AnyObject        *channelData,
                                                uint16_t          subStreamNum ):
    AsyncSocketHandler( poller, transport, channelData, subStreamNum )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  AsyncTlsSocketHandler::~AsyncTlsSocketHandler()
  {
  }

  //----------------------------------------------------------------------------
  // Connect returned
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::OnConnectionReturn()
  {
    AsyncSocketHandler::OnConnectionReturn();

    if( pSocket->GetStatus() == Socket::Connected )
    {
      //------------------------------------------------------------------------
      // Upgrade socket to TLS
      //------------------------------------------------------------------------
      // what to do with status ??? TODO
      pSocket->EnableEncryption( this );
    }
  }
}

