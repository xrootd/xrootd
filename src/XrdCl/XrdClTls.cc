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

namespace XrdCl
{

  Tls::Tls( XrdTlsContext &ctx, int sfd ) 
      : io( ctx, sfd, XrdTlsConnection::TLS_RNB_WNB,
                      XrdTlsConnection::TLS_HS_XYBLK, true )
  {

  }

  Tls::~Tls()
  {

  }

  Status Tls::Read( char *buffer, size_t size, int &bytesRead )
  {
    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
    return ToStatus( io.Read( buffer, size, bytesRead ) );
  }

  Status Tls::Write( char *buffer, size_t size, int &bytesWritten )
  {
    //------------------------------------------------------------------------
    // If necessary, SSL_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
    return ToStatus( io.Write( buffer, size, bytesWritten ) );
  }

  Status Tls::ToStatus( int error )
  {
    switch( error )
    {
      case SSL_ERROR_NONE: return Status();

      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_READ: return Status( stOK, suRetry );

      case SSL_ERROR_ZERO_RETURN:
      case SSL_ERROR_SYSCALL:
      default:
        return Status( stError, errTlsError, error );
    }
  }

  //----------------------------------------------------------------------------
  // Read from TLS layer helper
  //----------------------------------------------------------------------------
  Status ReadFrom( Tls *tls, char *buffer, size_t size, int &bytesRead )
  {
    return tls->Read( buffer, size, bytesRead );
  }

}
