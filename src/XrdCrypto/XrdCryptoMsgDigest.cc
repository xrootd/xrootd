/******************************************************************************/
/*                                                                            */
/*                 X r d C r y p t o M s g D i g e s t . c c                  */
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
/* Abstract interface for Message Digest crypto functionality.                */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include <cstring>

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoMsgDigest.hh"

//_____________________________________________________________________________
bool XrdCryptoMsgDigest::IsValid()
{
   // Check key validity
   ABSTRACTMETHOD("XrdCryptoMsgDigest::IsValid");
   return 0;
}

//______________________________________________________________________________
bool XrdCryptoMsgDigest::operator==(const XrdCryptoMsgDigest md)
{
   // Compare msg digest md to local md: return 1 if matches, 0 if not

   if (md.Length() == Length()) {
      if (!memcmp(md.Buffer(),Buffer(),Length()))
         return 1;
   }
   return 0;
}
//_____________________________________________________________________________
int XrdCryptoMsgDigest::Reset(const char *dgst)
{
   // Re-Init the message digest calculation

   ABSTRACTMETHOD("XrdCryptoMsgDigest::Reset");
   return -1;
}

//_____________________________________________________________________________
int XrdCryptoMsgDigest::Update(const char *b, int l)
{
   // Update message digest with the MD of l bytes at b.

   ABSTRACTMETHOD("XrdCryptoMsgDigest::Update");
   return -1;   
}

//_____________________________________________________________________________
int XrdCryptoMsgDigest::Final()
{
   // Finalize message digest calculation.

   ABSTRACTMETHOD("XrdCryptoMsgDigest::Final");
   return -1;   
}
