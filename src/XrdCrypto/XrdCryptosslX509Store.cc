/******************************************************************************/
/*                                                                            */
/*               X r d C r y p t o s s l X 5 0 9 S t o r e . c c              */
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
/* OpenSSL implementation of XrdCryptoX509Store                               */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptosslX509Store.hh"


//_____________________________________________________________________________
XrdCryptosslX509Store::XrdCryptosslX509Store(XrdCryptoX509 *xca) :
                       XrdCryptoX509Store()
{
   // Constructor

   chain = 0;
   store = X509_STORE_new();
   if (store) {
      // Init with CA certificate
      X509_STORE_set_verify_cb_func(store,0);
      // add CA certificate
      X509_STORE_add_cert(store,xca->cert);
      // Init chain
      if (!(chain = sk_X509_new_null())) {
         // Cleanup, if init failure
         X509_STORE_free(store);
         store = 0;
      }
   }
}

//_____________________________________________________________________________
bool XrdCryptosslX509Store::IsValid()
{
   // Test validity

   return (store && chain);
}

//_____________________________________________________________________________
void XrdCryptoX509Store::Dump()
{
   // Dump content
   ABSTRACTMETHOD("XrdCryptoX509Store::Dump");
}

//_____________________________________________________________________________
int XrdCryptoX509Store::Import(XrdSutBucket *bck)
{
   // Import certificates contained in bucket bck, if any

   ABSTRACTMETHOD("XrdCryptoX509Store::Add");
   return -1;
}

//_____________________________________________________________________________
bool XrdCryptoX509Store::Verify()
{
   // Verify certicate chain stored
   ABSTRACTMETHOD("XrdCryptoX509Store::Verify");
   return -1;
}
