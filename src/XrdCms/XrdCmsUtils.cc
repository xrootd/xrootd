/******************************************************************************/
/*                                                                            */
/*                        X r d C m s U t i l s . c c                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include "XrdCms/XrdCmsUtils.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
XrdOucTList *GetLocalSite()
             {const char *sname = getenv("XRDSITE");
              if (!sname || !(*sname)) sname = "local";
              return new XrdOucTList(sname);
             }

XrdOucTList *siteList  = 0;
int          siteIndex = 0;
}

/******************************************************************************/
/* Private:                      D i s p l a y                                */
/******************************************************************************/
  
void XrdCmsUtils::Display(XrdSysError *eDest, const char *hSpec,
                                              const char *hName, bool isBad)
{
   XrdNetAddr *nP;
   const char *eTxt, *eSfx = (isBad ? " *** Invalid ***" : 0);
   int i, n, abLen, numIP = 0;
   char *abP, aBuff[1024];

// Get all of the addresses
//
   eTxt = XrdNetUtils::GetAddrs(hName, &nP, numIP, XrdNetUtils::prefAuto, 0);

// Check for errors
//
   if (eTxt)
      {eDest->Say("Config Manager ", hSpec, " -> ", hName, " ", eTxt);
       return;
      }
   eDest->Say("Config Manager ", hSpec, " -> ", hName, eSfx);

// Prepare the buffer
//
   n = strlen(hSpec)+4;
   if (n+64 > (int)sizeof(aBuff)) return;
   memset(aBuff, int(' '), n);
   abP = aBuff+n; abLen = sizeof(aBuff) - n;

// Format the addresses
//
   for (i = 0; i < numIP; i++)
       {if (!nP[i].Format(abP, abLen, XrdNetAddrInfo::fmtAddr,
                                      XrdNetAddrInfo::noPort)) break;
        eDest->Say("Config Manager ", aBuff);
       }

// All done
//
   delete [] nP;
}

/******************************************************************************/
/*                              P a r s e M a n                               */
/******************************************************************************/
  
bool XrdCmsUtils::ParseMan(XrdSysError *eDest, XrdOucTList **oldMans,
                           char  *hSpec, char *hPort, int *sPort, bool hush)
{
   static const size_t maxSNLen = 63;
   XrdOucTList *newMans, *newP, *oldP, *appList = (oldMans ? *oldMans : 0);
   XrdOucTList *sP = siteList;
   const char *eText;
   char *plus, *atsn;
   int nPort, maxIP = 1, snum = 0;
   bool isBad;

// Generate local site name if we haven't done so yet
//
   if (!siteList) siteList = GetLocalSite();

// Handle site qualification first
//
   if ((atsn = index(hPort, '@')))
      {if (*(atsn+1) == '\0')
          {eDest->Emsg("Config", "site name missing for",  hSpec); return 0;}
       *atsn++ = 0;
       if (strlen(atsn) > maxSNLen)
          {eDest->Emsg("Config", "site name too long for", hSpec); return 0;}
       while(sP && strcmp(sP->text, atsn)) sP = sP->next;
       if (sP) snum = sP->val;
          else {siteIndex++;
                siteList = new XrdOucTList(atsn, siteIndex, siteList);
                snum = siteIndex;
               }
      }

// Check if this is a multi request
//
   if (!(plus = index(hSpec, '+')) || *(plus+1) != 0) plus = 0;
      else {*plus = 0; maxIP = 8;
            if (XrdNetAddr::DynDNS())
               {eDest->Emsg("Config", "Hostname globbing is not supported "
                                      "via dynamic DNS!");
                return false;
               }
           }

// Check if the port was specified
//
   if (isdigit(*hPort))
      {if (XrdOuca2x::a2i(*eDest,"manager port",hPort,&nPort,1,65535))
           return false;
      } else {
       if (!(nPort = XrdNetUtils::ServPort(hPort, "tcp")))
          {eDest->Emsg("Config", "Unable to find tcp service",hPort,".");
           return false;
          }
      }

// Obtain the list. We can't fully resolve this now if we are using a dynamic
// DNS so that part will have to wait.
//
   if (XrdNetAddr::DynDNS())
      {if (sPort)
          {XrdNetAddr myAddr(0);
           if (!strcmp(myAddr.Name("???"), hSpec)) *sPort = nPort;
          }
       newMans = new XrdOucTList(hSpec, nPort);
      } else {
       if (!(newMans = XrdNetUtils::Hosts(hSpec, nPort, maxIP, sPort, &eText)))
          {char buff[1024];
           snprintf(buff,sizeof(buff),"'%s'; %c%s",hSpec,tolower(*eText),eText+1);
           eDest->Emsg("Config", "Unable to add host", buff);
           return false;
          }
      }

// If there is no pointer to a list, then the caller merely wanted sPort
//
   if (!oldMans)
      {while((newP = newMans)) {newMans = newMans->next; delete newP;}
       return true;
      }

// Merge new list with old list
//
   while((newP = newMans))
        {newMans = newMans->next;
         newP->ival[1] = snum;
         oldP = *oldMans;
         while(oldP)
              {if (newP->val == oldP->val && !strcmp(newP->text, oldP->text))
                  {eDest->Say("Config warning: duplicate manager ",newP->text);
                   delete newP;
                   break;
                  }
               oldP = oldP->next;
              }
         if (!plus || strcmp(hSpec, newP->text)) isBad = false;
            else {eDest->Say("Config warning: "
                             "Cyclic DNS registration for ",newP->text,"\n"
                             "Config warning: This cluster will exhibit "
                             "undefined behaviour!!!");
             isBad = true;
            }
         if (!oldP) 
            {appList = SInsert(appList, newP);
             if (plus && !hush) Display(eDest, hSpec, newP->text, isBad);
            }
        }

// Set the new list and return
//
   *oldMans = appList;
   return true;
}

/******************************************************************************/
/*                          P a r s e M a n P o r t                           */
/******************************************************************************/
  
char *XrdCmsUtils::ParseManPort(XrdSysError *eDest, XrdOucStream &CFile,
                                char *hSpec)
{
   char *pSpec;

// Screen out IPV6 specifications
//
   if (*hSpec == '[')
      {if (!(pSpec = index(hSpec, ']')))
          {eDest->Emsg("Config", "Invalid manager specification -",hSpec);
           return 0;
          }
      } else pSpec = hSpec;

// Grab the port number if in the host name. Otherwise make sure it follows.
//
         if ((pSpec = index(pSpec, ':')))
            {if (!(*(pSpec+1))) pSpec = 0;
                else *pSpec++ = '\0';
            }
    else if (!(pSpec = CFile.GetWord()) || !strcmp(pSpec, "if")) pSpec = 0;

// We should have a port specification now
//
   if (!pSpec) {eDest->Emsg("Config", "manager port not specified for", hSpec);
                return 0;
               }

// All is well
//
   return strdup(pSpec);
}

/******************************************************************************/
/* Private:                      S I n s e r t                                */
/******************************************************************************/

XrdOucTList *XrdCmsUtils::SInsert(XrdOucTList *oldP, XrdOucTList *newP)
{
   XrdOucTList *fstP = oldP, *preP = 0;

// We insert in logically increasing order
//
   while(oldP && (newP->val < oldP->val || strcmp(newP->text, oldP->text) < 0))
        {preP = oldP; oldP = oldP->next;}

// Insert the new element
//
   if (preP) preP->next = newP;
      else   fstP       = newP;
   newP->next = oldP;

// Return the first element in the list (may have changed)
//
   return fstP;
}
  
/******************************************************************************/
/*                              S i t e N a m e                               */
/******************************************************************************/
  
const char *XrdCmsUtils::SiteName(int snum)
{
   XrdOucTList *sP = siteList;

// Find matching site
//
   while(sP && snum != sP->val) sP = sP->next;

// Return result
//
   return (sP ? sP->text : "anonymous");
}
