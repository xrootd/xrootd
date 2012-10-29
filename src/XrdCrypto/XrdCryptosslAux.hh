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

/******************************************************************************/
/*          E r r o r   L o g g i n g / T r a c i n g   F l a g s             */
/******************************************************************************/
#define sslTRACE_ALL       0x0007
#define sslTRACE_Dump      0x0004
#define sslTRACE_Debug     0x0002
#define sslTRACE_Notify    0x0001

#endif

