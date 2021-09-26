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
#include <cerrno>

#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"

#include <openssl/pem.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static RSA *EVP_PKEY_get0_RSA(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_RSA) {
        return NULL;
    }
    return pkey->pkey.rsa;
}
#endif

static int XrdCheckRSA (EVP_PKEY *pkey) {
   int rc;
#if OPENSSL_VERSION_NUMBER < 0x10101000L
   RSA *rsa = EVP_PKEY_get0_RSA(pkey);
   if (rsa)
      rc = RSA_check_key(rsa);
   else
      rc = -2;
#else
   EVP_PKEY_CTX *ckctx = EVP_PKEY_CTX_new(pkey, 0);
   rc = EVP_PKEY_check(ckctx);
   EVP_PKEY_CTX_free(ckctx);
#endif
   return rc;
}

#define BIO_PRINT(b,c) \
   BUF_MEM *bptr; \
   BIO_get_mem_ptr(b, &bptr); \
   if (bptr) { \
      char *s = new char[bptr->length+1]; \
      memcpy(s, bptr->data, bptr->length); \
      s[bptr->length] = '\0'; \
      PRINT(c << s); \
      delete [] s; \
   } else { \
      PRINT("ERROR: "<<c<<" BIO internal buffer undefined!"); \
   } \
   if (b) BIO_free(b);

const char *XrdCryptosslX509::cpxytype[5] = { "", "unknown", "RFC", "GSI3", "legacy" };

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
   pxytype = 0;    // Proxy sub-type

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
   CertType();

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
         if (XrdCheckRSA(evpp) == 1) {
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
   pxytype = 0;    // Proxy sub-type

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
   CertType();

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
   pxytype = 0;    // Proxy sub-type

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
   CertType();

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
void XrdCryptosslX509::CertType()
{
   // Determine the certificate type
   // Check the type of this certificate
   EPNAME("X509::CertType");

   // Make sure we got something to look for
   if (!cert) {
      PRINT("ERROR: certificate is not initialized");
      return;
   }

   // Default for an initialized certificate
   type = kEEC;

   // Are there any extension?
   int numext = X509_get_ext_count(cert);
   if (numext <= 0) {
      DEBUG("certificate has got no extensions");
      return;
   }
   TRACE(ALL,"certificate has "<<numext<<" extensions");
  
   bool done = 0;
   // Check the extensions
   X509_EXTENSION *ext = 0;
   int idx = -1;

   // For CAs we are looking for a "basicConstraints"
   int crit;
   BASIC_CONSTRAINTS *bc = 0;
   if ((bc = (BASIC_CONSTRAINTS *)X509_get_ext_d2i(cert, NID_basic_constraints, &crit, &idx)) &&
        bc->ca) {
      type = kCA;
      DEBUG("CA certificate");
      done = 1;
   }
   if (bc) BASIC_CONSTRAINTS_free(bc);
   if (done) return;

   // Is this a proxy?
   idx = -1;
   // Proxy names
   XrdOucString common(subject, 0, subject.rfind("/CN=") - 1);
   bool pxyname = 0;
   if (issuer == common) {
      pxyname = 1;
      pxytype = 1;
   }

   if (pxyname) {
      type = kUnknown;
      if ((idx = X509_get_ext_by_NID(cert, NID_proxyCertInfo,-1)) == -1) {
         int xcp = -1;
         XrdOucString emsg;
         if ((xcp = XrdCryptosslX509CheckProxy3(this, emsg)) == 0) {
            type = kProxy;
            pxytype = 3;
            DEBUG("Found GSI 3 proxyCertInfo extension");
         } else if (xcp == -1) {
            PRINT("ERROR: "<<emsg);
         }
      } else {
         if ((ext = X509_get_ext(cert,idx)) == 0) {
            PRINT("ERROR: could not get proxyCertInfo extension");
         }
      }
   }
   if (ext) {
      // RFC compliant or GSI 3 proxy
      if (X509_EXTENSION_get_critical(ext)) {
         PROXY_CERT_INFO_EXTENSION *pci = (PROXY_CERT_INFO_EXTENSION *)X509V3_EXT_d2i(ext);
         if (pci != 0) {
            if ((pci->proxyPolicy) != 0) {
               if ((pci->proxyPolicy->policyLanguage) != 0) {
                  type = kProxy;
                  done = 1;
                  pxytype = 2;
                  DEBUG("Found RFC 382{0,1}compliant proxyCertInfo extension");
                  if (X509_get_ext_by_NID(cert, NID_proxyCertInfo, idx) != -1) {
                     PRINT("WARNING: multiple proxyCertInfo extensions found: taking the first");
                  }
               } else {
                  PRINT("ERROR: accessing policy language from proxyCertInfo extension");
               }
            } else {
               PRINT("ERROR: accessing policy from proxyCertInfo extension");
            }
            PROXY_CERT_INFO_EXTENSION_free(pci);
         } else {
            PRINT("ERROR: proxyCertInfo conversion error");
         }
      } else {
         PRINT("ERROR: proxyCertInfo not flagged as critical");
      }
   }
   if (!pxyname || done) return;

   // Check if GSI 2 legacy proxy
   XrdOucString lastcn(subject, subject.rfind("/CN=") + 4, -1);
   if (lastcn == "proxy" || lastcn == "limited proxy") {
      pxytype = 4;
      type = kProxy;
   }

   // We are done
   return;
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
time_t XrdCryptosslX509::NotBefore()
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
time_t XrdCryptosslX509::NotAfter()
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

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L && !defined(__APPLE__))
   if (alg == 1) {
      // md5 based
      if (issueroldhash.length() <= 0) {
         // Make sure we have a certificate
         if (cert) {
            char chash[30] = {0};
            snprintf(chash, sizeof(chash),
                     "%08lx.0",X509_NAME_hash_old(X509_get_issuer_name(cert)));
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
         char chash[30] = {0};
         snprintf(chash, sizeof(chash),
                  "%08lx.0",X509_NAME_hash(X509_get_issuer_name(cert)));
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

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L && !defined(__APPLE__))
   if (alg == 1) {
      // md5 based
      if (subjectoldhash.length() <= 0) {
         // Make sure we have a certificate
         if (cert) {
            char chash[30] = {0};
            snprintf(chash, sizeof(chash),
                     "%08lx.0",X509_NAME_hash_old(X509_get_subject_name(cert)));
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
         char chash[30] = {0};
         snprintf(chash, sizeof(chash),
                  "%08lx.0",X509_NAME_hash(X509_get_subject_name(cert)));
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

//____________________________________________________________________________
int XrdCryptosslX509::DumpExtensions(bool dumpunknown)
{
   // Dump our extensions, if any
   // Returns -1 on failure, 0 on success 
   EPNAME("DumpExtensions");

   int rc = -1;
   // Point to the cerificate
   X509 *xpi = (X509 *) Opaque();

   // Make sure we got the right inputs
   if (!xpi) {
      PRINT("we are empty! Do nothing");
      return rc;
   }

   rc = 1;
   // Go through the extensions
   X509_EXTENSION *xpiext = 0;
   int npiext = X509_get_ext_count(xpi);
   PRINT("found "<<npiext<<" extensions ");
   int i = 0;
   for (i = 0; i< npiext; i++) {
      xpiext = X509_get_ext(xpi, i);
      char s[256];
      OBJ_obj2txt(s, sizeof(s), X509_EXTENSION_get_object(xpiext), 1);
      int crit = X509_EXTENSION_get_critical(xpiext);
      // Notify what we found
      PRINT(i << ": found extension '"<<s<<"', critical: " << crit);
      // Dump its content
      rc = 0;
      XRDGSI_CONST unsigned char *pp = (XRDGSI_CONST unsigned char *) X509_EXTENSION_get_data(xpiext)->data;
      long length = X509_EXTENSION_get_data(xpiext)->length;
      int ret = FillUnknownExt(&pp, length, dumpunknown);
      PRINT("ret: " << ret);
   }

   // Done
   return rc;
}

//____________________________________________________________________________
int XrdCryptosslX509::FillUnknownExt(XRDGSI_CONST unsigned char **pp, long length, bool dump)
{
   // Do the actual filling of the bio; can be called recursevely
   EPNAME("FillUnknownExt");

   XRDGSI_CONST unsigned char *p,*ep,*tot,*op,*opp;
   long len;
   int tag, xclass, ret = 0;
   int nl,hl,j,r;
   ASN1_OBJECT *o = 0;
   ASN1_OCTET_STRING *os = 0;
   /* ASN1_BMPSTRING *bmp=NULL;*/
   int dump_indent = 6;
   int depth = 0;
   int indent = 0;

   p = *pp;
   tot = p + length;
   op = p - 1;
   while ((p < tot) && (op < p)) {
      op = p;
      j = ASN1_get_object(&p, &len, &tag, &xclass, length);
#ifdef LINT
      j = j;
#endif
      if (j & 0x80) {
         if (dump) PRINT("ERROR: error in encoding");
         ret = 0;
         goto end;
      }
      hl = (p-op);
      length -= hl;
      /* if j == 0x21 it is a constructed indefinite length object */

      if (j != (V_ASN1_CONSTRUCTED | 1)) {
         if (dump) PRINT("PRIM:  d="<<depth<<" hl="<<hl<<" l="<<len);
      } else {
         if (dump) PRINT("CONST: d="<<depth<<" hl="<<hl<<" l=inf  ");
      }
      if (!Asn1PrintInfo(tag, xclass, j, (indent) ? depth : 0))
         goto end;
      if (j & V_ASN1_CONSTRUCTED) {
         ep = p + len;
         if (dump) PRINT(" ");
         if (len > length) {
            if (dump) PRINT("ERROR:CONST: length is greater than " <<length);
            ret=0;
            goto end;
         }
         if ((j == 0x21) && (len == 0)) {
            for (;;) {
               r = FillUnknownExt(&p, (long)(tot-p), dump);
               if (r == 0) {
                  ret = 0;
                  goto end;
               }
               if ((r == 2) || (p >= tot))
                  break;
            }
         } else {
            while (p < ep) {
               r = FillUnknownExt(&p, (long)len, dump);
               if (r == 0) {
                  ret = 0;
                  goto end;
               }
            }
         }
      } else if (xclass != 0) {
         p += len;
         if (dump) PRINT(" ");
      } else {
         nl = 0;
         if ((tag == V_ASN1_PRINTABLESTRING) ||
             (tag == V_ASN1_T61STRING) ||
             (tag == V_ASN1_IA5STRING) ||
             (tag == V_ASN1_VISIBLESTRING) ||
             (tag == V_ASN1_NUMERICSTRING) ||
             (tag == V_ASN1_UTF8STRING) ||
             (tag == V_ASN1_UTCTIME) ||
             (tag == V_ASN1_GENERALIZEDTIME)) {
            if (len > 0) {
               char *s = new char[len + 1];
               memcpy(s, p, len);
               s[len] = 0;
               if (dump) PRINT("GENERIC:" << s <<" (len: "<<(int)len<<")");
               delete [] s;
            } else {
               if (dump) PRINT("GENERIC: (len: "<<(int)len<<")");
            }
         } else if (tag == V_ASN1_OBJECT) {
            opp = op;
            if (d2i_ASN1_OBJECT(&o, &opp, len+hl)) {
               BIO *mem = BIO_new(BIO_s_mem());
               i2a_ASN1_OBJECT(mem, o);
               XrdOucString objstr;
               if (dump) { BIO_PRINT(mem, "AOBJ:"); }
            } else {
               if (dump) PRINT("ERROR:AOBJ: BAD OBJECT");
            }
         } else if (tag == V_ASN1_BOOLEAN) {
            if (len != 1) {
               if (dump) PRINT("ERROR:BOOL: Bad boolean");
               goto end;
            }
            if (dump) PRINT("BOOL:"<< p[0]);
         } else if (tag == V_ASN1_BMPSTRING) {
            /* do the BMP thang */
         } else if (tag == V_ASN1_OCTET_STRING) {
            int i, printable = 1;
            opp = op;
            os = d2i_ASN1_OCTET_STRING(0, &opp, len + hl);
            if (os && os->length > 0) {
               opp = os->data;
               /* testing whether the octet string is * printable */
               for (i=0; i<os->length; i++) {
                  if (( (opp[i] < ' ') && (opp[i] != '\n') &&
                        (opp[i] != '\r') && (opp[i] != '\t')) || (opp[i] > '~')) {
                     printable = 0;
                     break;
                  }
               }
               if (printable) {
                  /* printable string */
                  char *s = new char[os->length + 1];
                  memcpy(s, opp, os->length);
                  s[os->length] = 0;
                  if (dump) PRINT("OBJS:" << s << " (len: "<<os->length<<")");
                  delete [] s;
               } else {
                  /* print the normal dump */
                  if (!nl) PRINT("OBJS:");
                  BIO *mem = BIO_new(BIO_s_mem());
                  if (BIO_dump_indent(mem, (const char *)opp, os->length, dump_indent) <= 0) {
                     if (dump) PRINT("ERROR:OBJS: problems dumping to BIO");
                     BIO_free(mem);                  
                     goto end;
                  }
                  if (dump) { BIO_PRINT(mem, "OBJS:"); }
                  nl = 1;
               }
            }
            if (os) {
               ASN1_OCTET_STRING_free(os);
               os = 0;
            }
         } else if (tag == V_ASN1_INTEGER) {
            ASN1_INTEGER *bs;
            int i;

            opp = op;
            bs = d2i_ASN1_INTEGER(0, &opp, len+hl);
            if (bs) {
               if (dump) PRINT("AINT:");
               if (bs->type == V_ASN1_NEG_INTEGER)
                  if (dump) PRINT("-");
               BIO *mem = BIO_new(BIO_s_mem());
               for (i = 0; i < bs->length; i++) {
                  if (BIO_printf(mem, "%02X", bs->data[i]) <= 0) {
                     if (dump) PRINT("ERROR:AINT: problems printf-ing to BIO");
                     BIO_free(mem); 
                     goto end;
                  }
               }
               if (dump) { BIO_PRINT(mem, "AINT:"); }
               if (bs->length == 0) PRINT("00");
            } else {
               if (dump) PRINT("ERROR:AINT: BAD INTEGER");
            }
            ASN1_INTEGER_free(bs);
         } else if (tag == V_ASN1_ENUMERATED) {
            ASN1_ENUMERATED *bs;
            int i;

            opp = op;
            bs = d2i_ASN1_ENUMERATED(0, &opp, len+hl);
            if (bs) {
               if (dump) PRINT("AENU:");
               if (bs->type == V_ASN1_NEG_ENUMERATED)
                  if (dump) PRINT("-");
               BIO *mem = BIO_new(BIO_s_mem());
               for (i = 0; i < bs->length; i++) {
                  if (BIO_printf(mem, "%02X", bs->data[i]) <= 0) {
                     if (dump) PRINT("ERROR:AENU: problems printf-ing to BIO");
                     BIO_free(mem); 
                     goto end;
                  }
               }
               if (dump) { BIO_PRINT(mem, "AENU:"); }
               if (bs->length == 0) PRINT("00");
            } else {
               if (dump) PRINT("ERROR:AENU: BAD ENUMERATED");
            }
            ASN1_ENUMERATED_free(bs);
         }

         if (!nl && dump) PRINT(" ");

         p += len;
         if ((tag == V_ASN1_EOC) && (xclass == 0)) {
            ret = 2; /* End of sequence */
            goto end;
         }
      }
      length -= len;
   }
   ret = 1;
end:
   if (o) ASN1_OBJECT_free(o);
   if (os) ASN1_OCTET_STRING_free(os);
   *pp = p;
   if (dump) PRINT("ret: "<<ret);

   return ret;
}

//____________________________________________________________________________
int XrdCryptosslX509::Asn1PrintInfo(int tag, int xclass, int constructed, int indent)
{
   // Print the BIO content
   EPNAME("Asn1PrintInfo");

   static const char fmt[]="%-18s";
   static const char fmt2[]="%2d %-15s";
   char str[128];
   const char *p, *p2 = 0;

   BIO *bp = BIO_new(BIO_s_mem());
   if (constructed & V_ASN1_CONSTRUCTED)
      p = "cons: ";
   else
      p = "prim: ";
   if (BIO_write(bp, p, 6) < 6)
      goto err;
   BIO_indent(bp, indent, 128);

   p = str;
   if ((xclass & V_ASN1_PRIVATE) == V_ASN1_PRIVATE)
      BIO_snprintf(str,sizeof str,"priv [ %d ] ",tag);
   else if ((xclass & V_ASN1_CONTEXT_SPECIFIC) == V_ASN1_CONTEXT_SPECIFIC)
      BIO_snprintf(str,sizeof str,"cont [ %d ]",tag);
   else if ((xclass & V_ASN1_APPLICATION) == V_ASN1_APPLICATION)
      BIO_snprintf(str,sizeof str,"appl [ %d ]",tag);
   else if (tag > 30)
      BIO_snprintf(str,sizeof str,"<ASN1 %d>",tag);
   else
      p = ASN1_tag2str(tag);

   if (p2) {
      if (BIO_printf(bp,fmt2,tag,p2) <= 0)
         goto err;
   } else {
      if (BIO_printf(bp, fmt, p) <= 0)
         goto err;
   }
   BIO_PRINT(bp, "A1PI:");
   return(1);
err:
   BIO_free(bp);
   return(0);
}

//____________________________________________________________________________
bool XrdCryptosslX509::MatchesSAN(const char *fqdn, bool &hasSAN)
{
   EPNAME("MatchesSAN");

   // Statically allocated array for hostname lengths.  RFC1035 limits
   // valid lengths to 255 characters.
   char san_fqdn[256];

   // Assume we have no SAN extension. Failure may allow the caller to try
   // using the common name before giving up.
   hasSAN = false;

   GENERAL_NAMES *gens = static_cast<GENERAL_NAMES *>(X509_get_ext_d2i(cert,
      NID_subject_alt_name, NULL, NULL));
   if (!gens)
      return false;

   // Only an EEC is usable as a host certificate.
   if (type != kEEC)
      return false;

   // All failures are under the notion that we have a SAN extension.
   hasSAN = true;

   if (!fqdn)
      return false;

   bool success = false;
   for (int idx = 0; idx < sk_GENERAL_NAME_num(gens); idx++) {
      GENERAL_NAME *gen;
      ASN1_STRING *cstr;
      gen = sk_GENERAL_NAME_value(gens, idx);
      if (gen->type != GEN_DNS)
         continue;
      cstr = gen->d.dNSName;
      if (ASN1_STRING_type(cstr) != V_ASN1_IA5STRING)
         continue;
      int san_fqdn_len = ASN1_STRING_length(cstr);
      if (san_fqdn_len > 255)
         continue;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      memcpy(san_fqdn, ASN1_STRING_get0_data(cstr), san_fqdn_len);
#else
      memcpy(san_fqdn, ASN1_STRING_data(cstr), san_fqdn_len);
#endif
      san_fqdn[san_fqdn_len] = '\0';
      if (strlen(san_fqdn) != static_cast<size_t>(san_fqdn_len)) // Avoid embedded null's.
         continue;
      DEBUG("Comparing SAN " << san_fqdn << " with " << fqdn);
      if (MatchHostnames(san_fqdn, fqdn)) {
         DEBUG("SAN " << san_fqdn << " matches with " << fqdn);
         success = true;
         break;
      }
   }
   sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
   return success;
}
