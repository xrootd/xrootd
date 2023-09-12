/******************************************************************************/
/*                                                                            */
/*                 X r d C r y p t o L i t e _ b f 3 2 . c c                  */
/*                                                                            */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include "XrdCrypto/XrdCryptoLite.hh"

#ifdef HAVE_SSL

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <netinet/in.h>
#include <cinttypes>

#include <openssl/evp.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include "XrdTls/XrdTlsContext.hh"
#endif
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#endif

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*              C l a s s   X r d C r y p t o L i t e _ b f 3 2               */
/******************************************************************************/
  
class XrdCryptoLite_bf32 : public XrdCryptoLite
{
public:

virtual int  Decrypt(const char *key,      // Decryption key
                     int         keyLen,   // Decryption key byte length
                     const char *src,      // Buffer to be decrypted
                     int         srcLen,   // Bytes length of src  buffer
                     char       *dst,      // Buffer to hold decrypted result
                     int         dstLen);  // Bytes length of dst  buffer

virtual int  Encrypt(const char *key,      // Encryption key
                     int         keyLen,   // Encryption key byte length
                     const char *src,      // Buffer to be encrypted
                     int         srcLen,   // Bytes length of src  buffer
                     char       *dst,      // Buffer to hold encrypted result
                     int         dstLen);  // Bytes length of dst  buffer

         XrdCryptoLite_bf32(const char deType) : XrdCryptoLite(deType, 4) {}
        ~XrdCryptoLite_bf32() {}
};

/******************************************************************************/
/*                               D e c r y p t                                */
/******************************************************************************/

int XrdCryptoLite_bf32::Decrypt(const char *key,
                                int         keyLen,
                                const char *src,
                                int         srcLen,
                                char       *dst,
                                int         dstLen)
{
   unsigned char ivec[8] = {0,0,0,0,0,0,0,0};
   unsigned int crc32;
   int wLen;
   int dLen = srcLen - sizeof(crc32);

// Make sure we have data
//
   if (dstLen <= (int)sizeof(crc32) || dstLen < srcLen) return -EINVAL;

// Decrypt
//
   EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
   EVP_DecryptInit_ex(ctx, EVP_bf_cfb64(), NULL, NULL, NULL);
   EVP_CIPHER_CTX_set_padding(ctx, 0);
   EVP_CIPHER_CTX_set_key_length(ctx, keyLen);
   EVP_DecryptInit_ex(ctx, NULL, NULL, (unsigned char *)key, ivec);
   EVP_DecryptUpdate(ctx, (unsigned char *)dst, &wLen,
                          (unsigned char *)src, srcLen);
   EVP_DecryptFinal_ex(ctx, (unsigned char *)dst, &wLen);
   EVP_CIPHER_CTX_free(ctx);

// Perform the CRC check to verify we have valid data here
//
   memcpy(&crc32, dst+dLen, sizeof(crc32));
   crc32 = ntohl(crc32);
   if (crc32 != XrdOucCRC::CRC32((const unsigned char *)dst, dLen))
      return -EPROTO;

// Return success
//
   return dLen;
}
  
/******************************************************************************/
/*                               E n c r y p t                                */
/******************************************************************************/

int XrdCryptoLite_bf32::Encrypt(const char *key,
                                int         keyLen,
                                const char *src,
                                int         srcLen,
                                char       *dst,
                                int         dstLen)
{
   unsigned char buff[4096], *bP, *mP = 0, ivec[8] = {0,0,0,0,0,0,0,0};
   unsigned int crc32;
   int wLen;
   int dLen = srcLen + sizeof(crc32);

// Make sure that the destination if at least 4 bytes larger and we have data
//
   if (dstLen-srcLen < (int)sizeof(crc32) || srcLen <= 0) return -EINVAL;

// Normally, the msg is 4k or less but if more, get a new buffer
//
   if (dLen <= (int)sizeof(buff)) bP = buff;
      else {if (!(mP = (unsigned char *)malloc(dLen))) return -ENOMEM;
               else bP = mP;
           }

// Append a crc
//
   memcpy(bP, src, srcLen);
   crc32 = XrdOucCRC::CRC32(bP, srcLen);
   crc32 = htonl(crc32);
   memcpy((bP+srcLen), &crc32, sizeof(crc32));

// Encrypt
//
   EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
   EVP_EncryptInit_ex(ctx, EVP_bf_cfb64(), NULL, NULL, NULL);
   EVP_CIPHER_CTX_set_padding(ctx, 0);
   EVP_CIPHER_CTX_set_key_length(ctx, keyLen);
   EVP_EncryptInit_ex(ctx, NULL, NULL, (unsigned char *)key, ivec);
   EVP_EncryptUpdate(ctx, (unsigned char *)dst, &wLen, bP, dLen);
   EVP_EncryptFinal_ex(ctx, (unsigned char *)dst, &wLen);
   EVP_CIPHER_CTX_free(ctx);

// Free temp buffer and return success
//
   if (mP) free(mP);
   return dLen;
}
#endif

/******************************************************************************/
/*                X r d C r y p t o L i t e _ N e w _ b f 3 2                 */
/******************************************************************************/
  
XrdCryptoLite *XrdCryptoLite_New_bf32(const char Type)
{
#ifdef HAVE_SSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
   // In case nothing has yet configured a libcrypto thread-id callback
   // function we provide one via the XrdTlsContext Init method. Compared
   // to the default the aim is to provide better properies when libcrypto
   // uses the thread-id as hash-table keys for the per-thread error state.
   static struct configThreadid {
     configThreadid() {eText = XrdTlsContext::Init();}
     const char *eText;
   } ctid;
   // Make sure all went well
   //
   if (ctid.eText) return (XrdCryptoLite *)0;
#endif
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   // With openssl v3 the blowfish cipher is only available via the "legacy"
   // provider. Legacy is typically not enabled by default (but can be via
   // openssl.cnf) so it is loaded here. Explicitly loading a provider will
   // disable the automatic loading of the "default" one. The default might
   // not have already been loaded, or standard algorithms might be available
   // via another configured provider, such as FIPS. So an attempt is made to
   // fetch a common default algorithm, possibly automaticlly loading the
   // default provider. Afterwards the legacy provider is loaded.
   static struct loadProviders {
      loadProviders() {
         EVP_MD *mdp = EVP_MD_fetch(NULL, "SHA2-256", NULL);
         if (mdp) EVP_MD_free(mdp);
         // Load legacy provider into the default (NULL) library context
         (void) OSSL_PROVIDER_load(NULL, "legacy");
      }
   } lp;
#endif
   return (XrdCryptoLite *)(new XrdCryptoLite_bf32(Type));
#else
   return (XrdCryptoLite *)0;
#endif
}
