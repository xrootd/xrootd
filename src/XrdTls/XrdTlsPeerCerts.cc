/******************************************************************************/
/*                                                                            */
/*                    X r d T l s P e e r C e r t s . c c                     */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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

#include "XrdTls/XrdTlsPeerCerts.hh"

/******************************************************************************/
/*         L o c a l   C o m p a t a b i l i t y   F u n c t i o n s          */
/******************************************************************************/
  
// Version of OpenSSL < 1.1 do not have X509_up_ref() so we need to implement
// it using basic functions which only appear in 1.0.x. What a hack!
//
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include <openssl/crypto.h>
namespace
{
int X509_up_ref(X509 *cert)
{
#ifdef CRYPTO_add
   if (cert && (CRYPTO_add(&(cert->references), 1, CRYPTO_LOCK_X509)) > 1)
      return 1;
#endif

   return 0;
}
}
#else
#include <openssl/x509.h>
#endif

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdTlsPeerCerts::~XrdTlsPeerCerts()
{
// Free the peer cert
//
if (cert) X509_free(cert);

// Free the chain (we don't have to as only get1 call creates a copy.
//
// if (chain) sk_X509_pop_free(chain, X509_free);
}

/******************************************************************************/
/*                               g e t C e r t                                */
/******************************************************************************/
  
X509 *XrdTlsPeerCerts::getCert(bool upref)
{
// If we have a cert and we need to up the reference, do so. Note that upref
// may fail; in which case we return a nil pointer to avoid a future segv.
//
   if (cert && upref && !X509_up_ref(cert)) return 0;
   return cert;
}
