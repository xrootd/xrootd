#ifndef __CRYPTO_MSGDGSTSSL_H__
#define __CRYPTO_MSGDGSTSSL_H__
/******************************************************************************/
/*                                                                            */
/*             X r d C r y p t o S s l M s g D i g e s t . h h                */
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
/* OpenSSL implementation of XrdSecCMsgDigest                                 */
/*                                                                            */
/* ************************************************************************** */

#include <openssl/evp.h>

#include "XrdCrypto/XrdCryptoMsgDigest.hh"

// ---------------------------------------------------------------------------//
//
// Message Digest implementation buffer
//
// ---------------------------------------------------------------------------//
class XrdCryptosslMsgDigest : public XrdCryptoMsgDigest
{
private:
   bool valid;
   EVP_MD_CTX *mdctx;

   int Init(const char *dgst);

public:
   XrdCryptosslMsgDigest(const char *dgst);
   virtual ~XrdCryptosslMsgDigest();

   // Validity
   bool IsValid() { return valid; }

   // Support
   static bool IsSupported(const char *dgst);

   int Reset(const char *dgst = 0);
   int Update(const char *b, int l);
   int Final();
};

#endif
