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

#include <string.h>
#include <errno.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "XrdTls/XrdTlsContext.hh"
#include "XrdTls/XrdTlsSocket.hh"
#include "XrdTls/XrdTlsNotary.hh"

#include <stdexcept>

/******************************************************************************/
/*                      X r d T l s S o c k e t I m p l                       */
/******************************************************************************/

struct XrdTlsSocketImpl
{
    XrdTlsSocketImpl() : tlsctx(0), ssl(0), traceID(0), sFD(-1),
                             hsWait(15), hsDone(false), fatal(false),
                             cOpts(0), cAttr(0), hsMode(0) {}

    XrdTlsContext   *tlsctx;    //!< Associated context object
    SSL             *ssl;       //!< Associated SSL     object
    const char      *traceID;   //!< Trace identifier
    int              sFD;       //!< Associated file descriptor (never closed)
    int              hsWait;    //!< Maximum amount of time to wait for handshake
    bool             hsDone;    //!< True if the handshake has completed
    bool             fatal;     //!< True if fatal error prevents shutdown call
    char             cOpts;     //!< Connection options
    char             cAttr;     //!< Connection attributes
    char             hsMode;    //!< Handshake handling
};

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class undoImpl
{
public:

   undoImpl(XrdTlsSocketImpl *pImpl) : theImpl(pImpl) {}
  ~undoImpl() {if (theImpl && theImpl->ssl)
                  {SSL_free( theImpl->ssl );
                   theImpl->ssl = 0;
                  }
              }

   void KeepImpl() {theImpl = 0;}

private:
XrdTlsSocketImpl *theImpl;
};
}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdTlsGlobal
{
extern XrdTls::msgCB_t msgCB;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdTlsSocket::XrdTlsSocket() : pImpl( new XrdTlsSocketImpl() )
{
  
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdTlsSocket::XrdTlsSocket( XrdTlsContext &ctx, int  sfd,
                                    XrdTlsSocket::RW_Mode rwm,
                                    XrdTlsSocket::HS_Mode hsm,
                                    bool isClient )
                 : pImpl( new XrdTlsSocketImpl() )
{

// Simply initialize this object and throw an exception if it fails
//
   const char *eMsg = Init(ctx, sfd, rwm, hsm, isClient);
   if (eMsg) throw std::invalid_argument( eMsg );
}

/******************************************************************************/
/*                           D e s t r u c t o r                            */
/******************************************************************************/

XrdTlsSocket::~XrdTlsSocket()
{
  if (pImpl->ssl) Shutdown(sdForce);
  delete pImpl;
}

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Accept()
{
   int rc, error;

// Make sure there is a context here
//
   if (pImpl->ssl == 0) return XrdTls::TLS_CTX_Missing;
   undoImpl ImplTracker(pImpl);

// An accept may require several tries, so we do that here.
//
do{if ((rc = SSL_accept( pImpl->ssl )) > 0)
      {if (pImpl->cOpts & xVerify)
          {X509 *theCert = SSL_get_peer_certificate(pImpl->ssl);
           if (!theCert) return XrdTls::TLS_CRT_Missing;
           X509_free(theCert);
           rc = SSL_get_verify_result(pImpl->ssl);
           if (rc != X509_V_OK)
              {XrdTls::Emsg(pImpl->traceID, "x509 cert verification failed");
               return XrdTls::TLS_VER_Error;
              }
          }
       ImplTracker.KeepImpl();
       return XrdTls::TLS_AOK;
      }

   // Get the actual SSL error code.
   //
   error = Diagnose(rc);

   // Check why we did not succeed. We may be able to recover.
   //
   if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE)
      {std::string eTxt("TLS Accept() failed; ");
       eTxt += Err2Text(error);
       XrdTls::Emsg(pImpl->traceID, eTxt.c_str());;
       errno = ECONNABORTED;
       return XrdTls::TLS_SYS_Error;
      }

  } while(Wait4OK(error == SSL_ERROR_WANT_READ));

// If we are here then we got a syscall error
//
   return XrdTls::TLS_SYS_Error;
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Connect(const char *thehost, XrdNetAddrInfo *netInfo,
                                 std::string *eMsg)
{

// Setup host verification of a host has been specified. This is a to-do
// when we move to new versions of SSL. For now, we use the notary object.
//

// Do the connect.
//
   int rc = SSL_connect( pImpl->ssl );
   if (rc != 1) return Diagnose(rc);

//  Set the hsDone flag!
//
   pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );

// Validate the host name if so desired. Note that cert verification is
// checked by the notary since only hostname validation requires it.

   if (thehost)
      {const char *eTxt = XrdTlsNotary::Validate(pImpl->ssl, thehost,
                                        (pImpl->cOpts & DNSok ? netInfo : 0));
       if (eTxt && eMsg)
          {
           if (eMsg)
              {
               *eMsg  = "Unable to validate "; *eMsg += thehost;
               *eMsg += "; "; *eMsg += eTxt;
              }
           return XrdTls::TLS_HNV_Error;
          }
      }

   return XrdTls::TLS_AOK;
}

/******************************************************************************/
/*                               C o n t e x t                                */
/******************************************************************************/

XrdTlsContext* XrdTlsSocket::Context()
{
  return pImpl->tlsctx;
}

/******************************************************************************/
/* Private:                     D i a g n o s e                               */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Diagnose(int sslrc)
{
int eCode = SSL_get_error( pImpl->ssl, sslrc );

// Make sure we can shutdown
//
   if (eCode == SSL_ERROR_SYSCALL || eCode == SSL_ERROR_SSL) pImpl->fatal = true;

// Return the errors
//
   return XrdTls::ssl2RC(eCode);
}
  
/******************************************************************************/
/* Private:                     E r r 2 T e x t                               */
/******************************************************************************/

std::string XrdTlsSocket::Err2Text(int sslerr)
{
   char *eP, eBuff[1024];

   if (sslerr == SSL_ERROR_SYSCALL)
      {int rc = errno;
       if (!rc) rc = EPIPE;
       snprintf(eBuff, sizeof(eBuff), "%s", strerror(rc));
       *eBuff = tolower(*eBuff);
       eP = eBuff;
      } else {
       ERR_error_string_n(sslerr, eBuff, sizeof(eBuff));
       if (pImpl->cOpts & Debug) eP = eBuff;
          else {char *colon = rindex(eBuff, ':');
                eP = (colon ? colon+1 : eBuff);
               }

      }
   return std::string(eP);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

const char *XrdTlsSocket::Init( XrdTlsContext &ctx, int sfd,
                                    XrdTlsSocket::RW_Mode rwm,
                                    XrdTlsSocket::HS_Mode hsm,
                                    bool isClient, const char *tid )
{
   BIO *rbio, *wbio = 0;

// Make sure this connection is not in use if this is a client. Servers are
// allowed to throw away the previous setup as they reuse sockets.
//
   if ( pImpl->ssl )
      {if (isClient) return "TLS I/O: connection is still in use.";
          else {SSL_free( pImpl->ssl );
                pImpl->ssl = 0;
               }
      }

// Get the ssl object from the context, there better be one.
//
   SSL_CTX *ssl_ctx = static_cast<SSL_CTX *>(ctx.Context());
   if (ssl_ctx == 0) return "TLS I/O: context inialization failed.";

// Initialze values from the context.
//
   pImpl->tlsctx = &ctx;
   const XrdTlsContext::CTX_Params *parms = ctx.GetParams();
   pImpl->hsWait = (parms->opts & XrdTlsContext::hsto) * 1000; // Poll timeout
   if (ctx.x509Verify()) pImpl->cOpts = xVerify;
      else pImpl->cOpts = 0;
   if (parms->opts & XrdTlsContext::debug) pImpl->cOpts |= Debug;
   if (parms->opts & XrdTlsContext::dnsok) pImpl->cOpts |= DNSok;
   pImpl->traceID = tid;

// Obtain the ssl object at this point.
//
   pImpl->ssl = SSL_new( ssl_ctx );
   if (pImpl->ssl == 0) return "TLS I/O: failed to get ssl object.";

// Set the ssl object state to correspond to client or server type
//
   if (isClient)
      {SSL_set_connect_state( pImpl->ssl );
       pImpl->cAttr = 0;
      } else {
       SSL_set_accept_state( pImpl->ssl );
       pImpl->cAttr = isServer;
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
          pImpl->cAttr |= wBlocking;
          break;

     case TLS_RBL_WNB:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          wbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( wbio, 1 );
          pImpl->cAttr |= rBlocking;
          break;

     case TLS_RBL_WBL:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          pImpl->cAttr |= (rBlocking | wBlocking);
          break;

     default: 
          return "TLS I/O: invalid TLS rw mode."; break;
   }

// Set correct handshake mode
//
   switch( hsm )
   {
     case TLS_HS_BLOCK: pImpl->hsMode = rwBlock; break;
     case TLS_HS_NOBLK: pImpl->hsMode = noBlock; break;
     case TLS_HS_XYBLK: pImpl->hsMode = xyBlock; break;

     default:
          return "TLS I/O: invalid TLS hs mode."; break;
    }

// Finally attach the bios to the ssl object. When the ssl object is freed
// the bios will be freed as well.
//
   pImpl->sFD = sfd;
   if (wbio == 0) wbio = rbio;
   SSL_set_bio( pImpl->ssl, rbio, wbio );

// Set timeouts on this socket to allow SSL to not block
//
/*??? Likely we don't need to do this, but of we do, include
   #include <time.h>
   #include <sys/socket.h>

   struct timeval tv;
   tv.tv_sec = 10;
   tv.tv_usec = 0;
   setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
*/

// All done. The caller will do an Accept() or Connect() afterwards.
//
   return 0;
}

/******************************************************************************/
/*                                  P e e k                                   */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Peek( char *buffer, size_t size, int &bytesPeek )
  {
    XrdTls::RC error;

    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_peek( pImpl->ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesPeek = rc;
       return XrdTls::TLS_AOK;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );
    error = Diagnose(rc);

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a read
    // may force us to wait until writes are possible. All of this is rare!
    //
    if ((!pImpl->hsMode || pImpl->hsDone || (error != XrdTls::TLS_WantRead &&
                               error != XrdTls::TLS_WantWrite))
    ||   (pImpl->hsMode == xyBlock && error == XrdTls::TLS_WantRead)) return error;

   } while(Wait4OK(error == XrdTls::TLS_WantRead));

    return XrdTls::TLS_SYS_Error;
  }

/******************************************************************************/
/*                               P e n d i n g                                */
/******************************************************************************/

int XrdTlsSocket::Pending(bool any)
{
   if (!any) return SSL_pending(pImpl->ssl);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
   return SSL_pending(pImpl->ssl) != 0;
#else
   return SSL_has_pending(pImpl->ssl);
#endif
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Read( char *buffer, size_t size, int &bytesRead )
  {
    XrdTls::RC error;

    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_read( pImpl->ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesRead = rc;
       if( !pImpl->hsDone ) pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );
       return XrdTls::TLS_AOK;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    error = Diagnose(SSL_get_error( pImpl->ssl, rc ));
    if( error == XrdTls::RC::TLS_AOK ) bytesRead = 0;
    pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a read
    // may force us to wait until writes are possible. All of this is rare!
    //
    if ((!pImpl->hsMode || pImpl->hsDone || (error != XrdTls::TLS_WantRead &&
                               error != XrdTls::TLS_WantWrite))
    ||   (pImpl->hsMode == xyBlock && error == XrdTls::TLS_WantRead)) return error;

   } while(Wait4OK(error == XrdTls::TLS_WantRead));

    return XrdTls::TLS_SYS_Error;
  }

/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/
  
void XrdTlsSocket::Shutdown(XrdTlsSocket::SDType sdType)
{
   int sdMode, rc;

// Make sure we have an ssl object
//
   if (pImpl->ssl == 0) return;

// Perform shutdown as needed. This is required before freeing the ssl object.
// If we previously encountered a SYSCALL or SSL error, shutdown is prohibited!
// The following code is patterned after code in the public TomCat server.
//
   if (!pImpl->fatal)
      {switch(sdType)
             {case sdForce: // Forced shutdown which violate TLS standard!
                   sdMode = SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN;
                   break;
              case sdWait:  // Wait for client acknowledgement
                   sdMode = 0;
                   break;
              default:      // Fast shutdown, don't wait for ack (compliant)
                   sdMode = SSL_RECEIVED_SHUTDOWN;
                   break;
             }

       SSL_set_shutdown(pImpl->ssl, sdMode);

       for (int i = 0; i < 4; i++)
           {rc = SSL_shutdown( pImpl->ssl );
            if (rc > 0) break;
            if (rc < 0)
               {rc = SSL_get_error( pImpl->ssl, rc );
                if (rc == SSL_ERROR_WANT_READ || rc == SSL_ERROR_WANT_WRITE)
                   {if (Wait4OK(rc == SSL_ERROR_WANT_READ)) continue;
                    rc = SSL_ERROR_SYSCALL;
                   }
                char msgBuff[512];
                std::string eMsg = Err2Text(rc);
                snprintf(msgBuff, sizeof(msgBuff),
                        "FD %d TLS shutdown failed; %s.\n",pImpl->sFD,eMsg.c_str());
                XrdTlsGlobal::msgCB(pImpl->traceID, msgBuff, false);
                break;
               }
           }
      }

// Now free the ssl object which will free all the BIO's associated with it
//
   SSL_free( pImpl->ssl );
   pImpl->ssl = 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Write( const char *buffer, size_t size,
                                int &bytesWritten )
  {
    XrdTls::RC error;

    //------------------------------------------------------------------------
    // If necessary, SSL_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_write( pImpl->ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesWritten = rc;
       if( !pImpl->hsDone ) pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );
       return XrdTls::TLS_AOK;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    error = Diagnose(SSL_get_error( pImpl->ssl, rc ));
    if( error == XrdTls::RC::TLS_AOK ) bytesWritten = 0;
    pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a write
    // may force us to wait until reads are possible. All of this is rare!
    //
    if ((!pImpl->hsMode || pImpl->hsDone || (error != XrdTls::TLS_WantRead &&
                                             error != XrdTls::TLS_WantWrite))
    ||   (pImpl->hsMode == xyBlock && error == XrdTls::TLS_WantWrite)) return error;

   } while(Wait4OK(error == XrdTls::TLS_WantRead));

    return XrdTls::TLS_SYS_Error;
  }

/******************************************************************************/
/*                         N e e d H a n d S h a k e                          */
/******************************************************************************/

  bool XrdTlsSocket::NeedHandShake()
  {
    return !pImpl->hsDone;
  }

/******************************************************************************/
/*                               V e r s i o n                                */
/******************************************************************************/

  const char *XrdTlsSocket::Version()
  {
     return SSL_get_version(pImpl->ssl);
  }

/******************************************************************************/
/* Private:                      W a i t 4 O K                                */
/******************************************************************************/
  
bool XrdTlsSocket::Wait4OK(bool wantRead)
{
   static const short rdOK = POLLIN |POLLRDNORM;
   static const short wrOK = POLLOUT|POLLWRNORM;
   struct pollfd polltab = {pImpl->sFD, (wantRead ? rdOK : wrOK), 0};
   int rc, timeout;

   // Establish how long we will wait.
   //
   timeout = (pImpl->hsDone ? pImpl->hsWait : -1);

   do {rc = poll(&polltab, 1, timeout);} while(rc < 0 && errno == EINTR);

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
