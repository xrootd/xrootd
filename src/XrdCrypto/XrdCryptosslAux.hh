#ifndef __CRYPTO_SSLAUX_H__
#define __CRYPTO_SSLAUX_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o S s l A u x . h h                       */
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
/* OpenSSL utility functions                                                  */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include <openssl/asn1.h>

#define kSslKDFunDefLen  24

//
// Password-Based Key Derivation Function 2, specified in PKCS #5
//
int XrdCryptosslKDFunLen(); // default buffer length
int XrdCryptosslKDFun(const char *pass, int plen, const char *salt, int slen,
                      char *key, int len);
//
// X509 manipulation: certificate verification
bool XrdCryptosslX509VerifyCert(XrdCryptoX509 *c, XrdCryptoX509 *r);
// chain verification
bool XrdCryptosslX509VerifyChain(XrdCryptoX509Chain *chain, int &errcode);
// chain export to bucket
XrdSutBucket *XrdCryptosslX509ExportChain(XrdCryptoX509Chain *c, bool key = 0);
// chain export to file (proxy file creation)
int XrdCryptosslX509ChainToFile(XrdCryptoX509Chain *c, const char *fn);
// certificates from file parsing
int XrdCryptosslX509ParseFile(const char *fname, XrdCryptoX509Chain *c);
// certificates from bucket parsing
int XrdCryptosslX509ParseBucket(XrdSutBucket *b, XrdCryptoX509Chain *c);
//
// Function to convert from ASN1 time format into UTC since Epoch (Jan 1, 1970) 
int XrdCryptosslASN1toUTC(ASN1_TIME *tsn1);

// Function to convert X509_NAME into a one-line human readable string
void XrdCryptosslNameOneLine(X509_NAME *nm, XrdOucString &s);

//
// X509 proxy auxilliary functions
// Function to check presence of a proxyCertInfo and retrieve the path length
// constraint. Written following RFC3820 and examples in openssl-<vers>/crypto
// source code. Extracts the policy field but ignores it contents.
bool XrdCryptosslProxyCertInfo(const void *ext, int &pathlen, bool *haspolicy = 0);
void XrdCryptosslSetPathLenConstraint(void *ext, int pathlen);
// Create proxy certificates
int XrdCryptosslX509CreateProxy(const char *, const char *, XrdProxyOpt_t *,
                             XrdCryptogsiX509Chain *, XrdCryptoRSA **, const char *);
// Create a proxy certificate request
int XrdCryptosslX509CreateProxyReq(XrdCryptoX509 *,
                                XrdCryptoX509Req **, XrdCryptoRSA **);
// Sign a proxy certificate request
int XrdCryptosslX509SignProxyReq(XrdCryptoX509 *, XrdCryptoRSA *,
                              XrdCryptoX509Req *, XrdCryptoX509 **);
// Get VOMS attributes, if any
int XrdCryptosslX509GetVOMSAttr(XrdCryptoX509 *, XrdOucString &);

/******************************************************************************/
/*          E r r o r   L o g g i n g / T r a c i n g   F l a g s             */
/******************************************************************************/
#define sslTRACE_ALL       0x0007
#define sslTRACE_Dump      0x0004
#define sslTRACE_Debug     0x0002
#define sslTRACE_Notify    0x0001

/******************************************************************************/
/*          E r r o r s   i n   P r o x y   M a n i p u l a t i o n s         */
/******************************************************************************/
#define kErrPX_Error            1      // Generic error condition
#define kErrPX_BadEECfile       2      // Absent or bad EEC cert or key file
#define kErrPX_BadEECkey        3      // Inconsistent EEC key
#define kErrPX_ExpiredEEC       4      // EEC is expired
#define kErrPX_NoResources      5      // Unable to create new objects
#define kErrPX_SetAttribute     6      // Unable to set a certificate attribute
#define kErrPX_SetPathDepth     7      // Unable to set path depth
#define kErrPX_Signing          8      // Problems signing
#define kErrPX_GenerateKey      9      // Problem generating the RSA key
#define kErrPX_ProxyFile       10      // Problem creating / updating proxy file
#define kErrPX_BadNames        11      // Names in certificates are bad
#define kErrPX_BadSerial       12      // Problems resolving serial number
#define kErrPX_BadExtension    13      // Problems with the extensions

#endif

