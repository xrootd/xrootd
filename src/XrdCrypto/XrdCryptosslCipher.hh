#ifndef __CRYPTO_SSLCIPHER_H__
#define __CRYPTO_SSLCIPHER_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o S s l C i p h e r . h h                 */
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
/* OpenSSL implementation of XrdCryptoCipher                                  */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoCipher.hh"

#include <openssl/evp.h>
#include <openssl/dh.h>

#define kDHMINBITS 128

// ---------------------------------------------------------------------------//
//
// OpenSSL Cipher Implementation
//
// ---------------------------------------------------------------------------//
class XrdCryptosslCipher : public XrdCryptoCipher
{
private:
   char       *fIV;
   int         lIV;
   const EVP_CIPHER *cipher;
   EVP_CIPHER_CTX *ctx;
   DH         *fDH;
   bool        deflength;
   bool        valid;

   void        GenerateIV();
   int         EncDec(int encdec, const char *bin, int lin, char *out);
   void        PrintPublic(BIGNUM *pub);
   int         Publen();

public:
   XrdCryptosslCipher(const char *t, int l = 0);
   XrdCryptosslCipher(const char *t, int l, const char *k,
                                     int liv, const char *iv);
   XrdCryptosslCipher(XrdSutBucket *b);
   XrdCryptosslCipher(bool padded, int len, char *pub, int lpub, const char *t);
   XrdCryptosslCipher(const XrdCryptosslCipher &c);
   virtual ~XrdCryptosslCipher();

   // Finalize key computation (key agreement)
   bool Finalize(bool padded, char *pub, int lpub, const char *t);
   void Cleanup();

   // Validity
   bool IsValid() { return valid; }

   // Support
   static bool IsSupported(const char *cip);

   // Required buffer size for encrypt / decrypt operations on l bytes
   int EncOutLength(int l);
   int DecOutLength(int l);
   char *Public(int &lpub);

   // Additional getter
   XrdSutBucket *AsBucket();
   char *IV(int &l) const { l = lIV; return fIV; }
   bool IsDefaultLength() const { return deflength; }
   int  MaxIVLength() const;

   // Additional setter
   void  SetIV(int l, const char *iv);

   // Additional methods
   int Encrypt(const char *bin, int lin, char *out);
   int Decrypt(const char *bin, int lin, char *out);
   char *RefreshIV(int &l);
};
#endif
