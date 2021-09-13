/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o S s l C i p h e r . c c                 */
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
/* OpenSSL implementation of XrdCryptoCipher                                  */
/*                                                                            */
/* ************************************************************************** */
#include <cstring>

#include "XrdSut/XrdSutRndm.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"
#include "XrdCrypto/XrdCryptosslCipher.hh"

//#include <openssl/dsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/dh.h>

// ---------------------------------------------------------------------------//
//
// Cipher interface
//
// ---------------------------------------------------------------------------//

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static void DH_get0_pqg(const DH *dh,
                        const BIGNUM **p, const BIGNUM **q, const BIGNUM **g)
{
    if (p != NULL)
        *p = dh->p;
    if (q != NULL)
        *q = dh->q;
    if (g != NULL)
        *g = dh->g;
}

static int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
    /* If the fields p and g in d are NULL, the corresponding input
     * parameters MUST be non-NULL.  q may remain NULL.
     */
    if ((dh->p == NULL && p == NULL) || (dh->g == NULL && g == NULL))
        return 0;
    if (p != NULL) {
        BN_free(dh->p);
        dh->p = p;
    }
    if (q != NULL) {
        BN_free(dh->q);
        dh->q = q;
    }
    if (g != NULL) {
        BN_free(dh->g);
        dh->g = g;
    }
    if (q != NULL) {
        dh->length = BN_num_bits(q);
    }
    return 1;
}

static void DH_get0_key(const DH *dh,
                        const BIGNUM **pub_key, const BIGNUM **priv_key)
{
    if (pub_key != NULL)
        *pub_key = dh->pub_key;
    if (priv_key != NULL)
        *priv_key = dh->priv_key;
}

static int DH_set0_key(DH *dh, BIGNUM *pub_key, BIGNUM *priv_key)
{
    /* If the field pub_key in dh is NULL, the corresponding input
     * parameters MUST be non-NULL.  The priv_key field may
     * be left NULL.
     */
    if (dh->pub_key == NULL && pub_key == NULL)
        return 0;
    if (pub_key != NULL) {
        BN_free(dh->pub_key);
        dh->pub_key = pub_key;
    }
    if (priv_key != NULL) {
        BN_free(dh->priv_key);
        dh->priv_key = priv_key;
    }
    return 1;
}

static int DSA_set0_key(DSA *d, BIGNUM *pub_key, BIGNUM *priv_key)
{
    /* If the field pub_key in d is NULL, the corresponding input
     * parameters MUST be non-NULL.  The priv_key field may
     * be left NULL.
     */
    if (d->pub_key == NULL && pub_key == NULL)
        return 0;
    if (pub_key != NULL) {
        BN_free(d->pub_key);
        d->pub_key = pub_key;
    }
    if (priv_key != NULL) {
        BN_free(d->priv_key);
        d->priv_key = priv_key;
    }
    return 1;
}
#endif

#if !defined(HAVE_DH_PADDED)
#if defined(HAVE_DH_PADDED_FUNC)
int DH_compute_key_padded(unsigned char *, const BIGNUM *, DH *);
#else
static int DH_compute_key_padded(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    int rv, pad;
    rv = dh->meth->compute_key(key, pub_key, dh);
    if (rv <= 0)
        return rv;
    pad = BN_num_bytes(dh->p) - rv;
    if (pad > 0) {
        memmove(key + pad, key, rv);
        memset(key, 0, pad);
    }
    return rv + pad;
}
#endif
#endif

//_____________________________________________________________________________
bool XrdCryptosslCipher::IsSupported(const char *cip)
{
   // Check if the specified cipher is supported

   return (EVP_get_cipherbyname(cip) != 0);
}

//____________________________________________________________________________
XrdCryptosslCipher::XrdCryptosslCipher(const char *t, int l)
{
   // Main Constructor
   // Complete initialization of a cipher of type t and length l
   // The initialization vector is also created
   // Used to create ciphers

   valid = 0;
   ctx = 0;
   fIV = 0;
   lIV = 0;
   cipher = 0;
   fDH = 0;
   deflength = 1;

   // Check and set type
   char cipnam[64] = {"bf-cbc"};
   if (t && strcmp(t,"default")) {
      strcpy(cipnam,t);
      cipnam[63] = 0;
   }
   cipher = EVP_get_cipherbyname(cipnam);

   if (cipher) {
      // Determine key length
      l = (l > EVP_MAX_KEY_LENGTH) ? EVP_MAX_KEY_LENGTH : l;
      int ldef = EVP_CIPHER_key_length(cipher);
      int lgen = (l > ldef) ? l : ldef;
      // Generate and set a new key
      char *ktmp = XrdSutRndm::GetBuffer(lgen);
      if (ktmp) {
         // Init context
         ctx = EVP_CIPHER_CTX_new();
         if (ctx) {
            valid = 1;
            // Try setting the key length
            if (l && l != ldef) {
               EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
               EVP_CIPHER_CTX_set_key_length(ctx,l);
               EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)ktmp, 0, 1);
               if (l == EVP_CIPHER_CTX_key_length(ctx)) {
                  // Use the l bytes at ktmp
                  SetBuffer(l,ktmp);
                  deflength = 0;
               }
            }
            if (!Length()) {
               EVP_CipherInit_ex(ctx, cipher, 0, (unsigned char *)ktmp, 0, 1);
               SetBuffer(ldef,ktmp);
            }
            // Set also the type
            SetType(cipnam);
         }
         // Cleanup
         delete[] ktmp;
      }
   }

   // Finally, generate and set a new IV
   if (valid)
      GenerateIV();
}

//____________________________________________________________________________
XrdCryptosslCipher::XrdCryptosslCipher(const char *t, int l,
                                       const char *k, int liv, const char *iv)
{
   // Constructor.
   // Initialize a cipher of type t and length l using the key at k and
   // the initialization vector at iv.
   // Used to import ciphers.
   valid = 0;
   ctx = 0;
   fIV = 0;
   lIV = 0;
   fDH = 0;
   cipher = 0;
   deflength = 1;

   // Check and set type
   char cipnam[64] = {"bf-cbc"};
   if (t && strcmp(t,"default")) {
      strcpy(cipnam,t);
      cipnam[63] = 0;
   }
   cipher = EVP_get_cipherbyname(cipnam);

   if (cipher) {
      // Init context
      ctx = EVP_CIPHER_CTX_new();
      if (ctx) {
         // Set the key
         SetBuffer(l,k);
         if (l != EVP_CIPHER_key_length(cipher))
            deflength = 0;
         // Set also the type
         SetType(cipnam);
         // Set validity flag
         valid = 1;
      }
   }
   //
   // Init cipher
   if (valid) {
      //
      // Set the IV
      SetIV(liv,iv);

      if (deflength) {
         EVP_CipherInit_ex(ctx, cipher, 0, (unsigned char *)Buffer(), 0, 1);
      } else {
         EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
         EVP_CIPHER_CTX_set_key_length(ctx,Length());
         EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)Buffer(), 0, 1);
      }
   }
}

//____________________________________________________________________________
XrdCryptosslCipher::XrdCryptosslCipher(XrdSutBucket *bck)
{
   // Constructor from bucket.
   // Initialize a cipher of type t and length l using the key at k
   // Used to import ciphers.
   valid = 0;
   ctx = 0;
   fIV = 0;
   lIV = 0;
   fDH = 0;
   cipher = 0;
   deflength = 1;

   if (bck && bck->size > 0) {

      valid = 1;

      kXR_int32 ltyp = 0;
      kXR_int32 livc = 0;
      kXR_int32 lbuf = 0;
      kXR_int32 lp = 0;
      kXR_int32 lg = 0;
      kXR_int32 lpub = 0;
      kXR_int32 lpri = 0;
      char *bp = bck->buffer;
      int cur = 0;
      memcpy(&ltyp,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      memcpy(&livc,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      memcpy(&lbuf,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      memcpy(&lp,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      memcpy(&lg,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      memcpy(&lpub,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      memcpy(&lpri,bp+cur,sizeof(kXR_int32));
      cur += sizeof(kXR_int32);
      // Type
      if (ltyp) {
         char *buf = new char[ltyp+1];
         if (buf) {
            memcpy(buf,bp+cur,ltyp);
            buf[ltyp] = 0;
            cipher = EVP_get_cipherbyname(buf);
            if (!cipher)
               cipher = EVP_get_cipherbyname("bf-cbc");
            if (cipher) {
               // Set the type
               SetType(buf);
            } else {
               valid = 0;
            }
            delete[] buf;
         } else
            valid = 0;
         cur += ltyp;
      }
      // IV
      if (livc) {
         char *buf = new char[livc];
         if (buf) {
            memcpy(buf,bp+cur,livc);
            cur += livc;
            // Set the IV
            SetIV(livc,buf);
            delete[] buf;
         } else
            valid = 0;
         cur += livc;
      }
      // buffer
      if (lbuf) {
         char *buf = new char[lbuf];
         if (buf) {
            memcpy(buf,bp+cur,lbuf);
            // Set the buffer
            UseBuffer(lbuf,buf);
            if (cipher && lbuf != EVP_CIPHER_key_length(cipher))
               deflength = 0;
         } else
            valid = 0;
         cur += lbuf;
      }
      // DH, if any
      if (lp > 0 || lg > 0 || lpub > 0 || lpri > 0) {
         if ((fDH = DH_new())) {
            char *buf = 0;
            BIGNUM *p = NULL, *g = NULL;
            BIGNUM *pub = NULL, *pri = NULL;
            // p
            if (lp > 0) {
               buf = new char[lp+1];
               if (buf) {
                  memcpy(buf,bp+cur,lp);
                  buf[lp] = 0;
                  BN_hex2bn(&p,buf);
                  delete[] buf;
               } else
                  valid = 0;
               cur += lp;
            }
            // g
            if (lg > 0) {
               buf = new char[lg+1];
               if (buf) {
                  memcpy(buf,bp+cur,lg);
                  buf[lg] = 0;
                  BN_hex2bn(&g,buf);
                  delete[] buf;
               } else
                  valid = 0;
               cur += lg;
            }
            DH_set0_pqg(fDH, p, NULL, g);
            // pub_key
            if (lpub > 0) {
               buf = new char[lpub+1];
               if (buf) {
                  memcpy(buf,bp+cur,lpub);
                  buf[lpub] = 0;
                  BN_hex2bn(&pub,buf);
                  delete[] buf;
               } else
                  valid = 0;
               cur += lpub;
            }
            // priv_key
            if (lpri > 0) {
               buf = new char[lpri+1];
               if (buf) {
                  memcpy(buf,bp+cur,lpri);
                  buf[lpri] = 0;
                  BN_hex2bn(&pri,buf);
                  delete[] buf;
               } else
                  valid = 0;
               cur += lpri;
            }
            DH_set0_key(fDH, pub, pri);
            int dhrc = 0;
            DH_check(fDH,&dhrc);
            if (dhrc == 0)
               valid = 1;
         } else
            valid = 0;
      }
   }
   //
   // Init cipher
   if (valid) {
      // Init context
      ctx = EVP_CIPHER_CTX_new();
      if (ctx) {
         if (deflength) {
            EVP_CipherInit_ex(ctx, cipher, 0, (unsigned char *)Buffer(), 0, 1);
         } else {
            EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
            EVP_CIPHER_CTX_set_key_length(ctx,Length());
            EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)Buffer(), 0, 1);
         }
      } else
         valid = 0;
   }
   if (!valid) {
      Cleanup();
   }
}

//____________________________________________________________________________
XrdCryptosslCipher::XrdCryptosslCipher(bool padded, int bits, char *pub,
                                       int lpub, const char *t)
{
   // Constructor for key agreement.
   // If pub is not defined, generates a DH full key,
   // the public part and parameters can be retrieved using Public().
   // The number of random bits to be used in 'bits'.
   // If pub is defined with the public part and parameters of the
   // counterpart fully initialize a cipher with that information.
   // Sets also the name to 't', if different from the default one.
   // Used for key agreement.
   EPNAME("sslCipher::XrdCryptosslCipher");

   valid = 0;
   ctx = 0;
   fIV = 0;
   lIV = 0;
   fDH = 0;
   cipher = 0;
   deflength = 1;

   if (!pub) {
      DEBUG("generate DH full key");
      //
      // at least 128 bits
      bits = (bits < kDHMINBITS) ? kDHMINBITS : bits;
      //
      // Generate params for DH object
      fDH = DH_new();
      if (fDH && DH_generate_parameters_ex(fDH, bits, DH_GENERATOR_5, NULL)) {
         int prc = 0;
         DH_check(fDH,&prc);
         if (prc == 0) {
            //
            // Generate DH key
            if (DH_generate_key(fDH)) {
               // Init context
               ctx = EVP_CIPHER_CTX_new();
               if (ctx)
                  valid = 1;
            }
         }
      }

   } else {
      DEBUG("initialize cipher from key-agreement buffer");
      //
      char *ktmp = 0;
      int ltmp = 0;
      // Extract string with bignumber
      BIGNUM *bnpub = 0;
      char *pb = strstr(pub,"---BPUB---");
      char *pe = strstr(pub,"---EPUB--"); // one less (pub not null-terminated)
      if (pb && pe) {
         lpub = (int)(pb-pub);
         pb += 10;
         *pe = 0;
         BN_hex2bn(&bnpub, pb);
         *pe = '-';
      }
      if (bnpub) {
         //
         // Prepare to decode the input buffer
         BIO *biop = BIO_new(BIO_s_mem());
         if (biop) {
            //
            // Write buffer into BIO
            BIO_write(biop,pub,lpub);
            //
            // Create a key object
            if ((fDH = DH_new())) {
               //
               // Read parms from BIO
               PEM_read_bio_DHparams(biop,&fDH,0,0);
               int prc = 0;
               DH_check(fDH,&prc);
               if (prc == 0) {
                  //
                  // generate DH key
                  if (DH_generate_key(fDH)) {
                     // Now we can compute the cipher
                     ktmp = new char[DH_size(fDH)];
                     memset(ktmp, 0, DH_size(fDH));
                     if (ktmp) {
                        if (padded) {
                           ltmp = DH_compute_key_padded((unsigned char *)ktmp,bnpub,fDH);
                        } else {
                           ltmp = DH_compute_key((unsigned char *)ktmp,bnpub,fDH);
                        }
                        if (ltmp > 0) valid = 1;
                     }
                  }
               }
            }
            BIO_free(biop);
         }
         BN_free( bnpub );
      }
      //
      // If a valid key has been computed, set the cipher
      if (valid) {
         // Init context
         ctx = EVP_CIPHER_CTX_new();
         if (ctx) {
            // Check and set type
            char cipnam[64] = {"bf-cbc"};
            if (t && strcmp(t,"default")) {
               strcpy(cipnam,t);
               cipnam[63] = 0;
            }
            if ((cipher = EVP_get_cipherbyname(cipnam))) {
               // At most EVP_MAX_KEY_LENGTH bytes
               ltmp = (ltmp > EVP_MAX_KEY_LENGTH) ? EVP_MAX_KEY_LENGTH : ltmp;
               int ldef = EVP_CIPHER_key_length(cipher);
               // Try setting the key length
               if (ltmp != ldef) {
                  EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
                  EVP_CIPHER_CTX_set_key_length(ctx,ltmp);
                  EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)ktmp, 0, 1);
                  if (ltmp == EVP_CIPHER_CTX_key_length(ctx)) {
                     // Use the ltmp bytes at ktmp
                     SetBuffer(ltmp,ktmp);
                     deflength = 0;
                  }
               }
               if (!Length()) {
                  EVP_CipherInit_ex(ctx, cipher, 0, (unsigned char *)ktmp, 0, 1);
                  SetBuffer(ldef,ktmp);
               }
               // Set also the type
               SetType(cipnam);
            }
         } else
           valid = 0;
      }
      // Cleanup
      if (ktmp) {delete[] ktmp; ktmp = 0;}
   }

   // Cleanup, if invalid
   if (!valid)
      Cleanup();
}

//____________________________________________________________________________
XrdCryptosslCipher::XrdCryptosslCipher(const XrdCryptosslCipher &c)
                   : XrdCryptoCipher()
{
   // Copy Constructor

   // Basics
   deflength = c.deflength;
   valid = c.valid;
   ctx = 0;
   // IV
   lIV = 0;
   fIV = 0;
   SetIV(c.lIV,c.fIV);

   // Cipher
   cipher = c.cipher;
   // Set the key
   SetBuffer(c.Length(),c.Buffer());
   // Set also the type
   SetType(c.Type());
   // DH
   fDH = 0;
   if (valid && c.fDH) {
      valid = 0;
      if ((fDH = DH_new())) {
         const BIGNUM *p, *g;
         DH_get0_pqg(c.fDH, &p, NULL, &g);
         DH_set0_pqg(fDH, p ? BN_dup(p) : NULL, NULL, g ? BN_dup(g) : NULL);
         const BIGNUM *pub, *pri;
         DH_get0_key(c.fDH, &pub, &pri);
         DH_set0_key(fDH, pub ? BN_dup(pub) : NULL, pri ? BN_dup(pri) : NULL);
         int dhrc = 0;
         DH_check(fDH,&dhrc);
         if (dhrc == 0)
            valid = 1;
      }
   }
   if (valid) {
      // Init context
      ctx = EVP_CIPHER_CTX_new();
      if (!ctx)
         valid = 0;
   }
   if (!valid) {
      Cleanup();
   }
}

//____________________________________________________________________________
XrdCryptosslCipher::~XrdCryptosslCipher()
{
   // Destructor.

   // Cleanup IV
   if (fIV)
      delete[] fIV;

   // Cleanups
   if (valid)
      EVP_CIPHER_CTX_free(ctx);
   Cleanup();
}

//____________________________________________________________________________
void XrdCryptosslCipher::Cleanup()
{
   // Cleanup temporary memory

   // Cleanup IV
   if (fDH) {
      DH_free(fDH);
      fDH = 0;
   }
}

//____________________________________________________________________________
bool XrdCryptosslCipher::Finalize(bool padded,
                                  char *pub, int /*lpub*/, const char *t)
{
   // Finalize cipher during key agreement. Should be called
   // for a cipher build with special constructor defining member fDH.
   // The buffer pub should contain the public part of the counterpart.
   // Sets also the name to 't', if different from the default one.
   // Used for key agreement.
   EPNAME("sslCipher::Finalize");

   if (!fDH) {
      DEBUG("DH undefined: this cipher cannot be finalized"
            " by this method");
      return 0;
   }

   char *ktmp = 0;
   int ltmp = 0;
   valid = 0;
   if (pub) {
      //
      // Extract string with bignumber
      BIGNUM *bnpub = 0;
      char *pb = strstr(pub,"---BPUB---");
      char *pe = strstr(pub,"---EPUB--");
      if (pb && pe) {
         //lpub = (int)(pb-pub);
         pb += 10;
         *pe = 0;
         BN_hex2bn(&bnpub, pb);
         *pe = '-';
      }
      if (bnpub) {
         // Now we can compute the cipher
         ktmp = new char[DH_size(fDH)];
         memset(ktmp, 0, DH_size(fDH));
         if (ktmp) {
            if (padded) {
               ltmp = DH_compute_key_padded((unsigned char *)ktmp,bnpub,fDH);
            } else {
               ltmp = DH_compute_key((unsigned char *)ktmp,bnpub,fDH);
            }
            if (ltmp > 0) valid = 1;
         }
         BN_free(bnpub);
         bnpub=0;
      }
      //
      // If a valid key has been computed, set the cipher
      if (valid) {
         // Check and set type
         char cipnam[64] = {"bf-cbc"};
         if (t && strcmp(t,"default")) {
            strcpy(cipnam,t);
            cipnam[63] = 0;
         }
         if ((cipher = EVP_get_cipherbyname(cipnam))) {
            // At most EVP_MAX_KEY_LENGTH bytes
            ltmp = (ltmp > EVP_MAX_KEY_LENGTH) ? EVP_MAX_KEY_LENGTH : ltmp;
            int ldef = EVP_CIPHER_key_length(cipher);
            // Try setting the key length
            if (ltmp != ldef) {
               EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
               EVP_CIPHER_CTX_set_key_length(ctx,ltmp);
               EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)ktmp, 0, 1);
               if (ltmp == EVP_CIPHER_CTX_key_length(ctx)) {
                  // Use the ltmp bytes at ktmp
                  SetBuffer(ltmp,ktmp);
                  deflength = 0;
               }
            }
            if (!Length()) {
               EVP_CipherInit_ex(ctx, cipher, 0, (unsigned char *)ktmp, 0, 1);
               SetBuffer(ldef,ktmp);
            }
            // Set also the type
            SetType(cipnam);
         }
      }
      // Cleanup
      if (ktmp) {delete[] ktmp; ktmp = 0;}
   }

   // Cleanup, if invalid
   if (!valid) {
      EVP_CIPHER_CTX_free(ctx);
      Cleanup();
   }

   // We are done
   return valid;
}

//_____________________________________________________________________________
int XrdCryptosslCipher::Publen()
{
   // Minimum length of export format of public key
   static int lhdr = strlen("-----BEGIN DH PARAMETERS-----"
                            "-----END DH PARAMETERS-----") + 3;
   if (fDH) {
      // minimum length of the core is 22 bytes
      int l = 2*DH_size(fDH);
      if (l < 22) l = 22;
      // for headers
      l += lhdr;
      // some margin
      return (l+20);
   } else
      return 0;
}

//_____________________________________________________________________________
char *XrdCryptosslCipher::Public(int &lpub)
{
   // Return buffer with the public part of the DH key and the shared
   // parameters; lpub contains the length of the meaningful bytes.
   // Buffer should be deleted by the caller.
   static int lhend = strlen("-----END DH PARAMETERS-----");

   if (fDH) {
      //
      // Calculate and write public key hex
      const BIGNUM *pub;
      DH_get0_key(fDH, &pub, NULL);
      char *phex = BN_bn2hex(pub);
      int lhex = strlen(phex);
      //
      // Prepare bio to export info buffer
      BIO *biop = BIO_new(BIO_s_mem());
      if (biop) {
         int ltmp = Publen() + lhex + 20;
         char *pub = new char[ltmp];
         if (pub) {
            // Write parms first
            PEM_write_bio_DHparams(biop,fDH);
            // Read key from BIO to buf
            BIO_read(biop,(void *)pub,ltmp);
            BIO_free(biop);
            // Add public key
            char *p = strstr(pub,"-----END DH PARAMETERS-----");
            // Buffer length up to now
            lpub = (int)(p - pub) + lhend + 1;
            if (phex && p) {
               // position at the end
               p += (lhend+1);
               // Begin of public key hex
               memcpy(p,"---BPUB---",10);
               p += 10;
               // Calculate and write public key hex
               memcpy(p,phex,lhex);
               OPENSSL_free(phex);
               // End of public key hex
               p += lhex;
               memcpy(p,"---EPUB---",10);
               // Calculate total length
               lpub += (20 + lhex);
            } else {
               if (phex) OPENSSL_free(phex);
            }
            // return
            return pub;
         }
      } else {
         if (phex) OPENSSL_free(phex);
      }
   }

   lpub = 0;
   return (char *)0;
}

//_____________________________________________________________________________
void XrdCryptosslCipher::PrintPublic(BIGNUM *pub)
{
   // Print public part

   //
   // Prepare bio to export info buffer
   BIO *biop = BIO_new(BIO_s_mem());
   if (biop) {
      // Use a DSA structure to export the public part
      DSA *dsa = DSA_new();
      if (dsa) {
         DSA_set0_key(dsa, BN_dup(pub), NULL);
         // Write public key to BIO
         PEM_write_bio_DSA_PUBKEY(biop,dsa);
         // Read key from BIO to buf
         int lpub = Publen();
         char *bpub = new char[lpub];
         if (bpub) {
            BIO_read(biop,(void *)bpub,lpub);
            cerr << bpub << endl;
            delete[] bpub;
         }
         DSA_free(dsa);
      }
      BIO_free(biop);
   }
}

//_____________________________________________________________________________
XrdSutBucket *XrdCryptosslCipher::AsBucket()
{
   // Return pointer to a bucket created using the internal information
   // serialized
   // The bucket is responsible for the allocated memory

   XrdSutBucket *buck = (XrdSutBucket *)0;

   if (valid) {

      // Serialize .. total length
      kXR_int32 lbuf = Length();
      kXR_int32 ltyp = Type() ? strlen(Type()) : 0;
      kXR_int32 livc = lIV;
      const BIGNUM *p, *g;
      const BIGNUM *pub, *pri;
      DH_get0_pqg(fDH, &p, NULL, &g);
      DH_get0_key(fDH, &pub, &pri);
      char *cp = BN_bn2hex(p);
      char *cg = BN_bn2hex(g);
      char *cpub = BN_bn2hex(pub);
      char *cpri = BN_bn2hex(pri);
      kXR_int32 lp = cp ? strlen(cp) : 0;
      kXR_int32 lg = cg ? strlen(cg) : 0;
      kXR_int32 lpub = cpub ? strlen(cpub) : 0;
      kXR_int32 lpri = cpri ? strlen(cpri) : 0;
      int ltot = 7*sizeof(kXR_int32) + ltyp + Length() + livc +
                 lp + lg + lpub + lpri;
      char *newbuf = new char[ltot];
      if (newbuf) {
         int cur = 0;
         memcpy(newbuf+cur,&ltyp,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(newbuf+cur,&livc,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(newbuf+cur,&lbuf,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(newbuf+cur,&lp,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(newbuf+cur,&lg,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(newbuf+cur,&lpub,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         memcpy(newbuf+cur,&lpri,sizeof(kXR_int32));
         cur += sizeof(kXR_int32);
         if (Type()) {
            memcpy(newbuf+cur,Type(),ltyp);
            cur += ltyp;
         }
         if (fIV) {
            memcpy(newbuf+cur,fIV,livc);
            cur += livc;
         }
         if (Buffer()) {
            memcpy(newbuf+cur,Buffer(),lbuf);
            cur += lbuf;
         }
         if (cp) {
            memcpy(newbuf+cur,cp,lp);
            cur += lp;
            OPENSSL_free(cp);
         }
         if (cg) {
            memcpy(newbuf+cur,cg,lg);
            cur += lg;
            OPENSSL_free(cg);
         }
         if (cpub) {
            memcpy(newbuf+cur,cpub,lpub);
            cur += lpub;
            OPENSSL_free(cpub);
         }
         if (cpri) {
            memcpy(newbuf+cur,cpri,lpri);
            cur += lpri;
            OPENSSL_free(cpri);
         }
         // The bucket now
         buck = new XrdSutBucket(newbuf,ltot,kXRS_cipher);
      }
   }

   return buck;
}

//____________________________________________________________________________
void XrdCryptosslCipher::SetIV(int l, const char *iv)
{
   // Set IV from l bytes at iv. If !iv, sets the IV length.

   if (fIV) {
      delete[] fIV;
      fIV = 0;
      lIV = 0;
   }

   if (l > 0) {
      if (iv) {
         fIV = new char[l];
         if (fIV) memcpy(fIV,iv,l);
      }
      lIV = l;
   }
}

//____________________________________________________________________________
char *XrdCryptosslCipher::RefreshIV(int &l)
{
   // Regenerate IV and return it

   // Generate a new IV
   GenerateIV();

   // Set output
   l = lIV;
   return fIV;
}

//____________________________________________________________________________
void XrdCryptosslCipher::GenerateIV()
{
   // Generate IV

   // Cleanup existing one, if any
   if (fIV) {
      delete[] fIV;
      fIV = 0;
      lIV = 0;
   }

   // Generate a new one, using crypt-like chars
   fIV = XrdSutRndm::GetBuffer(EVP_MAX_IV_LENGTH, 3);
   if (fIV)
      lIV = EVP_MAX_IV_LENGTH;
}

//____________________________________________________________________________
int XrdCryptosslCipher::Encrypt(const char *in, int lin, char *out)
{
   // Encrypt lin bytes at in with local cipher.
   // The outbut buffer must be provided by the caller for at least
   // EncOutLength(lin) bytes.
   // Returns number of meaningful bytes in out, or 0 in case of problems

   return EncDec(1, in, lin, out);
}

//____________________________________________________________________________
int XrdCryptosslCipher::Decrypt(const char *in, int lin, char *out)
{
   // Decrypt lin bytes at in with local cipher.
   // The outbut buffer must be provided by the caller for at least
   // DecOutLength(lin) bytes.
   // Returns number of meaningful bytes in out, or 0 in case of problems

   return EncDec(0, in, lin, out);
}

//____________________________________________________________________________
int XrdCryptosslCipher::EncDec(int enc, const char *in, int lin, char *out)
{
   // Encrypt (enc = 1)/ Decrypt (enc = 0) lin bytes at in with local cipher.
   // The outbut buffer must be provided by the caller for at least
   // EncOutLength(lin) or DecOutLength(lin) bytes.
   // Returns number of meaningful bytes in out, or 0 in case of problems
   EPNAME("Cipher::EncDec");

   int lout = 0;

   const char *action = (enc == 1) ? "encrypting" : "decrypting"; 

   // Check inputs
   if (!in || lin <= 0 || !out) {
      DEBUG("wrong inputs arguments");
      if (!in) DEBUG("in: NULL");
      if (lin <= 0) DEBUG("lin: "<<lin);
      if (!out) DEBUG("out: NULL");
      return 0;
   }

   // Set iv to the one in use
   unsigned char iv[EVP_MAX_IV_LENGTH];
   if (fIV) {
      memcpy((void *)iv,fIV,EVP_MAX_IV_LENGTH);
   } else {
      // We use 0's
      memset((void *)iv,0,EVP_MAX_IV_LENGTH);
   }

   // Action depend on the length of the key wrt default length
   if (deflength) {
      // Init ctx, set key (default length) and set IV
      if (!EVP_CipherInit_ex(ctx, cipher, 0, (unsigned char *)Buffer(), iv, enc)) {
         DEBUG("error initializing");
         return 0;
      }
   } else {
      // Init ctx
      if (!EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, enc)) {
         DEBUG("error initializing - 1");
         return 0;
      }
      // Set key length
      EVP_CIPHER_CTX_set_key_length(ctx,Length());
      // Set key and IV
      if (!EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)Buffer(), iv, enc)) {
         DEBUG("error initializing - 2");
         return 0;
      }
   }

   // Encrypt / Decrypt
   int ltmp = 0;
   if (!EVP_CipherUpdate(ctx, (unsigned char *)&out[0], &ltmp,
                               (unsigned char *)in, lin)) {
      DEBUG("error " << action);
      return 0;
   }
   lout = ltmp;
   if (!EVP_CipherFinal_ex(ctx, (unsigned char *)&out[lout], &ltmp)) {
      DEBUG("error finalizing");
      return 0;
   }

   // Results
   lout += ltmp;
   return lout;
}

//____________________________________________________________________________
int XrdCryptosslCipher::EncOutLength(int l)
{
   // Required buffer size for encrypting l bytes

   return (l+EVP_CIPHER_CTX_block_size(ctx));
}

//____________________________________________________________________________
int XrdCryptosslCipher::DecOutLength(int l)
{
   // Required buffer size for decrypting l bytes

   int lout = l+EVP_CIPHER_CTX_block_size(ctx)+1;
   lout = (lout <= 0) ? l : lout;
   return lout;
}

//____________________________________________________________________________
int XrdCryptosslCipher::MaxIVLength() const
{
   // Return the max cipher IV length

   return (lIV > 0) ? lIV : EVP_MAX_IV_LENGTH;
}
