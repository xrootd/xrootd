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

#include "XrdClTests/XRootDProtocolHelper.hh"
#include <arpa/inet.h>
#include <XProtocol/XProtocol.hh>

//------------------------------------------------------------------------------
// Handle XRootD Log-in
//------------------------------------------------------------------------------
bool XRootDProtocolHelper::HandleLogin( int socket, XrdCl::Log *log )
{
  //----------------------------------------------------------------------------
  // Handle the handshake
  //----------------------------------------------------------------------------
  char handShakeBuffer[20];
  if( ::read( socket, handShakeBuffer, 20 ) != 20 )
  {
    log->Error( 1, "Unable to read the handshake: %s", ::strerror( errno ) );
    return false;
  }

  //----------------------------------------------------------------------------
  // Respond to the handshake
  //----------------------------------------------------------------------------
  char serverHandShake[16]; memset( serverHandShake, 0, 16 );
  ServerInitHandShake *hs = (ServerInitHandShake *)(serverHandShake+4);
  hs->msglen   = ::htonl(8);
  hs->protover = ::htonl( kXR_PROTOCOLVERSION );
  hs->msgval   = ::htonl( kXR_DataServer );
  if( ::write( socket, serverHandShake, 16 ) != 16 )
  {
    log->Error( 1, "Unable to write the handshake response: %s",
                   ::strerror( errno ) );
    return false;
  }

  //----------------------------------------------------------------------------
  // Handle the protocol request
  //----------------------------------------------------------------------------
  char protocolBuffer[24];
  if( ::read( socket, protocolBuffer, 24 ) != 24 )
  {
    log->Error( 1, "Unable to read the protocol request: %s", ::strerror( errno ) );
    return false;
  }

  //----------------------------------------------------------------------------
  // Respond to protocol
  //----------------------------------------------------------------------------
  ServerResponse serverProtocol; memset( &serverProtocol, 0, 16 );
  serverProtocol.hdr.dlen            = ::htonl( 8 );
  serverProtocol.body.protocol.pval  = ::htonl( kXR_PROTOCOLVERSION );
  serverProtocol.body.protocol.flags = ::htonl( kXR_isServer );
  if( ::write( socket, &serverProtocol, 16 ) != 16 )
  {
    log->Error( 1, "Unable to write the protocol response: %s",
                   ::strerror( errno ) );
    return false;
  }

  //----------------------------------------------------------------------------
  // Handle the login
  //----------------------------------------------------------------------------
  char loginBuffer[24];
  if( ::read( socket, loginBuffer, 24 ) != 24 )
  {
    log->Error( 1, "Unable to read the login request: %s", ::strerror( errno ) );
    return false;
  }

  //----------------------------------------------------------------------------
  // Respond to login
  //----------------------------------------------------------------------------
  ServerResponse serverLogin; memset( &serverLogin, 0, 24 );
  serverLogin.hdr.dlen = ::htonl( 16 );
  if( ::write( socket, &serverLogin, 24 ) != 24 )
  {
    log->Error( 1, "Unable to write the login response: %s",
                   ::strerror( errno ) );
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
// Handle disconnection
//------------------------------------------------------------------------------
bool XRootDProtocolHelper::HandleClose( int socket, XrdCl::Log *log )
{
  return true;
}

//------------------------------------------------------------------------------
// Receive a message
//------------------------------------------------------------------------------
bool XRootDProtocolHelper::GetMessage( XrdCl::Message *msg, int socket,
                                       XrdCl::Log *log )
{
  return true;
}

