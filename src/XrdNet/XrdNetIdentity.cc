/******************************************************************************/
/*                                                                            */
/*                     X r d N e t I d e n t i t y . h h                      */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>
#include <unistd.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetIdentity.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysE2T.hh"

/******************************************************************************/
/*        O n e   T i m e   S t a t i c   I n i t i a l i z a t i o n         */
/******************************************************************************/

// Note that we are gauranteed that this will be fully initialzed prior
// to any method called that uses these values irrespective of static
// initialization order, even though statically initialized.

static char DNS_FQN[256];       // Fully qualified hostname
static const char *DNS_Domain;  // Starts with leading dot, points into DNS_FQN!
static const char *DNS_Error;   // Error indicator for debugging only

static bool getMyFQN()
{
// Initialize name, domain, and error to empty strings
//
   memset(DNS_FQN, '\0', sizeof(DNS_FQN));
   DNS_Domain = DNS_FQN;
   DNS_Error = nullptr;

// The identity can be specified via an envar. In this case, it short circuits
// all the subsequent code.
//
   const char *dnsName = nullptr;
   if ((dnsName = getenv("XRDNET_IDENTITY")))
      {strlcpy(DNS_FQN, dnsName, sizeof(DNS_FQN));
       XrdOucUtils::toLower(DNS_FQN);
       if (XrdNetAddrInfo::isHostName(DNS_FQN) && !(DNS_Domain = index(DNS_FQN, '.')))
         DNS_Domain = "";
       return false;
      }

// Obtain the host name, this is mandatory.
//
   if (gethostname(DNS_FQN, sizeof(DNS_FQN)))
      {DNS_Error = XrdSysE2T(errno); DNS_Domain = ""; return false;}

   int hnLen = strlen(DNS_FQN);
   XrdOucUtils::toLower(DNS_FQN);

// First step it to get all IP addresses configured on this machine
//
   XrdOucTList *ifList = nullptr;
   if (!XrdNetIF::GetIF(&ifList, &DNS_Error))
      {DNS_Domain = ""; return true;}

// Run through the interfaces and try to get the hostname associated with
// this machine. Note that we may have public and private addresses and
// they may have different hostname attached. We only accept the hostname
// that matches what is returned by gethostname().
//
   XrdNetAddr theAddr;
   const char *domP = nullptr;
   char *theIPA[2]  = { nullptr, nullptr };
   char *theName[2] = { nullptr, nullptr };
   XrdOucTList *ifNow = nullptr;

   while((ifNow = ifList))
        {int i = (ifNow->sval[1] ? 1 : 0); // Private | public

         if (i >= 0 && theName[i] == 0 && !theAddr.Set(ifNow->text, 0)
         &&  (dnsName = theAddr.Name(0,&DNS_Error)) && (domP = index(dnsName,'.')))
            {int n = domP - dnsName;
             if (n == hnLen && !strncmp(DNS_FQN, dnsName, n))
                {theName[i] = strdup(dnsName);
                } else {
                 if (theIPA[i]) free(theIPA[i]);
                 theIPA[i] = strdup(ifNow->text);
               }
            }
         ifList = ifList->next;
         delete ifNow;
        }

// Fix up error pointer
//
  if (DNS_Error == 0) DNS_Error = "no error";

// We prefer the public name should we have it
//
   if (theName[0])
      {strlcpy(DNS_FQN, theName[0], sizeof(DNS_FQN));
       goto done;
      }

// Use the private name should we have it
//
   if (theName[1])
      {strlcpy(DNS_FQN, theName[1], sizeof(DNS_FQN));
       goto done;
      }

// Concote a name using old-style DNS resolution. This may not work if DNS
// namespaces are being used (e.g. k8s environments) or if the hostname is not
// resolvable. We will catch that here and move on.
//
   if ((DNS_Error = theAddr.Set(DNS_FQN,0))) dnsName = nullptr;
      else dnsName = theAddr.Name(0, &DNS_Error);

// Check if this worked
//
   if (dnsName)
      {strlcpy(DNS_FQN, dnsName, sizeof(DNS_FQN));
       goto done;
      }

// Prefrentially return the hostname as an address as the value of gethostname()
// may actually fail. So, we defer naming the machine until later but we do
// know its IP address and that can be used as an identity. Return the public
// address first. Note that we prefrentially return the IPv6 address here.
//
   if (theIPA[0])
      {strlcpy(DNS_FQN, theIPA[0], sizeof(DNS_FQN));
       goto done;
      }

   if (theIPA[1])
      {strlcpy(DNS_FQN, theIPA[1], sizeof(DNS_FQN));
       goto done;
      }

// Fallback to using the simple unqualified hostname, this still may be OK but
// this is likely to fail in certain situations where DNS is screwed up.
//
   DNS_Domain = "";
   return true;

done:
   free(theName[0]);
   free(theName[1]);
   free(theIPA[0]);
   free(theIPA[1]);

   if (!(DNS_Domain = index(DNS_FQN, '.')))
     DNS_Domain = "";
   return true;
}

// True if the FQN is configured for this host
static bool FQN_DNS = getMyFQN();

/******************************************************************************/
/*                                D o m a i n                                 */
/******************************************************************************/
  
const char *XrdNetIdentity::Domain(const char **eText)
{
   if (eText) *eText = DNS_Error;
   return DNS_Domain;
}

/******************************************************************************/
/*                                   F Q N                                    */
/******************************************************************************/
  
const char *XrdNetIdentity::FQN(const char **eText)
{
   if (eText) *eText = DNS_Error;
   return DNS_FQN;
}

/******************************************************************************/
/*                                s e t F Q N                                 */
/******************************************************************************/
  
void XrdNetIdentity::SetFQN(const char *fqn)
{
   strlcpy(DNS_FQN, fqn, sizeof(DNS_FQN));
   if (!(DNS_Domain = index(DNS_FQN, '.'))) DNS_Domain = "";
   FQN_DNS = false;
}
