/******************************************************************************/
/*                                                                            */
/*                      X r d C r y p t o L i t e . c c                       */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <errno.h>
#include <string.h>

#include "XrdCrypto/XrdCryptoLite.hh"

/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/

/* This is simply a landing pattern for all supported crypto methods; to avoid
   requiring the client to include specific implementation include files. Add
   your implementation in the following way:
   1. Define an external function who's signature follows:
      XrdCryptoLite *XrdCryptoLite_New_xxxx(const char Type)
      where 'xxxx' corresponds to the passed Name argument.
   2. Insert the extern to the function.
   3. Insert the code segment that calls the function.
*/
  
XrdCryptoLite *XrdCryptoLite::Create(int &rc, const char *Name, const char Type)
{
   extern XrdCryptoLite *XrdCryptoLite_New_bf32(const char Type);
   XrdCryptoLite *cryptoP = 0;

   if (!strcmp(Name, "bf32"))     cryptoP = XrdCryptoLite_New_bf32(Type);

// Return appropriately
//
   rc = (cryptoP ? 0 : EPROTONOSUPPORT);
   return cryptoP;
}
