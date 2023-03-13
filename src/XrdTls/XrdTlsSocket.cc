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

#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <cstdio>
#include <ctime>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdTls/XrdTls.hh"
#include "XrdTls/XrdTlsContext.hh"
#include "XrdTls/XrdTlsNotary.hh"
#include "XrdTls/XrdTlsPeerCerts.hh"
#include "XrdTls/XrdTlsSocket.hh"
#include "XrdTls/XrdTlsTrace.hh"

#include <stdexcept>

/******************************************************************************/
/*                      X r d T l s S o c k e t I m p l                       */
/******************************************************************************/

struct XrdTlsSocketImpl
{
    XrdTlsSocketImpl() : tlsctx(0), ssl(0), traceID(""), sFD(-1), hsWait(15),
                         hsDone(false), fatal(0), isClient(false),
                         cOpts(0), cAttr(0), hsNoBlock(false), isSerial(true) {}

    XrdSysMutex      sslMutex;  //!< Mutex to serialize calls
    XrdTlsContext   *tlsctx;    //!< Associated context object
    SSL             *ssl;       //!< Associated SSL     object
    const char      *traceID;   //!< Trace identifier
    int              sFD;       //!< Associated file descriptor (never closed)
    int              hsWait;    //!< Maximum amount of time to wait for handshake
    bool             hsDone;    //!< True if the handshake has completed
    char             fatal;     //!< !0   if fatal error prevents shutdown call
    bool             isClient;  //!< True if for client use
    char             cOpts;     //!< Connection options
    char             cAttr;     //!< Connection attributes
    bool             hsNoBlock; //!< Handshake handling nonblocking if true
    bool             isSerial;  //!< True if calls must be serialized
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
extern XrdSysTrace SysTrace;
}

/******************************************************************************/
/*                                l o c a l s                                 */
/******************************************************************************/
  
namespace
{
static const int noBlock = 0;
static const int rwBlock = 'a';
static const int xyBlock = 'x';

static const int xVerify = 0x01;   //!< Peer cetrificate is to be verified
static const int DNSok   = 0x04;   //!< DNS can be used to verify peer.

static const int isServer  = 0x01;
static const int rBlocking = 0x02;
static const int wBlocking = 0x04;
static const int acc2Block = 0x08;
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
                                    bool isClient, bool serial )
                 : pImpl( new XrdTlsSocketImpl() )
{

// Simply initialize this object and throw an exception if it fails
//
   const char *eMsg = Init(ctx, sfd, rwm, hsm, isClient, serial);
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
  
XrdTls::RC XrdTlsSocket::Accept(std::string *eWhy)
{
   EPNAME("Accept");
   int rc, ssler;
   bool wOK, aOK = true;

// Make sure there is a context here
//
   if (pImpl->ssl == 0)
      {AcceptEMsg(eWhy, "TLS socket has no context");
       return XrdTls::TLS_CTX_Missing;
      }
   undoImpl ImplTracker(pImpl);

// Do some tracing
//
   DBG_SOK("Accepting a TLS connection...");

// An accept may require several tries, so we do that here.
//
do{if ((rc = SSL_accept( pImpl->ssl )) > 0)
      {if (pImpl->cOpts & xVerify)
          {X509 *theCert = SSL_get_peer_certificate(pImpl->ssl);
           if (!theCert)
              {AcceptEMsg(eWhy, "x509 certificate is missing");
               return XrdTls::TLS_CRT_Missing;
              }
           X509_free(theCert);
           rc = SSL_get_verify_result(pImpl->ssl);
           if (rc != X509_V_OK)
              {AcceptEMsg(eWhy, "x509 certificate verification failed");
               return XrdTls::TLS_VER_Error;
              }
          }
       ImplTracker.KeepImpl();

// Reset the socket to blocking mode if we need to. Note that we have to brute
// force this on the socket as setting a BIO after accept has no effect. We
// also tell ssl that we want to block on a handshake from now on.
//
   if (pImpl->cAttr & acc2Block)
//    BIO_set_nbio(SSL_get_rbio(pImpl->ssl), 0); *Does not work after accept*
      {int eNO = errno;
       int flags = fcntl(pImpl->sFD, F_GETFL, 0);
       flags &= ~O_NONBLOCK;
       fcntl(pImpl->sFD, F_SETFL, flags);
       SSL_set_mode(pImpl->ssl, SSL_MODE_AUTO_RETRY);
       errno = eNO;
      }
       return XrdTls::TLS_AOK;
      }

   // Get the actual SSL error code.
   //
   ssler = Diagnose("TLS_Accept", rc, XrdTls::dbgSOK);

   // Check why we did not succeed. We may be able to recover.
   //
   if (ssler != SSL_ERROR_WANT_READ && ssler != SSL_ERROR_WANT_WRITE) {
       if(ssler == SSL_ERROR_SSL){
           //In the case the accept does have an error related to OpenSSL,
           //shutdown the TLSSocket in case the link associated to that connection
           //is re-used
           Shutdown();
       }
       aOK = false; break;
   }

   if (pImpl->hsNoBlock) return XrdTls::ssl2RC(ssler);

  } while((wOK = Wait4OK(ssler == SSL_ERROR_WANT_READ)));

// If we are here then we got an error
//
   AcceptEMsg(eWhy, (!aOK ? Err2Text(ssler).c_str() : XrdSysE2T(errno)));
   errno = ECONNABORTED;
   return XrdTls::TLS_SYS_Error;
}

/******************************************************************************/
/* Private:                   A c c e p t E M s g                             */
/******************************************************************************/
  
void XrdTlsSocket::AcceptEMsg(std::string *eWhy, const char *reason)
{
   if (eWhy)
      {*eWhy  = "TLS connection from ";
       *eWhy += pImpl->traceID;
       *eWhy += " failed; ";
       *eWhy += reason;
      }
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Connect(const char *thehost, std::string *eWhy)
{
   EPNAME("Connect");
   int ssler, rc;
   bool wOK = true, aOK = true;

// Setup host verification of a host has been specified. This is a to-do
// when we move to new versions of SSL. For now, we use the notary object.
//

// Do some tracing
//
   DBG_SOK("Connecting to " <<(thehost ? thehost : "unverified host")
           <<(thehost && pImpl->cOpts & DNSok ? " dnsok" : "" ));

// Do the connect.
//
do{int rc = SSL_connect( pImpl->ssl );
   if (rc == 1) break;

   ssler = Diagnose("TLS_Connect", rc, XrdTls::dbgSOK);

   if (ssler != SSL_ERROR_WANT_READ && ssler != SSL_ERROR_WANT_WRITE)
      {aOK = false; break;}

   if (pImpl->hsNoBlock) return XrdTls::ssl2RC(ssler);

   } while((wOK = Wait4OK(ssler == SSL_ERROR_WANT_READ)));

// Check if everything went well. Note that we need to save the errno as
// we may be calling external methods that may generate other errors. We
//
   if (!aOK || !wOK)
      {rc = errno;
       DBG_SOK("Handshake failed; "<<(!aOK ? Err2Text(ssler) : XrdSysE2T(rc)));
       if (eWhy)
          {const char *hName = (thehost ? thehost : "host");
           *eWhy = "Unable to connect to ";
           *eWhy += hName;
           *eWhy += "; ";
           if (!aOK) *eWhy += Err2Text(ssler);
              else   *eWhy += XrdSysE2T(rc);
          }
       if (!aOK) return XrdTls::ssl2RC(ssler);
       errno = rc;
       return XrdTls::TLS_SYS_Error;
      }

//  Set the hsDone flag!
//
   pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );

// Validate the host name if so desired. Note that cert verification is
// checked by the notary since hostname validation requires it. We currently
// do not support dnsOK but doing so just means we need to check the option
// and if on, also pass a XrdNetAddrInfo object generated from the hostname.
//
   if (thehost)
      {const char *eTxt = XrdTlsNotary::Validate(pImpl->ssl, thehost, 0);
       if (eTxt)
          {DBG_SOK(thehost << " verification failed; " <<eTxt);
           if (eWhy)
              {
               *eWhy  = "Unable to validate "; *eWhy += thehost;
               *eWhy += "; "; *eWhy += eTxt;
              }
           return XrdTls::TLS_HNV_Error;
          }
      }

   DBG_SOK("Connect completed without error.");
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
  
int XrdTlsSocket::Diagnose(const char *what, int sslrc, int tcode)
{
   int eCode = SSL_get_error( pImpl->ssl, sslrc );

// We need to dispose of the error queue otherwise the next operation will
// fail. We do this by either printing them or flushing them down the drain.
// We avoid the tracing hangups indicated by SSL_ERROR_SYSCALL w/ errno == 0.
//
   if (TRACING(tcode)
   || (eCode != SSL_ERROR_WANT_READ && eCode != SSL_ERROR_WANT_WRITE))
      {int eNO = errno;
       if (!eNO && eCode == SSL_ERROR_SYSCALL) ERR_clear_error();
           else {char eBuff[256];
                 snprintf(eBuff, sizeof(eBuff),
                          "TLS error rc=%d ec=%d (%s) errno=%d.",
                          sslrc, eCode, XrdTls::ssl2Text(eCode), eNO);
                 XrdTls::Emsg(pImpl->traceID, eBuff, true);
                 errno = eNO;
                }
      } else ERR_clear_error();

// Make sure we can shutdown
//
   if (eCode == SSL_ERROR_SYSCALL)
      pImpl->fatal = (char)XrdTls::TLS_SYS_Error;
      else if (eCode == SSL_ERROR_SSL)
              pImpl->fatal = (char)XrdTls::TLS_SSL_Error;

// Return the errors
//
   return eCode;
}
  
/******************************************************************************/
/* Private:                     E r r 2 T e x t                               */
/******************************************************************************/

std::string XrdTlsSocket::Err2Text(int sslerr)
{
   const char *eP;
   char eBuff[256];

   if (sslerr == SSL_ERROR_SYSCALL)
      {int rc = errno;
       if (!rc) rc = EPIPE;
       snprintf(eBuff, sizeof(eBuff), "%s", XrdSysE2T(rc));
       *eBuff = tolower(*eBuff);
       eP = eBuff;
      } else eP = XrdTls::ssl2Text(sslerr,0);

   return std::string(eP);
}

/******************************************************************************/
/*                              g e t C e r t s                               */
/******************************************************************************/

XrdTlsPeerCerts *XrdTlsSocket::getCerts(bool ver)
{
   XrdSysMutexHelper mHelper;

// Serialize call if need be
//
   if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));

// If verified certs need to be returned, make sure the certs are verified
//
   if (ver && SSL_get_verify_result(pImpl->ssl) != X509_V_OK) return 0;

// Get the certs and return
//
   X509 *pcert = SSL_get_peer_certificate(pImpl->ssl);
   if (pcert == 0) return 0;
   return new XrdTlsPeerCerts(pcert, SSL_get_peer_cert_chain(pImpl->ssl));
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

const char *XrdTlsSocket::Init( XrdTlsContext &ctx, int sfd,
                                    XrdTlsSocket::RW_Mode rwm,
                                    XrdTlsSocket::HS_Mode hsm,
                                    bool isClient, bool serial,
                                    const char *tid )
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

// Obtain the ssl object at this point.
//
   pImpl->ssl = static_cast<SSL *>(ctx.Session());
   if (pImpl->ssl == 0) return "TLS I/O: failed to get ssl object.";

// Initialze values from the context.
//
   pImpl->tlsctx = &ctx;
   const XrdTlsContext::CTX_Params *parms = ctx.GetParams();
   pImpl->hsWait = (parms->opts & XrdTlsContext::hsto) * 1000; // Poll timeout
   if (ctx.x509Verify()) pImpl->cOpts = xVerify;
      else pImpl->cOpts = 0;
   if (parms->opts & XrdTlsContext::dnsok) pImpl->cOpts |= DNSok;
   pImpl->traceID = tid;
   pImpl->isClient= isClient;
   pImpl->isSerial= serial;

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
   if (hsm) pImpl->hsNoBlock = false;
      else  pImpl->hsNoBlock = true;

// Reset the handshake and fatal error indicators
//
   pImpl->hsDone = false;
   pImpl->fatal = 0;

// The glories of OpenSSL require that we do some fancy footwork with the
// handshake timeout. If there is one and this is a server and the server
// wants blocking reads, we initially set the socket as non-blocking as the
// bio's can handle it. Then after the accept we set it back to blocking mode.
// Note: doing this via the bio causes the socket to remain nonblocking. yech!
//
   if (pImpl->hsWait && !hsm &&  pImpl->cAttr & rBlocking)
      {int flags = fcntl(sfd, F_GETFL, 0);
       flags |= O_NONBLOCK;
       fcntl(sfd, F_SETFL, flags);
       pImpl->cAttr |= acc2Block;
      }

// Finally attach the bios to the ssl object. When the ssl object is freed
// the bios will be freed as well.
//
   pImpl->sFD = sfd;
   if (wbio == 0) wbio = rbio;
   SSL_set_bio( pImpl->ssl, rbio, wbio );

// All done. The caller will do an Accept() or Connect() afterwards.
//
   return 0;
}

/******************************************************************************/
/*                                  P e e k                                   */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Peek( char *buffer, size_t size, int &bytesPeek )
  {
   EPNAME("Peek");
   XrdSysMutexHelper mHelper;
   int ssler;

    //------------------------------------------------------------------------
    // Serialize call if need be
    //------------------------------------------------------------------------

    if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));

    //------------------------------------------------------------------------
    // Return an error if this socket received a fatal error as OpenSSL will
    // SEGV when called after such an error.
    //------------------------------------------------------------------------

    if (pImpl->fatal)
       {DBG_SIO("Failing due to previous error, fatal=" << (int)pImpl->fatal);
        return (XrdTls::RC)pImpl->fatal;
       }

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
    ssler = Diagnose("TLS_Peek", rc, XrdTls::dbgSIO);

    // If the error isn't due to blocking issues, we are done.
    //
    if (ssler != SSL_ERROR_WANT_READ && ssler != SSL_ERROR_WANT_WRITE)
       return XrdTls::ssl2RC(ssler);

    // If the caller is non-blocking, the return the issue. Otherwise, block.
    //
    if ((pImpl->hsNoBlock && NeedHS()) || !(pImpl->cAttr & rBlocking))
       return XrdTls::ssl2RC(ssler);

   } while(Wait4OK(ssler == SSL_ERROR_WANT_READ));

    // Return failure as the Wait failed.
    //
    return XrdTls::TLS_SYS_Error;
  }

/******************************************************************************/
/*                               P e n d i n g                                */
/******************************************************************************/

int XrdTlsSocket::Pending(bool any)
{
    XrdSysMutexHelper mHelper;

    //------------------------------------------------------------------------
    // Return an error if this socket received a fatal error as OpenSSL will
    // SEGV when called after such an error. So, return something reasonable.
    //------------------------------------------------------------------------

    if (pImpl->fatal) return 0;

    //------------------------------------------------------------------------
    // Serialize call if need be
    //------------------------------------------------------------------------

    if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));

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
    EPNAME("Read");
    XrdSysMutexHelper mHelper;
    int ssler;

    //------------------------------------------------------------------------
    // Serialize call if need be
    //------------------------------------------------------------------------

    if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));

    //------------------------------------------------------------------------
    // Return an error if this socket received a fatal error as OpenSSL will
    // SEGV when called after such an error.
    //------------------------------------------------------------------------

    if (pImpl->fatal) 
       {DBG_SIO("Failing due to previous error, fatal=" << (int)pImpl->fatal);
        return (XrdTls::RC)pImpl->fatal;
       }

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
       DBG_SIO(rc <<" out of " <<size <<" bytes.");
       return XrdTls::TLS_AOK;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    ssler = Diagnose("TLS_Read", rc, XrdTls::dbgSIO);
    if (ssler == SSL_ERROR_NONE)
       {bytesRead = 0;
        DBG_SIO("0 out of " <<size <<" bytes.");
        return XrdTls::TLS_AOK;
       }

    // If the error isn't due to blocking issues, we are done.
    //
    if (ssler != SSL_ERROR_WANT_READ && ssler != SSL_ERROR_WANT_WRITE)
       return XrdTls::ssl2RC(ssler);

    // If the caller is non-blocking for reads, return the issue. Otherwise,
    // block for the caller.
    //
    if ((pImpl->hsNoBlock && NeedHS()) || !(pImpl->cAttr & rBlocking))
       return XrdTls::ssl2RC(ssler);

    // Wait until we can read again.

   } while(Wait4OK(ssler == SSL_ERROR_WANT_READ));

    return XrdTls::TLS_SYS_Error;
  }

/******************************************************************************/
/*                            S e t T r a c e I D                             */
/******************************************************************************/
  
void XrdTlsSocket::SetTraceID(const char *tid)
{
   if (pImpl) pImpl->traceID = tid;
}

/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/
  
void XrdTlsSocket::Shutdown(XrdTlsSocket::SDType sdType)
{
   EPNAME("Shutdown");
   XrdSysMutexHelper mHelper;
   const char *how;
   int sdMode, rc;

// Make sure we have an ssl object.
//
   if (pImpl->ssl == 0) return;

// While we do not need to technically serialize here, we're being conservative
//
   if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));

// Perform shutdown as needed. This is required before freeing the ssl object.
// If we previously encountered a SYSCALL or SSL error, shutdown is prohibited!
// The following code is patterned after code in the public TomCat server.
//
   if (!pImpl->fatal)
      {switch(sdType)
             {case sdForce: // Forced shutdown which violate TLS standard!
                   sdMode = SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN;
                   how    = "forced";
                   break;
              case sdWait:  // Wait for client acknowledgement
                   sdMode = 0;
                   how    = "clean";
                   break;
              default:      // Fast shutdown, don't wait for ack (compliant)
                   sdMode = SSL_RECEIVED_SHUTDOWN;
                   how    = "fast";
                   break;
             }

       DBG_SOK("Doing " <<how <<" shutdown.");
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
                XrdTls::Emsg(pImpl->traceID, msgBuff, true);
                break;
               }
           }
      }

// Now free the ssl object which will free all the BIO's associated with it
//
   SSL_free( pImpl->ssl );
   pImpl->ssl = 0;
   pImpl->fatal = 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
XrdTls::RC XrdTlsSocket::Write( const char *buffer, size_t size,
                                int &bytesWritten )
{
    EPNAME("Write");
    XrdSysMutexHelper mHelper;
    int ssler;

    //------------------------------------------------------------------------
    // Serialize call if need be
    //------------------------------------------------------------------------

    if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));

    //------------------------------------------------------------------------
    // Return an error if this socket received a fatal error as OpenSSL will
    // SEGV when called after such an error.
    //------------------------------------------------------------------------

    if (pImpl->fatal)
       {DBG_SIO("Failing due to previous error, fatal=" << (int)pImpl->fatal);
        return (XrdTls::RC)pImpl->fatal;
       }

    //------------------------------------------------------------------------
    // If necessary, SSL_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------

 do{int rc = SSL_write( pImpl->ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if (rc > 0)
      {bytesWritten = rc;
       DBG_SIO(rc <<" out of " <<size <<" bytes.");
       return XrdTls::TLS_AOK;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    ssler = Diagnose("TLS_Write", rc, XrdTls::dbgSIO);
    if (ssler == SSL_ERROR_NONE)
       {bytesWritten = 0;
        DBG_SIO(rc <<" out of " <<size <<" bytes.");
        return XrdTls::TLS_AOK;
       }

    // If the error isn't due to blocking issues, we are done.
    //
    if (ssler != SSL_ERROR_WANT_READ && ssler != SSL_ERROR_WANT_WRITE)
       return XrdTls::ssl2RC(ssler);

    // If the caller is non-blocking for reads, return the issue. Otherwise,
    // block for the caller.
    //
    if ((pImpl->hsNoBlock && NeedHS()) || !(pImpl->cAttr & wBlocking))
       return XrdTls::ssl2RC(ssler);

    // Wait unil the write can get restarted

   } while(Wait4OK(ssler == SSL_ERROR_WANT_READ));

    return XrdTls::TLS_SYS_Error;
}

/******************************************************************************/
/*                         N e e d H a n d S h a k e                          */
/******************************************************************************/

  bool XrdTlsSocket::NeedHandShake()
  {
    XrdSysMutexHelper mHelper;

    //------------------------------------------------------------------------
    // Return an error if this socket received a fatal error as OpenSSL will
    // SEGV when called after such an error. So, return something reasonable.
    // Technically, we don't need to serialize this because nothing get
    // modified. We do so anyway out of abundance of caution.
    //------------------------------------------------------------------------

    if (pImpl->isSerial) mHelper.Lock(&(pImpl->sslMutex));
    if (pImpl->fatal) return false;
    pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );
    return !pImpl->hsDone;
  }

/******************************************************************************/
/* Private:                       N e e d H S                                 */
/******************************************************************************/

  bool XrdTlsSocket::NeedHS()
  {
    //------------------------------------------------------------------------
    // The following code is identical to NeedHandshake() except that it does
    // serialize the call because the caller already has done so. While we
    // could use a recursive mutex the overhead in doing so is not worth it
    // and it is only used for internal purposes.
    //------------------------------------------------------------------------

    if (pImpl->fatal) return false;
    pImpl->hsDone = bool( SSL_is_init_finished( pImpl->ssl ) );
    return !pImpl->hsDone;
  }
  
/******************************************************************************/
/*                               V e r s i o n                                */
/******************************************************************************/

  const char *XrdTlsSocket::Version()
  {
  // This call modifies nothing nor does it depend on modified data once the
  // connection is esablished and doesn't need serialization.
  //
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

   // Establish how long we will wait. This depends on hsDone being current!
   //
   if (pImpl->hsDone) timeout = -1;
      else timeout =  (pImpl->hsWait ? pImpl->hsWait : -1);

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
