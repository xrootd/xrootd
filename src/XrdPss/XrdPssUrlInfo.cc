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

#include <iostream>
#include <cstring>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucSid.hh"
#include "XrdOuc/XrdOucTPC.hh"
#include "XrdPss/XrdPssUrlInfo.hh"
#include "XrdPss/XrdPssUtils.hh"
#include "XrdSec/XrdSecEntity.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
bool XrdPssUrlInfo::MapID = false;

/******************************************************************************/
/*                               c o p y C G I                                */
/******************************************************************************/

namespace
{
int copyCGI(const char *cgi, char *Buff, int Blen)
{
   int n;

//std::cerr <<"PSS cgi IN: '" <<cgi <<"' " <<strlen(cgi) <<'\n' <<std::flush;

// Skip over initial ampersands
//
   while(*cgi == '&' && *cgi) cgi++;

// Check if there is anything here
//
   if (!cgi || *cgi == 0) {*Buff = 0; return 0;}

// Copy out all variables omitting the ones that cause trouble
//
   char *bP = Buff;
   const char *beg = cgi;
   do {if (!strncmp(cgi, "xrd.", 4) || !strncmp(cgi, "xrdcl.", 6))
          {int n = cgi - beg - 1;
           if (n > 0)
              {if (n >= Blen) {*bP = 0; return bP - Buff;}
               strncpy(bP, beg, n);
               bP += n; Blen -= n; *bP = 0;
              }
           if ((beg = index(cgi, '&')))
              {cgi = beg+1;
               if (bP == Buff) beg++;
              }
          } else {
            if ((cgi = index(cgi, '&'))) cgi++;
          }
      } while(beg && cgi);

// See if we have the end to copy
//
   if (beg)
      {n = strlen(beg) + 1;
       if (n < Blen)
          {strncpy(bP, beg, Blen);
           bP += n;
          }
      }

// Return length make sure buffer ends with a null
//
   *bP = 0;
//std::cerr <<"PSS cgi OT: '" <<Buff <<"' " <<(bP-Buff) <<'\n' <<std::flush;
   return bP - Buff;
}
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPssUrlInfo::XrdPssUrlInfo(XrdOucEnv  *envP, const char *path,
                             const char *xtra, bool addusrcgi, bool addident)
               : Path(path), CgiUsr(""), CgiUsz(0), CgiSsz(0), sidP(0),
                 eIDvalid(false)
{
   const char *amp1= "", *amp2 = "";

// Preset for no id in the url
//
   *theID  = 0;
    tident = 0;

// If there is an environment point, get user's cgi and set the tident from it
//
   if (envP)
      {if (addusrcgi && !(CgiUsr = envP->Env(CgiUsz))) CgiUsr = "";
       const XrdSecEntity *secP = envP->secEnv();
       if (secP)
          {entityID = secP->ueid;
           eIDvalid = true;
           tident = secP->tident;
          }
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
          else *CgiSfx = 0;
      }
}
  
/******************************************************************************/
/*                                a d d C G I                                 */
/******************************************************************************/

bool XrdPssUrlInfo::addCGI(const char *prot, char *buff, int blen)
{
   bool forXrd = XrdPssUtils::is4Xrootd(prot);

// Short circuit all of this if there is no cgi
//
   if (!CgiUsz && (!CgiSsz || forXrd))
      {*buff = 0;
       return true;
      }

// Make sure that we can fit whatever CGI we have into the buffer. Include the
// implicit question mark and ending null byte.
//
   int n = CgiUsz + (forXrd ? CgiSsz : 0) + 1;
   if (n >= blen) return false;
   *buff++ = '?'; blen--;

// If the protocol is headed to an xroot server then we need to remove any
// offending CGI elements from the user CGI. Otherwise, we can use the CGI
// that was specified by the client.
//
   if (CgiUsz)
      {if (forXrd) n = copyCGI(CgiUsr, buff, blen);
          else {n = CgiUsz;
                strcpy(buff, CgiUsr);
               }
       buff += n; blen -= n;
      }

// If this is destined to an xroot server, add any extended CGI.
//
   if (forXrd && CgiSsz)
      {if (CgiSsz >= blen) return false;
       strcpy(buff, CgiSfx);
      } else *buff = 0;

// All done
//
//std::cerr <<"Final URL: '" <<prot <<"' " <<strlen(prot) <<'\n' <<std::flush;
   return true;
}
  
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
   const char *atP, *colon;

// If we are mapping id then use the entity's idenification
//
   if (MapID && eIDvalid)
      {const char *fmt = (entityID & 0xf0000000 ? "%x@" : "U%x@");
       snprintf(theID,  sizeof(theID), fmt, entityID); // 8+1+nul = 10 bytes
       return;
      }

// Use the connection file descriptor number as the id lgnid.pid:fd@host
//
   if (tid == 0) tid = tident;
   if ((colon = index(tid, ':')) && (atP = index(colon+1, '@')))
      {int n = atP - colon;
       if (n <= (int)sizeof(theID))
          {*theID = 'u';
           strncpy(theID+1, colon+1, n); // Include '@'
           theID[n+1] = 0;
          } else *theID = 0;
       } else *theID = 0;
}
