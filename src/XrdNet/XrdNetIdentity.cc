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
#include "XrdSys/XrdSysE2T.hh"

/******************************************************************************/
/*        O n e   T i m e   S t a t i c   I n i t i a l i z a t i o n         */
/******************************************************************************/

namespace
{
char *getMyFQN(const char *&myDom, const char *&myErr)
{
   XrdNetAddr theAddr;
   XrdOucTList *ifList, *ifNow;
   const char *dnsName, *domP;
   char *theName[2] = {0}, *theDom[2] = {0}, hName[256];
   int hnLen;

// Obtain the host name, this is mandatory.
//
   if (gethostname(hName, sizeof(hName)))
      {myErr = XrdSysE2T(errno); myDom = 0; return 0;}
   hnLen = strlen(hName);

// First step it to get all IP addresses configured on this machine
//
   if (!XrdNetIF::GetIF(&ifList, &myErr))
      {myDom = 0; return strdup(hName);}

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

// Concote a name using old-style DNS resolution. This may not work if
// DNS namespaces are being used (e.g. k8s environments).
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

// Fallback to using the simple unqualified hostname, this still mae OK
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
const char *XrdNetIdentity::DNS_FQN = getMyFQN(DNS_Domain, DNS_Error);

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
