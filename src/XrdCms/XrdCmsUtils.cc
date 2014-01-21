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
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                              P a r s e M a n                               */
/******************************************************************************/
  
bool XrdCmsUtils::ParseMan(XrdSysError *eDest, XrdOucTList **oldMans,
                           char  *hSpec, char *hPort, int *sPort)
{
   XrdOucTList *newMans, *newP, *oldP, *appList = (oldMans ? *oldMans : 0);
   const char *eText;
   char *plus;
   int nPort, maxIP = 1;

// Check if this is a multi request
//
   if (!(plus = index(hSpec, '+')) || *(plus+1) != 0) plus = 0;
      else {*plus = 0; maxIP = 8;}

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

// Obtain the list
//
   if (!(newMans = XrdNetUtils::Hosts(hSpec, nPort, maxIP, sPort, &eText)))
      {char buff[1024];
       snprintf(buff, sizeof(buff), "; %s", eText);
       eDest->Emsg("Config", "Unable to add host", hSpec, buff);
       return false;
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
         oldP = *oldMans;
         while(oldP)
              {if (newP->val == oldP->val && !strcmp(newP->text, oldP->text))
                  {eDest->Say("Config warning: duplicate manager ",newP->text);
                   delete newP;
                   break;
                  }
               oldP = oldP->next;
              }
         if (!oldP) 
            {newP->next = appList; appList = newP;
             if (plus) eDest->Say("Config ",hSpec," -> all.manager ",newP->text);
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
