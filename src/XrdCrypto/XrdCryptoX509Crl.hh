#ifndef __CRYPTO_X509CRL_H__
#define __CRYPTO_X509CRL_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d C r y p t o X 5 0 9 C r l . h h                    */
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
/* Abstract interface for X509 CRLs        .                                  */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoX509.hh"

typedef void * XrdCryptoX509Crldata;

// ---------------------------------------------------------------------------//
//
// X509 CRL interface
// Describes one CRL certificate
//
// ---------------------------------------------------------------------------//
class XrdCryptoX509Crl {
public:

   XrdCryptoX509Crl() { }
   virtual ~XrdCryptoX509Crl() { }

   // Status
   virtual bool IsValid();
   virtual bool IsExpired(int when = 0);  // Expired

   // Access underlying data (in opaque form: used in chains)
   virtual XrdCryptoX509Crldata Opaque();

   // Dump information
   virtual void Dump();
   virtual const char *ParentFile();

   // Validity interval
   virtual time_t LastUpdate();  // time when last updated
   virtual time_t NextUpdate();  // time foreseen for next update

   // Issuer of top certificate
   virtual const char *Issuer();
   virtual const char *IssuerHash(int);   // hash 
   const char *IssuerHash() { return IssuerHash(0); }   // hash 

   // Chec certificate revocation
   virtual bool IsRevoked(int serialnumber, int when);
   virtual bool IsRevoked(const char *sernum, int when);

   // Verify signature
   virtual bool Verify(XrdCryptoX509 *ref);

};

#endif
