#ifndef __CRYPTO_MSGDGST_H__
#define __CRYPTO_MSGDGST_H__
/******************************************************************************/
/*                                                                            */
/*                 X r d C r y p t o M s g D i g e s t . h h                  */
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

#include "XrdCrypto/XrdCryptoBasic.hh"

// ---------------------------------------------------------------------------//
//
// Message Digest abstract buffer
//
// ---------------------------------------------------------------------------//
class XrdCryptoMsgDigest : public XrdCryptoBasic
{

public:
   XrdCryptoMsgDigest() : XrdCryptoBasic() { }
   virtual ~XrdCryptoMsgDigest() { }

   // Validity
   virtual bool IsValid();

   // Methods
   virtual int Reset(const char *dgst);
   virtual int Update(const char *b, int l);
   virtual int Final();

   // Equality operator
   bool operator==(const XrdCryptoMsgDigest md);
};

#endif
