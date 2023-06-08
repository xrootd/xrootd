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
#include <cassert>

#include "XrdSut/XrdSutRndm.hh"
#include "XrdCrypto/XrdCryptosslTrace.hh"
#include "XrdCrypto/XrdCryptosslCipher.hh"

//#include <openssl/dsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/dh.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#endif

// Hardcoded DH parameters that are acceptable to both OpenSSL 3.0 (RHEL9)
// and 1.0.2 (RHEL7).  OpenSSL 3.0 reworked the DH parameter generation algorithm
// and now produces DH params that don't pass OpenSSL 1.0.2's parameter verification
// function (`DH_check`).  Accordingly, since these are safe to reuse, we generated
// a single set of parameters for the server to always utilize.
static const char dh_param_enc[] =
R"(
-----BEGIN DH PARAMETERS-----
MIIBiAKCAYEAzcEAf3ZCkm0FxJLgKd1YoT16Hietl7QV8VgJNc5CYKmRu/gKylxT
MVZJqtUmoh2IvFHCfbTGEmZM5LdVaZfMLQf7yXjecg0nSGklYZeQQ3P0qshFLbI9
u3z1XhEeCbEZPq84WWwXacSAAxwwRRrN5nshgAavqvyDiGNi+GqYpqGPb9JE38R3
GJ51FTPutZlvQvEycjCbjyajhpItBB+XvIjWj2GQyvi+cqB0WrPQAsxCOPrBTCZL
OjM0NfJ7PQfllw3RDQev2u1Q+Rt8QyScJQCFUj/SWoxpw2ydpWdgAkrqTmdVYrev
x5AoXE52cVIC8wfOxaaJ4cBpnJui3Y0jZcOQj0FtC0wf4WcBpHnLLBzKSOQwbxts
WE8LkskPnwwrup/HqWimFFg40bC9F5Lm3CTDCb45mtlBxi3DydIbRLFhGAjlKzV3
s9G3opHwwfgXpFf3+zg7NPV3g1//HLgWCvooOvMqaO+X7+lXczJJLMafEaarcAya
Kyo8PGKIAORrAgEF
-----END DH PARAMETERS-----
)";

// ---------------------------------------------------------------------------//
//
// Cipher interface
//
// ---------------------------------------------------------------------------//

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static DH *EVP_PKEY_get0_DH(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_DH) {
        return NULL;
    }
    return pkey->pkey.dh;
}

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

static int XrdCheckDH (EVP_PKEY *pkey) {
   int rc;
#if OPENSSL_VERSION_NUMBER < 0x10101000L
   DH *dh = EVP_PKEY_get0_DH(pkey);
   if (dh) {
      DH_check(dh, &rc);
      rc = (rc == 0 ? 1 : 0);
   }
   else {
      rc = -2;
   }
#else
   EVP_PKEY_CTX *ckctx = EVP_PKEY_CTX_new(pkey, 0);
   rc = EVP_PKEY_param_check(ckctx);
   EVP_PKEY_CTX_free(ckctx);
#endif
   return rc;
}

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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
            if (p) OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p);
            if (g) OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g);
            if (pub) OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, pub);
            if (pri) OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, pri);
            OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(bld);
            OSSL_PARAM_BLD_free(bld);
            if (p) BN_free(p);
            if (g) BN_free(g);
            if (pub) BN_free(pub);
            if (pri) BN_free(pri);
            EVP_PKEY_CTX *pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, 0);
            EVP_PKEY_fromdata_init(pkctx);
            EVP_PKEY_fromdata(pkctx, &fDH, EVP_PKEY_KEYPAIR, params);
            EVP_PKEY_CTX_free(pkctx);
            OSSL_PARAM_free(params);
#else
            DH* dh = DH_new();
            DH_set0_pqg(dh, p, NULL, g);
            DH_set0_key(dh, pub, pri);
            fDH = EVP_PKEY_new();
            EVP_PKEY_assign_DH(fDH, dh);
#endif
            if (XrdCheckDH(fDH) != 1)
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
   // 'bits' is ignored (DH key is generated once)
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
      static EVP_PKEY *dhparms = [] {
         DEBUG("generate DH parameters");
         EVP_PKEY *dhParam = 0;

//
// Important historical context:
// - We used to generate DH params on every server startup (commented
//   out below).  This was prohibitively costly to do on startup for
//   DH parameters large enough to be considered secure.
// - OpenSSL 3.0 improved the DH parameter generation to avoid leaking
//   the first bit of the session key (see https://github.com/openssl/openssl/issues/9792
//   for more information).  However, a side-effect is that the new
//   parameters are not recognized as valid in OpenSSL 1.0.2.
// - Since we can't control old client versions and new servers can't
//   generate compatible DH parameters, we switch to a fixed, much stronger
//   set of DH parameters (3072 bits).
//
// The impact is that we continue leaking the first bit of the session key
// (meaning it's effectively 127 bits not 128 bits -- still plenty secure)
// but upgrade the DH parameters to something more modern (3072; previously,
// it was 512 bits which was not considered secure).  The downside
// of fixed DH parameters is that if a nation-state attacked our selected
// parameters (using technology not currently available), we would have
// to upgrade all servers with a new set of DH parameters.
//

/*
         EVP_PKEY_CTX *pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, 0);
         EVP_PKEY_paramgen_init(pkctx);
         EVP_PKEY_CTX_set_dh_paramgen_prime_len(pkctx, kDHMINBITS);
         EVP_PKEY_CTX_set_dh_paramgen_generator(pkctx, 5);
         EVP_PKEY_paramgen(pkctx, &dhParam);
         EVP_PKEY_CTX_free(pkctx);
*/
         BIO *biop = BIO_new(BIO_s_mem());
         BIO_write(biop, dh_param_enc, strlen(dh_param_enc));
         PEM_read_bio_Parameters(biop, &dhParam);
         BIO_free(biop);
         DEBUG("generate DH parameters done");
         return dhParam;
      }();

      DEBUG("configure DH parameters");
      //
      // Set params for DH object
      assert(dhparms);
      EVP_PKEY_CTX *pkctx = EVP_PKEY_CTX_new(dhparms, 0);
      EVP_PKEY_keygen_init(pkctx);
      EVP_PKEY_keygen(pkctx, &fDH);
      EVP_PKEY_CTX_free(pkctx);
      if (fDH) {
         // Init context
         ctx = EVP_CIPHER_CTX_new();
         if (ctx)
            valid = 1;
      }

   } else {
      DEBUG("initialize cipher from key-agreement buffer");
      //
      char *ktmp = 0;
      size_t ltmp = 0;
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
            // Read params from BIO
            EVP_PKEY *dhParam = 0;
            PEM_read_bio_Parameters(biop, &dhParam);
            if (dhParam) {
               if (XrdCheckDH(dhParam) == 1) {
                  //
                  // generate DH key
                  EVP_PKEY_CTX *pkctx = EVP_PKEY_CTX_new(dhParam, 0);
                  EVP_PKEY_keygen_init(pkctx);
                  EVP_PKEY_keygen(pkctx, &fDH);
                  EVP_PKEY_CTX_free(pkctx);
                  if (fDH) {
                     // Now we can compute the cipher
                     ltmp = EVP_PKEY_size(fDH);
                     ktmp = new char[ltmp];
                     memset(ktmp, 0, ltmp);
                     if (ktmp) {
                        // Create peer public key
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
                        EVP_PKEY *peer = 0;
                        OSSL_PARAM *params1 = 0;
                        EVP_PKEY_todata( dhParam, EVP_PKEY_KEY_PARAMETERS, &params1 );
                        OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
                        OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, bnpub);
                        OSSL_PARAM *params2 = OSSL_PARAM_BLD_to_param(bld);
                        OSSL_PARAM_BLD_free(bld);
                        OSSL_PARAM *params = OSSL_PARAM_merge( params1, params2 );
                        pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, 0);
                        EVP_PKEY_fromdata_init(pkctx);
                        EVP_PKEY_fromdata(pkctx, &peer, EVP_PKEY_KEYPAIR, params);
                        EVP_PKEY_CTX_free(pkctx);
                        OSSL_PARAM_free(params);
                        OSSL_PARAM_free(params1);
                        OSSL_PARAM_free(params2);
#else
                        DH* dh = DH_new();
                        DH_set0_key(dh, BN_dup(bnpub), NULL);
                        EVP_PKEY *peer = EVP_PKEY_new();
                        EVP_PKEY_assign_DH(peer, dh);
#endif
                        // Derive shared secret
                        pkctx = EVP_PKEY_CTX_new(fDH, 0);
                        EVP_PKEY_derive_init(pkctx);
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
                        EVP_PKEY_CTX_set_dh_pad(pkctx, padded);
#endif
                        EVP_PKEY_derive_set_peer(pkctx, peer);
                        EVP_PKEY_derive(pkctx, (unsigned char *)ktmp, &ltmp);
                        EVP_PKEY_CTX_free(pkctx);
                        EVP_PKEY_free(peer);
                        if (ltmp > 0) {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
                           if (padded) {
                              int pad = EVP_PKEY_size(fDH) - ltmp;
                              if (pad > 0) {
                                 memmove(ktmp + pad, ktmp, ltmp);
                                 memset(ktmp, 0, pad);
                                 ltmp += pad;
                              }
                           }
#endif
                           valid = 1;
                        }
                     }
                  }
               }
               EVP_PKEY_free(dhParam);
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
               if ((int)ltmp != ldef) {
                  EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
                  EVP_CIPHER_CTX_set_key_length(ctx,ltmp);
                  EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)ktmp, 0, 1);
                  if ((int)ltmp == EVP_CIPHER_CTX_key_length(ctx)) {
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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      BIGNUM *p = BN_new();
      BIGNUM *g = BN_new();
      BIGNUM *pub = BN_new();
      BIGNUM *pri = BN_new();
      OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
      if (EVP_PKEY_get_bn_param(c.fDH, OSSL_PKEY_PARAM_FFC_P, &p) == 1)
         OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_P, p);
      if (EVP_PKEY_get_bn_param(c.fDH, OSSL_PKEY_PARAM_FFC_G, &g) == 1)
         OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_FFC_G, g);
      if (EVP_PKEY_get_bn_param(c.fDH, OSSL_PKEY_PARAM_PUB_KEY, &pub) == 1)
         OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, pub);
      if (EVP_PKEY_get_bn_param(c.fDH, OSSL_PKEY_PARAM_PRIV_KEY, &pri) == 1)
         OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, pri);
      OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(bld);
      OSSL_PARAM_BLD_free(bld);
      BN_free(p);
      BN_free(g);
      BN_free(pub);
      BN_free(pri);
      EVP_PKEY_CTX *pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, 0);
      EVP_PKEY_fromdata_init(pkctx);
      EVP_PKEY_fromdata(pkctx, &fDH, EVP_PKEY_KEYPAIR, params);
      EVP_PKEY_CTX_free(pkctx);
      OSSL_PARAM_free(params);
#else
      DH* dh = DH_new();
      if (dh) {
         const BIGNUM *p, *g;
         DH_get0_pqg(EVP_PKEY_get0_DH(c.fDH), &p, NULL, &g);
         DH_set0_pqg(dh, p ? BN_dup(p) : NULL, NULL, g ? BN_dup(g) : NULL);
         const BIGNUM *pub, *pri;
         DH_get0_key(EVP_PKEY_get0_DH(c.fDH), &pub, &pri);
         DH_set0_key(dh, pub ? BN_dup(pub) : NULL, pri ? BN_dup(pri) : NULL);
         fDH = EVP_PKEY_new();
         EVP_PKEY_assign_DH(fDH, dh);
      }
#endif
      if (fDH) {
         if (XrdCheckDH(fDH) == 1)
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
      EVP_PKEY_free(fDH);
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
   size_t ltmp = 0;
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
         ltmp = EVP_PKEY_size(fDH);
         ktmp = new char[ltmp];
         if (ktmp) {
            memset(ktmp, 0, ltmp);
            // Create peer public key
            EVP_PKEY_CTX *pkctx;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            EVP_PKEY *peer = nullptr;
            OSSL_PARAM *params1 = nullptr;
            EVP_PKEY_todata(fDH, EVP_PKEY_KEY_PARAMETERS, &params1);
            OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
            OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, bnpub);
            OSSL_PARAM *params2 = OSSL_PARAM_BLD_to_param(bld);
            OSSL_PARAM_BLD_free(bld);
            OSSL_PARAM *params = OSSL_PARAM_merge(params1, params2);
            pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, 0);
            EVP_PKEY_fromdata_init(pkctx);
            EVP_PKEY_fromdata(pkctx, &peer, EVP_PKEY_PUBLIC_KEY, params);
            OSSL_PARAM_free(params1);
            OSSL_PARAM_free(params2);
            OSSL_PARAM_free(params);
            EVP_PKEY_CTX_free(pkctx);
#else
            DH* dh = DH_new();
            DH_set0_key(dh, BN_dup(bnpub), NULL);
            EVP_PKEY *peer = EVP_PKEY_new();
            EVP_PKEY_assign_DH(peer, dh);
#endif
            // Derive shared secret
            pkctx = EVP_PKEY_CTX_new(fDH, 0);
            EVP_PKEY_derive_init(pkctx);
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
            EVP_PKEY_CTX_set_dh_pad(pkctx, padded);
#endif
            EVP_PKEY_derive_set_peer(pkctx, peer);
            EVP_PKEY_derive(pkctx, (unsigned char *)ktmp, &ltmp);
            EVP_PKEY_CTX_free(pkctx);
            EVP_PKEY_free(peer);
            if (ltmp > 0) {
#if OPENSSL_VERSION_NUMBER < 0x10101000L
               if (padded) {
                  int pad = EVP_PKEY_size(fDH) - ltmp;
                  if (pad > 0) {
                     memmove(ktmp + pad, ktmp, ltmp);
                     memset(ktmp, 0, pad);
                     ltmp += pad;
                  }
               }
#endif
               valid = 1;
            }
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
            if ((int)ltmp != ldef) {
               EVP_CipherInit_ex(ctx, cipher, 0, 0, 0, 1);
               EVP_CIPHER_CTX_set_key_length(ctx,ltmp);
               EVP_CipherInit_ex(ctx, 0, 0, (unsigned char *)ktmp, 0, 1);
               if ((int)ltmp == EVP_CIPHER_CTX_key_length(ctx)) {
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
      int l = 2 * EVP_PKEY_size(fDH);
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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      BIGNUM *pub = BN_new();
      EVP_PKEY_get_bn_param(fDH, OSSL_PKEY_PARAM_PUB_KEY, &pub);
      char *phex = BN_bn2hex(pub);
      BN_free(pub);
#else
      const BIGNUM *pub;
      DH_get0_key(EVP_PKEY_get0_DH(fDH), &pub, NULL);
      char *phex = BN_bn2hex(pub);
#endif
      int lhex = strlen(phex);
      //
      // Prepare bio to export info buffer
      BIO *biop = BIO_new(BIO_s_mem());
      if (biop) {
         int ltmp = Publen() + lhex + 20;
         char *pub = new char[ltmp];
         if (pub) {
            // Write parms first
            PEM_write_bio_Parameters(biop, fDH);
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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      EVP_PKEY *dsa = 0;
      OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
      OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY, pub);
      OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(bld);
      OSSL_PARAM_BLD_free(bld);
      EVP_PKEY_CTX *pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DSA, 0);
      EVP_PKEY_fromdata_init(pkctx);
      EVP_PKEY_fromdata(pkctx, &dsa, EVP_PKEY_PUBLIC_KEY, params);
      EVP_PKEY_CTX_free(pkctx);
      OSSL_PARAM_free(params);
#else
      EVP_PKEY *dsa = EVP_PKEY_new();
      DSA *fdsa = DSA_new();
      DSA_set0_key(fdsa, BN_dup(pub), NULL);
      EVP_PKEY_assign_DSA(dsa, fdsa);
#endif
      if (dsa) {
         // Write public key to BIO
         PEM_write_bio_PUBKEY(biop, dsa);
         // Read key from BIO to buf
         int lpub = Publen();
         char *bpub = new char[lpub];
         if (bpub) {
            BIO_read(biop,(void *)bpub,lpub);
            std::cerr << bpub << std::endl;
            delete[] bpub;
         }
         EVP_PKEY_free(dsa);
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
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      BIGNUM *p = BN_new();
      BIGNUM *g = BN_new();
      BIGNUM *pub = BN_new();
      BIGNUM *pri = BN_new();
      EVP_PKEY_get_bn_param(fDH, OSSL_PKEY_PARAM_FFC_P, &p);
      EVP_PKEY_get_bn_param(fDH, OSSL_PKEY_PARAM_FFC_G, &g);
      EVP_PKEY_get_bn_param(fDH, OSSL_PKEY_PARAM_PUB_KEY, &pub);
      EVP_PKEY_get_bn_param(fDH, OSSL_PKEY_PARAM_PRIV_KEY, &pri);
#else
      const BIGNUM *p, *g;
      const BIGNUM *pub, *pri;
      DH_get0_pqg(EVP_PKEY_get0_DH(fDH), &p, NULL, &g);
      DH_get0_key(EVP_PKEY_get0_DH(fDH), &pub, &pri);
#endif
      char *cp = BN_bn2hex(p);
      char *cg = BN_bn2hex(g);
      char *cpub = BN_bn2hex(pub);
      char *cpri = BN_bn2hex(pri);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
      BN_free(p);
      BN_free(g);
      BN_free(pub);
      BN_free(pri);
#endif
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
