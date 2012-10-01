#ifndef __CRYPTO_LOCALFACTORY_H__
#define __CRYPTO_LOCALFACTORY_H__
/******************************************************************************/
/*                                                                            */
/*             X r d C r y p t o L o c a l F a c t o r y . h h                */
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
/* Implementation of the local crypto factory                                 */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoFactory.hh"

// The ID must be a unique number
#define XrdCryptolocalFactoryID  0

class XrdCryptolocalFactory : public XrdCryptoFactory 
{
public:
   XrdCryptolocalFactory();
   virtual ~XrdCryptolocalFactory() { }

   // Set trace flags
   void SetTrace(kXR_int32 trace);

   // Hook to local KDFun
   XrdCryptoKDFunLen_t KDFunLen(); // Length of buffer
   XrdCryptoKDFun_t KDFun();

   // Cipher constructors
   XrdCryptoCipher *Cipher(const char *t, int l = 0);
   XrdCryptoCipher *Cipher(const char *t, int l, const char *k,
                                          int liv, const char *iv);
   XrdCryptoCipher *Cipher(XrdSutBucket *b);
   XrdCryptoCipher *Cipher(int bits, char *pub, int lpub, const char *t = 0);
   XrdCryptoCipher *Cipher(const XrdCryptoCipher &c);

   // MsgDigest constructors
   XrdCryptoMsgDigest *MsgDigest(const char *dgst);

   // RSA constructors
   XrdCryptoRSA *RSA(int bits = XrdCryptoDefRSABits, int exp = XrdCryptoDefRSAExp);
   XrdCryptoRSA *RSA(const char *pub, int lpub = 0);
   XrdCryptoRSA *RSA(const XrdCryptoRSA &r);
};

#endif
