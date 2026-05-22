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
   not necessarily to protect data privacy. Note that this object is thread
   safe as blowfish ECB is stateless. So, the object can be used by multiple
   threads to perform encrypption/decryption using the supplied key.
*/

struct  bf_key_st;
typedef bf_key_st BF_KEY;

class XrdCryptoLite_BFecb
{
public:

//-----------------------------------------------------------------------------
//! Construct an ECB encryption/decryption object.
//!
//! @param  key     Pointer to the encryption key which should be 128 bits.
//! @param  keylen  The length of the key in bytes.
//-----------------------------------------------------------------------------

      XrdCryptoLite_BFecb(const unsigned char* key, unsigned int keylen);

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

private:
BF_KEY* bfKey;
};
#endif
