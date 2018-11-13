/******************************************************************************/
/*                                                                            */
/*                      X r d P s s U r l I n f o . c c                       */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucSid.hh"
#include "XrdPss/XrdPssUrlInfo.hh"
#include "XrdSec/XrdSecEntity.hh"

/******************************************************************************/
/*                                E x t e n d                                 */
/******************************************************************************/

bool XrdPssUrlInfo::Extend(const char *cgi, int cgiln)
{
   const char *amp = (*cgi == '&' ? "" : "&");
   int blen = sizeof(CgiSfx) - CgiSsz;

   if (blen <= cgiln) return false;
   int n = snprintf(&CgiSfx[CgiSsz], blen, "%s%s", amp, cgi);
   if (n >= blen) return false;
   CgiSsz += n;
   return true;
}
  
/******************************************************************************/
/*                                 s e t I D                                  */
/******************************************************************************/
  
void XrdPssUrlInfo::setID(const char *tid)
{
   const char *colon;
   char *atP;

   if (tid == 0) tid = tident;
   if ((colon = index(tid, ':')))
      {*theID = 'u';
       strncpy(theID+1, colon+1, sizeof(theID)-2);
       theID[sizeof(theID)-1] = 0;
       if ((atP = index(theID, '@'))) *(atP+1)=0;
          else *theID = 0;
      } else *theID = 0;
}

/******************************************************************************/
/* Private:                        S e t u p                                  */
/******************************************************************************/
  
void XrdPssUrlInfo::Setup(XrdOucEnv *envP, const char *xtra,
                    bool addusrcgi, bool addident)
{
   const char *amp1= "", *amp2 = "";

// Preset for no id in the url
//
   *theID  = 0;
   *CgiSfx = 0;

// If there is an environment point, get user's cgi and set the tident from it
//
   if (envP)
      {if (addusrcgi)
          {CgiUsr = envP->Env(CgiUsz);
           if (!CgiUsz) CgiUsr = "";
              else while(*CgiUsr == '&') {CgiUsr++; CgiUsz--;}
          }
           const XrdSecEntity *secP = envP->secEnv();
           if (secP) tident = secP->tident;
      }

// Make sure we have a tident
//
   if (!tident) tident = "unk.0:0@host";

// Generate additional cgi information as needed
//
   if (*xtra && *xtra != '&') amp2 = "&";
   if (CgiUsz) amp1 = "&";

   if (addident)
      {CgiSsz = snprintf(CgiSfx, sizeof(CgiSfx),
                         "%spss.tid=%s%s%s", amp1, tident, amp2, xtra);
      } else {
       if (*xtra) CgiSsz = snprintf(CgiSfx, sizeof(CgiSfx), "%s%s", amp1, xtra);
      }
}
