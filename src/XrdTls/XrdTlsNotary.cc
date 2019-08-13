/******************************************************************************/
/*                                                                            */
/*                       X r d T l s N o t a r y . c c                        */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdTls/XrdTlsNotary.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
bool XrdTlsNotary::cnOK  = true;

/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/

namespace
{
#include "XrdTls/XrdTlsHostcheck.hh"
#include "XrdTls/XrdTlsHostcheck.icc"
}

#include "XrdTls/XrdTlsNotaryUtils.hh"
#include "XrdTls/XrdTlsNotaryUtils.icc"

/******************************************************************************/
/*                              V a l i d a t e                               */
/******************************************************************************/

const char *XrdTlsNotary::Validate(const SSL *ssl, const char *hName,
                                   XrdNetAddrInfo *addrInfo)
{
   HostnameValidationResult rc;
   bool dnsOK = (addrInfo != 0);
   bool verChk= true;

// Obtain the certificate
//
  X509 *theCert = SSL_get_peer_certificate(ssl);
  if (!theCert) return "certificate not present.";

// Make sure the certificate was verified
//
   if (verChk && (SSL_get_verify_result(ssl) != X509_V_OK))
      {X509_free(theCert);
       return "certificate has not been verified.";
      }

// The first step is to check if the hostname can be verified using the SAN
// extension. Various version of openSSL have ways of doing this but we
// rely on the manual method which works for all versions. Eventually, we
// will migrate to the "standard" way of doing this.
//
   rc = matches_subject_alternative_name(hName, theCert);
   X509_free(theCert);
   if (rc == MatchFound) return 0;

// If a SAN was present then we stop here unless we can use DNS.
//
   if (rc != NoSANPresent && !dnsOK)
      {if (rc == MatchNotFound) return "hostname not in SAN extension.";
       return "malformed SAN extension.";
      }

// If we are allowed to use the common name, try that now.
//
   if (cnOK || dnsOK)
      {rc = matches_common_name(hName, theCert);
       if (rc == MatchFound) return 0;
       if (!dnsOK)
          {if (rc == Error) return "malformed certificate.";
           return "malformed common name.";
          }
      }

// The last resort is to try using DNS if so allowed
//
   if (dnsOK)
      {const char *dnsErr = 0;
       const char *dnsName = addrInfo->Name(0, &dnsErr);
       if (dnsName)
          {if (!strcmp(hName, dnsName)) return 0;
           return "DNS registered name does not match.";
          }
       if (dnsErr) return dnsErr;
       return "host not registered in DNS.";
      }

// Neither DNS nor common name is allowed here. That means there was no SAN.
//
   return "required SAN extension missing.";
}
