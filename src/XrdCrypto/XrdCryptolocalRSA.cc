// $Id$
/******************************************************************************/
/*                                                                            */
/*                 X r d C r y p t o L o c a l R S A . c c                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Local implementation of XrdCryptoRSA                                       */
/*                                                                            */
/* ************************************************************************** */

#include <XrdOuc/XrdOucString.hh>
#include <XrdSut/XrdSutRndm.hh>
#include <XrdCrypto/XrdCryptolocalRSA.hh>
#include <XrdCrypto/XrdCryptoTrace.hh>

#include <string.h>
#include <stdlib.h>

extern "C" {
#include  "rsalib.h"
}

const int kMAXRSATRIES    = 100;
const int kPRIMELENGTH    = 20;
const int kPRIMEEXP       = 40;

//_____________________________________________________________________________
XrdCryptolocalRSA::XrdCryptolocalRSA(int, int) : XrdCryptoRSA()
{
   // Constructor
   // Generate a RSA asymmetric key pair

   fPriKey.n.n_len = 0;
   fPubKey.n.n_len = 0;
   fPriKey.e.n_len = 0;
   fPubKey.e.n_len = 0;
   fPubExport.len = 0;
   fPubExport.keys = 0;

   // Try Key Generation
   GenerateKeys();
}

//_____________________________________________________________________________
XrdCryptolocalRSA::XrdCryptolocalRSA(const char *pub, int lpub) : XrdCryptoRSA()
{
   // Constructor
   // Allocate a RSA key pair and fill the public part importing 
   // from string representation (pub) to internal representation.
   // If lpub>0 use the first lpub bytes; otherwise use strlen(pub)
   // bytes.

   fPriKey.n.n_len = 0;
   fPubKey.n.n_len = 0;
   fPriKey.e.n_len = 0;
   fPubKey.e.n_len = 0;
   fPubExport.len = 0;
   fPubExport.keys = 0;

   // Import key
   ImportPublic(pub,lpub);
}

//_____________________________________________________________________________
XrdCryptolocalRSA::~XrdCryptolocalRSA()
{
   // Destructor
   // Destroy the RSA asymmetric key pair

   if (fPubExport.len && fPubExport.keys) {
      delete[] fPubExport.keys;
      fPubExport.len = 0;
      fPubExport.keys = 0;
   }
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::GenerateKeys()
{
   // Generate a pair of keys
   // Sometimes some bunch is not decrypted correctly
   // That's why we make retries to make sure that encryption/decryption
   // works as expected
   EPNAME("localRSA::GenerateKeys");

   bool NotOk = 1;
   rsa_NUMBER p1, p2, rsa_n, rsa_e, rsa_d;
   int l_n = 0, l_e = 0, l_d = 0;
   char buf_n[rsa_STRLEN], buf_e[rsa_STRLEN], buf_d[rsa_STRLEN];

   // Init random machinery, if needed
   XrdSutRndm::Init();

   int NAttempts = 0;
   int thePrimeLen = kPRIMELENGTH;
   int thePrimeExp = kPRIMEEXP + 5;   // Prime probability = 1-0.5^thePrimeExp
   while (NotOk && NAttempts < kMAXRSATRIES) {

      NAttempts++;
      if (NAttempts > 1) {
         DEBUG("retry #" << NAttempts);
         srand(rand());
      }

      // Valid pair of primes
      p1 = rsa_genprim(thePrimeLen, thePrimeExp);
      p2 = rsa_genprim(thePrimeLen+1, thePrimeExp);

      // Retry if equal
      int NPrimes = 0;
      while (rsa_cmp(&p1, &p2) == 0 && NPrimes < kMAXRSATRIES) {
         NPrimes++;
         DEBUG("equal primes: regenerate ("<< NPrimes <<" times)");
         srand(rand());
         p1 = rsa_genprim(thePrimeLen, thePrimeExp);
         p2 = rsa_genprim(thePrimeLen+1, thePrimeExp);
      }

      // Generate keys
      if (rsa_genrsa(p1, p2, &rsa_n, &rsa_e, &rsa_d)) {
         DEBUG("genrsa: attempt " << NAttempts
                                  <<" to generate keys failed");
         continue;
      }

      // Determine their lengths
      rsa_num_sput(&rsa_n, buf_n, rsa_STRLEN);
      l_n = strlen(buf_n);
      rsa_num_sput(&rsa_e, buf_e, rsa_STRLEN);
      l_e = strlen(buf_e);
      rsa_num_sput(&rsa_d, buf_d, rsa_STRLEN);
      l_d = strlen(buf_d);

      if (rsa_cmp(&rsa_n, &rsa_e) <= 0)
         continue;
      if (rsa_cmp(&rsa_n, &rsa_d) <= 0)
         continue;

      // Now we try the keys
      char Test[2 * rsa_STRLEN] = "ThisIsTheStringTest01203456-+/";
      int lTes = 31;
      XrdOucString Tdum;
      XrdSutRndm::GetString(0, lTes - 1, Tdum);
      strncpy(Test, Tdum.c_str(), lTes);
      char buf[2 * rsa_STRLEN];

      // Private/Public
      strncpy(buf, Test, lTes);
      buf[lTes] = 0;

      // Try encryption with private key
      int lout = rsa_encode(buf, lTes, rsa_n, rsa_e);

      // Try decryption with public key
      rsa_decode(buf, lout, rsa_n, rsa_d);
      buf[lTes] = 0;
      if (strncmp(Test, buf, lTes))
         continue;

      // Public/Private
      strncpy(buf, Test, lTes);
      buf[lTes] = 0;

      // Try encryption with public key
      lout = rsa_encode(buf, lTes, rsa_n, rsa_d);

      // Try decryption with private key
      rsa_decode(buf, lout, rsa_n, rsa_e);
      buf[lTes] = 0;

      if (strncmp(Test, buf, lTes))
         continue;

      NotOk = 0;
   }

   if (NotOk) {
      DEBUG("unable to generate good RSA"
            <<" key pair (" << kMAXRSATRIES <<" attempts)- return");
      return 1;
   }

   // Save Private key
   rsa_assign(&fPriKey.n, &rsa_n);
   rsa_assign(&fPriKey.e, &rsa_e);

   // Save Public key
   rsa_assign(&fPubKey.n, &rsa_n);
   rsa_assign(&fPubKey.e, &rsa_d);

   // Export form
   fPubExport.len = l_n + l_d + 4;
   if (fPubExport.keys)
      delete[] fPubExport.keys;
   fPubExport.keys = new char[fPubExport.len];

   if (fPubExport.keys) {
      fPubExport.keys[0] = '#';
      memcpy(fPubExport.keys + 1, buf_n, l_n);
      fPubExport.keys[l_n + 1] = '#';
      memcpy(fPubExport.keys + l_n + 2, buf_d, l_d);
      fPubExport.keys[l_n + l_d + 2] = '#';
      fPubExport.keys[l_n + l_d + 3] = 0;
      
      DEBUG("export pub length: bytes "<< fPubExport.len);
      DEBUG(fPubExport.keys);

      // Set status
      status = kComplete;
   }
   return 0;
}

//_____________________________________________________________________________
void XrdCryptolocalRSA::Dump()
{
   // Dump some info about the key
   EPNAME("localRSA::Dump");

   DEBUG("---------------------------------------");
   DEBUG("address: "<<this);
   if (IsValid()) {
      DEBUG("export pub length: bytes "<< fPubExport.len);
      DEBUG("export pub key:"<<endl<< fPubExport.keys);
   } else {
      DEBUG("key is invalid");
   }
   DEBUG("---------------------------------------");
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::GetOutlen(int lin)
{
   // Get minimal length of output buffer
   EPNAME("localRSA::GetOutlen");

   int primx = (fPriKey.n.n_len) ? rsa_encode_size(fPriKey.n) : 0;
   int pubmx = (fPubKey.n.n_len) ? rsa_encode_size(fPubKey.n) : 0;
   int lcmax = (primx > pubmx) ? primx : pubmx;

   if (lcmax) {
      return ((lin / lcmax) + 1) * lcmax;
   } else {
      if (IsValid()) {
         DEBUG("WARNING: keys are undefined: returning input length");
      } else {
         DEBUG("WARNING: invalid key: returning input length");
      }
      return lin;
   }
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::ImportPublic(const char *pub, int lpub)
{
   // Import a public key
   // Allocate a RSA key pair and fill the public part importing 
   // from string representation (pub) to internal representation.
   // If lpub>0 use the first lpub bytes; otherwise use strlen(pub)
   // bytes.
   // Return 0 in case of success, -1 in case of failure
   EPNAME("localRSA::ImportPublic");

   if (!pub || lpub <= 0) {
      DEBUG("bad inputs ("<<pub<<","<<lpub<<")");
      return -1;
   }

   // The format of the key is #<hex_n>#<hex_d>#
   char *pd1 = 0, *pd2 = 0, *pd3 = 0;
   pd1 = (char *)strstr(pub, "#");
   if (pd1) pd2 = strstr(pd1 + 1, "#");
   if (pd2) pd3 = strstr(pd2 + 1, "#");
   if (!pd1 || !pd2 || !pd3) {
      DEBUG("bad format");
      return -1;
   }

   // Get <hex_n> ...
   int l1 = (int) (pd2 - pd1 - 1);
   char *RSA_n_exp = new char[l1 + 1];
   strncpy(RSA_n_exp, pd1 + 1, l1);
   RSA_n_exp[l1] = 0;
   DEBUG("got "<< strlen(RSA_n_exp) <<" bytes for RSA_n_exp");

   // Now <hex_d>
   int l2 = (int) (pd3 - pd2 - 1);
   char *RSA_d_exp = new char[l2 + 1];
   strncpy(RSA_d_exp, pd2 + 1, l2);
   RSA_d_exp[l2] = 0;
   DEBUG("got "<< strlen(RSA_d_exp) <<" bytes for RSA_d_exp");

   rsa_num_sget(&fPubKey.n, RSA_n_exp);
   rsa_num_sget(&fPubKey.e, RSA_d_exp);

   if (RSA_n_exp) delete[] RSA_n_exp;
   if (RSA_d_exp) delete[] RSA_d_exp;

   // Fill also public export
   if (fPubExport.keys)
      delete[] fPubExport.keys;
   fPubExport.keys = new char[lpub];
   if (fPubExport.keys) {
      memcpy(fPubExport.keys, pub, lpub);
      fPubExport.len = lpub;
      // Set status
      status = kPublic;
   }

   return 0;
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::ExportPublic(char *out, int)
{
   // Export the public key into buffer out, allocated by the caller
   // for at least GetPublen()+1 bytes.
   // Return 0 in case of success, -1 in case of failure
   EPNAME("localRSA::ExportPublic");

   // Make sure we have a valid key
   if (!IsValid()) {
      DEBUG("key not valid");
      return -1;
   }

   // Make sure we got a buffer where to write
   if (!out) {
      DEBUG("output buffer undefined");
      return -1;
   }

   // Fill output
   memcpy(out,fPubExport.keys,fPubExport.len);

   return 0;
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::EncryptPrivate(const char *in, int lin, char *out, int)
{
   // Encrypt lin bytes at 'in' using the internal private key.
   // The output buffer 'out' is allocated by the caller for at least 
   // GetOutlen() bytes.
   // The number of meaningful bytes in out is returned in case of success;
   // -1 in case of error.
   EPNAME("localRSA::EncryptPrivate");

   // Make sure we got something to encrypt
   if (!in || lin <= 0) {
      DEBUG("input buffer undefined");
      return -1;
   }

   // Make sure we got a buffer where to write
   if (!out) {
      DEBUG("output buffer undefined");
      return -1;
   }
   
   // Ok, proceed
   memcpy(out, in, lin);
   out[lin] = 0;
   int lout = rsa_encode(out, lin, fPriKey.n, fPriKey.e);

   // Return   
   return lout;
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::EncryptPublic(const char *in, int lin, char *out, int)
{
   // Encrypt lin bytes at 'in' using the internal public key.
   // The output buffer 'out' is allocated by the caller for at least 
   // GetOutlen() bytes.
   // The number of meaningful bytes in out is returned in case of success;
   // -1 in case of error.
   EPNAME("localRSA::EncryptPublic");

   // Make sure we got something to encrypt
   if (!in || lin <= 0) {
      DEBUG("input buffer undefined");
      return -1;
   }

   // Make sure we got a buffer where to write
   if (!out) {
      DEBUG("output buffer undefined");
      return -1;
   }
   
   // Ok, proceed
   memcpy(out, in, lin);
   out[lin] = 0;
   int lout = rsa_encode(out, lin, fPubKey.n, fPubKey.e);

   // Return   
   return lout;
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::DecryptPrivate(const char *in, int lin, char *out, int)
{
   // Decrypt lin bytes at 'in' using the internal private key.
   // The output buffer 'out' is allocated by the caller for at least 
   // GetOutlen() bytes.
   // The number of meaningful bytes in out is returned in case of success;
   // -1 in case of error.
   EPNAME("localRSA::DecryptPrivate");

   // Make sure we got something to decrypt
   if (!in || lin <= 0) {
      DEBUG("input buffer undefined");
      return -1;
   }

   // Make sure we got a buffer where to write
   if (!out) {
      DEBUG("output buffer undefined");
      return -1;
   }
   
   // Ok, proceed
   memcpy(out, in, lin);
   out[lin] = 0;
#if 0
   rsa_decode(out, lin, fPriKey.n, fPriKey.e);
   int lout = strlen(out);
   // Get rid of control characters at the end of the string
   while (out[lout-1] <= 0x20) { out[lout-1] = 0; lout--; }
#else
   int lout = rsa_decode(out, lin, fPriKey.n, fPriKey.e);
   // Get rid of control characters at the end of the string
   while (out[lout-1] && (out[lout-1] <= 0x20)) { out[lout-1] = 0; lout--; }
#endif
   
   // Return   
   return lout;
}

//_____________________________________________________________________________
int XrdCryptolocalRSA::DecryptPublic(const char *in, int lin, char *out, int)
{
   // Decrypt lin bytes at 'in' using the internal public key.
   // The output buffer 'out' is allocated by the caller for at least 
   // GetOutlen() bytes.
   // The number of meaningful bytes in out is returned in case of success;
   // -1 in case of error.
   EPNAME("localRSA::DecryptPublic");

   // Make sure we got something to decrypt
   if (!in || lin <= 0) {
      DEBUG("input buffer undefined");
      return -1;
   }

   // Make sure we got a buffer where to write
   if (!out) {
      DEBUG("output buffer undefined");
      return -1;
   }
   
   // Ok, proceed
   memcpy(out, in, lin);
   out[lin] = 0;
#if 0
   rsa_decode(out, lin, fPubKey.n, fPubKey.e);
   int lout = strlen(out);
   // Get rid of control characters at the end of the string
   while (out[lout-1] <= 0x20) { out[lout-1] = 0; lout--; }
#else
   int lout = rsa_decode(out, lin, fPubKey.n, fPubKey.e);
   // Get rid of control characters at the end of the string
   while (out[lout-1] && (out[lout-1] <= 0x20)) { out[lout-1] = 0; lout--; }
#endif
   
   // Return   
   return lout;
}
