#ifndef __CRYPTO_SSLRSA_H__
#define __CRYPTO_SSLRSA_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d C r y p t o S s l R S A . h h                      */
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
/* OpenSSL implementation of XrdCryptoRSA                                     */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoRSA.hh"

#include <openssl/evp.h>

// ---------------------------------------------------------------------------//
//
// RSA interface
//
// ---------------------------------------------------------------------------//
class XrdCryptosslRSA : public XrdCryptoRSA
{
private:
   EVP_PKEY *fEVP;     // The key pair
   int       publen;   // Length of export public key
   int       prilen;   // Length of export private key

public:
   XrdCryptosslRSA(int bits = XrdCryptoMinRSABits, int exp = XrdCryptoDefRSAExp);
   XrdCryptosslRSA(const char *pub, int lpub = 0);
   XrdCryptosslRSA(EVP_PKEY *key, bool check = 1);
   XrdCryptosslRSA(const XrdCryptosslRSA &r);
   virtual ~XrdCryptosslRSA();

   // Access underlying data (in opaque form)
   XrdCryptoRSAdata Opaque() { return fEVP; }

   // Dump information
   void Dump();

   // Output lengths
   int GetOutlen(int lin);   // Length of encrypted buffers
   int GetPublen();          // Length of export public key
   int GetPrilen();          // Length of export private key

   // Import / Export methods
   int ImportPublic(const char *in, int lin);
   int ExportPublic(char *out, int lout);
   int ImportPrivate(const char *in, int lin);
   int ExportPrivate(char *out, int lout);

   // Encryption / Decryption methods
   int EncryptPrivate(const char *in, int lin, char *out, int lout);
   int DecryptPublic(const char *in, int lin, char *out, int lout);
   int EncryptPublic(const char *in, int lin, char *out, int lout);
   int DecryptPrivate(const char *in, int lin, char *out, int lout);
};

#endif
