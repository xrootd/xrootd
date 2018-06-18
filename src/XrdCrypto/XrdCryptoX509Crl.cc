/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o X 5 0 9 C r l. c c                      */
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
/* Abstract interface for X509 CRLs.                                          */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */
#include <time.h>
#include "XrdCrypto/XrdCryptoX509Crl.hh"

//_____________________________________________________________________________
void XrdCryptoX509Crl::Dump()
{
   // Dump content
   ABSTRACTMETHOD("XrdCryptoX509Crl::Dump");
}

//_____________________________________________________________________________
bool XrdCryptoX509Crl::IsValid()
{
   // Check validity
   ABSTRACTMETHOD("XrdCryptoX509Crl::IsValid");
   return 0;
}

//_____________________________________________________________________________
bool XrdCryptoX509Crl::IsExpired(int when)
{
   // Check expiration at UTC time 'when'. Use when =0 (default) to check
   // at present time.

   int now = (when > 0) ? when : (int)time(0);
   return (now > NextUpdate());
}

//_____________________________________________________________________________
time_t XrdCryptoX509Crl::LastUpdate()
{
   // Time of last update
   ABSTRACTMETHOD("XrdCryptoX509Crl::LastUpdate");
   return -1;
}

//_____________________________________________________________________________
time_t XrdCryptoX509Crl::NextUpdate()
{
   // Time of next update
   ABSTRACTMETHOD("XrdCryptoX509Crl::NextUpdate");
   return -1;
}

//_____________________________________________________________________________
const char *XrdCryptoX509Crl::ParentFile()
{
   // Return parent file name
   ABSTRACTMETHOD("XrdCryptoX509Crl::ParentFile");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509Crl::Issuer()
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509Crl::Issuer");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509Crl::IssuerHash(int)
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509Crl::IssuerHash");
   return (const char *)0;
}

//_____________________________________________________________________________
XrdCryptoX509Crldata XrdCryptoX509Crl::Opaque()
{
   // Return underlying certificate in raw format
   ABSTRACTMETHOD("XrdCryptoX509Crl::Opaque");
   return (XrdCryptoX509Crldata)0;
}

//_____________________________________________________________________________
bool XrdCryptoX509Crl::Verify(XrdCryptoX509 *)
{
   // Verify certificate signature with pub key of ref cert
   ABSTRACTMETHOD("XrdCryptoX509Crl::Verify");
   return 0;
}

//_____________________________________________________________________________
bool XrdCryptoX509Crl::IsRevoked(int, int)
{
   // Verify if certificate with specified serial number has been revoked
   ABSTRACTMETHOD("XrdCryptoX509Crl::IsRevoked");
   return 1;
}

//_____________________________________________________________________________
bool XrdCryptoX509Crl::IsRevoked(const char *, int)
{
   // Verify if certificate with specified serial number has been revoked
   ABSTRACTMETHOD("XrdCryptoX509Crl::IsRevoked");
   return 1;
}
