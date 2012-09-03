#ifndef __CRYPTO_LOCALCIPHER_H__
#define __CRYPTO_LOCALCIPHER_H__
/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o L o c a l C i p h e r . h h                */
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
/* Local implentation of XrdCryptoCipher based on PC1.                        */
/*                                                                            */
/* ************************************************************************** */

#include <XrdCrypto/XrdCryptoCipher.hh>

// ---------------------------------------------------------------------------//
//
// Cipher interface
//
// ---------------------------------------------------------------------------//
class XrdCryptolocalCipher : public XrdCryptoCipher
{
private:
   bool valid;
   unsigned char *bpub;      // Key agreement: temporary store local public info
   unsigned char *bpriv;     // Key agreement: temporary store local private info

public:
   XrdCryptolocalCipher(const char *t = "PC1", int l = 0);
   XrdCryptolocalCipher(const char *t, int l, const char *k);
   XrdCryptolocalCipher(XrdSutBucket *b);
   XrdCryptolocalCipher(int len, char *pub, int lpub, const char *t = "PC1");
   XrdCryptolocalCipher(const XrdCryptolocalCipher &c);
   virtual ~XrdCryptolocalCipher() { Cleanup(); }

   // Finalize key computation (key agreement)
   bool Finalize(char *pub, int lpub, const char *t = "PC1");
   void Cleanup();

   // Validity
   bool IsValid() { return valid; }

   // Additional getters
   XrdSutBucket *AsBucket();
   bool IsDefaultLength() const;
   char *Public(int &lpub);

   // Required buffer size for encrypt / decrypt operations on l bytes
   int EncOutLength(int l);
   int DecOutLength(int l);

   // Additional methods
   int Encrypt(const char *in, int lin, char *out);
   int Decrypt(const char *in, int lin, char *out);
};

#endif
