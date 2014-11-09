/******************************************************************************/
/*                                                                            */
/*            X r d C r y p t o S s l F a c t o r y . c c                     */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Gerri Ganis for CERN                                         */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Implementation of the OpenSSL crypto factory                               */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptosslFactory.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslCipher.hh"
#include "XrdCrypto/XrdCryptosslMsgDigest.hh"
#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdCrypto/XrdCryptosslX509Crl.hh"
#include "XrdCrypto/XrdCryptosslX509Req.hh"

#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSut/XrdSutRndm.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"

#include "XrdVersion.hh"

#include <openssl/rand.h>
#include <openssl/ssl.h>

//
// For error logging and tracing
static XrdSysLogger Logger;
static XrdSysError eDest(0,"cryptossl_");

// Mutexes for OpenSSL
XrdSysMutex *XrdCryptosslFactory::CryptoMutexPool[SSLFACTORY_MAX_CRYPTO_MUTEX];

/******************************************************************************/
/*             T h r e a d - S a f e n e s s   F u n c t i o n s              */
/******************************************************************************/
#ifdef __solaris__
extern "C" {
#endif
static unsigned long sslfactory_id_callback(void) {
  return (unsigned long)XrdSysThread::ID();
}

void sslfactory_lock(int mode, int n, const char *file, int line)
{
  if (mode & CRYPTO_LOCK) {
    if (XrdCryptosslFactory::CryptoMutexPool[n]) {
      XrdCryptosslFactory::CryptoMutexPool[n]->Lock();
    }
  } else {
    if (XrdCryptosslFactory::CryptoMutexPool[n]) {
      XrdCryptosslFactory::CryptoMutexPool[n]->UnLock();
    }
  }
}
#ifdef __solaris__
}
#endif


//______________________________________________________________________________
XrdCryptosslFactory::XrdCryptosslFactory() :
                     XrdCryptoFactory("ssl",XrdCryptosslFactoryID)
{
   // Constructor: init the needed components of the OpenSSL library
   EPNAME("sslFactory::XrdCryptosslFactory");

   // Init SSL ...
   SSL_library_init();
   //  ... and its error strings
   SSL_load_error_strings();
   // Load Ciphers
   OpenSSL_add_all_ciphers();
   // Load Msg Digests
   OpenSSL_add_all_digests();

   if (SSLFACTORY_MAX_CRYPTO_MUTEX < CRYPTO_num_locks() ) {
      SetTrace(0);
      TRACE(ALL, "WARNING: do not have enough crypto mutexes as required by crypto_ssl");
      TRACE(ALL, "        (suggestion: recompile increasing SSLFACTORY_MAX_CRYPTO_MUTEX to "<< CRYPTO_num_locks()<<")");
   } else {
      // This code taken from XrdSecProtocolssl thread-safety
      for (int i = 0; i < SSLFACTORY_MAX_CRYPTO_MUTEX; i++) {
         XrdCryptosslFactory::CryptoMutexPool[i] = new XrdSysMutex();
      }
   }

#if defined(OPENSSL_THREADS)
   // Thread support enabled: set callback functions
   CRYPTO_set_locking_callback(sslfactory_lock);
   CRYPTO_set_id_callback(sslfactory_id_callback);
#else
   SetTrace(0);
   TRACE(ALL, "WARNING: OpenSSL lacks thread support: possible thread-safeness problems!");
   TRACE(ALL, "         (suggestion: recompile enabling thread support)");
#endif

   // Init Random machinery
   int klen = 32;
   char *ktmp = XrdSutRndm::GetBuffer(klen);
   if (ktmp) {
      // Feed the random engine
      RAND_seed(ktmp,klen);
      delete[] ktmp;
   }
}

//______________________________________________________________________________
void XrdCryptosslFactory::SetTrace(kXR_int32 trace)
{
   // Set trace flags according to 'trace'

   //
   // Initiate error logging and tracing
   eDest.logger(&Logger);
   if (!sslTrace)
      sslTrace = new XrdOucTrace(&eDest);
   if (sslTrace) {
      // Set debug mask
      sslTrace->What = 0;
      // Low level only
      if ((trace & sslTRACE_Notify))
         sslTrace->What |= sslTRACE_Notify;
      // Medium level
      if ((trace & sslTRACE_Debug))
         sslTrace->What |= (sslTRACE_Notify | sslTRACE_Debug);
      // High level
      if ((trace & sslTRACE_Dump))
         sslTrace->What |= sslTRACE_ALL;
   }
}

//______________________________________________________________________________
XrdCryptoKDFunLen_t XrdCryptosslFactory::KDFunLen()
{
   // Return an instance of an implementation of the PBKDF2 fun length.

   return &XrdCryptosslKDFunLen;
}

//______________________________________________________________________________
XrdCryptoKDFun_t XrdCryptosslFactory::KDFun()
{
   // Return an instance of an implementation of the PBKDF2 function.

   return &XrdCryptosslKDFun;
}

//______________________________________________________________________________
bool XrdCryptosslFactory::SupportedCipher(const char *t)
{
   // Returns true if specified cipher is supported

   return XrdCryptosslCipher::IsSupported(t);
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptosslFactory::Cipher(const char *t, int l)
{
   // Return an instance of a ssl implementation of XrdCryptoCipher.

   XrdCryptoCipher *cip = new XrdCryptosslCipher(t,l);
   if (cip) {
      if (cip->IsValid())
         return cip;
      else
         delete cip;
   }
   return (XrdCryptoCipher *)0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptosslFactory::Cipher(const char *t, 
                                             int l, const char *k, 
                                             int liv, const char *iv)
{
   // Return an instance of a ssl implementation of XrdCryptoCipher.

   XrdCryptoCipher *cip = new XrdCryptosslCipher(t,l,k,liv,iv);
   if (cip) {
      if (cip->IsValid())
         return cip;
      else
         delete cip;
   }
   return (XrdCryptoCipher *)0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptosslFactory::Cipher(XrdSutBucket *b)
{
   // Return an instance of a Local implementation of XrdCryptoCipher.

   XrdCryptoCipher *cip = new XrdCryptosslCipher(b);
   if (cip) {
      if (cip->IsValid())
         return cip;
      else
         delete cip;
   }
   return (XrdCryptoCipher *)0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptosslFactory::Cipher(int b, char *p,
                                             int l, const char *t)
{
   // Return an instance of a Ssl implementation of XrdCryptoCipher.

   XrdCryptoCipher *cip = new XrdCryptosslCipher(b,p,l,t);
   if (cip) {
      if (cip->IsValid())
         return cip;
      else
         delete cip;
   }
   return (XrdCryptoCipher *)0;
}

//______________________________________________________________________________
XrdCryptoCipher *XrdCryptosslFactory::Cipher(const XrdCryptoCipher &c)
{
   // Return an instance of a Ssl implementation of XrdCryptoCipher.

   XrdCryptoCipher *cip = new XrdCryptosslCipher(*((XrdCryptosslCipher *)&c));
   if (cip) {
      if (cip->IsValid())
         return cip;
      else
         delete cip;
   }
   return (XrdCryptoCipher *)0;
}

//______________________________________________________________________________
bool XrdCryptosslFactory::SupportedMsgDigest(const char *dgst)
{
   // Returns true if specified digest is supported

   return XrdCryptosslMsgDigest::IsSupported(dgst);
}

//______________________________________________________________________________
XrdCryptoMsgDigest *XrdCryptosslFactory::MsgDigest(const char *dgst)
{
   // Return an instance of a ssl implementation of XrdCryptoMsgDigest.

   XrdCryptoMsgDigest *md = new XrdCryptosslMsgDigest(dgst);
   if (md) {
      if (md->IsValid())
         return md;
      else
         delete md;
   }
   return (XrdCryptoMsgDigest *)0;
}

//______________________________________________________________________________
XrdCryptoRSA *XrdCryptosslFactory::RSA(int bits, int exp)
{
   // Return an instance of a ssl implementation of XrdCryptoRSA.

   XrdCryptoRSA *rsa = new XrdCryptosslRSA(bits,exp);
   if (rsa) {
      if (rsa->IsValid())
         return rsa;
      else
         delete rsa;
   }
   return (XrdCryptoRSA *)0;
}

//______________________________________________________________________________
XrdCryptoRSA *XrdCryptosslFactory::RSA(const char *pub, int lpub)
{
   // Return an instance of a ssl implementation of XrdCryptoRSA.

   XrdCryptoRSA *rsa = new XrdCryptosslRSA(pub,lpub);
   if (rsa) {
      if (rsa->IsValid())
         return rsa;
      else
         delete rsa;
   }
   return (XrdCryptoRSA *)0;
}

//______________________________________________________________________________
XrdCryptoRSA *XrdCryptosslFactory::RSA(const XrdCryptoRSA &r)
{
   // Return an instance of a Ssl implementation of XrdCryptoRSA.

   XrdCryptoRSA *rsa = new XrdCryptosslRSA(*((XrdCryptosslRSA *)&r));
   if (rsa) {
      if (rsa->IsValid())
         return rsa;
      else
         delete rsa;
   }
   return (XrdCryptoRSA *)0;
}

//______________________________________________________________________________
XrdCryptoX509 *XrdCryptosslFactory::X509(const char *cf, const char *kf)
{
   // Return an instance of a ssl implementation of XrdCryptoX509.

   XrdCryptoX509 *x509 = new XrdCryptosslX509(cf, kf);
   if (x509) {
      if (x509->Opaque())
         return x509;
      else
         delete x509;
   }
   return (XrdCryptoX509 *)0;
}

//______________________________________________________________________________
XrdCryptoX509 *XrdCryptosslFactory::X509(XrdSutBucket *b)
{
   // Return an instance of a ssl implementation of XrdCryptoX509.

   XrdCryptoX509 *x509 = new XrdCryptosslX509(b);
   if (x509) {
      if (x509->Opaque())
         return x509;
      else
         delete x509;
   }
   return (XrdCryptoX509 *)0;
}

//______________________________________________________________________________
XrdCryptoX509Crl *XrdCryptosslFactory::X509Crl(const char *cf, int opt)
{
   // Return an instance of a ssl implementation of XrdCryptoX509Crl.

   XrdCryptoX509Crl *x509Crl = new XrdCryptosslX509Crl(cf, opt);
   if (x509Crl) {
      if (x509Crl->Opaque())
         return x509Crl;
      else
         delete x509Crl;
   }
   return (XrdCryptoX509Crl *)0;
}

//______________________________________________________________________________
XrdCryptoX509Crl *XrdCryptosslFactory::X509Crl(XrdCryptoX509 *ca)
{
   // Return an instance of a ssl implementation of XrdCryptoX509Crl.

   XrdCryptoX509Crl *x509Crl = new XrdCryptosslX509Crl(ca);
   if (x509Crl) {
      if (x509Crl->Opaque())
         return x509Crl;
      else
         delete x509Crl;
   }
   return (XrdCryptoX509Crl *)0;
}

//______________________________________________________________________________
XrdCryptoX509Req *XrdCryptosslFactory::X509Req(XrdSutBucket *b)
{
   // Return an instance of a ssl implementation of XrdCryptoX509Crl.

   XrdCryptoX509Req *x509Req = new XrdCryptosslX509Req(b);
   if (x509Req) {
      if (x509Req->Opaque())
         return x509Req;
      else
         delete x509Req;
   }
   return (XrdCryptoX509Req *)0;
}

//______________________________________________________________________________
XrdCryptoX509VerifyCert_t XrdCryptosslFactory::X509VerifyCert()
{
   // Return hook to the OpenSSL implementation of the verification
   // function for X509 certificate.

   return &XrdCryptosslX509VerifyCert;
}

//______________________________________________________________________________
XrdCryptoX509VerifyChain_t XrdCryptosslFactory::X509VerifyChain()
{
   // Return hook to the OpenSSL implementation of the verification
   // function for X509 certificate chains.

   return &XrdCryptosslX509VerifyChain;
}

//______________________________________________________________________________
XrdCryptoX509ExportChain_t XrdCryptosslFactory::X509ExportChain()
{
   // Return an instance of an implementation of a function
   // to export a X509 certificate chain.

   return &XrdCryptosslX509ExportChain;
}

//______________________________________________________________________________
XrdCryptoX509ChainToFile_t XrdCryptosslFactory::X509ChainToFile()
{
   // Return an instance of an implementation of a function
   // to dump a X509 certificate chain to a file.

   return &XrdCryptosslX509ChainToFile;
}

//______________________________________________________________________________
XrdCryptoX509ParseFile_t XrdCryptosslFactory::X509ParseFile()
{
   // Return an instance of an implementation of a function
   // to parse a file supposed to contain for X509 certificates.

   return &XrdCryptosslX509ParseFile;
}

//______________________________________________________________________________
XrdCryptoX509ParseBucket_t XrdCryptosslFactory::X509ParseBucket()
{
   // Return an instance of an implementation of a function
   // to parse a file supposed to contain for X509 certificates.

   return &XrdCryptosslX509ParseBucket;
}

//______________________________________________________________________________
XrdCryptoProxyCertInfo_t XrdCryptosslFactory::ProxyCertInfo()
{
   // Check if the proxyCertInfo extension exists

   return &XrdCryptosslProxyCertInfo;
}

//______________________________________________________________________________
XrdCryptoSetPathLenConstraint_t XrdCryptosslFactory::SetPathLenConstraint()
{
   // Set the path length constraint

   return &XrdCryptosslSetPathLenConstraint;
}

//______________________________________________________________________________
XrdCryptoX509CreateProxy_t XrdCryptosslFactory::X509CreateProxy()
{
   // Create a proxy certificate

   return &XrdCryptosslX509CreateProxy;
}

//______________________________________________________________________________
XrdCryptoX509CreateProxyReq_t XrdCryptosslFactory::X509CreateProxyReq()
{
   // Create a proxy request

   return &XrdCryptosslX509CreateProxyReq;
}

//______________________________________________________________________________
XrdCryptoX509SignProxyReq_t XrdCryptosslFactory::X509SignProxyReq()
{
   // Sign a proxy request

   return &XrdCryptosslX509SignProxyReq;
}

//______________________________________________________________________________
XrdCryptoX509GetVOMSAttr_t XrdCryptosslFactory::X509GetVOMSAttr()
{
   // Get VOMS attributes, if any

   return &XrdCryptosslX509GetVOMSAttr;
}


/******************************************************************************/
/*            X r d C r y p t o S s l F a c t o r y O b j e c t               */
/******************************************************************************/

XrdVERSIONINFO(XrdCryptosslFactoryObject,cryptossl);

extern "C" {
XrdCryptoFactory *XrdCryptosslFactoryObject()
{
   // Return a pointer to the instantiated Ssl factory singleton.
   // Instantiate the singleton on the first call.

   static XrdCryptosslFactory SslCryptoFactory;

   return &SslCryptoFactory;
}}
