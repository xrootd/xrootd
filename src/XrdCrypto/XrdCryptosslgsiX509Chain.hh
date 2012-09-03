#ifndef __CRYPTO_SSLGSIX509CHAIN_H__
#define __CRYPTO_SSLGSIX509CHAIN_H__
/******************************************************************************/
/*                                                                            */
/*           X r d C r y p t o s s l g s i X 5 0 9 C h a i n . h h            */
/*                                                                            */
/* (c) 2005 G. Ganis , CERN                                                   */
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
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Chain of X509 certificates following GSI policy(ies).                      */
/*                                                                            */
/* ************************************************************************** */

#include <XrdCrypto/XrdCryptoX509Chain.hh>

// ---------------------------------------------------------------------------//
//                                                                            //
// XrdCryptosslgsiX509Chain                                                   //
//                                                                            //
// Enforce GSI policies on X509 certificate chains                            //
//                                                                            //
// ---------------------------------------------------------------------------//

const int kOptsRfc3820 = 0x1;

class XrdCryptosslgsiX509Chain : public XrdCryptoX509Chain {

public:
   XrdCryptosslgsiX509Chain(XrdCryptoX509 *c = 0) : XrdCryptoX509Chain(c) { }
   XrdCryptosslgsiX509Chain(XrdCryptosslgsiX509Chain *c) : XrdCryptoX509Chain(c) { }
   virtual ~XrdCryptosslgsiX509Chain() { }

   // Verify chain
   bool Verify(EX509ChainErr &e, x509ChainVerifyOpt_t *vopt = 0);

private:

   // Proxy naming rules 
   bool SubjectOK(EX509ChainErr &e, XrdCryptoX509 *xcer);
};

#endif
