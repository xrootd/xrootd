#ifndef __CRYPTO_X509_H__
#define __CRYPTO_X509_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C r y p t o X 5 0 9 . h h                      */
/*                                                                            */
/* (c) 2005 G. Ganis , CERN                                                   */
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
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Abstract interface for X509 certificates.                                  */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include "XProtocol/XPtypes.hh"
#include "XrdSut/XrdSutBucket.hh"
#include "XrdCrypto/XrdCryptoRSA.hh"

typedef void * XrdCryptoX509data;

// ---------------------------------------------------------------------------//
//
// X509 interface
// Describes one certificate
//
// ---------------------------------------------------------------------------//
class XrdCryptoX509 {
public:

   // Certificate type
   enum EX509Type { kUnknown = -1, kCA = 0, kEEC = 1, kProxy = 2 };
   EX509Type    type;


   XrdCryptoX509() { type = kUnknown; }
   virtual ~XrdCryptoX509() { }

   // Status
   virtual bool IsValid(int when = 0);   // object correctly loaded
   virtual bool IsExpired(int when = 0);  // Expired

   // Access underlying data (in opaque form: used in chains)
   virtual XrdCryptoX509data Opaque();

   // Access certificate key
   virtual XrdCryptoRSA *PKI();
   virtual void SetPKI(XrdCryptoX509data pki);

   // Export in form of bucket (for transfers)
   virtual XrdSutBucket *Export();

   // Dump information
   virtual void Dump();
   virtual int DumpExtensions(); // extensions

   const char *Type(EX509Type t = kUnknown) const
                 { return ((t == kUnknown) ? ctype[type+1] : ctype[t+1]); }
   virtual const char *ParentFile();

   // Key strength
   virtual int BitStrength();

   // Serial number
   virtual kXR_int64 SerialNumber();
   virtual XrdOucString SerialNumberString();

   // Validity interval
   virtual int  NotBefore();  // begin-validity time in secs since Epoch
   virtual int  NotAfter();   // end-validity time in secs since Epoch

   // Issuer of top certificate
   virtual const char *Issuer();
   virtual const char *IssuerHash(int);   // hash 
   const char *IssuerHash() { return IssuerHash(0); }   // hash 

   // Subject of bottom certificate
   virtual const char *Subject();
   virtual const char *SubjectHash(int);   // hash 
   const char *SubjectHash() { return SubjectHash(0); }  // hash 

   // Retrieve a given extension if there (in opaque form) 
   virtual XrdCryptoX509data GetExtension(const char *oid);

   // Verify signature
   virtual bool Verify(XrdCryptoX509 *ref);

private:

   static const char *ctype[4];  // Names of types
};

#endif
