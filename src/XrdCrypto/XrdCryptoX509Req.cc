/******************************************************************************/
/*                                                                            */
/*                  X r d C r y p t o X 5 0 9 R e q. c c                      */
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
/* Abstract interface for X509 certificates requests.                         */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */

#include "XrdCrypto/XrdCryptoX509Req.hh"
#include "XrdCrypto/XrdCryptoTrace.hh"

//_____________________________________________________________________________
void XrdCryptoX509Req::Dump()
{
   // Dump content
   EPNAME("X509Req::Dump");

   PRINT("+++++++++++++++ X509 request dump ++++++++++++++++");
   PRINT("+");
   PRINT("+ Subject: "<<Subject());
   PRINT("+ Subject hash: "<<SubjectHash(0));
   PRINT("+");
   if (PKI()) {
      PRINT("+ PKI: "<<PKI()->Status());
   } else {
      PRINT("+ PKI: missing");
   }
   PRINT("+");
   PRINT("+++++++++++++++++++++++++++++++++++++++++++++++++");
}

//_____________________________________________________________________________
bool XrdCryptoX509Req::IsValid()
{
   // Check validity
   ABSTRACTMETHOD("XrdCryptoX509Req::IsValid");
   return 0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509Req::Subject()
{
   // Return subject name
   ABSTRACTMETHOD("XrdCryptoX509Req::Subject");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509Req::SubjectHash(int)
{
   // Return subject name
   ABSTRACTMETHOD("XrdCryptoX509Req::SubjectHash");
   return (const char *)0;
}

//_____________________________________________________________________________
XrdCryptoX509Reqdata XrdCryptoX509Req::Opaque()
{
   // Return underlying certificate in raw format
   ABSTRACTMETHOD("XrdCryptoX509Req::Opaque");
   return (XrdCryptoX509Reqdata)0;
}

//_____________________________________________________________________________
XrdCryptoRSA *XrdCryptoX509Req::PKI()
{
   // Return PKI key of the certificate
   ABSTRACTMETHOD("XrdCryptoX509Req::PKI");
   return (XrdCryptoRSA *)0;
}

//_____________________________________________________________________________
XrdCryptoX509Reqdata XrdCryptoX509Req::GetExtension(const char *)
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509Req::GetExtension");
   return (XrdCryptoX509Reqdata)0;
}

//_____________________________________________________________________________
XrdSutBucket *XrdCryptoX509Req::Export()
{
   // EXport in form of bucket
   ABSTRACTMETHOD("XrdCryptoX509Req::Export");
   return (XrdSutBucket *)0;
}

//_____________________________________________________________________________
bool XrdCryptoX509Req::Verify()
{
   // Verify certificate signature with pub key of ref cert
   ABSTRACTMETHOD("XrdCryptoX509Req::Verify");
   return 0;
}
