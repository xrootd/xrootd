#ifndef __CRYPTO_SSLX509STORE_H__
#define __CRYPTO_SSLX509STORE_H__
/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o s s l X 5 0 9 S t o r e . h h              */
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
/* OpenSSL implementation of XrdCryptoX509Store                               */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoX509Store.hh"

// ---------------------------------------------------------------------------//
//
// OpenSSL X509 implementation
//
// ---------------------------------------------------------------------------//
class XrdCryptosslX509Store : public XrdCryptoX509Store
{
public:
   XrdCryptosslX509Store();
   virtual ~XrdCryptosslX509Store();

   // Dump information
   void Dump();

   // Validity
   bool IsValid();

   // Add certificates to store
   int  Add(XrdCryptoX509 *);

   // Verify the chain stored
   bool Verify();

private:
   X509_STORE     *store;        // the store
   STACK_OF(X509) *chain;        // chain of certificates other than CA
};

#endif
