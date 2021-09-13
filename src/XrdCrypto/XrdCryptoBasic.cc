/******************************************************************************/
/*                                                                            */
/*                     X r d C r y p t o B a s i c. h h                       */
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
/* Generic buffer for crypto functions needed in XrdCrypto                    */
/* Different crypto implementation (OpenSSL, Botan, ...) available as plug-in */
/*                                                                            */
/* ************************************************************************** */

#include <cstdio>
#include <cstring>

#include "XrdSut/XrdSutAux.hh"
#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoBasic.hh"

// ---------------------------------------------------------------------------//
//
// Basic crypto buffer implementation
//
// ---------------------------------------------------------------------------//

//_____________________________________________________________________________
XrdCryptoBasic::XrdCryptoBasic(const char *t, int l, const char *b)
{
   // Basic constructor.
   // This class has responsibility over both its buffers.

   type = 0;
   membuf = 0;
   lenbuf = 0;
   //
   // Fill in the type, if any
   if (t) {
      int tl = strlen(t);
      if (tl) {
         type = new char[tl+1];
         if (type) {
            memcpy(type,t,tl);
            type[tl] = 0;
         }
      }
   }
   //
   // Fill the buffer and length
   if (l > 0) {
      membuf = new char[l];
      if (membuf) {
         lenbuf = l;
         if (b)
            memcpy(membuf,b,l);
         else
            memset(membuf,0,l);
      }
   }
}

//_____________________________________________________________________________
XrdSutBucket *XrdCryptoBasic::AsBucket()
{
   // Return pointer to a bucket created using the internal buffer
   // Type is not copied.
   // The bucket is responsible for the allocated memory

   XrdSutBucket *buck = (XrdSutBucket *)0;

   if (Length()) {
      char *nbuf = new char[Length()];
      if (nbuf) {
         memcpy(nbuf,Buffer(),Length());
         buck = new XrdSutBucket(nbuf,Length());
      }
   }

   return buck;
}

//_____________________________________________________________________________
char *XrdCryptoBasic::AsHexString()
{
   // Return the internal buffer as a hexadecimal string
   static char out[XrdSutMAXBUF];

   int lmax = XrdSutMAXBUF / 2 - 1 ;
   int lconv = (Length() > lmax) ? lmax : Length();

   if (!XrdSutToHex(Buffer(),lconv,&out[0]))
      return &out[0];
   return 0;
}

//_____________________________________________________________________________
int XrdCryptoBasic::FromHex(const char *hex)
{
   // Set a binary buffer from a null-terminated hexadecimal string
   // Returns 0 in case of success, -1 otherwise.

   if (!hex)
      return -1;

   // Determine length
   int lhex = strlen(hex);
   int lout = lhex / 2;
   if (lout * 2 < lhex) lout++;

   // Allocate buffer
   char *bout = new char[lout];
   if (bout) {
      if (XrdSutFromHex(hex, bout, lout) != 0) {
         delete[] bout;
         return -1;
      }
      UseBuffer(lout,bout);
      return 0;
   }

   // Failure
   return -1;
}

//_____________________________________________________________________________
int XrdCryptoBasic::SetLength(int l)
{
   // Truncate or enlarge the data buffer length to l; new bytes are filled
   // with 0 in case of enlargement
   // Returns 0 in case of success, -1 in case of error (in buffer allocation).

   if (l > 0) {
      //
      // Create new buffer
      char *newbuf = new char[l];
      if (newbuf) {
         //
         // Save existing info
         memcpy(newbuf,membuf,l);
         //
         // Reset additional bytes, if any
         if (l > lenbuf)
            memset(newbuf+lenbuf,0,(l-lenbuf));
         //
         // Release old buffer
         delete[] membuf;
         //
         // Set the new length and buffer
         lenbuf = l;
         membuf = newbuf;
      } else
         return -1;
   } else {
      //
      // Release existing buffer, if any
      if (membuf)
         delete[] membuf;
      lenbuf = 0;
      membuf = 0;
   }

   return 0;
}

//_____________________________________________________________________________
int XrdCryptoBasic::SetBuffer(int l, const char *b)
{
   // Substitute buffer with the l bytes at b.
   // Returns 0 in case of success, -1 in case of error (in buffer allocation).

   if (l > 0) {
      //
      // Allocate new buffer
      char *tmpbuf = new char[l];
      if (tmpbuf) {
         if (b)
            memcpy(tmpbuf,b,l);
         else
            memset(tmpbuf,0,l);
         if (membuf)
            delete[] membuf;
         lenbuf = l;
         membuf = tmpbuf;
      } else
         return -1;
   } else {
      //
      // Release existing buffer, if any
      if (membuf)
         delete[] membuf;
      lenbuf = 0;
      membuf = 0;
   }

   return 0;
}

//_____________________________________________________________________________
int XrdCryptoBasic::SetType(const char *t)
{
   // Substitute type with the string at t.
   // Returns 0 in case of success, -1 in case of error (in buffer allocation).

   if (t) {
      //
      // Allocate new buffer
      int tl = strlen(t);
      char *tmpbuf = new char[tl+1];
      if (tmpbuf) {
         strcpy(tmpbuf,t);
         delete[] type;
         type = tmpbuf;
      } else
         return -1;
   } else {
      //
      // Release existing buffer, if any
      if (type)
         delete[] type;
      type = 0;
   }

   return 0;
}
