#ifndef __CRYPTO_FACTORY_H__
#define __CRYPTO_FACTORY_H__
/******************************************************************************/
/*                                                                            */
/*                 X r d C r y p t o F a c t o r y . h h                      */
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
/* Abstract interface for a crypto factory                                    */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoAux.hh"

#define MAXFACTORYNAMELEN  10
// ---------------------------------------------------------------------------//
//
// Abstract Crypto Factory
//
// ---------------------------------------------------------------------------//

class XrdSutBucket;
class XrdOucString;
class XrdCryptoCipher;
class XrdCryptoMsgDigest;
class XrdCryptoRSA;
class XrdCryptoX509;
class XrdCryptoX509Chain;
class XrdCryptogsiX509Chain;
class XrdCryptoX509Crl;
class XrdCryptoX509Req;

//
// Prototypes for some Utility Functions

// Key derivation function
typedef int (*XrdCryptoKDFunLen_t)();
typedef int (*XrdCryptoKDFun_t)(const char *pass, int plen,
                                const char *salt, int slen,
                                char *key, int klen);

// X509 manipulation: certificate verification
typedef bool (*XrdCryptoX509VerifyCert_t)(XrdCryptoX509 *c, XrdCryptoX509 *r);
// chain verification
typedef bool (*XrdCryptoX509VerifyChain_t)(XrdCryptoX509Chain *chain,
                                           int &errcode);
// chain export
typedef XrdSutBucket *(*XrdCryptoX509ExportChain_t)(XrdCryptoX509Chain *, bool);

// chain to file
typedef int (*XrdCryptoX509ChainToFile_t)(XrdCryptoX509Chain *, const char *);

// certificates from file parsing
typedef int (*XrdCryptoX509ParseFile_t)(const char *fname,
                                        XrdCryptoX509Chain *);
// certificates from bucket parsing
typedef int (*XrdCryptoX509ParseBucket_t)(XrdSutBucket *,
                                          XrdCryptoX509Chain *);
// Proxies
// The OID of the extension
#define gsiProxyCertInfo_OID "1.3.6.1.4.1.3536.1.222"
// check presence of proxyCertInfo extension (RFC 3820)
typedef bool (*XrdCryptoProxyCertInfo_t)(const void *, int &, bool *);
// set path length constraint
typedef void (*XrdCryptoSetPathLenConstraint_t)(void *, int);
// create a proxy certificate
typedef struct {
   int   bits;          // Number of bits in the RSA key [512]
   int   valid;         // Duration validity in secs [43200 (12 hours)]
   int   depthlen;      // Maximum depth of the path of proxy certificates
                        // that can signed by this proxy certificates
                        // [-1 (== unlimited)]
} XrdProxyOpt_t;
typedef int (*XrdCryptoX509CreateProxy_t)(const char *, const char *, XrdProxyOpt_t *,
                                          XrdCryptogsiX509Chain *, XrdCryptoRSA **, const char *);
// create a proxy certificate request
typedef int (*XrdCryptoX509CreateProxyReq_t)(XrdCryptoX509 *,
                                             XrdCryptoX509Req **, XrdCryptoRSA **);
// sign a proxy certificate request
typedef int (*XrdCryptoX509SignProxyReq_t)(XrdCryptoX509 *, XrdCryptoRSA *,
                                           XrdCryptoX509Req *, XrdCryptoX509 **);
// get VOMS attributes
typedef int (*XrdCryptoX509GetVOMSAttr_t)(XrdCryptoX509 *, XrdOucString &);

class XrdCryptoFactory
{
private:
   char    name[MAXFACTORYNAMELEN];
   int     fID;
public:
   XrdCryptoFactory(const char *n = "Unknown", int id = -1);
   virtual ~XrdCryptoFactory() { }

   // Set trace flags
   virtual void SetTrace(kXR_int32 trace);

   // Get the factory name
   char *Name() const { return (char *)&name[0]; }
   int   ID() const { return fID; }

   // Get the right factory
   static XrdCryptoFactory *GetCryptoFactory(const char *factoryname);
   
   // Any possible notification
   virtual void Notify() { }

   // Hook to a Key Derivation Function (PBKDF2 when possible)
   virtual XrdCryptoKDFunLen_t KDFunLen(); // Length of buffer
   virtual XrdCryptoKDFun_t KDFun();

   // Cipher constructors
   virtual bool SupportedCipher(const char *t);
   virtual XrdCryptoCipher *Cipher(const char *t, int l = 0);
   virtual XrdCryptoCipher *Cipher(const char *t, int l, const char *k, 
                                   int liv, const char *iv);
   virtual XrdCryptoCipher *Cipher(XrdSutBucket *b);
   virtual XrdCryptoCipher *Cipher(int bits, char *pub, int lpub, const char *t = 0);
   virtual XrdCryptoCipher *Cipher(const XrdCryptoCipher &c);

   // MsgDigest constructors
   virtual bool SupportedMsgDigest(const char *dgst);
   virtual XrdCryptoMsgDigest *MsgDigest(const char *dgst);

   // RSA constructors
   virtual XrdCryptoRSA *RSA(int b = 0, int e = 0);
   virtual XrdCryptoRSA *RSA(const char *p, int l = 0);
   virtual XrdCryptoRSA *RSA(const XrdCryptoRSA &r);

   // X509 constructors
   virtual XrdCryptoX509 *X509(const char *cf, const char *kf = 0);
   virtual XrdCryptoX509 *X509(XrdSutBucket *b);

   // X509 CRL constructors
   virtual XrdCryptoX509Crl *X509Crl(const char *crlfile, int opt = 0);
   virtual XrdCryptoX509Crl *X509Crl(XrdCryptoX509 *cacert);

   // X509 REQ constructors
   virtual XrdCryptoX509Req *X509Req(XrdSutBucket *bck);

   // Hooks to handle X509 certificates
   virtual XrdCryptoX509VerifyCert_t X509VerifyCert();
   virtual XrdCryptoX509VerifyChain_t X509VerifyChain();
   virtual XrdCryptoX509ParseFile_t X509ParseFile();
   virtual XrdCryptoX509ParseBucket_t X509ParseBucket();
   virtual XrdCryptoX509ExportChain_t X509ExportChain();
   virtual XrdCryptoX509ChainToFile_t X509ChainToFile();

   // Hooks to handle X509 proxy certificates
   virtual XrdCryptoProxyCertInfo_t ProxyCertInfo();
   virtual XrdCryptoSetPathLenConstraint_t SetPathLenConstraint();
   virtual XrdCryptoX509CreateProxy_t X509CreateProxy();
   virtual XrdCryptoX509CreateProxyReq_t X509CreateProxyReq();
   virtual XrdCryptoX509SignProxyReq_t X509SignProxyReq();
   virtual XrdCryptoX509GetVOMSAttr_t X509GetVOMSAttr();

   // Equality operator
   bool operator==(const XrdCryptoFactory factory);
};
#endif
