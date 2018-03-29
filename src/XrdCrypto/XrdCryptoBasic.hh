#ifndef __CRYPTO_BASIC_H__
#define __CRYPTO_BASIC_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d C r y p t o B a s i c. h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Geri Ganis for CERN                                          */
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

#include "XProtocol/XProtocol.hh"
#include "XrdSut/XrdSutBucket.hh"

// ---------------------------------------------------------------------------//
//
// Basic buffer
//
// ---------------------------------------------------------------------------//
class XrdCryptoBasic
{
public:
   // ctor
   XrdCryptoBasic(const char *t = 0, int l = 0, const char *b = 0);
   // dtor
   virtual ~XrdCryptoBasic() 
          { if (type) delete[] type; if (membuf) delete[] membuf; }
   // getters
   virtual XrdSutBucket *AsBucket();
   char *AsHexString();
   virtual int   Length() const { return lenbuf; }
   virtual char *Buffer() const { return membuf; }
   virtual char *Type() const { return type; }
   // setters
   virtual int   FromHex(const char *hex);
   virtual int   SetLength(int l);
   virtual int   SetBuffer(int l, const char *b);
   virtual int   SetType(const char *t);
   // special setter to avoid buffer re-allocation
   virtual void  UseBuffer(int l, const char *b)
          { if (membuf) delete[] membuf; membuf = (char *)b; lenbuf = l; }

private:
   kXR_int32  lenbuf;
   char      *membuf;
   char      *type;
};

#endif
