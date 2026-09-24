#ifndef __XRD_GSIRTAG_H__
#define __XRD_GSIRTAG_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S e c g s i R t a g . h h                       */
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

#include <cstring>

#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdCrypto/XrdCryptoMsgDigest.hh"

/******************************************************************************/
/*                                                                            */
/*  Helpers for the GSI random tag (kXRS_rtag).                               */
/*                                                                            */
/*  A peer signs the random tag of the other side with its private key to     */
/*  prove that it owns the key. The signature uses RSA with PKCS#1 v1.5       */
/*  padding and no digest, so the signed value must never be under the        */
/*  control of the peer. Two rules keep it safe:                              */
/*                                                                            */
/*  1. Sign only a well formed random tag. XrdSutRndm::GetRndmTag() always    */
/*     produces 8 characters in [a-zA-Z0-9./], and every ASN.1 DigestInfo     */
/*     is at least 34 bytes long, so the length check alone excludes them.    */
/*     This rule applies to every peer, whatever version it claims.           */
/*                                                                            */
/*  2. From version XrdSecgsiVersRtagHash on, sign the context bound digest   */
/*     of the tag instead of the tag itself. The digest is a bare hash, not   */
/*     a DigestInfo, so no PKCS#1 v1.5 verifier accepts a signature over it.  */
/*                                                                            */
/******************************************************************************/

//
// Length of a GSI random tag, see XrdSutRndm::GetRndmTag()
#define kXRSrtagLen  8

//
// Context prefix for the digest of the random tag
#define kXRSrtagCtx  "XRD-GSI-RTAG-v1"

//
// Digest used for the random tag; SHA-256 gives 32 bytes
#define kXRSrtagMD   "sha256"

//
// Buffer size that holds the digest of a random tag (EVP_MAX_MD_SIZE)
#define kXRSrtagMDMax  64

//_____________________________________________________________________________
inline bool XrdSecgsiRtagIsValid(const char *rtag, int len)
{
   // Return true if rtag holds a well formed GSI random tag, that is
   // exactly kXRSrtagLen characters in the set [a-zA-Z0-9./].

   if (!rtag || len != kXRSrtagLen) return false;

   for (int i = 0; i < len; i++) {
      const char c = rtag[i];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '/') continue;
      return false;
   }

   return true;
}

//_____________________________________________________________________________
inline int XrdSecgsiRtagDigest(XrdCryptoFactory *cf, const char *rtag, int len,
                               char *out, int outmax)
{
   // Fill out with the context bound digest of rtag, that is
   // SHA-256(kXRSrtagCtx || rtag). Return the number of bytes written,
   // or -1 in case of error.

   if (!cf || !rtag || len <= 0 || !out) return -1;

   // The factory returns a valid and initialized digest, or null
   XrdCryptoMsgDigest *md = cf->MsgDigest(kXRSrtagMD);
   if (!md) return -1;

   int lout = -1;

   if (md->Update(kXRSrtagCtx, (int)strlen(kXRSrtagCtx)) == 0 &&
       md->Update(rtag, len) == 0 && md->Final() == 0 &&
       md->Length() > 0 && md->Length() <= outmax) {
      lout = md->Length();
      memcpy(out, md->Buffer(), lout);
   }

   delete md;

   return lout;
}

#endif
