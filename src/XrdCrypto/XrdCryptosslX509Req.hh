#ifndef __CRYPTO_SSLX509REQ_H__
#define __CRYPTO_SSLX509REQ_H__
/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o s s l X 5 0 9 R e q . h h                  */
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
/* OpenSSL implementation of XrdCryptoX509                                    */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoX509Req.hh"

#include <openssl/x509v3.h>
#include <openssl/bio.h>

// ---------------------------------------------------------------------------//
//
// OpenSSL X509 request implementation
//
// ---------------------------------------------------------------------------//
class XrdCryptosslX509Req : public XrdCryptoX509Req
{

public:
   XrdCryptosslX509Req(XrdSutBucket *bck);
   XrdCryptosslX509Req(X509_REQ *creq);
   virtual ~XrdCryptosslX509Req();

   // Access underlying data (in opaque form: used in chains)
   XrdCryptoX509Reqdata Opaque() { return (XrdCryptoX509Reqdata)creq; }

   // Access certificate key
   XrdCryptoRSA *PKI() { return pki; }

   // Export in form of bucket (for transfers)
   XrdSutBucket *Export();

   // Relevant Names
   const char *Subject();  // get subject name

   // Relevant hashes
   const char *SubjectHash(int);  // get hash of subject name

   // Retrieve a given extension if there (in opaque form)
   XrdCryptoX509Reqdata GetExtension(const char *oid);

   // Verify signature
   bool        Verify();

private:
   X509_REQ    *creq;       // The certificate request object
   XrdOucString subject;    // subject;
   XrdOucString subjecthash; // hash of subject (default algorithm);
   XrdOucString subjectoldhash; // hash of subject (md5 algorithm);
   XrdSutBucket *bucket;    // Bucket for export operations
   XrdCryptoRSA *pki;       // PKI of the certificate
};

#endif
