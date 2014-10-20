#ifndef __CRYPTO_SSLX509_H__
#define __CRYPTO_SSLX509_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d C r y p t o s s l X 5 0 9 . h h                    */
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

#include "XrdCrypto/XrdCryptoX509.hh"

#include <openssl/x509v3.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER >= 0x0090800f
#  define XRDGSI_CONST const
#else
#  define XRDGSI_CONST
#endif

// ---------------------------------------------------------------------------//
//
// OpenSSL X509 implementation
//
// ---------------------------------------------------------------------------//
class XrdCryptosslX509 : public XrdCryptoX509
{

public:
   XrdCryptosslX509(const char *cf, const char *kf = 0);
   XrdCryptosslX509(XrdSutBucket *bck);
   XrdCryptosslX509(X509 *cert);
   virtual ~XrdCryptosslX509();

   // Access underlying data (in opaque form: used in chains)
   XrdCryptoX509data Opaque() { return (XrdCryptoX509data)cert; }

   // Dump extensions
   int DumpExtensions();

   // Access certificate key
   XrdCryptoRSA *PKI() { return pki; }
   void SetPKI(XrdCryptoX509data pki);

   // Export in form of bucket (for transfers)
   XrdSutBucket *Export();

   // Parent file
   const char *ParentFile() { return (const char *)(srcfile.c_str()); }

   // Key strength
   int BitStrength() { return ((cert) ? EVP_PKEY_bits(X509_get_pubkey(cert)) : -1);}

   // Serial number
   kXR_int64 SerialNumber();
   XrdOucString SerialNumberString();

   // Validity
   int NotBefore();  // get begin-validity time in secs since Epoch
   int NotAfter();   // get end-validity time in secs since Epoch

   // Relevant Names
   const char *Subject();  // get subject name
   const char *Issuer();   // get issuer name

   // Relevant hashes
   const char *SubjectHash(int = 0);  // get hash of subject name
   const char *IssuerHash(int = 0);   // get hash of issuer name 

   // Retrieve a given extension if there (in opaque form)
   XrdCryptoX509data GetExtension(const char *oid);

   // Verify signature
   bool        Verify(XrdCryptoX509 *ref);

private:
   X509        *cert;       // The certificate object
   int          notbefore;  // begin-validity time in secs since Epoch
   int          notafter;   // end-validity time in secs since Epoch
   XrdOucString subject;    // subject;
   XrdOucString issuer;     // issuer name;
   XrdOucString subjecthash; // Default hash of subject;
   XrdOucString issuerhash;  // Default hash of issuer name;
   XrdOucString subjectoldhash; // Old (md5) hash of subject if v >= 1.0.0;
   XrdOucString issueroldhash;  // Old (md5) hash of issuer name if v >= 1.0.0;
   XrdOucString srcfile;    // source file name, if any;
   XrdSutBucket *bucket;    // Bucket for export operations
   XrdCryptoRSA *pki;       // PKI of the certificate

   bool         IsCA();     // Find out if we are a CA

   int          FillUnknownExt(XRDGSI_CONST unsigned char **pp, long length);
   int          Asn1PrintInfo(int tag, int xclass, int constructed, int indent);
};

#endif
