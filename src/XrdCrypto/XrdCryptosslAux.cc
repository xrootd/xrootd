/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o S s l A u x . c c                       */
/*                                                                            */
/* (c) 2005 G. Ganis, CERN                                                    */
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
/* OpenSSL utility functions                                                  */
/*                                                                            */
/* ************************************************************************** */
#include <ctime>
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslRSA.hh"
#include "XrdCrypto/XrdCryptosslX509.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"
#include "XrdTls/XrdTlsPeerCerts.hh"
#include <openssl/pem.h>

// Error code from verification set by verify callback function
static int gErrVerifyChain = 0;
XrdOucTrace *sslTrace = 0;

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

//____________________________________________________________________________
int XrdCryptosslX509VerifyCB(int ok, X509_STORE_CTX *ctx)
{
   // Verify callback function

   // Reset global error
   gErrVerifyChain = 0;

   if (ok != 0) {

      // Error analysis
      gErrVerifyChain = 1;
   }

   // We are done
   return ok;
}

//____________________________________________________________________________
int XrdCryptosslKDFunLen()
{
   // default buffer length
   return kSslKDFunDefLen;
}

//____________________________________________________________________________
int XrdCryptosslKDFun(const char *pass, int plen, const char *salt, int slen,
                      char *key, int klen)
{
   // Password-Based Key Derivation Function 2, specified in PKCS #5
   // Following (J.Viega, M.Messier, "Secure programming Cookbook", p.141),
   // the default number of iterations is set to 10000 .
   // It can be specified at the beginning of the salt using a construct
   // like this: salt = "$$<number_of_iterations>$<effective_salt>"
  
   klen = (klen <= 0) ? 24 : klen;

   // Defaults
   char *realsalt = (char *)salt;
   int realslen = slen;
   int it = 10000;
   //
   // Extract iteration number from salt, if any
   char *ibeg = (char *)memchr(salt+1,'$',slen-1);
   if (ibeg) {
      char *del = 0;
      int newit = strtol(ibeg+1, &del, 10);
      if (newit > 0 && del[0] == '$' && errno != ERANGE) {
         // found iteration number
         it = newit;
         realsalt = del+1;
         realslen = slen - (int)(realsalt-salt);
      }
   }

   PKCS5_PBKDF2_HMAC_SHA1(pass, plen,
                         (unsigned char *)realsalt, realslen, it,
                          klen, (unsigned char *)key);
   return klen;
}

//____________________________________________________________________________
bool XrdCryptosslX509VerifyCert(XrdCryptoX509 *cert, XrdCryptoX509 *ref)
{
   // Verify signature of cert using public key of ref

   // Input must make sense
   X509 *c = cert ? (X509 *)(cert->Opaque()) : 0;
   X509 *r = ref ? (X509 *)(ref->Opaque()) : 0;
   EVP_PKEY *rk = r ? X509_get_pubkey(r) : 0;
   if (!c || !rk) return 0;

   // Ok: we can verify
   return (X509_verify(c, rk) > 0);
}

//____________________________________________________________________________
bool XrdCryptosslX509VerifyChain(XrdCryptoX509Chain *chain, int &errcode)
{
   // Verifies crossed signatures of X509 certificate 'chain'
   // In case of failure, and error code is returned in errcode.

   // Make sure we got a potentially meaningful chain
   if (!chain || chain->Size() <= 1)
      return 0;

   // Create a store
   X509_STORE *store = X509_STORE_new();
   if (!store)
      return 0;

   // Set the verify callback function
   X509_STORE_set_verify_cb_func(store, 0);

   // Add the first (the CA) certificate
   XrdCryptoX509 *cert = chain->Begin();
   if (cert->type != XrdCryptoX509::kCA && cert->Opaque())
      return 0;
   X509_STORE_add_cert(store, (X509 *)(cert->Opaque()));

   // Create a stack
   STACK_OF(X509) *stk = sk_X509_new_null();
   if (!stk)
      return 0;

   // Fill it with chain we have
   X509 *cref = 0;
   while ((cert = chain->Next()) && cert->Opaque()) {
      if (!cref)
         cref = (X509 *)(cert->Opaque());
      sk_X509_push(stk, (X509 *)(cert->Opaque()));
   }

   // Make sure all the certificates have been inserted
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   if (sk_X509_num(stk) != chain->Size() - 1)
#else /* OPENSSL */
   if (sk_num(stk) != chain->Size() - 1)
#endif /* OPENSSL */
      return 0;

   // Create a store ctx ...
   X509_STORE_CTX *ctx = X509_STORE_CTX_new();
   if (!ctx)
      return 0;

   // ... and initialize it
   X509_STORE_CTX_init(ctx, store, cref, stk);

   // verify ?
   bool verify_ok = (X509_verify_cert(ctx) == 1);

   // Fill error code, if any
   errcode = 0;
   if (!verify_ok)
      errcode = gErrVerifyChain;

   // Free context, stack, and store
   X509_STORE_CTX_free(ctx);
   sk_X509_pop_free(stk, X509_free);
   X509_STORE_free(store);

   return verify_ok;
}

//____________________________________________________________________________
XrdSutBucket *XrdCryptosslX509ExportChain(XrdCryptoX509Chain *chain,
                                          bool withprivatekey)
{
   // Export non-CA content of 'chain' into a bucket for transfer.
   EPNAME("X509ExportChain");
   XrdSutBucket *bck = 0;

   // Make sure we got something to export
   if (!chain || chain->Size() <= 0) {
      DEBUG("chain undefined or empty: nothing to export");
      return bck;
   }

   // Do not export CA selfsigned certificates
   if (chain->Size() == 1 && chain->Begin()->type == XrdCryptoX509::kCA &&
       !strcmp(chain->Begin()->IssuerHash(),chain->Begin()->SubjectHash())) {
      DEBUG("chain contains only a CA certificate: nothing to export");
      return bck;
   }

   // Now we create a bio_mem to serialize the certificates
   BIO *bmem = BIO_new(BIO_s_mem());
   if (!bmem) {
      DEBUG("unable to create BIO for memory operations");
      return bck;
   }

   // Reorder the chain
   chain->Reorder();

   // Write the last cert first
   XrdCryptoX509 *c = chain->End();
   if (!PEM_write_bio_X509(bmem, (X509 *)c->Opaque())) {
      DEBUG("error while writing proxy certificate"); 
      BIO_free(bmem);
      return bck;
   }
   // Write its private key, if any and if asked
   if (withprivatekey) {
      XrdCryptoRSA *k = c->PKI();
      if (k->status == XrdCryptoRSA::kComplete) {
         if (!PEM_write_bio_PrivateKey(bmem, (EVP_PKEY *)(k->Opaque()),
                                  0, 0, 0, 0, 0)) {
            DEBUG("error while writing proxy private key"); 
            BIO_free(bmem);
            return bck;
         }
      }
   }
   // Now write all other certificates (except selfsigned CAs ...)
   while ((c = chain->SearchBySubject(c->Issuer()))) {
      if (c->type == XrdCryptoX509::kCA) {
         DEBUG("Encountered CA in chain; breaking.  Subject: " << c->Subject());
         break;
      }
      if (strcmp(c->IssuerHash(), c->SubjectHash())) {
         // Write to bucket
         if (!PEM_write_bio_X509(bmem, (X509 *)c->Opaque())) {
            DEBUG("error while writing proxy certificate"); 
            BIO_free(bmem);
            return bck;
         }
      } else {
         DEBUG("Encountered self-signed CA in chain; breaking.  Subject: " << c->Subject());
         break;
      }
   }

   // Extract pointer to BIO data and length of segment
   char *bdata = 0;  
   int blen = BIO_get_mem_data(bmem, &bdata);
   DEBUG("BIO data: "<<blen<<" bytes at 0x"<<(int *)bdata);

   // create the bucket now
   bck = new XrdSutBucket(0, 0, kXRS_x509);
   if (bck) {
      // Fill bucket
      bck->SetBuf(bdata, blen);
      DEBUG("result of serialization: "<<bck->size<<" bytes");
   } else {
      DEBUG("unable to create bucket for serialized format");
      BIO_free(bmem);
      return bck;
   }
   //
   // Free BIO
   BIO_free(bmem);
   //
   // We are done
   return bck;
}

//____________________________________________________________________________
int XrdCryptosslX509ToFile(XrdCryptoX509 *x509, FILE *file, const char *fname)
{
   // Dump a single X509 certificate to a file in PEM format.
   EPNAME("X509ChainToFile");

   // Check inputs
   if (!x509 || !file) {
      DEBUG("Invalid inputs");
      return -1;
   }

   if (PEM_write_X509(file, (X509 *)x509->Opaque()) != 1) {
      DEBUG("error while writing certificate " << fname);
      return -1;
   }

   return 0;
}

//____________________________________________________________________________
int XrdCryptosslX509ChainToFile(XrdCryptoX509Chain *ch, const char *fn)
{
   // Dump non-CA content of chain 'c' into file 'fn'
   EPNAME("X509ChainToFile");

   // Check inputs
   if (!ch || !fn) {
      DEBUG("Invalid inputs");
      return -1;
   }

   // We proceed only if we can lock for write
   FILE *fp = fopen(fn,"w");
   if (!fp) {
      DEBUG("cannot open file to save chain (file: "<<fn<<")"); 
      return -1;
   }
   int ifp = fileno(fp);
   if (ifp == -1) {
      DEBUG("got invalid file descriptor (file: "<<fn<<")"); 
      fclose(fp);
      return -1;
   }

   // We need to lock from now on
   {  XrdSutFileLocker fl(ifp,XrdSutFileLocker::kExcl);

      // If not successful, return
      if (!fl.IsValid()) { 
         DEBUG("could not lock file: "<<fn<<")"); 
         fclose(fp);
         return -1;
      }

      // Set permissions to 0600
      if (fchmod(ifp, 0600) == -1) {
         DEBUG("cannot set permissions on file: "<<fn<<" (errno: "<<errno<<")"); 
         fclose(fp);
         return -1;
      }

      // Reorder the chain
      ch->Reorder();

      // Write the last cert first
      XrdCryptoX509 *c = ch->End();
      if (PEM_write_X509(fp, (X509 *)c->Opaque()) != 1) {
         DEBUG("error while writing proxy certificate"); 
         fclose(fp);
         return -1;
      }
      // Write its private key, if any
      XrdCryptoRSA *k = c->PKI();
      if (k->status == XrdCryptoRSA::kComplete) {
         if (PEM_write_PrivateKey(fp, (EVP_PKEY *)(k->Opaque()),
                                  0, 0, 0, 0, 0) != 1) {
            DEBUG("error while writing proxy private key"); 
            fclose(fp);
            return -1;
         }
      }
      // Now write all other certificates
      while ((c = ch->SearchBySubject(c->Issuer())) && c->type != XrdCryptoX509::kCA) {
         // Write to file
         if (PEM_write_X509(fp, (X509 *)c->Opaque()) != 1) {
            DEBUG("error while writing proxy certificate"); 
            fclose(fp);
            return -1;
         }
      }
   } // Unlocks the file

   // CLose the file
   fclose(fp);
   //
   // We are done
   return 0;
}

//______________________________________________________________________________
int XrdCryptosslX509ParseStack(XrdTlsPeerCerts* pc, XrdCryptoX509Chain *chain)
{
   EPNAME("X509ParseStack");
   int nci = 0;
   // Make sure we got a chain where to add the certificates
   if (!chain) {
      DEBUG("chain undefined: can do nothing");
      return nci;
   }

   if (pc->hasCert()) {
     XrdCryptoX509 *c = new XrdCryptosslX509(pc->getCert());

     if (c) {
       chain->PushBack(c);
       nci ++;
     }
   }

   if (!pc->hasChain()) {
     return nci;
   }

   STACK_OF(X509) *pChain = pc->getChain();

   for (int i=0; i < sk_X509_num(pChain); i++) {
      X509 *cert = sk_X509_value(pChain, i);
      XrdCryptoX509 *c = new XrdCryptosslX509(cert);

      if (c) {
         // The SSL_get_peer_chain method does not increment the
         // refcount; the XrdCryptoX509 object assumes it owns
         // the X509* but also does not increment the refcount.
         // Hence, we increment manually.
#if OPENSSL_VERSION_NUMBER < 0x010100000L
         CRYPTO_add(&(cert->references), 1, CRYPTO_LOCK_X509);
#else
         X509_up_ref(cert);
#endif
         chain->PushBack(c);
      } else {
         X509_free(cert);
         DEBUG("could not create certificate: memory exhausted?");
         chain->Reorder();
         return nci;
      }
      nci ++;
   }
   chain->Reorder();
   return nci;
}

//____________________________________________________________________________
int XrdCryptosslX509ParseFile(const char *fname,
                              XrdCryptoX509Chain *chain, const char *fkey)
{
   EPNAME("X509ParseFile");

   //
   // Open file and read the content:
   // it should contain blocks on information in PEM form
   FILE *fcer = fopen(fname, "r");
   if (!fcer) {
      DEBUG("unable to open file (errno: "<<errno<<")");
      return 0;
   }

   auto retval = XrdCryptosslX509ParseFile(fcer, chain, fname, fkey);
   fclose(fcer);
   return retval;
}

//____________________________________________________________________________
int XrdCryptosslX509ParseFile(FILE *fcer,
                              XrdCryptoX509Chain *chain,
                              const char *fname, const char *fkey)
{
   // Parse content of file 'fname' and add X509 certificates to
   // chain (which must be initialized by the caller).
   // If a private key matching the public key of one of the certificates
   // is found in the file, the certificate key is completed.
   EPNAME("X509ParseFile");
   int nci = 0;

   // Make sure we got a valid file
   if (!fcer) {
      DEBUG("FILE object undefined: can do nothing");
      return nci;
   }

   // Make sure we got a chain where to add the certificates
   if (!chain) {
      DEBUG("chain undefined: can do nothing");
      return nci;
   }

   // Now read out certificates and add them to the chain
   X509 *xcer = 0;
   while (PEM_read_X509(fcer, &xcer, 0, 0)) {
      // Add it to the chain
      XrdCryptoX509 *c = new XrdCryptosslX509(xcer);
      if (c) {
         chain->PushBack(c);
         nci++;
         DEBUG("certificate for '"<<c->Subject()<<"'added to the chain - ord: "<<chain->Size());
      } else {
         DEBUG("could not create certificate: memory exhausted?");
         fclose(fcer);
         return nci;
      }
      xcer = 0;
   }

   // If we found something, and we are asked to extract a key,
   // rewind and look for it
   if (nci) {
      FILE *fcersave = 0;
      if (!fkey) {
         // Look in the same file, after rewinding
         rewind(fcer);
      } else {
         // We can close the file now
         fcersave = fcer;
         // Open key file
         fcer = fopen(fkey, "r");
         if (!fcer) {
           DEBUG("unable to open key file (errno: "<<errno<<")");
           fcer = fcersave;
           return nci;
         }
      }
      EVP_PKEY *rsa = 0;
      if (!PEM_read_PrivateKey(fcer, &rsa, 0, 0)) {
         DEBUG("no RSA private key found in file " << fname);
      } else {
         DEBUG("found a RSA private key in file " << fname);
         // We need to complete the key
         // check all the public keys of the loaded certificates
         if (XrdCheckRSA(rsa) == 1) {
            // Loop over the chain certificates
            XrdCryptoX509 *cert = chain->Begin();
            while (cert && cert->Opaque()) {
               if (cert->type != XrdCryptoX509::kCA) {
                  // Get the public key
                  EVP_PKEY *evpp = X509_get_pubkey((X509 *)(cert->Opaque()));
                  if (evpp) {
                        // Test consistency
#if OPENSSL_VERSION_NUMBER < 0x30000000L
                        int rc = EVP_PKEY_cmp(evpp, rsa);
#else
                        int rc = EVP_PKEY_eq(evpp, rsa);
#endif
                        EVP_PKEY_free(evpp);
                        if (rc == 1) {
                           DEBUG("RSA key completed");
                           // Update PKI in certificate
                           cert->SetPKI((XrdCryptoX509data)rsa);
                           // Update status
                           cert->PKI()->status = XrdCryptoRSA::kComplete;
                           break;
                        }
                  }
               }
               // Get next
               cert = chain->Next();
            }
            if (!cert)
               EVP_PKEY_free(rsa);
         }
         else
            EVP_PKEY_free(rsa);
      }
      if (fkey) {
         // Re-establish original fcer pointer
         fclose(fcer);
         fcer = fcersave;
      }
   }

   // We are done
   return nci;
}

//____________________________________________________________________________
int XrdCryptosslX509ParseBucket(XrdSutBucket *b, XrdCryptoX509Chain *chain)
{
   // Import certificate(s) from bucket b adding them to 'chain'
   // (which must be initialized by the caller).
   EPNAME("X509ParseBucket");
   int nci = 0;

   // Make sure we got something to import
   if (!b || b->size <= 0) {
      DEBUG("bucket undefined or empty: can do nothing");
      return nci;
   }

   // Make sure we got a chain where to add the certificates
   if (!chain) {
      DEBUG("chain undefined: can do nothing");
      return nci;
   }
   //
   // Now we create a bio_mem to store the certificates
   BIO *bmem = BIO_new(BIO_s_mem());
   if (!bmem) {
      DEBUG("unable to create BIO to import certificates");
      return nci;
   }

   // Write data to BIO
   if (BIO_write(bmem,(const void *)(b->buffer),b->size) != b->size) {
      DEBUG("problems writing data to BIO");
      BIO_free(bmem);
      return nci;
   }

   // Get certificates from BIO
   X509 *xcer = 0;
   while (PEM_read_bio_X509(bmem, &xcer, 0, 0)) {
      //
      // Create container and add to the list
      XrdCryptoX509 *c = new XrdCryptosslX509(xcer);
      if (c) {
         chain->PushBack(c);
         nci++;
         DEBUG("certificate added to the chain - ord: "<<chain->Size());
      } else {
         DEBUG("could not create certificate: memory exhausted?");
         BIO_free(bmem);
         return nci;
      }
      // reset cert otherwise the next one is not fetched
      xcer = 0;
   }

   // If we found something, and we are asked to extract a key,
   // refill the BIO and search again for the key (this is mandatory
   // as read operations modify the BIO contents; a read-only BIO
   // may be more efficient)
   if (nci && BIO_write(bmem,(const void *)(b->buffer),b->size) == b->size) {
      EVP_PKEY *rsa = 0;
      if (!PEM_read_bio_PrivateKey(bmem, &rsa, 0, 0)) {
         DEBUG("no RSA private key found in bucket");
      } else {
         DEBUG("found a RSA private key in bucket");
         // We need to complete the key
         // check all the public keys of the loaded certificates
         if (XrdCheckRSA(rsa) == 1) {
            // Loop over the chain certificates
            XrdCryptoX509 *cert = chain->Begin();
            while (cert && cert->Opaque()) {
               if (cert->type != XrdCryptoX509::kCA) {
                  // Get the public key
                  EVP_PKEY *evpp = X509_get_pubkey((X509 *)(cert->Opaque()));
                  if (evpp) {
                        // Test consistency
#if OPENSSL_VERSION_NUMBER < 0x30000000L
                        int rc = EVP_PKEY_cmp(evpp, rsa);
#else
                        int rc = EVP_PKEY_eq(evpp, rsa);
#endif
                        EVP_PKEY_free(evpp);
                        if (rc == 1) {
                           DEBUG("RSA key completed");
                           // Update PKI in certificate
                           cert->SetPKI((XrdCryptoX509data)rsa);
                           // Update status
                           cert->PKI()->status = XrdCryptoRSA::kComplete;
                           break;
                        }
                  }
               }
               // Get next
               cert = chain->Next();
            }
            if (!cert)
               EVP_PKEY_free(rsa);
         }
         else
            EVP_PKEY_free(rsa);
      }
   }

   // Cleanup
   BIO_free(bmem);

   // We are done
   return nci;
}

//____________________________________________________________________________
time_t XrdCryptosslASN1toUTC(const ASN1_TIME *tsn1)
{
   // Function to convert from ASN1 time format into UTC
   // since Epoch (Jan 1, 1970) 
   // Return -1 if something went wrong
   time_t etime = -1;
// EPNAME("ASN1toUTC");

   //
   // Make sure there is something to convert
   if (!tsn1) return etime;

   //
   // Parse the input string: here we basically cut&paste from GRIDSITE
   // They finally use timegm to convert to UTC seconds, which is less
   // standard and seems to give an offset of 3600 secs.
   // Our result is in agreement with 'date +%s`. 
   struct tm ltm;
   char zz;
   if ((sscanf((const char *)(tsn1->data),
       "%02d%02d%02d%02d%02d%02d%c", 
       &(ltm.tm_year), &(ltm.tm_mon), &(ltm.tm_mday),
       &(ltm.tm_hour), &(ltm.tm_min), &(ltm.tm_sec),
                                      &zz) != 7) || (zz != 'Z')) {
       // Try GeneralizedTime
       if ((sscanf((const char *)(tsn1->data),
           "%04d%02d%02d%02d%02d%02d%c", 
           &(ltm.tm_year), &(ltm.tm_mon), &(ltm.tm_mday),
           &(ltm.tm_hour), &(ltm.tm_min), &(ltm.tm_sec),
                                          &zz) != 7) || (zz != 'Z')) {
           return -1;
       }
   }
   // Init also the ones not used by mktime
   ltm.tm_wday  = 0;        // day of the week 
   ltm.tm_yday  = 0;        // day in the year
   ltm.tm_isdst = 0;        // we will correct with an offset without dst
   //
   // Renormalize some values (year should be modulo 1900), honouring all cases
   if (ltm.tm_year < 50) {
      ltm.tm_year += 2000;
   } else if (ltm.tm_year < 100) {
      ltm.tm_year += 1900;
   }
   ltm.tm_year -= 1900;
   //
   // month should in [0, 11]
   (ltm.tm_mon)--;
   //
   // Calculate as if the UTC stamp was a localtime with no dst
   etime = mktime(&ltm);
   // Correct to UTC
   etime += XrdCryptoTZCorr();
   // Notify, if requested
// DEBUG(" UTC: "<<etime<<"  isdst: "<<ltm.tm_isdst);
   //
   // We are done
   return etime;
} 

//____________________________________________________________________________
void XrdCryptosslNameOneLine(X509_NAME *nm, XrdOucString &s)
{
   // Function to convert X509_NAME into a one-line human readable string

#ifndef USEX509NAMEONELINE
   BIO *mbio = BIO_new(BIO_s_mem());
   X509_NAME_print_ex(mbio, nm, 0, XN_FLAG_SEP_MULTILINE);
   char *data = 0;
   long len = BIO_get_mem_data(mbio, &data);
   s = "/";
   s.insert(data, 1, len);
   BIO_free(mbio);
   s.replace("\n", "/");
#else
   char *xn = X509_NAME_oneline(nm, 0, 0);
   s = xn;
   OPENSSL_free(xn);
#endif

   // Done
   return;
}
