#ifndef __CRYPTO_X509REQ_H__
#define __CRYPTO_X509REQ_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d C r y p t o X 5 0 9 R e q. h h                   */
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

#include "XrdSut/XrdSutBucket.hh"
#include "XrdCrypto/XrdCryptoRSA.hh"

typedef void * XrdCryptoX509Reqdata;

// ---------------------------------------------------------------------------//
//
// X509 request interface
// Describes a one certificate request
//
// ---------------------------------------------------------------------------//
class XrdCryptoX509Req {
public:

   XrdCryptoX509Req(int v = -1) { SetVersion(v); }
   virtual ~XrdCryptoX509Req() { }

   // Status
   virtual bool IsValid();

   // Access underlying data (in opaque form: used in chains)
   virtual XrdCryptoX509Reqdata Opaque();

   // Access certificate key
   virtual XrdCryptoRSA *PKI();

   // Export in form of bucket (for transfers)
   virtual XrdSutBucket *Export();

   // Dump information
   virtual void Dump();

   // Subject of bottom certificate
   virtual const char *Subject();
   virtual const char *SubjectHash(int);   // hash 
   const char *SubjectHash() { return SubjectHash(0); }   // hash 

   // Retrieve a given extension if there (in opaque form) 
   virtual XrdCryptoX509Reqdata GetExtension(const char *oid);

   // Verify signature
   virtual bool Verify();

   // Set / Get version
   int Version() const { return version; }
   void SetVersion(int v) { version = v; }

private:
   int version;   // Version of the plugin producing the request
};

#endif
