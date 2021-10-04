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
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"

#include "XrdTls/XrdTls.hh"
#include "XrdTls/XrdTlsContext.hh"

#include <string>
#include <stdexcept>

namespace
{
  //------------------------------------------------------------------------
  // Helper class for setting the message callback for the TLS layer for
  // logging purposes
  //------------------------------------------------------------------------
  struct SetTlsMsgCB
  {
    //----------------------------------------------------------------------
    // The message callback
    //----------------------------------------------------------------------
    static void MsgCallBack(const char *tid, const char *msg, bool sslmsg)
    {
      XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();
      if( sslmsg )
        log->Debug( XrdCl::TlsMsg, "[%s] %s", tid, msg );
      else
        log->Error( XrdCl::TlsMsg, "[%s] %s", tid, msg );
    }

    inline static void Once()
    {
      static SetTlsMsgCB instance;
    }

    private:

      //--------------------------------------------------------------------
      // Constructor. Sets the callback, there should be only one static
      // instance
      //--------------------------------------------------------------------
      inline SetTlsMsgCB()
      {
        XrdTls::SetMsgCB( MsgCallBack );
        XrdTls::SetDebug( TlsDbgLvl(), MsgCallBack );
      }

      //--------------------------------------------------------------------
      // Get TLS debug level
      //--------------------------------------------------------------------
      static int TlsDbgLvl()
      {
        XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
        std::string tlsDbgLvl;
        env->GetString( "TlsDbgLvl", tlsDbgLvl );

        if( tlsDbgLvl == "OFF" ) return XrdTls::dbgOFF;
        if( tlsDbgLvl == "CTX" ) return XrdTls::dbgCTX;
        if( tlsDbgLvl == "SOK" ) return XrdTls::dbgSOK;
        if( tlsDbgLvl == "SIO" ) return XrdTls::dbgSIO;
        if( tlsDbgLvl == "ALL" ) return XrdTls::dbgALL;
        if( tlsDbgLvl == "OUT" ) return XrdTls::dbgOUT;

        return XrdTls::dbgOFF;
      }
  };

  //------------------------------------------------------------------------
  // Helper function for setting the CA directory in TLS context
  //------------------------------------------------------------------------
  static const char* GetCaDir()
  {
    static const char *envval = getenv("X509_CERT_DIR");
    static const std::string cadir = envval ? envval :
                                     "/etc/grid-security/certificates";
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
    //----------------------------------------------------------------------
    // Set the message callback for TLS layer
    //----------------------------------------------------------------------
    SetTlsMsgCB::Once();
    //----------------------------------------------------------------------
    // we only need one instance of TLS
    //----------------------------------------------------------------------
    std::string emsg;
    static XrdTlsContext tlsContext( 0, 0, GetCaDir(), 0, 0, &emsg );

    //----------------------------------------------------------------------
    // If the context is not valid throw an exception! We throw generic
    // exception as this will be translated to TlsError anyway.
    //----------------------------------------------------------------------
    if( !tlsContext.isOK() ) throw std::runtime_error( emsg );

    pTls.reset(
        new XrdTlsSocket( tlsContext, pSocket->GetFD(), XrdTlsSocket::TLS_RNB_WNB,
                          XrdTlsSocket::TLS_HS_NOBLK, true ) );
  }

  //------------------------------------------------------------------------
  // Establish a TLS/SSL session and perform host verification.
  //------------------------------------------------------------------------
  XRootDStatus Tls::Connect( const std::string &thehost, XrdNetAddrInfo *netInfo )
  {
    std::string errmsg;
    const char *verhost = 0;
    if( thehost != "localhost" && thehost != "127.0.0.1" && thehost != "[::1]" )
      verhost = thehost.c_str();
    XrdTls::RC error = pTls->Connect( verhost, &errmsg );
    XRootDStatus status = ToStatus( error );
    if( !status.IsOK() )
      status.SetErrorMessage( errmsg );

    //--------------------------------------------------------------------------
    // There's no follow up if the read simply failed
    //--------------------------------------------------------------------------
    if( !status.IsOK() )
    {
      XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();
      log->Error( XrdCl::TlsMsg, "Failed to do TLS connect: %s", errmsg.c_str() );
      return status;
    }


    if( pTls->NeedHandShake() )
    {
      //------------------------------------------------------------------------
      // Make sure the socket is uncorked so the TLS hand-shake can go through
      //------------------------------------------------------------------------
      if( pSocket->IsCorked() )
      {
        XRootDStatus st = pSocket->Uncork();
        if( !st.IsOK() ) return st;
      }

      //----------------------------------------------------------------------
      // Check if TLS hand-shake wants to write something
      //----------------------------------------------------------------------
      if( error == XrdTls::TLS_WantWrite )
      {
        XRootDStatus st = pSocketHandler->EnableUplink();
        if( !st.IsOK() ) return st;
      }
      //----------------------------------------------------------------------
      // Otherwise disable uplink
      //----------------------------------------------------------------------
      else if( error == XrdTls::TLS_WantRead )
      {
        XRootDStatus st = pSocketHandler->DisableUplink();
        if( !st.IsOK() ) return st;
      }
    }

    return status;
  }

  XRootDStatus Tls::Read( char *buffer, size_t size, int &bytesRead )
  {
    //--------------------------------------------------------------------------
    // If necessary, TLS_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call connect or do_handshake.
    //--------------------------------------------------------------------------
    XrdTls::RC error = pTls->Read( buffer, size, bytesRead );
    XRootDStatus status = ToStatus( error );

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
        XRootDStatus st = pSocket->Uncork();
        if( !st.IsOK() ) return st;
      }

      //----------------------------------------------------------------------
      // Check if we need to switch on a revert state
      //----------------------------------------------------------------------
      if( error == XrdTls::TLS_WantWrite )
      {
        pTlsHSRevert = ReadOnWrite;
        XRootDStatus st = pSocketHandler->EnableUplink();
        if( !st.IsOK() ) status = st;
        //--------------------------------------------------------------------
        // Return early so the revert state won't get cleared
        //--------------------------------------------------------------------
        return status;
      }
    }

    //------------------------------------------------------------------------
    // If we got up until here we need to clear the revert state
    //------------------------------------------------------------------------
    if( pTlsHSRevert == ReadOnWrite )
    {
      XRootDStatus st = pSocketHandler->DisableUplink();
      if( !st.IsOK() ) status = st;
    }
    pTlsHSRevert = None;

    //------------------------------------------------------------------------
    // If we didn't manage to read any data wait for another read event
    //------------------------------------------------------------------------
    if( bytesRead == 0 )
      return XRootDStatus( stOK, suRetry );

    return status;
  }

  //------------------------------------------------------------------------
  //! (Fake) ReadV through the TLS layer from the socket
  //! If necessary, will establish a TLS/SSL session.
  //------------------------------------------------------------------------
  XRootDStatus Tls::ReadV( iovec *iov, int iocnt, int &bytesRead )
  {
    bytesRead = 0;
    for( int i = 0; i < iocnt; ++i )
    {
      int btsread = 0;
      auto st = Read( static_cast<char*>( iov[i].iov_base ),
                      iov[i].iov_len, btsread );
      if( !st.IsOK() ) return st;
      bytesRead += btsread;
      if( st.code == suRetry ) return st;
    }
    return XRootDStatus();
  }

  XRootDStatus Tls::Send( const char *buffer, size_t size, int &bytesWritten )
  {
    //--------------------------------------------------------------------------
    // If necessary, TLS_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call connect or do_handshake.
    //--------------------------------------------------------------------------
    XrdTls::RC error = pTls->Write( buffer, size, bytesWritten );
    XRootDStatus status = ToStatus( error );

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
        XRootDStatus st = pSocket->Uncork();
        if( !st.IsOK() ) return st;
      }

      //------------------------------------------------------------------------
      // Check if we need to switch on a revert state
      //------------------------------------------------------------------------
      if( error == XrdTls::TLS_WantRead )
      {
        pTlsHSRevert = WriteOnRead;
        XRootDStatus st = pSocketHandler->DisableUplink();
        if( !st.IsOK() ) status = st;
        //----------------------------------------------------------------------
        // Return early so the revert state won't get cleared
        //----------------------------------------------------------------------
        return status;
      }
    }

    //--------------------------------------------------------------------------
    // If we got up until here we need to clear the revert state
    //--------------------------------------------------------------------------
    if( pTlsHSRevert == WriteOnRead )
    {
      XRootDStatus st = pSocketHandler->EnableUplink();
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
      return XRootDStatus( stOK, suRetry );

    return status;
  }

  //------------------------------------------------------------------------
  // Shutdown the TLS/SSL connection
  //------------------------------------------------------------------------
  void Tls::Shutdown()
  {
    pTls->Shutdown();
  }

  XRootDStatus Tls::ToStatus( XrdTls::RC rc )
  {
    std::string msg = XrdTls::RC2Text( rc, true );

    switch( rc )
    {
      case XrdTls::TLS_AOK: return XRootDStatus();

      case XrdTls::TLS_WantConnect:
      case XrdTls::TLS_WantWrite:
      case XrdTls::TLS_WantRead:  return XRootDStatus( stOK, suRetry, 0, msg );

      case XrdTls::TLS_UNK_Error:
      case XrdTls::TLS_SYS_Error: return XRootDStatus( stError, errTlsError, 0, msg );

      case XrdTls::TLS_SSL_Error: return XRootDStatus( stFatal, errTlsError, EAGAIN, msg );

      case XrdTls::TLS_VER_Error:
      case XrdTls::TLS_HNV_Error: return XRootDStatus( stFatal, errTlsError, 0, msg );

      // the connection was closed by the server, treat this as a socket error
      case XrdTls::TLS_CON_Closed: return XRootDStatus( stError, errSocketError );

      default:
        return XRootDStatus( stError, errTlsError, 0, msg );
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

  void Tls::ClearErrorQueue()
  {
    XrdTls::ClearErrorQueue();
  }
}
