#ifndef __CRYPTO_SSLGSIAUX_H__
#define __CRYPTO_SSLGSIAUX_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o s s l g s i A u x . h h                 */
/*                                                                            */
/* (c) 2005, G. Ganis / CERN                                                  */
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
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* GSI utility functions                                                      */
/*                                                                            */
/* ************************************************************************** */
#include "XrdCrypto/XrdCryptosslgsiX509Chain.hh"
#include "XrdCrypto/XrdCryptoX509Req.hh"
#include "XrdCrypto/XrdCryptoRSA.hh"
#include "XrdOuc/XrdOucString.hh"

// The OID of the extension
#define gsiProxyCertInfo_OLD_OID "1.3.6.1.4.1.3536.1.222"
#define gsiProxyCertInfo_OID     "1.3.6.1.5.5.7.1.14"

//
// Function to check presence of a proxyCertInfo and retrieve the path length
// constraint. Written following RFC3820 and examples in openssl-<vers>/crypto
// source code. Extracts the policy field but ignores it contents.
bool XrdSslgsiProxyCertInfo(const void *ext, int &pathlen, bool *haspolicy = 0);
void XrdSslgsiSetPathLenConstraint(void *ext, int pathlen);

//
// Proxies
//
typedef struct {
   int   bits;          // Number of bits in the RSA key [512]
   int   valid;         // Duration validity in secs [43200 (12 hours)]
   int   depthlen;      // Maximum depth of the path of proxy certificates
                        // that can signed by this proxy certificates
                        // [-1 (== unlimited)]
} XrdProxyOpt_t;
//
// Create proxy certificates
int XrdSslgsiX509CreateProxy(const char *, const char *, XrdProxyOpt_t *,
                             XrdCryptosslgsiX509Chain *, XrdCryptoRSA **, const char *);
//
// Create a proxy certificate request
int XrdSslgsiX509CreateProxyReq(XrdCryptoX509 *,
                                XrdCryptoX509Req **, XrdCryptoRSA **);
//
// Sign a proxy certificate request
int XrdSslgsiX509SignProxyReq(XrdCryptoX509 *, XrdCryptoRSA *,
                              XrdCryptoX509Req *, XrdCryptoX509 **);
//
// Dump extensions
int XrdSslgsiX509DumpExtensions(XrdCryptoX509 *);
//
// Get VOMS attributes, if any
int XrdSslgsiX509GetVOMSAttr(XrdCryptoX509 *, XrdOucString &);
//
// Check GSI 3 proxy info extension
int XrdSslgsiX509CheckProxy3(XrdCryptoX509 *, XrdOucString &);

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

