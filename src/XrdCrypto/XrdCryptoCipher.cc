/******************************************************************************/
/*                                                                            */
/*                   X r d C r y p t o C i p h e r . c c                      */
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
/* Generic interface to a cipher class                                        */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include <cstring>

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoCipher.hh"

//_____________________________________________________________________________
bool XrdCryptoCipher::Finalize(bool, char *, int, const char *)
{
   // Finalize key computation (key agreement)
   ABSTRACTMETHOD("XrdCryptoCipher::Finalize");
   return 0;
}

//_____________________________________________________________________________
bool XrdCryptoCipher::IsValid()
{
   // Check key validity
   ABSTRACTMETHOD("XrdCryptoCipher::IsValid");
   return 0;
}

//____________________________________________________________________________
void XrdCryptoCipher::SetIV(int l, const char *iv)
{
   // Set IV from l bytes at iv. If !iv, sets the IV length.

   ABSTRACTMETHOD("XrdCryptoCipher::SetIV");
}

//____________________________________________________________________________
char *XrdCryptoCipher::RefreshIV(int &l)
{
   // Regenerate IV and return it

   ABSTRACTMETHOD("XrdCryptoCipher::RefreshIV");
   return 0;
}

//____________________________________________________________________________
char *XrdCryptoCipher::IV(int &l) const
{
   // Get IV

   ABSTRACTMETHOD("XrdCryptoCipher::IV");
   return 0;
}

//____________________________________________________________________________
char *XrdCryptoCipher::Public(int &lpub)
{
   // Getter for public part during key agreement

   ABSTRACTMETHOD("XrdCryptoCipher::Public");
   return 0;
}

//_____________________________________________________________________________
XrdSutBucket *XrdCryptoCipher::AsBucket()
{
   // Return pointer to a bucket created using the internal information
   // serialized
 
   ABSTRACTMETHOD("XrdCryptoCipher::AsBucket");
   return 0;
}
//____________________________________________________________________________
int XrdCryptoCipher::Encrypt(const char *, int, char *)
{
   // Encrypt lin bytes at in with local cipher.

   ABSTRACTMETHOD("XrdCryptoCipher::Encrypt");
   return 0;
}

//____________________________________________________________________________
int XrdCryptoCipher::Decrypt(const char *, int, char *)
{
   // Decrypt lin bytes at in with local cipher.

   ABSTRACTMETHOD("XrdCryptoCipher::Decrypt");
   return 0;
}

//____________________________________________________________________________
int XrdCryptoCipher::EncOutLength(int)
{
   // Required buffer size for encrypting l bytes

   ABSTRACTMETHOD("XrdCryptoCipher::EncOutLength");
   return 0;
}

//____________________________________________________________________________
int XrdCryptoCipher::DecOutLength(int)
{
   // Required buffer size for decrypting l bytes

   ABSTRACTMETHOD("XrdCryptoCipher::DecOutLength");
   return 0;
}

//____________________________________________________________________________
bool XrdCryptoCipher::IsDefaultLength() const
{
   // Test if cipher length is the default one

   ABSTRACTMETHOD("XrdCryptoCipher::IsDefaultLength");
   return 0;
}

//____________________________________________________________________________
int XrdCryptoCipher::MaxIVLength() const
{
   // Return the max cipher IV length

   ABSTRACTMETHOD("XrdCryptoCipher::MaxIVLength");
   return 0;
}

//____________________________________________________________________________
int XrdCryptoCipher::Encrypt(XrdSutBucket &bck, bool useiv)
{
   // Encrypt bucket bck with local cipher
   // Return size of encoded bucket or -1 in case of error
   int snew = -1;

   int liv = 0;
   char *iv = 0;
   if (useiv) {
      iv = RefreshIV(liv);
      if (!iv) return snew;
   }

   int sz = EncOutLength(bck.size) + liv;
   char *newbck = new char[sz];
   if (newbck) {
      memset(newbck, 0, sz);
      if (liv > 0) memcpy(newbck, iv, liv);
      snew = Encrypt(bck.buffer,bck.size,newbck+liv);
      if (snew > -1)
         bck.Update(newbck,snew + liv);
   }
   return snew;
}

//____________________________________________________________________________
int XrdCryptoCipher::Decrypt(XrdSutBucket &bck, bool useiv)
{
   // Decrypt bucket bck with local cipher
   // Return size of encoded bucket or -1 in case of error
   int snew = -1;

   int liv = (useiv) ? MaxIVLength() : 0;

   int sz = DecOutLength(bck.size - liv);
   char *newbck = new char[sz];
   if (newbck) {

      if (useiv) {
         char *iv = new char[liv];
         if (iv) {
            memcpy(iv,bck.buffer,liv);
            SetIV(liv, iv);
            delete[] iv;
         } else {
            return snew;
         }
      }
      memset(newbck, 0, sz);
      snew = Decrypt(bck.buffer + liv, bck.size - liv, newbck);
      if (snew > -1)
         bck.Update(newbck,snew);
   }
   return snew;
}
