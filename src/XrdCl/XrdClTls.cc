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

#include "XrdCl/XrdClTls.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClSocket.hh"

#include "XrdTls/XrdTlsContext.hh"

namespace
{
  static const char* GetCaDir()
  {
    const char *envval = getenv("X509_CERT_DIR");
    static const std::string cadir = envval ? envval : "/etc/grid-security/certificates";;
    return cadir.c_str();
  }
}

namespace XrdCl
{
  //------------------------------------------------------------------------
  // Constructor
  //------------------------------------------------------------------------
  Tls::Tls( Socket *socket, AsyncSocketHandler *socketHandler ) : pSocket( socket ), pTlsHSRevert( None ), pSocketHandler( socketHandler )
  {
    static XrdTlsContext tlsContext( 0, 0, GetCaDir(), 0, 0 );
    //----------------------------------------------------------------------
    // If the context is not valid throw an exception! We throw generic
    // exception as this will be translated to TlsError anyway.
    //----------------------------------------------------------------------
    if( !tlsContext.Context() ) throw std::exception();

    pTls.reset(
        new XrdTlsSocket( tlsContext, pSocket->GetFD(), XrdTlsSocket::TLS_RNB_WNB,
                          XrdTlsSocket::TLS_HS_NOBLK, true ) );
  }

  //------------------------------------------------------------------------
  // Establish a TLS/SSL session and perform host verification.
  //------------------------------------------------------------------------
  Status Tls::Connect( const std::string &thehost, XrdNetAddrInfo *netInfo )
  {
    XrdTls::RC error = pTls->Connect( thehost.c_str(), netInfo );
    Status status = ToStatus( error );

    //--------------------------------------------------------------------------
    // There's no follow up if the read simply failed
    //--------------------------------------------------------------------------
    if( !status.IsOK() ) return status;

    if( pTls->NeedHandShake() )
    {
      //------------------------------------------------------------------------
      // Make sure the socket is uncorked so the TLS hand-shake can go through
      //------------------------------------------------------------------------
      if( pSocket->IsCorked() )
      {
        Status st = pSocket->Uncork();
        if( !st.IsOK() ) return st;
      }

      //----------------------------------------------------------------------
      // Check if TLS hand-shake wants to write something
      //----------------------------------------------------------------------
      if( error == XrdTls::TLS_WantWrite )
      {
        Status st = pSocketHandler->EnableUplink();
        if( !st.IsOK() ) return st;
      }
      //----------------------------------------------------------------------
      // Otherwise disable uplink
      //----------------------------------------------------------------------
      else if( error == XrdTls::TLS_WantRead )
      {
        Status st = pSocketHandler->DisableUplink();
        if( !st.IsOK() ) return st;
      }
    }

    return status;
  }

  Status Tls::Read( char *buffer, size_t size, int &bytesRead )
  {
    //--------------------------------------------------------------------------
    // If necessary, TLS_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call connect or do_handshake.
    //--------------------------------------------------------------------------
    XrdTls::RC error = pTls->Read( buffer, size, bytesRead );
    Status status = ToStatus( error );

    //--------------------------------------------------------------------------
    // There's no follow up if the read simply failed
    //--------------------------------------------------------------------------
    if( !status.IsOK() ) return status;

    if( pTls->NeedHandShake() )
    {
      //------------------------------------------------------------------------
      // Make sure the socket is uncorked so the TLS hand-shake can go through
      //------------------------------------------------------------------------
      if( pSocket->IsCorked() )
      {
        Status st = pSocket->Uncork();
        if( !st.IsOK() ) return st;
      }

      //----------------------------------------------------------------------
      // Check if we need to switch on a revert state
      //----------------------------------------------------------------------
      if( error == XrdTls::TLS_WantWrite )
      {
        pTlsHSRevert = ReadOnWrite;
        Status st = pSocketHandler->EnableUplink();
        if( !st.IsOK() ) status = st;
        //--------------------------------------------------------------------
        // Return early so the revert state wont get cleared
        //--------------------------------------------------------------------
        return status;
      }
    }

    //------------------------------------------------------------------------
    // If we got up until here we need to clear the revert state
    //------------------------------------------------------------------------
    if( pTlsHSRevert == ReadOnWrite )
    {
      Status st = pSocketHandler->DisableUplink();
      if( !st.IsOK() ) status = st;
    }
    pTlsHSRevert = None;

    //------------------------------------------------------------------------
    // If we didn't manage to read any data wait for another read event
    //------------------------------------------------------------------------
    if( bytesRead == 0 )
      return Status( stOK, suRetry );

    return status;
  }

  Status Tls::Send( const char *buffer, size_t size, int &bytesWritten )
  {
    //--------------------------------------------------------------------------
    // If necessary, TLS_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call connect or do_handshake.
    //--------------------------------------------------------------------------
    XrdTls::RC error = pTls->Write( buffer, size, bytesWritten );
    Status status = ToStatus( error );

    //--------------------------------------------------------------------------
    // There's no follow up if the write simply failed
    //--------------------------------------------------------------------------
    if( !status.IsOK() ) return status;

    //--------------------------------------------------------------------------
    // We are in the middle of a TLS hand-shake
    //--------------------------------------------------------------------------
    if( pTls->NeedHandShake() )
    {
      //------------------------------------------------------------------------
      // Make sure the socket is uncorked so the TLS hand-shake can go through
      //------------------------------------------------------------------------
      if( pSocket->IsCorked() )
      {
        Status st = pSocket->Uncork();
        if( !st.IsOK() ) return st;
      }

      //------------------------------------------------------------------------
      // Check if we need to switch on a revert state
      //------------------------------------------------------------------------
      if( error == XrdTls::TLS_WantRead )
      {
        pTlsHSRevert = WriteOnRead;
        Status st = pSocketHandler->DisableUplink();
        if( !st.IsOK() ) status = st;
        //----------------------------------------------------------------------
        // Return early so the revert state wont get cleared
        //----------------------------------------------------------------------
        return status;
      }
    }

    //--------------------------------------------------------------------------
    // If we got up until here we need to clear the revert state
    //--------------------------------------------------------------------------
    if( pTlsHSRevert == WriteOnRead )
    {
      Status st = pSocketHandler->EnableUplink();
      if( !st.IsOK() ) status = st;
    }
    pTlsHSRevert = None;

    //------------------------------------------------------------------------
    // If we didn't manage to read any data wait for another write event
    //
    // Adding this by symmetry, never actually experienced this (for reads
    // it has been experienced)
    //------------------------------------------------------------------------
    if( bytesWritten == 0 )
      return Status( stOK, suRetry );

    return status;
  }

  Status Tls::ToStatus( int error )
  {
    switch( error )
    {
      case XrdTls::TLS_AOK: return Status();

      case XrdTls::TLS_WantConnect:
      case XrdTls::TLS_WantWrite:
      case XrdTls::TLS_WantRead:  return Status( stOK, suRetry, error );

      case XrdTls::TLS_SYS_Error: return Status( stError, errTlsError, errno );

      default:
        return Status( stError, errTlsError, error );
    }
  }

  //------------------------------------------------------------------------
  // Map:
  //     * in case the TLS layer requested reads on writes map
  //       ReadyToWrite to ReadyToRead
  //     * in case the TLS layer requested writes on reads map
  //       ReadyToRead to ReadyToWrite
  //------------------------------------------------------------------------
  uint8_t Tls::MapEvent( uint8_t event )
  {
    if( pTlsHSRevert == ReadOnWrite )
    {
      //------------------------------------------------------------------------
      // In this case we would like to call the OnRead routine on the Write event
      //------------------------------------------------------------------------
      if( event & SocketHandler::ReadyToWrite ) return SocketHandler::ReadyToRead;
    }
    else if( pTlsHSRevert == WriteOnRead )
    {
      //------------------------------------------------------------------------
      // In this case we would like to call the OnWrite routine on the Read event
      //------------------------------------------------------------------------
      if( event & SocketHandler::ReadyToRead ) return SocketHandler::ReadyToWrite;
    }

    return event;
  }
}
