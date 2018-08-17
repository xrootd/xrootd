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

#include <errno.h>
#include <iostream>
#include <poll.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "XrdTls/XrdTlsConnection.hh"
#include "XrdTls/XrdTlsContext.hh"

#include <stdexcept>

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdTlsConnection::XrdTlsConnection( XrdTlsContext &ctx, int  sfd,
                                    XrdTlsConnection::RW_Mode rwm,
                                    XrdTlsConnection::HS_Mode hsm,
                                    bool isClient )
                 : hsDone( false )
{

// Simply initialize this object and throw an exception if it fails
//
   const char *eMsg = Init(ctx, sfd, rwm, hsm, isClient);
   if (eMsg) throw std::invalid_argument( eMsg );
}

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/
  
int XrdTlsConnection::Accept()
  {
    int rc = SSL_accept( ssl );
    int error = SSL_get_error( ssl, rc );
    return error;
  }

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/
  
int XrdTlsConnection::Connect()
  {
    int rc = SSL_connect( ssl );
    int error = SSL_get_error( ssl, rc );
    return error;
  }
  
/******************************************************************************/
/*                             G e t E r r o r s                              */
/******************************************************************************/

std::string XrdTlsConnection::GetErrs(const char *pfx)
{
   return (tlsctx ? tlsctx->GetErrs(pfx) : std::string(""));
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

const char *XrdTlsConnection::Init( XrdTlsContext &ctx, int sfd,
                                    XrdTlsConnection::RW_Mode rwm,
                                    XrdTlsConnection::HS_Mode hsm,
                                    bool isClient )
{
   BIO *rbio, *wbio = 0;

// Make sure this connection is not in use
//
   if ( ssl ) return "TLS I/O: connection is still in use.";

// Get the ssl object from the context, there better be one.
//
   SSL_CTX *ssl_ctx = ctx.Context();
   if (ssl_ctx == 0) return "TLS I/O: context inialization failed.";

// Save the tls context, we need it to print errors
//
   tlsctx = &ctx;

// Obtain the ssl object at this point. Anything after this cannot fail.
//
   ssl = SSL_new( ssl_ctx );
   if (ssl == 0) return "TLS I/O: failed to get ssl object.";

// Set the ssl object state to correspond to client or server type
//
   if (isClient)
      {SSL_set_connect_state( ssl );
       cAttr = isServer;
      } else {
       SSL_set_accept_state( ssl );
       cAttr = 0;
      }

// Allocate right number of bio's and initialize them as requested. Note
// that when the read and write bios have the same attribue, we use only one.
//
   switch( rwm )
   {
     case TLS_RNB_WNB:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( rbio, 1 );
          break;

     case TLS_RNB_WBL:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( rbio, 1 );
          wbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          cAttr |= wBlocking;
          break;

     case TLS_RBL_WNB:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          wbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( wbio, 1 );
          cAttr |= rBlocking;
          break;

     case TLS_RBL_WBL:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          cAttr |= (rBlocking | wBlocking);
          break;

     default: return "TLS I/O: invalid TLS rw mode."; break;
   }

// Set correct handshake mode
//
   switch( hsm )
   {
     case TLS_HS_BLOCK: hsMode = rwBlock; break;
     case TLS_HS_NOBLK: hsMode = noBlock; break;
     case TLS_HS_XYBLK: hsMode = xyBlock; break;

     default: return "TLS I/O: invalid TLS hs mode."; break;
    }

// Finally attach the bios to the ssl object. When the ssl object is freed
// the bios will be freed as well.
//
   sFD = sfd;
   if (wbio == 0) wbio = rbio;
   SSL_set_bio( ssl, rbio, wbio );
   return 0;
}

/******************************************************************************/
/*                             P r i n t E r r s                              */
/******************************************************************************/

void XrdTlsConnection::PrintErrs(const char *pfx, XrdSysError *eDest)
{
   if (tlsctx) tlsctx->PrintErrs(pfx, eDest);
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/
  
  int XrdTlsConnection::Read( char *buffer, size_t size, int &bytesRead )
  {
    int error;

    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_read( ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesRead = rc;
       return SSL_ERROR_NONE;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    hsDone = bool( SSL_is_init_finished( ssl ) );
    error = SSL_get_error( ssl, rc );

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a read
    // may force us to wait until writes are possible. All of this is rare!
    //
    if ((!hsMode || hsDone || (error != SSL_ERROR_WANT_READ &&
                               error != SSL_ERROR_WANT_WRITE))
    ||   (hsMode == xyBlock && error == SSL_ERROR_WANT_READ)) return error;

   } while(Wait4OK(error == SSL_ERROR_WANT_READ));

    return SSL_ERROR_SYSCALL;
  }

/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/
  
void XrdTlsConnection::Shutdown(bool force)
{
// Make sure we have an ssl object
//
   if (ssl == 0) return;

// Perform shutdown as needed. This is required before freeing the ssl object
//
   if (force) SSL_set_shutdown( ssl, 1 );
      else {int rc = SSL_shutdown( ssl );
            if (!rc) rc = SSL_shutdown( ssl );
            if (rc < 0)
               {rc = SSL_get_error( ssl, rc );
                if (rc)
                   {const char *eText = ERR_reason_error_string( rc );
                    if (eText) std::cerr<< "TlsCon: Shutdown("<<sFD<<") failed; "
                                        <<eText<<'\n'<<std::flush;
                   }
               }
           }

// Now free the ssl object which will free all the BIO's associated with it
//
   SSL_free( ssl );
   ssl = 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
  int XrdTlsConnection::Write( char *buffer, size_t size, int &bytesWritten )
  {
    int error;

    //------------------------------------------------------------------------
    // If necessary, SSL_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_write( ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesWritten = rc;
       return SSL_ERROR_NONE;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    hsDone = bool( SSL_is_init_finished( ssl ) );
    error = SSL_get_error( ssl, rc );

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a write
    // may force us to wait until reads are possible. All of this is rare!
    //
    if ((!hsMode || hsDone || (error != SSL_ERROR_WANT_READ &&
                               error != SSL_ERROR_WANT_WRITE))
    ||   (hsMode == xyBlock && error == SSL_ERROR_WANT_WRITE)) return error;

   } while(Wait4OK(error == SSL_ERROR_WANT_READ));

    return SSL_ERROR_SYSCALL;
  }

/******************************************************************************/
/* Private:                      W a i t 4 O K                                */
/******************************************************************************/
  
bool XrdTlsConnection::Wait4OK(bool wantRead)
{
   static const short rdOK = POLLIN |POLLRDNORM;
   static const short wrOK = POLLOUT|POLLWRNORM;
   struct pollfd polltab = {sFD, (wantRead ? rdOK : wrOK), 0};
   int rc;

   do {rc = poll(&polltab, 1, -1);} while(rc < 0 && errno == EINTR);

   // Make sure we have a clean state, otherwise indicate we failed. The
   // caller will need to perform the correct action.
   //
   if (rc == 1)
      {if (polltab.revents & (wantRead ? rdOK : wrOK)) return true;
       if (polltab.revents & POLLERR) errno = EIO;
          else if (polltab.revents & (POLLHUP|POLLNVAL)) errno = EPIPE;
                  else errno = EINVAL;
      } else if (!rc) errno = ETIMEDOUT; // This is not possible
   return false;
}
