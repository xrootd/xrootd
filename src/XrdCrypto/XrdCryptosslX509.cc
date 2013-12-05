/******************************************************************************/
/*                                                                            */
/*                   X r d C r y p t o s s l X 5 0 9 . c c                    */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"

#include <openssl/pem.h>

//_____________________________________________________________________________
XrdCryptosslX509::XrdCryptosslX509(const char *cf, const char *kf)
                 : XrdCryptoX509()
{
   // Constructor certificate from file 'cf'. If 'kf' is defined,
   // complete the key of the certificate with the private key in kf.
   EPNAME("X509::XrdCryptosslX509_file");

   // Init private members
   cert = 0;        // The certificate object
   notbefore = -1;  // begin-validity time in secs since Epoch
   notafter = -1;   // end-validity time in secs since Epoch
   subject = "";    // subject;
   issuer = "";     // issuer;
   subjecthash = ""; // hash of subject;
   issuerhash = "";  // hash of issuer;
   subjectoldhash = ""; // hash of subject (md5 algorithm);
   issueroldhash = "";  // hash of issuer (md5 algorithm);
   srcfile = "";    // source file;
   bucket = 0;      // bucket for serialization
   pki = 0;         // PKI of the certificate

   // Make sure file name is defined;
   if (!cf) {
      DEBUG("file name undefined");
      return;
   }
   // Make sure file exists;
   struct stat st;
   if (stat(cf, &st) != 0) {
      if (errno == ENOENT) {
         DEBUG("file "<<cf<<" does not exist - do nothing");
      } else {
         DEBUG("cannot stat file "<<cf<<" (errno: "<<errno<<")");
      }
      return;
   }
   //
   // Open file in read mode
   FILE *fc = fopen(cf, "r");
   if (!fc) {
      DEBUG("cannot open file "<<cf<<" (errno: "<<errno<<")");
      return;
   }
   //
   // Read the content:
   if (!PEM_read_X509(fc, &cert, 0, 0)) {
      DEBUG("Unable to load certificate from file");
      return;
   } else {
      DEBUG("certificate successfully loaded");
   }
   //
   // Close the file
   fclose(fc);
   //
   // Save source file name
   srcfile = cf;
   // Init some of the private members (the others upon need)
   Subject();
   Issuer();
   //
   // Find out type of certificate
   if (IsCA()) {
      type = kCA;
   } else {
      XrdOucString common(issuer,0,issuer.find('/',issuer.find("/CN=")+1));
      if (subject.beginswith(common))
         type = kProxy;
      else
         type = kEEC;
   }
   // Get the public key
   EVP_PKEY *evpp = 0;
   // Read the private key file, if specified
   if (kf) {
      if (stat(kf, &st) == -1) {
         DEBUG("cannot stat private key file "<<kf<<" (errno:"<<errno<<")");
         return;
      }
      if (!S_ISREG(st.st_mode) || S_ISDIR(st.st_mode) ||
            (st.st_mode & (S_IROTH | S_IWOTH)) != 0 ||
            (st.st_mode & (S_IWGRP)) != 0) {
         DEBUG("private key file "<<kf<<" has wrong permissions "<<
               (st.st_mode & 0777) << " (should be at most 0640)");
         return;
      }
      // Open file in read mode
      FILE *fk = fopen(kf, "r");
      if (!fk) {
         DEBUG("cannot open file "<<kf<<" (errno: "<<errno<<")");
         return;
      }
      // This call fills the full key, i.e. also the public part (not really documented, though)
      if ((evpp = PEM_read_PrivateKey(fk,0,0,0))) {
         DEBUG("RSA key completed ");
         // Test consistency
         if (RSA_check_key(evpp->pkey.rsa) != 0) {
            // Save it in pki
            pki = new XrdCryptosslRSA(evpp);
         }
      } else {
         DEBUG("cannot read the key from file");
      }
      // Close the file
      fclose(fk);
   }
   // If there were no private key or we did not manage to import it
   // init pki with the partial key
   if (!pki)
      pki = new XrdCryptosslRSA(X509_get_pubkey(cert), 0);
}

//_____________________________________________________________________________
XrdCryptosslX509::XrdCryptosslX509(XrdSutBucket *buck) : XrdCryptoX509()
{
   // Constructor certificate from BIO 'bcer'
   EPNAME("X509::XrdCryptosslX509_bio");

   // Init private members
   cert = 0;        // The certificate object
   notbefore = -1;  // begin-validity time in secs since Epoch
   notafter = -1;   // end-validity time in secs since Epoch
   subject = "";    // subject;
   issuer = "";     // issuer;
   subjecthash = ""; // hash of subject;
   issuerhash = "";  // hash of issuer;
   subjectoldhash = ""; // hash of subject (md5 algorithm);
   issueroldhash = "";  // hash of issuer (md5 algorithm);
   srcfile = "";    // source file;
   bucket = 0;      // bucket for serialization
   pki = 0;         // PKI of the certificate

   // Make sure we got something;
   if (!buck) {
      DEBUG("got undefined opaque buffer");
      return;
   }

   //
   // Create a bio_mem to store the certificates
   BIO *bmem = BIO_new(BIO_s_mem());
   if (!bmem) {
      DEBUG("unable to create BIO for memory operations");
      return; 
   }

   // Write data to BIO
   int nw = BIO_write(bmem,(const void *)(buck->buffer),buck->size);
   if (nw != buck->size) {
      DEBUG("problems writing data to memory BIO (nw: "<<nw<<")");
      return; 
   }

   // Get certificate from BIO
   if (!(cert = PEM_read_bio_X509(bmem,0,0,0))) {
      DEBUG("unable to read certificate to memory BIO");
      return;
   }
   //
   // Free BIO
   BIO_free(bmem);
   //
   // Init some of the private members (the others upon need)
   Subject();
   Issuer();
   //
   // Find out type of certificate
   if (IsCA()) {
      type = kCA;
   } else {
      XrdOucString common(issuer,0,issuer.find('/',issuer.find("/CN=")+1));
      if (subject.beginswith(common))
         type = kProxy;
      else
         type = kEEC;
   }
   // Get the public key
   EVP_PKEY *evpp = X509_get_pubkey(cert);
   //
   if (evpp) {
      // init pki with the partial key
      if (!pki)
         pki = new XrdCryptosslRSA(evpp, 0);
   } else {
      DEBUG("could not access the public key");
   }
}

//_____________________________________________________________________________
XrdCryptosslX509::XrdCryptosslX509(X509 *xc) : XrdCryptoX509()
{
   // Constructor: import X509 object
   EPNAME("X509::XrdCryptosslX509_x509");

   // Init private members
   cert = 0;        // The certificate object
   notbefore = -1;  // begin-validity time in secs since Epoch
   notafter = -1;   // end-validity time in secs since Epoch
   subject = "";    // subject;
   issuer = "";     // issuer;
   subjecthash = ""; // hash of subject;
   issuerhash = "";  // hash of issuer;
   subjectoldhash = ""; // hash of subject (md5 algorithm);
   issueroldhash = "";  // hash of issuer (md5 algorithm);
   srcfile = "";    // source file;
   bucket = 0;      // bucket for serialization
   pki = 0;         // PKI of the certificate

   // Make sure we got something;
   if (!xc) {
      DEBUG("got undefined X509 object");
      return;
   }

   // Set certificate
   cert = xc;
   //
   // Init some of the private members (the others upon need)
   Subject();
   Issuer();
   //
   // Find out type of certificate
   if (IsCA()) {
      type = kCA;
   } else {
      XrdOucString common(issuer,0,issuer.find('/',issuer.find("/CN=")+1));
      if (subject.beginswith(common))
         type = kProxy;
      else
         type = kEEC;
   }
   // Get the public key
   EVP_PKEY *evpp = X509_get_pubkey(cert);
   //
   if (evpp) {
      // init pki with the partial key
      if (!pki)
         pki = new XrdCryptosslRSA(evpp, 0);
   } else {
      DEBUG("could not access the public key");
   }
}

//_____________________________________________________________________________
XrdCryptosslX509::~XrdCryptosslX509()
{
   // Destructor

   // Cleanup certificate
   if (cert) X509_free(cert);
   // Cleanup key
   if (pki) delete pki;
}

//_____________________________________________________________________________
void XrdCryptosslX509::SetPKI(XrdCryptoX509data newpki)
{
   // Set PKI

   // Cleanup key first
   if (pki)
      delete pki;
   if (newpki)
      pki = new XrdCryptosslRSA((EVP_PKEY *)newpki, 1);

}

//_____________________________________________________________________________
int XrdCryptosslX509::NotBefore()
{
   // Begin-validity time in secs since Epoch

   // If we do not have it already, try extraction
   if (notbefore < 0) {
      // Make sure we have a certificate
      if (cert)
         // Extract UTC time in secs from Epoch
         notbefore = XrdCryptosslASN1toUTC(X509_get_notBefore(cert));
   }
   // return what we have
   return notbefore;
}

//_____________________________________________________________________________
int XrdCryptosslX509::NotAfter()
{
   // End-validity time in secs since Epoch

   // If we do not have it already, try extraction
   if (notafter < 0) {
      // Make sure we have a certificate
      if (cert)
         // Extract UTC time in secs from Epoch
         notafter = XrdCryptosslASN1toUTC(X509_get_notAfter(cert));
   }
   // return what we have
   return notafter;
}

//_____________________________________________________________________________
const char *XrdCryptosslX509::Subject()
{
   // Return subject name
   EPNAME("X509::Subject");

   // If we do not have it already, try extraction
   if (subject.length() <= 0) {

      // Make sure we have a certificate
      if (!cert) {
         DEBUG("WARNING: no certificate available - cannot extract subject name");
         return (const char *)0;
      }

      // Extract subject name
      XrdCryptosslNameOneLine(X509_get_subject_name(cert), subject);
   }

   // return what we have
   return (subject.length() > 0) ? subject.c_str() : (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptosslX509::Issuer()
{
   // Return issuer name
   EPNAME("X509::Issuer");

   // If we do not have it already, try extraction
   if (issuer.length() <= 0) {

      // Make sure we have a certificate
      if (!cert) {
         DEBUG("WARNING: no certificate available - cannot extract issuer name");
         return (const char *)0;
      }

      // Extract issuer name
      XrdCryptosslNameOneLine(X509_get_issuer_name(cert), issuer);
   }

   // return what we have
   return (issuer.length() > 0) ? issuer.c_str() : (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptosslX509::IssuerHash(int alg)
{
   // Return hash of issuer name
   // Use default algorithm (X509_NAME_hash) for alg = 0, old algorithm
   // (for v>=1.0.0) when alg = 1
   EPNAME("X509::IssuerHash");

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
   if (alg == 1) {
      // md5 based
      if (issueroldhash.length() <= 0) {
         // Make sure we have a certificate
         if (cert) {
            char chash[15] = {0};
            snprintf(chash,15,"%08lx.0",X509_NAME_hash_old(cert->cert_info->issuer));
            issueroldhash = chash;
         } else {
            DEBUG("WARNING: no certificate available - cannot extract issuer hash (md5)");
         }
      }
      // return what we have
      return (issueroldhash.length() > 0) ? issueroldhash.c_str() : (const char *)0;
   }
#else
   if (alg == 1) { }
#endif

   // If we do not have it already, try extraction
   if (issuerhash.length() <= 0) {

      // Make sure we have a certificate
      if (cert) {
         char chash[15] = {0};
         if (chash[0] == 0)
            snprintf(chash,15,"%08lx.0",X509_NAME_hash(cert->cert_info->issuer));
         issuerhash = chash;
      } else {
         DEBUG("WARNING: no certificate available - cannot extract issuer hash (default)");
      }
   }

   // return what we have
   return (issuerhash.length() > 0) ? issuerhash.c_str() : (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptosslX509::SubjectHash(int alg)
{
   // Return hash of subject name
   // Use default algorithm (X509_NAME_hash) for alg = 0, old algorithm
   // (for v>=1.0.0) when alg = 1
   EPNAME("X509::SubjectHash");

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
   if (alg == 1) {
      // md5 based
      if (subjectoldhash.length() <= 0) {
         // Make sure we have a certificate
         if (cert) {
            char chash[15] = {0};
            snprintf(chash,15,"%08lx.0",X509_NAME_hash_old(cert->cert_info->subject));
            subjectoldhash = chash;
         } else {
            DEBUG("WARNING: no certificate available - cannot extract subject hash (md5)");
         }
      }
      // return what we have
      return (subjectoldhash.length() > 0) ? subjectoldhash.c_str() : (const char *)0;
   }
#else
   if (alg == 1) { }
#endif

   // If we do not have it already, try extraction
   if (subjecthash.length() <= 0) {

      // Make sure we have a certificate
      if (cert) {
         char chash[15] = {0};
         if (chash[0] == 0)
            snprintf(chash,15,"%08lx.0",X509_NAME_hash(cert->cert_info->subject));
         subjecthash = chash;
      } else {
         DEBUG("WARNING: no certificate available - cannot extract subject hash (default)");
      }
   }

   // return what we have
   return (subjecthash.length() > 0) ? subjecthash.c_str() : (const char *)0;
}

//_____________________________________________________________________________
kXR_int64 XrdCryptosslX509::SerialNumber()
{
   // Return serial number as a kXR_int64

   kXR_int64 sernum = -1;
   if (cert && X509_get_serialNumber(cert)) {
      BIGNUM *bn = BN_new();
      ASN1_INTEGER_to_BN(X509_get_serialNumber(cert), bn);
      char *sn = BN_bn2dec(bn);
      sernum = strtoll(sn, 0, 10);
      BN_free(bn);
      OPENSSL_free(sn);
   }

   return sernum;
}

//_____________________________________________________________________________
XrdOucString XrdCryptosslX509::SerialNumberString()
{
   // Return serial number as a hex string

   XrdOucString sernum;
   if (cert && X509_get_serialNumber(cert)) {
      BIGNUM *bn = BN_new();
      ASN1_INTEGER_to_BN(X509_get_serialNumber(cert), bn);
      char *sn = BN_bn2hex(bn);
      sernum = sn;
      BN_free(bn);
      OPENSSL_free(sn);
   }

   return sernum;
}

//_____________________________________________________________________________
XrdCryptoX509data XrdCryptosslX509::GetExtension(const char *oid)
{
   // Return pointer to extension with OID oid, if any, in
   // opaque form
   EPNAME("X509::GetExtension");
   XrdCryptoX509data ext = 0;

   // Make sure we got something to look for
   if (!oid) {
      DEBUG("OID string not defined");
      return ext;
   }
 
   // Make sure we got something to look for
   if (!cert) {
      DEBUG("certificate is not initialized");
      return ext;
   }

   // Are there any extension?
   int numext = X509_get_ext_count(cert);
   if (numext <= 0) {
      DEBUG("certificate has got no extensions");
      return ext;
   }
   DEBUG("certificate has "<<numext<<" extensions");

   // If the string is the Standard Name of a known extension check
   // searche the corresponding NID
   int nid = OBJ_sn2nid(oid);
   bool usenid = (nid > 0);

   // Loop to identify the one we would like
   int i = 0;
   X509_EXTENSION *wext = 0;
   for (i = 0; i< numext; i++) {
      wext = X509_get_ext(cert, i);
      if (usenid) {
         int enid = OBJ_obj2nid(X509_EXTENSION_get_object(wext));
         if (enid == nid)
            break;
      } else {
         // Try matching of the text
         char s[256];
         OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(wext), 1);
         if (!strcmp(s, oid)) 
            break;
      }
      // Do not free the extension: its owned by the certificate
      wext = 0;
   }

   // We are done if nothing was found
   if (!wext) {
      DEBUG("Extension "<<oid<<" not found"); 
      return ext;
   }

   // We are done
   return (XrdCryptoX509data)wext;
}

//_____________________________________________________________________________
bool XrdCryptosslX509::IsCA() 
{
   // Check if this certificate is a CA certificate
   EPNAME("X509::IsCA");

   // Make sure we got something to look for
   if (!cert) {
      DEBUG("certificate is not initialized");
      return 0;
   }

   // Are there any extension?
   int numext = X509_get_ext_count(cert);
   if (numext <= 0) {
      DEBUG("certificate has got no extensions");
      return 0;
   }
   TRACE(ALL,"certificate has "<<numext<<" extensions");

   X509_EXTENSION *ext = 0;
   int i = 0;
   for (; i < numext; i++) {
      // Get the extension
      ext = X509_get_ext(cert,i);
      // We are looking for a "basicConstraints"
      if (OBJ_obj2nid(X509_EXTENSION_get_object(ext)) ==
          OBJ_sn2nid("basicConstraints")) {
         break;
      }
      // Do not free the extension: its owned by the certificate
      ext = 0;
   }

   // Return it there were none
   if (!ext) 
      return 0;

   // Analyse the structure
   unsigned char *p = ext->value->data;
#if OPENSSL_VERSION_NUMBER >= 0x0090800f
   BASIC_CONSTRAINTS *bc =
      d2i_BASIC_CONSTRAINTS(0, const_cast<const unsigned char**>(&p), ext->value->length);
#else
   BASIC_CONSTRAINTS *bc = d2i_BASIC_CONSTRAINTS(0, &p, ext->value->length);
#endif

   // CA?
   bool isca = (bc->ca != 0);
   if (isca) {
      DEBUG("CA certificate"); 
   }

   BASIC_CONSTRAINTS_free(bc);

   // We are done
   return isca;
}

//_____________________________________________________________________________
XrdSutBucket *XrdCryptosslX509::Export()
{
   // Export in form of bucket
   EPNAME("X509::Export");

   // If we have already done it, return the previous result
   if (bucket) {
      DEBUG("serialization already performed:"
            " return previous result ("<<bucket->size<<" bytes)");
      return bucket;
   }

   // Make sure we got something to export
   if (!cert) {
      DEBUG("certificate is not initialized");
      return 0;
   }

   //
   // Now we create a bio_mem to serialize the certificate
   BIO *bmem = BIO_new(BIO_s_mem());
   if (!bmem) {
      DEBUG("unable to create BIO for memory operations");
      return 0;
   }

   // Write certificate to BIO
   if (!PEM_write_bio_X509(bmem, cert)) {
      DEBUG("unable to write certificate to memory BIO");
      return 0;
   }

   // Extract pointer to BIO data and length of segment
   char *bdata = 0;  
   int blen = BIO_get_mem_data(bmem, &bdata);
   DEBUG("BIO data: "<<blen<<" bytes at 0x"<<(int *)bdata);

   // create the bucket now
   bucket = new XrdSutBucket(0,0,kXRS_x509);
   if (bucket) {
      // Fill bucket
      bucket->SetBuf(bdata, blen);
      DEBUG("result of serialization: "<<bucket->size<<" bytes");
   } else {
      DEBUG("unable to create bucket for serialized format");
      BIO_free(bmem);
      return 0;
   }
   //
   // Free BIO
   BIO_free(bmem);
   //
   // We are done
   return bucket;
}

//_____________________________________________________________________________
bool XrdCryptosslX509::Verify(XrdCryptoX509 *ref)
{
   // Verify certificate signature with pub key of ref cert
   EPNAME("X509::Verify");

   // We must have been initialized
   if (!cert)
      return 0;

   // We must have something to check with
   X509 *r = ref ? (X509 *)(ref->Opaque()) : 0;
   EVP_PKEY *rk = r ? X509_get_pubkey(r) : 0;
   if (!rk)
      return 0;

   // Ok: we can verify
   int rc = X509_verify(cert, rk);
   EVP_PKEY_free(rk);
   if (rc <= 0) {
      if (rc == 0) {
         // Signatures are not OK
         DEBUG("signature not OK");
      } else {
         // General failure
         DEBUG("could not verify signature");
      }
      return 0;
   }
   // Success
   return 1;
}
