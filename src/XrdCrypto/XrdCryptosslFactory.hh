#ifndef __CRYPTO_SSLFACTORY_H__
#define __CRYPTO_SSLFACTORY_H__
/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o S s l F a c t o r y . h h                  */
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

#ifndef __CRYPTO_FACTORY_H__
#include "XrdCrypto/XrdCryptoFactory.hh"
#endif

#include "XrdSys/XrdSysPthread.hh"

int DebugON = 1;

// The ID must be a unique number
#define XrdCryptosslFactoryID  1

#define SSLFACTORY_MAX_CRYPTO_MUTEX 256

class XrdCryptosslFactory : public XrdCryptoFactory 
{
public:
   XrdCryptosslFactory();
   virtual ~XrdCryptosslFactory() { }

   // Set trace flags
   void SetTrace(kXR_int32 trace);

   // Hook to Key Derivation Function (PBKDF2)
   XrdCryptoKDFunLen_t KDFunLen(); // Default Length of buffer
   XrdCryptoKDFun_t KDFun();

   // Cipher constructors
   bool SupportedCipher(const char *t);
   XrdCryptoCipher *Cipher(const char *t, int l = 0);
   XrdCryptoCipher *Cipher(const char *t, int l, const char *k,
                                          int liv, const char *iv);
   XrdCryptoCipher *Cipher(XrdSutBucket *b);
   XrdCryptoCipher *Cipher(int bits, char *pub, int lpub, const char *t = 0);
   XrdCryptoCipher *Cipher(const XrdCryptoCipher &c);

   // MsgDigest constructors
   bool SupportedMsgDigest(const char *dgst);
   XrdCryptoMsgDigest *MsgDigest(const char *dgst);

   // RSA constructors
   XrdCryptoRSA *RSA(int bits = XrdCryptoDefRSABits, int exp = XrdCryptoDefRSAExp);
   XrdCryptoRSA *RSA(const char *pub, int lpub = 0);
   XrdCryptoRSA *RSA(const XrdCryptoRSA &r);

   // X509 constructors
   XrdCryptoX509 *X509(const char *cf, const char *kf = 0);
   XrdCryptoX509 *X509(XrdSutBucket *b);

   // X509 CRL constructor
   XrdCryptoX509Crl *X509Crl(const char *crlfile, int opt = 0);
   XrdCryptoX509Crl *X509Crl(XrdCryptoX509 *cacert);

   // X509 REQ constructors
   XrdCryptoX509Req *X509Req(XrdSutBucket *bck);

   // Hooks to handle X509 certificates
   XrdCryptoX509VerifyCert_t X509VerifyCert();
   XrdCryptoX509VerifyChain_t X509VerifyChain();
   XrdCryptoX509ParseFile_t X509ParseFile();
   XrdCryptoX509ParseBucket_t X509ParseBucket();
   XrdCryptoX509ExportChain_t X509ExportChain();
   XrdCryptoX509ChainToFile_t X509ChainToFile();

   // Hooks to handle X509 proxy certificates
   XrdCryptoProxyCertInfo_t ProxyCertInfo();
   XrdCryptoSetPathLenConstraint_t SetPathLenConstraint();
   XrdCryptoX509CreateProxy_t X509CreateProxy();
   XrdCryptoX509CreateProxyReq_t X509CreateProxyReq();
   XrdCryptoX509SignProxyReq_t X509SignProxyReq();
   XrdCryptoX509GetVOMSAttr_t X509GetVOMSAttr();

   // Required SSL mutexes.
  static  XrdSysMutex*              CryptoMutexPool[SSLFACTORY_MAX_CRYPTO_MUTEX];

};

#endif
