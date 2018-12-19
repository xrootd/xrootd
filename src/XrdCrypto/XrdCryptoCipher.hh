#ifndef __CRYPTO_CIPHER_H__
#define __CRYPTO_CIPHER_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d C r y p t o C i p h e r . h h                    */
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
/* Abstract interface for a symmetric Cipher functionality.                   */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include "XrdSut/XrdSutBucket.hh"
#include "XrdCrypto/XrdCryptoBasic.hh"

// ---------------------------------------------------------------------------//
//
// Cipher interface
//
// ---------------------------------------------------------------------------//
class XrdCryptoCipher : public XrdCryptoBasic
{
public:
   XrdCryptoCipher() : XrdCryptoBasic() {}
   virtual ~XrdCryptoCipher() {}

   // Finalize key computation (key agreement)
   virtual bool Finalize(bool padded, char *pub, int lpub, const char *t);
   bool Finalize(char *pub, int lpub, const char *t)
               { return Finalize(false, pub, lpub, t); }

   // Validity
   virtual bool IsValid();

   // Required buffer size for encrypt / decrypt operations on l bytes
   virtual int EncOutLength(int l);
   virtual int DecOutLength(int l);

   // Additional getters
   virtual XrdSutBucket *AsBucket();
   virtual char *IV(int &l) const;
   virtual bool IsDefaultLength() const;
   virtual char *Public(int &lpub);
   virtual int  MaxIVLength() const;

   // Additional setters
   virtual void SetIV(int l, const char *iv);

   // Additional methods
   virtual int Encrypt(const char *in, int lin, char *out);
   virtual int Decrypt(const char *in, int lin, char *out);
   int Encrypt(XrdSutBucket &buck, bool useiv = true);
   int Decrypt(XrdSutBucket &buck, bool useiv = true);
   virtual char *RefreshIV(int &l); 
};

#endif
