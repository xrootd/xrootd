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

#include <cstdio>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#include <sys/stat.h>

#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysRAtomic.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

#include "XrdTls/XrdTls.hh"
#include "XrdTls/XrdTlsContext.hh"
#include "XrdTls/XrdTlsTrace.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdTlsGlobal
{
extern XrdSysTrace SysTrace;
};
  
/******************************************************************************/
/*                      X r d T l s C o n t e x t I m p l                     */
/******************************************************************************/

struct XrdTlsContextImpl
{
    XrdTlsContextImpl(XrdTlsContext *p)
                     : ctx(0), ctxnew(0), owner(p), flsCVar(0),
                       flushT(0),
                       crlRunning(false), flsRunning(false) {}
   ~XrdTlsContextImpl() {if (ctx)     SSL_CTX_free(ctx);
                         if (ctxnew)  delete ctxnew;
                         if (flsCVar) delete flsCVar;
                        }

    SSL_CTX                      *ctx;
    XrdTlsContext                *ctxnew;
    XrdTlsContext                *owner;
    XrdTlsContext::CTX_Params     Parm;
    XrdSysRWLock                  crlMutex;
    XrdSysCondVar                *flsCVar;
    short                         flushT;
    bool                          crlRunning;
    bool                          flsRunning;
    time_t                        lastCertModTime = 0;
    int                           sessionCacheOpts = -1;
    std::string                   sessionCacheId;
};
  
/******************************************************************************/
/*                   C r l   R e f r e s h   S u p p o r t                    */
/******************************************************************************/
  
namespace XrdTlsCrl
{
// Inital entry for refreshing crls
//
void *Refresh(void *parg)
{
   EPNAME("Refresh");
   int sleepTime;
   bool doreplace;

// Get the implementation details
//
   XrdTlsContextImpl *ctxImpl = static_cast<XrdTlsContextImpl*>(parg);

// Indicate we have started in the trace record
//
   DBG_CTX("CRL refresh started.")

// Do this forever but first get the sleep time
//
do{ctxImpl->crlMutex.ReadLock();
   sleepTime = ctxImpl->Parm.crlRT;
   ctxImpl->crlMutex.UnLock();

// We may have been cancelled, in which case we just exit
//
   if (!sleepTime)
      {ctxImpl->crlMutex.WriteLock();
       ctxImpl->crlRunning = false;
       ctxImpl->crlMutex.UnLock();
       DBG_CTX("CRL refresh ending by request!");
       return (void *)0;
      }

// Indicate we how long before a refresh
//
   DBG_CTX("CRL refresh will happen in " <<sleepTime <<" seconds.");

// Now sleep the request amount of time
//
   XrdSysTimer::Snooze(sleepTime);

   if (ctxImpl->owner->x509Verify() || ctxImpl->owner->newHostCertificateDetected()) {
       // Check if this context is still alive. Generally, it never gets deleted.
       //
       ctxImpl->crlMutex.WriteLock();
       if (!ctxImpl->owner) break;

       // We clone the original, this will give us the latest crls (i.e. refreshed).
       // We drop the lock while doing so as this may take a long time. This is
       // completely safe to do because we implicitly own the implementation.
       //
       ctxImpl->crlMutex.UnLock();
       XrdTlsContext *newctx = ctxImpl->owner->Clone();

       // Verify that the context was properly built
       //
       if (!newctx || !newctx->isOK())
       {XrdTls::Emsg("CrlRefresh:","Refresh of context failed!!!",false);
           continue;
       }

       // OK, set the new context to be used next time Session() is called.
       //
       ctxImpl->crlMutex.WriteLock();
       doreplace = (ctxImpl->ctxnew != 0);
       if (doreplace) delete ctxImpl->ctxnew;
       ctxImpl->ctxnew = newctx;
       ctxImpl->crlMutex.UnLock();

       // Do some debugging
       //
       if (doreplace) {DBG_CTX("CRL refresh created replacement x509 store.");}
       else {DBG_CTX("CRL refresh created new x509 store.");}
   }
  } while(true);

// If we are here the context that started us has gone away and we are done
//
   bool keepctx = ctxImpl->flsRunning;
   ctxImpl->crlRunning = false;
   ctxImpl->crlMutex.UnLock();
   if (!keepctx) delete ctxImpl;
   return (void *)0;
}
}

/******************************************************************************/
/*                   C a c h e   F l u s h   S u p p o r t                    */
/******************************************************************************/

namespace XrdTlsFlush
{
/******************************************************************************/
/*                               F l u s h e r                                */
/******************************************************************************/
// Inital entry for refreshing crls
//
void *Flusher(void *parg)
{
   EPNAME("Flusher");
   time_t tStart, tWaited;
   int    flushT, waitT, hits, miss, sesn, tmos;
   long   tNow;

// Get the implementation details
//
   XrdTlsContextImpl *ctxImpl = static_cast<XrdTlsContextImpl*>(parg);

// Get the interval as it may change as we are running
//
   ctxImpl->crlMutex.ReadLock();
   waitT = flushT = ctxImpl->flushT;
   ctxImpl->crlMutex.UnLock();

// Indicate we have started in the trace record
//
   DBG_CTX("Cache flusher started; interval="<<flushT<<" seconds.");

// Do this forever
//
do{tStart = time(0);
   ctxImpl->flsCVar->Wait(waitT);
   tWaited= time(0) - tStart;

// Check if this context is still alive. Generally, it never gets deleted.
//
   ctxImpl->crlMutex.ReadLock();
   if (!ctxImpl->owner) break;

// If the interval changed, see if we should wait a bit longer
//
   if (flushT != ctxImpl->flushT && tWaited < ctxImpl->flushT-1)
      {waitT = ctxImpl->flushT - tWaited;
       ctxImpl->crlMutex.UnLock();
       continue;
      }

// Get the new values and drop the lock
//
   waitT = flushT = ctxImpl->flushT;
   ctxImpl->crlMutex.UnLock();

// Get some relevant statistics
//
   sesn = SSL_CTX_sess_number(ctxImpl->ctx);
   hits = SSL_CTX_sess_hits(ctxImpl->ctx);
   miss = SSL_CTX_sess_misses(ctxImpl->ctx);
   tmos = SSL_CTX_sess_timeouts(ctxImpl->ctx);

// Flush the cache
//
   tNow = time(0);
   SSL_CTX_flush_sessions(ctxImpl->ctx, tNow);

// Print some stuff should debugging be on
//
   if (TRACING(XrdTls::dbgCTX))
      {char mBuff[512];
       snprintf(mBuff, sizeof(mBuff), "sess=%d hits=%d miss=%d timeouts=%d",
               sesn, hits, miss, tmos);
       DBG_CTX("Cache flushed; " <<mBuff);
      }
  } while(true);

// If we are here the context that started us has gone away and we are done
//
   bool keepctx = ctxImpl->crlRunning;
   ctxImpl->flsRunning = false;
   ctxImpl->crlMutex.UnLock();
   if (!keepctx) delete ctxImpl;
   return (void *)0;
}
  
/******************************************************************************/
/*                         S e t u p _ F l u s h e r                          */
/******************************************************************************/
  
bool Setup_Flusher(XrdTlsContextImpl *pImpl, int flushT)
{
   pthread_t tid;
   int rc;

// Set the new flush interval
//
   pImpl->crlMutex.WriteLock();
   pImpl->flushT = flushT;
   pImpl->crlMutex.UnLock();

// If the flush thread is already running, then wake it up to get the new value
//
   if (pImpl->flsRunning)
      {pImpl->flsCVar->Signal();
       return true;
      }

// Start the flusher thread
//
   pImpl->flsCVar = new XrdSysCondVar();
   if ((rc = XrdSysThread::Run(&tid, XrdTlsFlush::Flusher, (void *)pImpl,
                                     0, "Cache Flusher")))
      {char eBuff[512];
       snprintf(eBuff, sizeof(eBuff),
                "Unable to start cache flusher thread; rc=%d", rc);
       XrdTls::Emsg("SessCache:", eBuff, false);
       return false;
      }

// Finish up
//
   pImpl->flsRunning = true;
   SSL_CTX_set_session_cache_mode(pImpl->ctx, SSL_SESS_CACHE_NO_AUTO_CLEAR);
   return true;
}
}
  
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

template<bool is32>
struct tlsmix;

template<>
struct tlsmix<false> {
  static unsigned long mixer(unsigned long x) {
    // mixer based on splitmix64
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9UL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebUL;
    x ^= x >> 31;
    return x;
  }
};

template<>
struct tlsmix<true> {
  static unsigned long mixer(unsigned long x) {
    // mixer based on murmurhash3
    x ^= x >> 16;
    x *= 0x85ebca6bU;
    x ^= x >> 13;
    x *= 0xc2b2ae35U;
    x ^= x >> 16;
    return x;
  }
};

unsigned long sslTLS_id_callback(void)
{
   // base thread-id on the id given by XrdSysThread;
   // but openssl 1.0 uses thread-id as a key for looking
   // up per thread crypto ERR structures in a hash-table.
   // So mix bits so that the table's hash function gives
   // better distribution.

   unsigned long x = (unsigned long)XrdSysThread::ID();
   return tlsmix<sizeof(unsigned long)==4>::mixer(x);
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
// The following is the default cipher list. Note that for OpenSSL v1.0.2+ we
// use the recommended cipher list from Mozilla. Otherwise, we use the dumber
// less secure ciphers as older versions of openssl have issues with them. See
// ssl-config.mozilla.org/#config=intermediate&openssl=1.0.2k&guideline=5.4
//
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
const char *sslCiphers = "ECDHE-ECDSA-AES128-GCM-SHA256:"
                         "ECDHE-RSA-AES128-GCM-SHA256:"
                         "ECDHE-ECDSA-AES256-GCM-SHA384:"
                         "ECDHE-RSA-AES256-GCM-SHA384:"
                         "ECDHE-ECDSA-CHACHA20-POLY1305:"
                         "ECDHE-RSA-CHACHA20-POLY1305:"
                         "DHE-RSA-AES128-GCM-SHA256:"
                         "DHE-RSA-AES256-GCM-SHA384";
#else
const char *sslCiphers = "ALL:!LOW:!EXP:!MD5:!MD2";
#endif

XrdSysMutex            dbgMutex, tlsMutex;
XrdSys::RAtomic<bool>  initDbgDone{ false };
bool                   initTlsDone{ false };

/******************************************************************************/
/*                               I n i t T L S                                */
/******************************************************************************/
  
void InitTLS() // This is strictly a one-time call!
{
   XrdSysMutexHelper tlsHelper(tlsMutex);

// Make sure we are not trying to load the ssl library more than once. This can
// happen when a server and a client instance happen to be both defined.
//
   if (initTlsDone) return;
   initTlsDone = true;

// SSL library initialisation
//
   SSL_library_init();
   OpenSSL_add_all_algorithms();
   SSL_load_error_strings();
   OpenSSL_add_all_ciphers();
#if OPENSSL_VERSION_NUMBER < 0x30000000L
   ERR_load_BIO_strings();
#endif
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

/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/
  
void Fatal(std::string *eMsg, const char *msg, bool sslmsg=false)
{
// If there is an outboard error string object, return the message there.
//
   if (eMsg) *eMsg = msg;

// Now route the message to the message callback function. If this is an ssl
// related error we also flush the ssl error queue to prevent suprises.
//
   XrdTls::Emsg("TLS_Context:", msg, sslmsg);
}

/******************************************************************************/
/*                          G e t T l s M e t h o d                           */
/******************************************************************************/

const char *GetTlsMethod(const SSL_METHOD *&meth)
{
#if OPENSSL_VERSION_NUMBER > 0x1010000fL /* v1.1.0 */
  meth = TLS_method();
#else
  meth = SSLv23_method();
#endif
  if (meth == 0) return "No negotiable TLS method available.";
  return 0;
}
  
/******************************************************************************/
/*                              V e r P a t h s                               */
/******************************************************************************/

bool VerPaths(const char *cert, const char *pkey,
              const char *cadr, const char *cafl, std::string &eMsg)
{
   static const mode_t cert_mode = S_IRUSR | S_IWUSR | S_IRWXG | S_IROTH;
   static const mode_t pkey_mode = S_IRUSR | S_IWUSR;
   static const mode_t cadr_mode = S_IRWXU | S_IRGRP | S_IXGRP
                                           | S_IROTH | S_IXOTH;
   static const mode_t cafl_mode = S_IRUSR | S_IWUSR | S_IRWXG | S_IROTH;
   const char *emsg;

// If the ca cert directory is present make sure it's a directory and
// only the ower can write to that directory (anyone can read from it).
//
   if (cadr && (emsg = XrdOucUtils::ValPath(cadr, cadr_mode, true)))
      {eMsg  = "Unable to use CA cert directory ";
       eMsg += cadr; eMsg += "; "; eMsg += emsg;
       return false;
      }

// If a ca cert file is present make sure it's a file and only the owner can
// write it (anyone can read it).
//
   if (cafl && (emsg = XrdOucUtils::ValPath(cafl, cafl_mode, false)))
      {eMsg  = "Unable to use CA cert file ";
       eMsg += cafl; eMsg += "; "; eMsg += emsg;
       return false;
      }

// If a private key is present than make sure it's a file and only the
// owner has access to it.
//
   if (pkey && (emsg = XrdOucUtils::ValPath(pkey, pkey_mode, false)))
      {eMsg  = "Unable to use key file ";
       eMsg += pkey; eMsg += "; "; eMsg += emsg;
       return false;
      }

// If a cert file is present then make sure it's a file. If a keyfile is
// present then anyone can read it but only the owner can write it.
// Otherwise, only the owner can gave access to it (it contains the key).
//
   if (cert)
      {mode_t cmode = (pkey ? cert_mode : pkey_mode);
       if ((emsg = XrdOucUtils::ValPath(cert, cmode, false)))
          {if (pkey) eMsg = "Unable to use cert file ";
              else   eMsg = "Unable to use cert+key file ";
           eMsg += cert; eMsg += "; "; eMsg += emsg;
           return false;
          }
      }

// All tests succeeded.
//
   return true;
}

/******************************************************************************/
/*                                 V e r C B                                  */
/******************************************************************************/

extern "C"
{
int VerCB(int aOK, X509_STORE_CTX *x509P)
{
   if (!aOK)
      {X509 *cert = X509_STORE_CTX_get_current_cert(x509P);
       int depth  = X509_STORE_CTX_get_error_depth(x509P);
       int err    = X509_STORE_CTX_get_error(x509P);
       char name[512], info[1024];

       X509_NAME_oneline(X509_get_subject_name(cert), name, sizeof(name));
       snprintf(info,sizeof(info),"Cert verification failed for DN=%s",name);
       XrdTls::Emsg("CertVerify:", info, false);

       X509_NAME_oneline(X509_get_issuer_name(cert), name, sizeof(name));
       snprintf(info,sizeof(info),"Failing cert issuer=%s", name);
       XrdTls::Emsg("CertVerify:", info, false);

       snprintf(info, sizeof(info), "Error %d at depth %d [%s]", err, depth,
                                    X509_verify_cert_error_string(err));
       XrdTls::Emsg("CertVerify:", info, true);
      }

   return aOK;
}
}
  
} // Anonymous namespace end

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

#define KILL_CTX(x) if (x) {SSL_CTX_free(x); x = 0;}

#define FATAL(msg) {Fatal(eMsg, msg); KILL_CTX(pImpl->ctx); return;}

#define FATAL_SSL(msg) {Fatal(eMsg, msg, true); KILL_CTX(pImpl->ctx); return;}
  
XrdTlsContext::XrdTlsContext(const char *cert,  const char *key,
                             const char *caDir, const char *caFile,
                             uint64_t opts, std::string *eMsg)
                            : pImpl( new XrdTlsContextImpl(this) )
{
   class ctx_helper
        {public:

         void Keep() {ctxLoc = 0;}

              ctx_helper(SSL_CTX **ctxP) : ctxLoc(ctxP) {}
             ~ctx_helper() {if (ctxLoc && *ctxLoc)
                               {SSL_CTX_free(*ctxLoc); *ctxLoc = 0;}
                           }
         private:
         SSL_CTX **ctxLoc;
        } ctx_tracker(&pImpl->ctx);

   static const int sslOpts = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3
                            | SSL_OP_NO_COMPRESSION;

   std::string certFN, eText;
   const char *emsg;

// Assume we will fail
//
   pImpl->ctx   = 0;

// Verify that initialzation has occurred. This is not heavy weight as
// there will usually be no more than two instances of this object.
//
   if (!initDbgDone)
      {XrdSysMutexHelper dbgHelper(dbgMutex);
       if (!initDbgDone)
          {const char *dbg;
           if (!(opts & servr) && (dbg = getenv("XRDTLS_DEBUG")))
              {int dbgOpts = 0;
               if (strstr(dbg, "ctx")) dbgOpts |= XrdTls::dbgCTX;
               if (strstr(dbg, "sok")) dbgOpts |= XrdTls::dbgSOK;
               if (strstr(dbg, "sio")) dbgOpts |= XrdTls::dbgSIO;
               if (!dbgOpts) dbgOpts = XrdTls::dbgALL;
               XrdTls::SetDebug(dbgOpts|XrdTls::dbgOUT);
              }
           if ((emsg = Init())) FATAL(emsg);
           initDbgDone = true;
          }
      }

// If no CA cert information is specified and this is not a server context,
// then get the paths from the environment. They must exist as we need to
// verify peer certs in order to verify target host names client-side. We
// also use this setupt to see if we should use a specific cert and key.
//
   if (!(opts & servr))
      {if (!caDir && !caFile)
          {caDir  = getenv("X509_CERT_DIR");
           caFile = getenv("X509_CERT_FILE");
           if (!caDir && !caFile)
              FATAL("No CA cert specified; host identity cannot be verified.");
          }
       if (!key)  key  = getenv("X509_USER_KEY");
       if (!cert) cert = getenv("X509_USER_PROXY");
       if (!cert)
          {struct stat Stat;
           long long int uid = static_cast<long long int>(getuid());
           certFN = std::string("/tmp/x509up_u") + std::to_string(uid);
           if (!stat(certFN.c_str(), &Stat)) cert = certFN.c_str();
          }
      }

// Before we try to use any specified files, make sure they exist, are of
// the right type and do not have excessive access privileges.
//                                                                              .a
   if (!VerPaths(cert, key, caDir, caFile, eText)) FATAL( eText.c_str());

// Copy parameters to out parm structure.
//
   if (cert)   {
       pImpl->Parm.cert   = cert;
       //This call should not fail as a stat is already performed in the call of VerPaths() above
       XrdOucUtils::getModificationTime(pImpl->Parm.cert.c_str(),pImpl->lastCertModTime);
   }
   if (key)    pImpl->Parm.pkey   = key;
   if (caDir)  pImpl->Parm.cadir  = caDir;
   if (caFile) pImpl->Parm.cafile = caFile;
   pImpl->Parm.opts   = opts;
   if (opts & crlRF) {
       // What we store in crlRF is the time in minutes, convert it back to seconds
       pImpl->Parm.crlRT = static_cast<int>((opts & crlRF) >> crlRS) * 60;
   }

// Get the correct method to use for TLS and check if successful create a
// server context that uses the method.
//
   const SSL_METHOD *meth;
   emsg = GetTlsMethod(meth);
   if (emsg) FATAL(emsg);

   pImpl->ctx = SSL_CTX_new(meth);

// Make sure we have a context here
//
   if (pImpl->ctx == 0) FATAL_SSL("Unable to allocate TLS context!");

// Always prohibit SSLv2 & SSLv3 as these are not secure.
//
   SSL_CTX_set_options(pImpl->ctx, sslOpts);

// Handle session re-negotiation automatically
//
// SSL_CTX_set_mode(pImpl->ctx, sslMode);

// Turn off the session cache as it's useless with peer cert chains
//
   SSL_CTX_set_session_cache_mode(pImpl->ctx, SSL_SESS_CACHE_OFF);

// Establish the CA cert locations, if specified. Then set the verification
// depth and turn on peer cert validation. For now, we don't set a callback.
// In the future we may to grab debugging information.
//
   if (caDir || caFile)
     {if (!SSL_CTX_load_verify_locations(pImpl->ctx, caFile, caDir))
         FATAL_SSL("Unable to load the CA cert file or directory.");

      int vDepth = (opts & vdept) >> vdepS;
      SSL_CTX_set_verify_depth(pImpl->ctx, (vDepth ? vDepth : 9));

      bool LogVF = (opts & logVF) != 0;
      SSL_CTX_set_verify(pImpl->ctx, SSL_VERIFY_PEER, (LogVF ? VerCB : 0));

      unsigned long xFlags = (opts & nopxy ? 0 : X509_V_FLAG_ALLOW_PROXY_CERTS);
      if (opts & crlON)
         {xFlags |= X509_V_FLAG_CRL_CHECK;
          if (opts & crlFC) xFlags |= X509_V_FLAG_CRL_CHECK_ALL;
         }
      if (opts) X509_STORE_set_flags(SSL_CTX_get_cert_store(pImpl->ctx),xFlags);
     } else {
      SSL_CTX_set_verify(pImpl->ctx, SSL_VERIFY_NONE, 0);
     }

// Set cipher list
//
   if (!SSL_CTX_set_cipher_list(pImpl->ctx, sslCiphers))
      FATAL_SSL("Unable to set SSL cipher list; no supported ciphers.");

// If we need to enable eliptic-curve support, do so now. Note that for
// OpenSSL 1.1.0+ this is automatically done for us.
//
#if SSL_CTRL_SET_ECDH_AUTO
   SSL_CTX_set_ecdh_auto(pImpl->ctx, 1);
#endif

// We normally handle renegotiation during reads and writes or selective
// prohibit on a SSL socket basis. The calle may request this be applied
// to all SSL's generated from this context. If so, do it here.
//
   if (opts & artON) SSL_CTX_set_mode(pImpl->ctx, SSL_MODE_AUTO_RETRY);

// If there is no cert then assume this is a generic context for a client
//
   if (cert == 0)
      {ctx_tracker.Keep();
       return;
      }

// We have a cert. If the key is missing then we assume the key is in the
// cert file (ssl will complain if it isn't).
//
   if (!key) key = cert;

// Load certificate
//
   if (SSL_CTX_use_certificate_chain_file(pImpl->ctx, cert) != 1)
      FATAL_SSL("Unable to create TLS context; invalid certificate.");

// Load the private key
//
   if (SSL_CTX_use_PrivateKey_file(pImpl->ctx, key, SSL_FILETYPE_PEM) != 1 )
      FATAL_SSL("Unable to create TLS context; invalid private key.");

// Make sure the key and certificate file match.
//
   if (SSL_CTX_check_private_key(pImpl->ctx) != 1 )
      FATAL_SSL("Unable to create TLS context; cert-key mismatch.");

// All went well, start the CRL refresh thread and keep the context.
//
   if(opts & rfCRL) {
       SetCrlRefresh();
   }
   ctx_tracker.Keep();
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdTlsContext::~XrdTlsContext()
{
// We can delet eour implementation of there is no refresh thread running. If
// there is then the refresh thread has to delete the implementation.
//
   if (pImpl->crlRunning | pImpl->flsRunning)
      {pImpl->crlMutex.WriteLock();
       pImpl->owner = 0;
       pImpl->crlMutex.UnLock();
      } else delete pImpl;
}

/******************************************************************************/
/*                                 C l o n e                                  */
/******************************************************************************/

XrdTlsContext *XrdTlsContext::Clone(bool full,bool startCRLRefresh)
{
  XrdTlsContext::CTX_Params &my = pImpl->Parm;
  const char *cert = (my.cert.size()   ? my.cert.c_str()   : 0);
  const char *pkey = (my.pkey.size()   ? my.pkey.c_str()   : 0);
  const char *caD  = (my.cadir.size()  ? my.cadir.c_str()  : 0);
  const char *caF  = (my.cafile.size() ? my.cafile.c_str() : 0);

// If this is a non-full context, get rid of any verification
//
   if (!full) caD = caF = 0;

// Cloning simply means getting a object with the old parameters.
//
   uint64_t myOpts = my.opts;
    if(startCRLRefresh){
        myOpts |= XrdTlsContext::rfCRL;
    } else {
        myOpts &= ~XrdTlsContext::rfCRL;
    }
   XrdTlsContext *xtc = new XrdTlsContext(cert, pkey, caD, caF, myOpts);

// Verify that the context was built
//
   if (xtc->isOK()) {
       if(pImpl->sessionCacheOpts != -1){
           //A SessionCache() call was done for the current context, so apply it for this new cloned context
           xtc->SessionCache(pImpl->sessionCacheOpts,pImpl->sessionCacheId.c_str(),pImpl->sessionCacheId.size());
       }
       return xtc;
   }

// We failed, cleanup.
//
   delete xtc;
   return 0;
}

/******************************************************************************/
/*                               C o n t e x t                                */
/******************************************************************************/

void *XrdTlsContext::Context()
{
   return pImpl->ctx;
}
  
/******************************************************************************/
/*                             G e t P a r a m s                              */
/******************************************************************************/

const XrdTlsContext::CTX_Params *XrdTlsContext::GetParams()
{
  return &pImpl->Parm;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
const char *XrdTlsContext::Init()
{

// Disallow use if this object unless SSL provides thread-safety!
//
#ifndef OPENSSL_THREADS
   return "Installed OpenSSL lacks the required thread support!";
#endif

// Initialize the library (one time call)
//
   InitTLS();
   return 0;
}

/******************************************************************************/
/*                                  i s O K                                   */
/******************************************************************************/

bool XrdTlsContext::isOK()
{
   return pImpl->ctx != 0;
}
  
/******************************************************************************/
/*                               S e s s i o n                                */
/******************************************************************************/

// Note: The reason we handle the x509 store update here is because allow the
// SSL context to be exported and then have no lock control over it. This may
// happen for transient purposes other than creating sessions. Once we
// disallow direct access to the context, the exchange can happen in the
// refresh thread which simplifies this whole process.

void *XrdTlsContext::Session()
{
#if OPENSSL_VERSION_NUMBER >= 0x10002000L

   EPNAME("Session");
   SSL *ssl;

// Check if we have a refreshed context. If so, we need to replace the X509
// store in the current context with the new one before we create the session.
//
   pImpl->crlMutex.ReadLock();
   if (!(pImpl->ctxnew))
      {ssl = SSL_new(pImpl->ctx);
       pImpl->crlMutex.UnLock();
       return ssl;
      }

// Things have changed, so we need to take the long route here. We need to
// replace the x509 cache with the current cache. Get a R/W lock now.
//
   pImpl->crlMutex.UnLock();
   pImpl->crlMutex.WriteLock();

// If some other thread beat us to the punch, just return what we have.
//
   if (!(pImpl->ctxnew))
      {ssl = SSL_new(pImpl->ctx);
       pImpl->crlMutex.UnLock();
       return ssl;
      }

// Do some tracing
//
   DBG_CTX("Replacing x509 store with new contents.");

// Get the new store and set it in our context. Setting the store is black
// magic. For OpenSSL < 1.1, Two stores need to be set with the "set1" variant.
// Newer version only require SSL_CTX_set1_cert_store() to be used.
//
   //We have a new context generated by Refresh, so we must use it.
   XrdTlsContext * ctxnew = pImpl->ctxnew;

#if OPENSSL_VERSION_NUMBER < 0x10101000L
   /*X509_STORE *newX509 = SSL_CTX_get_cert_store(ctxnew->pImpl->ctx);
   SSL_CTX_set1_verify_cert_store(pImpl->ctx, newX509);
   SSL_CTX_set1_chain_cert_store(pImpl->ctx, newX509);*/
   //The above two macros actually do not replace the certificate that has
   //to be used for that SSL session, so we will create the session with the SSL_CTX * of
   //the TlsContext created by Refresh()
   //First, free the current SSL_CTX, if it is used by any transfer, it will just decrease
   //the reference counter of it. There is therefore no risk of double free...
   SSL_CTX_free(pImpl->ctx);
   pImpl->ctx = ctxnew->pImpl->ctx;
   //In the destructor of XrdTlsContextImpl, SSL_CTX_Free() is
   //called if ctx is != 0. As this new ctx is used by the session
   //we just created, we don't want that to happen. We therefore set it to 0.
   //The SSL_free called on the session will cleanup the context for us.
   ctxnew->pImpl->ctx = 0;
#else
   X509_STORE *newX509 = SSL_CTX_get_cert_store(ctxnew->pImpl->ctx);
   SSL_CTX_set1_cert_store(pImpl->ctx, newX509);
#endif

// Save the generated context and clear it's presence
//
   XrdTlsContext *ctxold = pImpl->ctxnew;
   pImpl->ctxnew = 0;

// Generate a new session (might as well to keep the lock we have)
//
   ssl = SSL_new(pImpl->ctx);

// OK, now we can drop all the locks and get rid of the old context
//
   pImpl->crlMutex.UnLock();
   delete ctxold;
   return ssl;

#else
// If we did not compile crl refresh code, we can simply return the OpenSSL
// session using our context. Otherwise, we need to see if we have a refreshed
// context and if so, carry forward the X509_store to our original context.
//
   return SSL_new(pImpl->ctx);
#endif
}
  
/******************************************************************************/
/*                          S e s s i o n C a c h e                           */
/******************************************************************************/

int XrdTlsContext::SessionCache(int opts, const char *id, int idlen)
{
   static const int doSet = scSrvr | scClnt | scOff;
   long sslopt = 0;
   int flushT = opts & scFMax;

   pImpl->sessionCacheOpts = opts;
   pImpl->sessionCacheId = id;

// If initialization failed there is nothing to do
//
   if (pImpl->ctx == 0) return 0;

// Set options as appropriate
//
   if (opts & doSet)
      {if (opts & scOff) sslopt = SSL_SESS_CACHE_OFF;
          else {if (opts & scSrvr) sslopt  = SSL_SESS_CACHE_SERVER;
                if (opts & scClnt) sslopt |= SSL_SESS_CACHE_CLIENT;
               }
      }

// Check if we should set any cache options or simply get them
//
   if (!(opts & doSet)) sslopt = SSL_CTX_get_session_cache_mode(pImpl->ctx);
      else {sslopt = SSL_CTX_set_session_cache_mode(pImpl->ctx, sslopt);
            if (opts & scOff) SSL_CTX_set_options(pImpl->ctx, SSL_OP_NO_TICKET);
           }

// Compute what he previous cache options were
//
   opts = scNone;
   if (sslopt & SSL_SESS_CACHE_SERVER) opts |= scSrvr;
   if (sslopt & SSL_SESS_CACHE_CLIENT) opts |= scClnt;
   if (!opts) opts = scOff;
   if (sslopt & SSL_SESS_CACHE_NO_AUTO_CLEAR) opts |= scKeep;
   opts |= (static_cast<int>(pImpl->flushT) & scFMax);

// Set the id is so wanted
//
   if (id && idlen > 0)
      {if (!SSL_CTX_set_session_id_context(pImpl->ctx,
                                          (unsigned const char *)id,
                                          (unsigned int)idlen)) opts |= scIdErr;
      }

// If a flush interval was specified and it is different from what we have
// then reset the flush interval.
//
   if (flushT && flushT != pImpl->flushT)
      XrdTlsFlush::Setup_Flusher(pImpl, flushT);

// All done
//
   return opts;
}
  
/******************************************************************************/
/*                     S e t C o n t e x t C i p h e r s                      */
/******************************************************************************/

bool XrdTlsContext::SetContextCiphers(const char *ciphers)
{
   if (pImpl->ctx && SSL_CTX_set_cipher_list(pImpl->ctx, ciphers)) return true;

   char eBuff[2048];
   snprintf(eBuff,sizeof(eBuff),"Unable to set context ciphers '%s'",ciphers);
   Fatal(0, eBuff, true);
   return false;
}

/******************************************************************************/
/*                     S e t D e f a u l t C i p h e r s                      */
/******************************************************************************/

void XrdTlsContext::SetDefaultCiphers(const char *ciphers)
{
   sslCiphers = ciphers;
}
  
/******************************************************************************/
/*                         S e t C r l R e f r e s h                          */
/******************************************************************************/

bool XrdTlsContext::SetCrlRefresh(int refsec)
{
#if OPENSSL_VERSION_NUMBER >= 0x10002000L

   pthread_t tid;
   int       rc;

// If it's negative or equal to 0, use the current setting
//
   if (refsec <= 0)
      {pImpl->crlMutex.WriteLock();
       refsec = pImpl->Parm.crlRT;
       pImpl->crlMutex.UnLock();
       if (!refsec) refsec = XrdTlsContext::DEFAULT_CRL_REF_INT_SEC;
      }

// Make sure this is at least 60 seconds between refreshes
//
// if (refsec < 60) refsec = 60;

// We will set the new interval and start a refresh thread if not running.
//
   pImpl->crlMutex.WriteLock();
   pImpl->Parm.crlRT = refsec;
   if (!pImpl->crlRunning)
      {if ((rc = XrdSysThread::Run(&tid, XrdTlsCrl::Refresh, (void *)pImpl,
                                   0, "CRL Refresh")))
          {char eBuff[512];
           snprintf(eBuff, sizeof(eBuff),
                    "Unable to start CRL refresh thread; rc=%d", rc);
           XrdTls::Emsg("CrlRefresh:", eBuff, false);
           pImpl->crlMutex.UnLock();
           return false;
          } else pImpl->crlRunning = true;
       pImpl->crlMutex.UnLock();
      }

// All done
//
   return true;

#else
// We use features present on OpenSSL 1.02 and above to implement crl refresh.
// Older version are too difficult to deal with. Issue a message if this
// feature is being enabled on an old version.
//
   XrdTls::Emsg("CrlRefresh:", "Refreshing CRLs only supported in "
                "OpenSSL version >= 1.02; CRL refresh disabled!", false);
   return false;
#endif
}
  
/******************************************************************************/
/*                            x 5 0 9 V e r i f y                             */
/******************************************************************************/
  
bool XrdTlsContext::x509Verify()
{
   return !(pImpl->Parm.cadir.empty()) || !(pImpl->Parm.cafile.empty());
}

bool XrdTlsContext::newHostCertificateDetected() {
    const std::string certPath = pImpl->Parm.cert;
    if(certPath.empty()) {
        //No certificate provided, should not happen though
        return false;
    }
    time_t modificationTime;
    if(!XrdOucUtils::getModificationTime(certPath.c_str(),modificationTime)){
        if (pImpl->lastCertModTime != modificationTime) {
            //The certificate file has changed
            pImpl->lastCertModTime = modificationTime;
            return true;
        }
    }
    return false;
}
