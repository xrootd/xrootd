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

namespace
{
char *getMyFQN(const char *&myDom, bool &myFQN, const char *&myErr)
{
   XrdNetAddr theAddr;
   XrdOucTList *ifList, *ifNow;
   const char *dnsName, *domP;
   char *theName[2] = {0}, *theDom[2] = {0}, *theIPA[2] = {0}, hName[1025];
   int hnLen;

// Make sure domain is set to something that is valid
//
   myDom = "";

// The identity can be specified via an envar. In this case, it short circuits
// all the subsequent code.
//
   if ((dnsName = getenv("XRDNET_IDENTITY")))
      {if (XrdNetAddrInfo::isHostName(dnsName)
       &&  !(myDom = index(dnsName, '.'))) myDom = "";
       myFQN = false;
       char* tmp = strdup(dnsName);
       XrdOucUtils::toLower(tmp);
       return tmp;
      }
   myFQN = true;

// Obtain the host name, this is mandatory.
//
   if (gethostname(hName, sizeof(hName)))
      {myErr = XrdSysE2T(errno); myDom = 0; return 0;}
   hnLen = strlen(hName);
   XrdOucUtils::toLower(hName);

// First step it to get all IP addresses configured on this machine
//
   if (!XrdNetIF::GetIF(&ifList, &myErr))
      {myDom = ""; return strdup(hName);}

// Run through the interfaces and try to get the hostname associated with
// this machine. Note that we may have public and private addresses and
// they may have different hostname attached. We only accept the hostname
// that matches what is returned by gethostname().
//
   while((ifNow = ifList))
        {int i = (ifNow->sval[1] ? 1 : 0); // Private | public

         if (i >= 0 && theName[i] == 0 && !theAddr.Set(ifNow->text, 0)
         &&  (dnsName = theAddr.Name(0,&myErr)) && (domP = index(dnsName,'.')))
            {int n = domP - dnsName;
             if (n == hnLen && !strncmp(hName, dnsName, n))
                {theName[i] = strdup(dnsName);
                 theDom[i]  = theName[i] + n;
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
  if (myErr == 0) myErr = "no error";

// We prefer the public name should we have it
//
   if (theName[0])
      {if (theName[1]) free(theName[1]);
       myDom = theDom[0];
       return theName[0];
      }
  
// Use the private name should we have it
//
   if (theName[1])
      {myDom = theDom[1];
       return theName[1];
      }

// Concote a name using old-style DNS resolution. This may not work if DNS
// namespaces are being used (e.g. k8s environments) or if the hostname is not
// resolvable. We will catch that here and move on.
//
   if ((myErr = theAddr.Set(hName,0))) dnsName = 0;
      else dnsName = theAddr.Name(0, &myErr);

// Check if this worked
//
   if (dnsName)
      {theName[0] = strdup(dnsName);
       if (!(myDom = index(theName[0], '.'))) myDom = "";
       return theName[0];
      }

// Prefrentially return the hostname as an address as the value of gethostname()
// may actually fail. So, we defer naming the machine until later but we do
// know its IP address and that can be used as an identity. Return the public
// address first. Note that we prefrentially return the IPv6 address here.
//
   if (theIPA[0])
      {if (theIPA[1]) free(theIPA[1]);
       return theIPA[0];
      }
   if (theIPA[1]) return theIPA[1];
   
// Fallback to using the simple unqualified hostname, this still may be OK but
// this is likely to fail in certain situations where DNS is screwed up.
//
   theName[0] = strdup(hName);
   myDom = theName[0] + hnLen;
   return theName[0];
}
}
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

// Note that we are gauranteed that this will be fully initialzed prior
// to any method called that uses these values irrespective of static
// initialization order, even though statically initialized.
  
const char *XrdNetIdentity::DNS_Domain;
const char *XrdNetIdentity::DNS_Error;
      char *XrdNetIdentity::DNS_FQN = getMyFQN(DNS_Domain, FQN_DNS, DNS_Error);
      bool  XrdNetIdentity::FQN_DNS;

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
   if (DNS_FQN) free(DNS_FQN);
   DNS_FQN = strdup(fqn);
   if (!(DNS_Domain = index(DNS_FQN, '.'))) DNS_Domain = "";
   FQN_DNS = false;
}
