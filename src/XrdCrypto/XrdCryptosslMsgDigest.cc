/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o M s g D i g e s t . c c                    */
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
/* OpenSSL implementation of XrdCryptoMsgDigest                               */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"
#include "XrdCrypto/XrdCryptosslMsgDigest.hh"

//_____________________________________________________________________________
XrdCryptosslMsgDigest::XrdCryptosslMsgDigest(const char *dgst)
                      : XrdCryptoMsgDigest(), valid(0), mdctx(0)
{
   // Constructor.
   // Init the message digest calculation

   SetType(0);
   Init(dgst);
}

//_____________________________________________________________________________
XrdCryptosslMsgDigest::~XrdCryptosslMsgDigest()
{
   // Destructor.

   if (valid) {
      unsigned char mdval[EVP_MAX_MD_SIZE];
      EVP_DigestFinal_ex(mdctx, mdval, 0);
      EVP_MD_CTX_destroy(mdctx);
   }
}

//_____________________________________________________________________________
bool XrdCryptosslMsgDigest::IsSupported(const char *dgst)
{
   // Check if the specified MD is supported

   return (EVP_get_digestbyname(dgst) != 0);
}

//_____________________________________________________________________________
int XrdCryptosslMsgDigest::Init(const char *dgst)
{
   // Initialize the buffer for the message digest calculation
   EPNAME("MsgDigest::Init");

   // We use the input digest type; or the old one; or the default, sha-256
   if (dgst) {
      SetType(dgst);
   } else if (!Type()) {
      SetType("sha256");
   }

   // Get message digest handle
   const EVP_MD *md = 0;
   if (!(md = EVP_get_digestbyname(Type()))) {
      PRINT("EROOR: cannot get msg digest by name");
      return -1;
   }

   // Init digest machine
   mdctx = EVP_MD_CTX_create();
   if (!EVP_DigestInit_ex(mdctx, md, NULL)) {
      PRINT("ERROR: cannot initialize digest");
      EVP_MD_CTX_destroy(mdctx);
      return -1;
   }

   // Successful initialization
   valid = 1;

   // OK
   return 0;   
}

//_____________________________________________________________________________
int XrdCryptosslMsgDigest::Reset(const char *dgst)
{
   // Re-Init the message digest calculation
   if (valid) {
      unsigned char mdval[EVP_MAX_MD_SIZE];
      EVP_DigestFinal_ex(mdctx, mdval, 0);
      SetBuffer(0,0);
      EVP_MD_CTX_destroy(mdctx);
   }
   valid = 0;
   Init(dgst);
   if (!valid) return -1;

   return 0;
}

//_____________________________________________________________________________
int XrdCryptosslMsgDigest::Update(const char *b, int l)
{
   // Update message digest with the MD of l bytes at b.
   // Create the internal buffer if needed (first call)
   // Returns -1 if unsuccessful (digest not initialized), 0 otherwise.

   if (Type()) {
      EVP_DigestUpdate(mdctx, (char *)b, l);
      return 0;
   }
   return -1;   
}

//_____________________________________________________________________________
int XrdCryptosslMsgDigest::Final()
{
   // Finalize message digest calculation.
   // Finalize the operation
   // Returns -1 if unsuccessful (digest not initialized), 0 otherwise.
   EPNAME("MsgDigest::Final");

   // MD outputs in these variables
   unsigned char mdval[EVP_MAX_MD_SIZE] = {0};
   unsigned int mdlen = 0;

   if (Type()) {
      // Finalize what we have
      if (EVP_DigestFinal_ex(mdctx, mdval, &mdlen) == 1) {
         // Save result
         SetBuffer(mdlen,(const char *)mdval);
         // Notify, if requested
         DEBUG("result length is "<<mdlen <<
               " bytes (hex: " << AsHexString() <<")");
         return 0;
      } else {
         PRINT("ERROR: problems finalizing digest");
      }
   }
   return -1;   
}
