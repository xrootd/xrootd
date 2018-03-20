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

#include "XrdTlsConnection.hh"

#include <stdexcept>

namespace XrdTls
{

  Connection::Connection( int sfd, Mode mode ) : hsDone( false )
  {
    sbio = BIO_new_socket( sfd, BIO_NOCLOSE );
    BIO_set_nbio( sbio, 1 );
    ssl  = SSL_new( XrdTls::Context::Instance() );

    switch( mode )
    {
      case TLS_CLIENT:
      {
        SSL_set_connect_state( ssl );
        break;
      }

      case TLS_SERVER:
      {
        SSL_set_accept_state( ssl );
        break;
      }

      default:
        throw std::invalid_argument( "TLS I/O: expected TLS mode." );
    }

    SSL_set_bio( ssl, sbio, sbio );
  }

  Connection::~Connection()
  {
    SSL_free( ssl );   /* free the SSL object and its BIO's */
  }

  int Connection::Accept()
  {
    int rc = SSL_accept( ssl );
    int error = SSL_get_error( ssl, rc );
    return error;
  }

  int Connection::Connect()
  {
    int rc = SSL_connect( ssl );
    int error = SSL_get_error( ssl, rc );
    return error;
  }

  int Connection::Read( char *buffer, size_t size, int &bytesRead )
  {
    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
    int rc = SSL_read( ssl, buffer, size );
    if( rc > 0 ) bytesRead = rc;
    else hsDone = bool( SSL_is_init_finished( ssl ) );
    int error = SSL_get_error( ssl, rc );
    return error;
  }

  int Connection::Write( char *buffer, size_t size, int &bytesWritten )
  {
    //------------------------------------------------------------------------
    // If necessary, SSL_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
    int rc = SSL_write( ssl, buffer, size );
    if( rc > 0 ) bytesWritten = rc;
    else hsDone = bool( SSL_is_init_finished( ssl ) );
    int error = SSL_get_error( ssl, rc );
    return error;
  }

}
