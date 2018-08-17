//------------------------------------------------------------------------------
// Copyright (c) 2011-2018 by European Organization for Nuclear Research (CERN)
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

#include <iostream>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdTls/XrdTlsContext.hh"

/******************************************************************************/
/*                 S S L   T h r e a d i n g   S u p p o r t                  */
/******************************************************************************/

// The following may confusing because SSL MT support is somewhat bizarre.
// Versions  < 1.0 require a numeric thread_id and lock callbasks.
// Versions  < 1.1 require a lock_callbacks but the thread_is callback is
//                 optional. While the numeric thread_id callback can be used
//                 it's deprecated and fancier pointer/numeric call should be
//                 used. In our case, we use the deprecated version.
// Versions >- 1.1 Do not need any callbacks as all threading functions are
//                 internally defined to use native MT functions.
  
#if OPENSSL_VERSION_NUMBER < 0x10100000L && defined(OPENSSL_THREADS)
namespace
{
#define XRDTLS_SET_CALLBACKS 1
#ifdef __solaris__
extern "C" {
#endif

unsigned long sslTLS_id_callback(void)
{
   return (unsigned long)XrdSysThread::ID();
}

XrdSysMutex *MutexVector = 0;

void sslTLS_lock(int mode, int n, const char *file, int line)
{
// Use exclusive locks. At some point, SSL categorizes these as read and
// write locks but it's not clear when this actually occurs, sigh.
//
   if (mode & CRYPTO_LOCK) MutexVector[n].Lock();
      else                 MutexVector[n].UnLock();
}
#ifdef __solaris__
}
#endif
}   // namespace
#else
#undef XRDTLS_SET_CALLBACKS
#endif

/******************************************************************************/
/*                F i l e   L o c a l   D e f i n i t i o n s                 */
/******************************************************************************/
  
namespace
{
int sslOpts = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
int sslMode = SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE;

XrdSysMutex ctxMutex;
bool        initDone = false;

void InitTLS()
{
   XrdSysMutexHelper ctxHelper(ctxMutex);

// Make sure we are not trying to load the ssl library more than once. This can
// happen when a server and a client instance happen to be both defined.
//
   if (initDone) return;

// SSL library initialisation
//
   SSL_library_init();
   OpenSSL_add_all_algorithms();
   SSL_load_error_strings();
   OpenSSL_add_all_ciphers();
   ERR_load_BIO_strings();
   ERR_load_crypto_strings();

// Set callbacks if we need to do this
//
#ifdef XRDTLS_SET_CALLBACKS

   int n =  CRYPTO_num_locks();
   if (n > 0)
      {MutexVector = new XrdSysMutex[n];
       CRYPTO_set_locking_callback(sslTLS_lock);
      }
   CRYPTO_set_id_callback(sslTLS_id_callback);

#endif
}

void eMsg(XrdSysError *eDest, const char *pfx, const char *eTxt)
{
   if (eDest)
      {if (pfx) eDest->Say(pfx, ": ", eTxt);
          else  eDest->Say(eTxt);
      } else {
      if (pfx) std::cerr <<pfx <<": " <<eTxt <<'\n' <<std::flush;
         else  std::cerr <<eTxt <<'\n' <<std::flush;
      }
}
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdTlsContext::XrdTlsContext(const char *cert, const char *key, XrdTlsContext::Protocol prot)
{

// Assume we will fail
//
   ctx   = 0;
   eText = 0;

// Disallow use if this object unless SSL provides thread-safety!
//
#ifndef OPENSSL_THREADS
   eText = "Installed OpenSSL lacks the required thread support!";
   return;
#endif

// Verify that initialzation has occurred. This is not heavy weight as
// there will usually be no more than two instances of this object.
//
   AtomicBeg(ctxMutex);
   bool done = AtomicGet(initDone);
   AtomicEnd(ctxMutex);
   if (!done) InitTLS();

// Create the SSL server context. By default we just talk TLS but the
// caller may enable the less secure SSL protocol (e.g. https).
//
   if (prot == doSSL) ctx = SSL_CTX_new(SSLv23_method());
      else            ctx = SSL_CTX_new( TLSv1_method());

// Make sure we have a context here
//
   if (ctx == 0)
      {eText = "Unable to create TLS context; initialization failed.";
       return;
      }

// Recommended to avoid SSLv2 & SSLv3
//
   SSL_CTX_set_options(ctx, sslOpts);

// Handle session re-negotiation automatically
//
   SSL_CTX_set_mode(ctx, sslMode);

// If there is no cert then assume this is a genric context for a client
//
   if (cert == 0) return;

// We have a cert. If the key is missing then we assume the key is in the
// cert file (ssl will complain if it isn't).
//
   if (!key) key = cert;

// Load certificate
//
   if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) != 1)
      {eText = "Unable to create TLS context; certificate error.";
       SSL_CTX_free(ctx); ctx = 0;
       return;
      }

// Load the private key
//
   if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1 )
      {eText = "Unable to create TLS context; private key error.";
       SSL_CTX_free(ctx); ctx = 0;
       return;
      }

// Make sure the key and certificate file match.
//
   if (SSL_CTX_check_private_key(ctx) != 1 )
      {eText = "Unable to create TLS context; cert-key mismatch.";
       SSL_CTX_free(ctx); ctx = 0;
       return;
      }
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdTlsContext::~XrdTlsContext() {if (ctx) SSL_CTX_free(ctx);}

/******************************************************************************/
/*                               G e t E r r s                                */
/******************************************************************************/

std::string XrdTlsContext::GetErrs(const char *pfx)
{
  std::string eBlob, ePfx;
  unsigned long eCode;

  if (pfx) ePfx = std::string(pfx) + ": ";

  if (eText)
     {eBlob = ePfx + eText + '\n';
      eText = 0;
     }

  if (!(eCode = ERR_get_error()))
     eBlob += ePfx + "No OpenSSL complaints found.\n";
     else {eBlob += "OpenSSL complaints...\n";
           do {eBlob += ePfx + ERR_reason_error_string(eCode) + '\n';
              } while((eCode = ERR_get_error()));
          }

  return eBlob;
}

/******************************************************************************/
/*                             P r i n t E r r s                              */
/******************************************************************************/
  
void XrdTlsContext::PrintErrs(const char *pfx, XrdSysError *eDest)
{
  unsigned long eCode;

  if (eText) {eMsg(eDest, pfx, eText); eText = 0;}

  if (!(eCode = ERR_get_error())) eMsg(eDest,pfx,"No OpenSSL complaints found.");
     else {eMsg(eDest, pfx, "OpenSSL complaints...");
           do {eMsg(eDest, pfx,  ERR_reason_error_string(eCode));
              } while((eCode = ERR_get_error()));
          }

  eText = 0;
}
