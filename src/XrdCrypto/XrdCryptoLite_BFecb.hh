#ifndef __XRDCRYPTOLITE_BFecb_H__
#define __XRDCRYPTOLITE_BFecb_H__
/******************************************************************************/
/*                                                                            */
/*                X r d C r y p t o L i t e _ B F e c b . h h                 */
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

/* This class implements Bflowfish ecb cipher which works on a single 64-bit
   block to produce another 64-bit block. While not particularly secure it
   is useful for encrypting short messages in order to prevent spoofing but
   not necessarily to protect data privacy. While blowfish ECB is naturally
   thread-safe, the OPENSSL EVP implementation destroys that notion and we
   must serialize all access to the underlying blowfish implementation to
   make it thread-safe.
*/

#include "XrdSys/XrdSysPthread.hh"

struct  evp_cipher_ctx_st;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

class XrdCryptoLite_BFecb
{
public:

//-----------------------------------------------------------------------------
//! Construct an ECB encryption/decryption object.
//!
//! @param  aOK     Upon return must be true if all went well. It will be
//!                 false otherwise and this object will not be safely usable.
//! @param  key     Pointer to the encryption key which should be 128 bits.
//!                 When null, a random 128 bit key is generated for use.
//! @param  keylen  The length of the key in bytes if key is specified.
//!
//! @note When initializing a static pointer you may wish to use the Instance()
//!       method that automatically returns a null pointer on a failure.
//-----------------------------------------------------------------------------

      XrdCryptoLite_BFecb(bool &aOK, const unsigned char* key=0,
                                           unsigned int   keylen=0);

     ~XrdCryptoLite_BFecb();

//-----------------------------------------------------------------------------
//! Decrypt exactly one blowfish block of 8 bytes (64 bits)
//!
//! @param  in8     Pointer to exactly 8 bytes of data to be decrypted.
//! @param  out8    Pointer to a 8 bytes or more buffer to hold the result.
//-----------------------------------------------------------------------------

void Decrypt(const unsigned char* in8, unsigned char* out8);

//-----------------------------------------------------------------------------
//! Encrypt exactly one blowfish block of 8 bytes (64 bits)
//!
//! @param  in8     Pointer to exactly 8 bytes of data to be encrypted.
//! @param  out8    Pointer to a 8 bytes or more buffer to hold the result.
//-----------------------------------------------------------------------------

void Encrypt(const unsigned char* in8, unsigned char* out8);

//-----------------------------------------------------------------------------
//! Return an instance of an ECB encryption/decryption object upon success.
//!
//! @param  key     Pointer to the encryption key which should be 128 bits.
//!                 When null, a random 128 bit key is generated for use.
//! @param  keylen  The length of the key in bytes if key is specified.
//!
//! @return A pointer to the crypto object or a null pointer upon failure.
//-----------------------------------------------------------------------------

static
XrdCryptoLite_BFecb* Instance(const unsigned char* key=0,
                                    unsigned int   klen=0);

private:
EVP_CIPHER_CTX* decCTX;
EVP_CIPHER_CTX* encCTX;
XrdSysMutex     evpMutex;
};
#endif
