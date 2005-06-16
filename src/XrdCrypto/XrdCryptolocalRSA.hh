// $Id$
#ifndef __CRYPTO_LOCALRSA_H__
#define __CRYPTO_LOCALRSA_H__
/******************************************************************************/
/*                                                                            */
/*                 X r d C r y p t o L o c a l R S A . h h                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Local implementation of XrdCryptoRSA                                       */
/* (see rsalib.c for credits)                                                 */
/*                                                                            */
/* ************************************************************************** */

#include <XrdCrypto/XrdCryptoRSA.hh>

extern "C" {
#include  "rsadef.h"
}

// ---------------------------------------------------------------------------//
//
// RSA interface
//
// ---------------------------------------------------------------------------//
class XrdCryptolocalRSA : public XrdCryptoRSA
{
private:
   rsa_KEY        fPriKey;
   rsa_KEY        fPubKey;
   rsa_KEY_export fPubExport;

   // Key generator
   int GenerateKeys();

public:
   XrdCryptolocalRSA(int bits = XrdCryptoMinRSABits, int exp = XrdCryptoDefRSAExp);
   XrdCryptolocalRSA(const char *pub, int lpub = 0);
   virtual ~XrdCryptolocalRSA();

   // Dump information
   void Dump();

   // Getters
   int GetOutlen(int lin);   // Length of encrypted buffers
   int GetPublen() { return fPubExport.len; } // Length of export public key

   // Import / Export methods
   int ImportPublic(const char *in, int lin);
   int ExportPublic(char *out, int lout);

   // Encryption / Decryption methods
   int EncryptPrivate(const char *in, int lin, char *out, int lout);
   int DecryptPublic(const char *in, int lin, char *out, int lout);
   int EncryptPublic(const char *in, int lin, char *out, int lout);
   int DecryptPrivate(const char *in, int lin, char *out, int lout);
};

#endif
