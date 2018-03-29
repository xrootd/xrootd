#ifndef __CRYPTO_RSA_H__
#define __CRYPTO_RSA_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C r y p t o R S A . h h                        */
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
/* Abstract interface for RSA PKI functionality.                              */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include "XrdSut/XrdSutBucket.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdCrypto/XrdCryptoAux.hh"

typedef void * XrdCryptoRSAdata;

// ---------------------------------------------------------------------------//
//
// RSA interface
//
// ---------------------------------------------------------------------------//
class XrdCryptoRSA
{
public:
   XrdCryptoRSA() { status = kInvalid; }
   virtual ~XrdCryptoRSA() {}

   // Status
   enum ERSAStatus { kInvalid = 0, kPublic = 1, kComplete = 2};
   ERSAStatus  status;
   const char *Status(ERSAStatus t = kInvalid) const
                 { return ((t == kInvalid) ? cstatus[status] : cstatus[t]); }

   // Access underlying data (in opaque form)
   virtual XrdCryptoRSAdata Opaque();

   // Dump information
   virtual void Dump();

   // Validity
   bool IsValid() { return (status != kInvalid); }

   // Output lengths
   virtual int GetOutlen(int lin);   // Length of encrypted buffers
   virtual int GetPublen();          // Length of export public key
   virtual int GetPrilen();          // Length of export private key

   // Import / Export methods
   virtual int ImportPublic(const char *in, int lin);
   virtual int ExportPublic(char *out, int lout);
   int ExportPublic(XrdOucString &exp);
   virtual int ImportPrivate(const char *in, int lin);
   virtual int ExportPrivate(char *out, int lout);
   int ExportPrivate(XrdOucString &exp);

   // Encryption / Decryption methods
   virtual int EncryptPrivate(const char *in, int lin, char *out, int lout);
   virtual int DecryptPublic(const char *in, int lin, char *out, int lout);
   virtual int EncryptPublic(const char *in, int lin, char *out, int lout);
   virtual int DecryptPrivate(const char *in, int lin, char *out, int lout);
   int EncryptPrivate(XrdSutBucket &buck);
   int DecryptPublic (XrdSutBucket &buck);
   int EncryptPublic (XrdSutBucket &buck);
   int DecryptPrivate(XrdSutBucket &buck);

private:
   static const char *cstatus[3];  // Names of status
};

#endif
