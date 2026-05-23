/******************************************************************************/
/*                                                                            */
/*                X r d C r y p t o L i t e _ B F e c b . c c                 */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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

#include <openssl/evp.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#endif

#include "XrdCrypto/XrdCryptoLite_BFecb.hh"
#include "XrdOuc/XrdOucUtils.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdCryptoLite_BFecb::XrdCryptoLite_BFecb(bool&                aOK,
                                         const unsigned char* key,
                                               unsigned int   keylen)
                    : decCTX(0), encCTX(0)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
// With openssl v3 the blowfish cipher is only available via the "legacy"
// provider. Legacy is typically not enabled by default (but can be via
// openssl.cnf) so it is loaded here. Explicitly loading a provider will
// disable the automatic loading of the "default" one. The default might
// not have already been loaded, or standard algorithms might be available
// via another configured provider, such as FIPS. So an attempt is made to
// fetch a common default algorithm, possibly automaticlly loading the
// default provider. Afterwards the legacy provider is loaded.
//
   static struct loadProviders {
      loadProviders() {
         EVP_MD *mdp = EVP_MD_fetch(NULL, "SHA2-256", NULL);
         if (mdp) EVP_MD_free(mdp);
         // Load legacy provider into the default (NULL) library context
         (void) OSSL_PROVIDER_load(NULL, "legacy");
      }
   } lp;
#endif

// Handle auto generation of a random key
//
   unsigned char bfKey[16];
   if (!key || !keylen)
      {XrdOucUtils::Random(bfKey, sizeof(bfKey));
       key    = bfKey;
       keylen = sizeof(bfKey);
      }

// The legacy openssl EVP is rather outdated, cumbersome, non thread-safe,
// and badly documented. Unfortunately, it is the only one generally availabe
// on all platforms (modern versions like CryptoPP need manual installation).
// So, we need to construct a decryption context and an encryption context
// because the context can only do one type of action at a time and resetting
// the key when switching actions is CPU intensive. What a pain in the but!
//
   aOK = false;
   if (!(decCTX = EVP_CIPHER_CTX_new())) return;
   if (1 != EVP_DecryptInit_ex(decCTX, EVP_bf_ecb(), NULL, NULL, NULL)) return;
   EVP_CIPHER_CTX_set_padding(decCTX, 0);
   EVP_CIPHER_CTX_set_key_length(decCTX, keylen);
   if (1 != EVP_DecryptInit_ex(decCTX, NULL, NULL, key, NULL)) return;

   if (!(encCTX = EVP_CIPHER_CTX_new())) return;
   if (1 != EVP_EncryptInit_ex(encCTX, EVP_bf_ecb(), NULL, NULL, NULL)) return;
   EVP_CIPHER_CTX_set_padding(encCTX, 0);
   EVP_CIPHER_CTX_set_key_length(encCTX, keylen);
   if (1 != EVP_EncryptInit_ex(encCTX, NULL, NULL, key, NULL)) return;
   aOK = true;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdCryptoLite_BFecb::~XrdCryptoLite_BFecb()
{
   EVP_CIPHER_CTX_free(decCTX);
   EVP_CIPHER_CTX_free(encCTX);
}

/******************************************************************************/
/*                               D e c r y p t                                */
/******************************************************************************/

void XrdCryptoLite_BFecb::Decrypt(const unsigned char* in8,
                                        unsigned char* out8)
{
   int dlen;

// Perform the action. Since we said padding is zero and the input must be
// 8 bytes, and we are using blowfish ECB when we decrypt the result will
// not be buffered but placed in the output buffer upon return.
//
   evpMutex.Lock();
   EVP_DecryptUpdate(decCTX, out8, &dlen, in8, 8);
   evpMutex.UnLock();
}

/******************************************************************************/
/*                               E n c r y p t                                */
/******************************************************************************/

void XrdCryptoLite_BFecb::Encrypt(const unsigned char* in8,
                                        unsigned char* out8)
{
   int dlen;

// Perform the action
//
// Perform the action. Since we said padding is zero and the input must be
// 8 bytes, and we are using blowfish ECB when we encrypt the result will
// not be buffered but placed in the output buffer upon return.
//
   evpMutex.Lock();
   EVP_EncryptUpdate(encCTX, out8, &dlen, in8, 8);
   evpMutex.UnLock();
}

/******************************************************************************/
/* Static:                      I n s t a n c e                               */
/******************************************************************************/

XrdCryptoLite_BFecb* XrdCryptoLite_BFecb::Instance(const unsigned char* key,
                                                         unsigned int   klen)
{
   XrdCryptoLite_BFecb* obj;
   bool isOK;

// Get an instance or return a nil pointer
//
   obj = new XrdCryptoLite_BFecb(isOK, key, klen);
   if (!isOK) {delete obj; obj = 0;}
   return obj;
}
