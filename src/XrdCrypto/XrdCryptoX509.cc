/******************************************************************************/
/*                                                                            */
/*                       X r d C r y p t o X 5 0 9 . c c                      */
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
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Abstract interface for X509 certificates.                                  */
/* Allows to plug-in modules based on different crypto implementation         */
/* (OpenSSL, Botan, ...)                                                      */
/*                                                                            */
/* ************************************************************************** */
#include <time.h>

#include "XrdCrypto/XrdCryptoX509.hh"
#include "XrdCrypto/XrdCryptoTrace.hh"

const char *XrdCryptoX509::ctype[4] = { "Unknown", "CA", "EEC", "Proxy" };

#define kAllowedSkew 600

//_____________________________________________________________________________
void XrdCryptoX509::Dump()
{
   // Dump content
   EPNAME("X509::Dump");

   // Time strings
   struct tm tst;
   char stbeg[256] = {0};
   time_t tbeg = NotBefore();
   localtime_r(&tbeg,&tst);
   asctime_r(&tst,stbeg);
   stbeg[strlen(stbeg)-1] = 0;
   char stend[256] = {0};
   time_t tend = NotAfter();
   localtime_r(&tend,&tst);
   asctime_r(&tst,stend);
   stend[strlen(stend)-1] = 0;

   PRINT("+++++++++++++++ X509 dump +++++++++++++++++++++++");
   PRINT("+");
   PRINT("+ File:    "<<ParentFile());
   PRINT("+");
   PRINT("+ Type: "<<Type());
   PRINT("+ Serial Number: "<<SerialNumber());
   PRINT("+ Subject: "<<Subject());
   PRINT("+ Subject hash: "<<SubjectHash(0));
   PRINT("+ Issuer:  "<<Issuer());
   PRINT("+ Issuer hash:  "<<IssuerHash(0));
   PRINT("+");
   if (IsExpired()) {
      PRINT("+ Validity: (expired!)");
   } else {
      PRINT("+ Validity:");
   }
   PRINT("+ NotBefore:  "<<tbeg<<" UTC - "<<stbeg);
   PRINT("+ NotAfter:   "<<tend<<" UTC - "<<stend);
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
int XrdCryptoX509::BitStrength()
{
   // Return number of bits in key
   ABSTRACTMETHOD("XrdCryptoX509::BitStrength");
   return -1;
}

//_____________________________________________________________________________
bool XrdCryptoX509::IsValid(int when)
{
   // Check validity at local time 'when'. Use when =0 (default) to check
   // at present time.

   int now = (when > 0) ? when : (int)time(0);
   // Correct for time zone (certificate times are UTC plus, eventually, DST
   now -= XrdCryptoTZCorr();
   return (now >= (NotBefore()-kAllowedSkew) && now <= NotAfter());
}

//_____________________________________________________________________________
bool XrdCryptoX509::IsExpired(int when)
{
   // Check expiration at UTC time 'when'. Use when =0 (default) to check
   // at present time.

   int now = (when > 0) ? when : (int)time(0);
   // Correct for time zone (certificate times are UTC plus, eventually, DST
   now -= XrdCryptoTZCorr();
   return (now > NotAfter());
}

//_____________________________________________________________________________
int XrdCryptoX509::NotBefore()
{
   // Begin-validity time in secs since Epoch
   ABSTRACTMETHOD("XrdCryptoX509::NotBefore");
   return -1;
}

//_____________________________________________________________________________
int XrdCryptoX509::NotAfter()
{
   // End-validity time in secs since Epoch
   ABSTRACTMETHOD("XrdCryptoX509::NotAfter");
   return -1;
}

//_____________________________________________________________________________
const char *XrdCryptoX509::Subject()
{
   // Return subject name
   ABSTRACTMETHOD("XrdCryptoX509::Subject");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509::ParentFile()
{
   // Return parent file name
   ABSTRACTMETHOD("XrdCryptoX509::ParentFile");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509::Issuer()
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509::Issuer");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509::SubjectHash(int)
{
   // Return subject name
   ABSTRACTMETHOD("XrdCryptoX509::SubjectHash");
   return (const char *)0;
}

//_____________________________________________________________________________
const char *XrdCryptoX509::IssuerHash(int)
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509::IssuerHash");
   return (const char *)0;
}

//_____________________________________________________________________________
XrdCryptoX509data XrdCryptoX509::Opaque()
{
   // Return underlying certificate in raw format
   ABSTRACTMETHOD("XrdCryptoX509::Opaque");
   return (XrdCryptoX509data)0;
}

//_____________________________________________________________________________
XrdCryptoRSA *XrdCryptoX509::PKI()
{
   // Return PKI key of the certificate
   ABSTRACTMETHOD("XrdCryptoX509::PKI");
   return (XrdCryptoRSA *)0;
}

//_____________________________________________________________________________
void XrdCryptoX509::SetPKI(XrdCryptoX509data)
{
   // Set PKI

   ABSTRACTMETHOD("XrdCryptoX509::SetPKI");
}

//_____________________________________________________________________________
kXR_int64 XrdCryptoX509::SerialNumber()
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509::SerialNumber");
   return -1;
}

//_____________________________________________________________________________
XrdOucString XrdCryptoX509::SerialNumberString()
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509::SerialNumberString");
   return XrdOucString("");
}

//_____________________________________________________________________________
XrdCryptoX509data XrdCryptoX509::GetExtension(const char *)
{
   // Return issuer name
   ABSTRACTMETHOD("XrdCryptoX509::GetExtension");
   return (XrdCryptoX509data)0;
}

//_____________________________________________________________________________
XrdSutBucket *XrdCryptoX509::Export()
{
   // EXport in form of bucket
   ABSTRACTMETHOD("XrdCryptoX509::Export");
   return (XrdSutBucket *)0;
}

//_____________________________________________________________________________
bool XrdCryptoX509::Verify(XrdCryptoX509 *)
{
   // Verify certificate signature with pub key of ref cert
   ABSTRACTMETHOD("XrdCryptoX509::Verify");
   return 0;
}

//_____________________________________________________________________________
int XrdCryptoX509::DumpExtensions()
{
   // Dump extensions, if any
   ABSTRACTMETHOD("XrdCryptoX509::DumpExtensions");
   return -1;
}
