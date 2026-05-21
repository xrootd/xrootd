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

#include <openssl/blowfish.h>

#include "XrdCrypto/XrdCryptoLite_BFecb.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdCryptoLite_BFecb::XrdCryptoLite_BFecb(const unsigned char* key,
                                               unsigned int   keylen)
{
// Allocate and set the key. This is an expensive operation so we do it once.
//
   bfKey = new BF_KEY;
   BF_set_key(bfKey, keylen, key);
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdCryptoLite_BFecb::~XrdCryptoLite_BFecb() {delete bfKey;}

/******************************************************************************/
/*                                 A p p l y                                  */
/******************************************************************************/

void XrdCryptoLite_BFecb::Apply(const unsigned char* in8, unsigned char* out8,
                                bool encrypt)
{
   int action = (encrypt ? BF_ENCRYPT : BF_DECRYPT);

// Perform the action
//
   BF_ecb_encrypt(in8, out8, bfKey, action);
}
