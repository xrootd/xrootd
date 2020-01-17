/******************************************************************************/
/*                                                                            */
/*                          X r d O u c T P C . c c                           */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
//#include <iostream>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucTPC.hh"

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
const char *XrdOucTPC::tpcCks = "tpc.cks";
const char *XrdOucTPC::tpcDlg = "tpc.dlg";
const char *XrdOucTPC::tpcDst = "tpc.dst";
const char *XrdOucTPC::tpcKey = "tpc.key";
const char *XrdOucTPC::tpcLfn = "tpc.lfn";
const char *XrdOucTPC::tpcOrg = "tpc.org";
const char *XrdOucTPC::tpcPsh = "tpc.psh";
const char *XrdOucTPC::tpcSgi = "tpc.scgi";
const char *XrdOucTPC::tpcSrc = "tpc.src";
const char *XrdOucTPC::tpcSpr = "tpc.spr";
const char *XrdOucTPC::tpcStr = "tpc.str";
const char *XrdOucTPC::tpcTpr = "tpc.tpr";
const char *XrdOucTPC::tpcTtl = "tpc.ttl";
const char *XrdOucTPC::tpcDlgOn = "tpc.dlgon";

/******************************************************************************/
/*                              c g i C 2 D s t                               */
/******************************************************************************/
  
const char *XrdOucTPC::cgiC2Dst(const char *cKey, const char *xSrc,
                                const char *xLfn, const char *xCks,
                                      char *Buff, int Blen, int strms,
                                const char *iHst,
                                const char *sprt, const char *tprt,
                                      bool dlgon,
                                      bool  push)
{
   tpcInfo Info;
   char    *bP = Buff;
   int     n;

// Make sure we have the minimum amount of information here
//
   if (!cKey || !xSrc || Blen <= 0) return "!Invalid cgi parameters.";

// Generate the full name of the source
//
   if (!cgiHost(Info, xSrc)) return "!Invalid source specification.";

// Construct the cgi string. For the destination we need the full source spec
//
   n = snprintf(bP, Blen, "%s=%s&%s=%s%s%s", tpcKey, cKey, tpcSrc,
                Info.uName, Info.hName, Info.pName);
   if (xLfn)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcLfn, xLfn);
      }
   if (xCks)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcCks, xCks);
      }

   if (strms > 0)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%d", tpcStr, strms);
      }

   if (iHst)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcDlg, iHst);
      }

   if (sprt)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcSpr, sprt);
      }

   if (tprt)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcTpr, tprt);
      }

   if (push)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=1", tpcPsh);
      }

   // tpc.dlgon
      {bP += n; Blen -= n;
       const char* cgistr = dlgon ? "&%s=1" : "&%s=0";
       if (Blen > 1) n = snprintf(bP, Blen, cgistr, tpcDlgOn);
      }


// All done
//
   return (n > Blen ? "!Unable to generate full cgi." : Buff);
}

/******************************************************************************/
/*                              c g i C 2 S r c                               */
/******************************************************************************/
  
const char *XrdOucTPC::cgiC2Src(const char *cKey, const char *xDst, int xTTL,
                                      char *Buff, int Blen)
{
   tpcInfo Info;
   char    *bP = Buff;
   int     n;

// Make sure we have the minimum amount of information here
//
   if (!cKey || !xDst || Blen <= 0) return "!Invalid cgi parameters.";

// Generate the full name of the source
//
   if (!cgiHost(Info, xDst)) return "!Invalid destination specification.";

// Construct the cgi string. The source needs only the dest hostname.
//
   n = snprintf(Buff, Blen, "%s=%s&%s=%s", tpcKey, cKey, tpcDst, Info.hName);
   if (xTTL >= 0)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%d", tpcTtl, xTTL);
      }

// All done
//
   return (n > Blen ? "!Unable to generate full cgi." : Buff);
}

/******************************************************************************/
/*                              c g i D 2 S r c                               */
/******************************************************************************/

const char *XrdOucTPC::cgiD2Src(const char *cKey, const char *cOrg,
                                      char *Buff, int Blen)
{
   int    n;

// Make sure we have the minimum amount of information here
//
   if (!cKey || !cOrg || Blen <= 0) return "!Invalid cgi parameters.";

// Construct the cgi string
//
   n = snprintf(Buff, Blen, "%s=%s&%s=%s", tpcKey, cKey, tpcOrg, cOrg);

// All done
//
   return (n > Blen ? "!Unable to generate full cgi." : Buff);
}

/******************************************************************************/
/*                               c g i H o s t                                */
/******************************************************************************/
  
bool XrdOucTPC::cgiHost(tpcInfo &Info, const char *hSpec)
{
   const char *Colon, *hName;
   XrdNetAddr hAddr;
   char hBuff[256];
   int n;

// Extract out the username, if any
//
   if (!(hName = index(hSpec, '@'))) hName = hSpec;
      else {hName ++;
            n = hName - hSpec;
            if (n >= int(sizeof(Info.User))) return false;
            Info.uName = Info.User;
            strncpy(Info.User, hSpec, n); Info.User[n] = 0;
           }

// Preaccomodate ipv6 addresses
//
   if (*hName != '[') Colon = hName;
      else if (!(Colon = index(hName, ']'))) return 0;


// Extract out the port specification, if any.
//
   if ((Colon = index(Colon, ':')))
      {n = Colon - hName;
       if (n >= int(sizeof(hBuff))) return false;
       Info.pName = Colon;
       strncpy(hBuff, hName, n); hBuff[n] = 0; hName = hBuff;
      }

// Resolve the host name
//
   hAddr.Set(hName,0);
   if ((hName = hAddr.Name())) Info.hName = strdup(hName);
   return hName != 0;
}

/******************************************************************************/
/*                               c o p y C G I                                */
/******************************************************************************/
  
int XrdOucTPC::copyCGI(const char *cgi, char *Buff, int Blen)
{
   const char *bgi;
   char *bP = Buff;
   int xlen;
   bool eqs;

// Skip over initial ampersands
//
   while(*cgi == '&' && *cgi) cgi++;

// Check if there is anything here
//
   if (!cgi || *cgi == 0) {*Buff = 0; return 0;}
   Blen--;

// Copy all keys except system oriented ones.
//
//std::cerr <<"TPC cgi IN: " <<cgi <<'\n' <<std::flush;
do{bgi = cgi; eqs = false;
   while(*cgi != '\t' && *cgi)
        {if (*cgi == '=') eqs = true;
         cgi++;
        }
   if (*bgi && eqs && strncmp(bgi, "tpc.", 4) && strncmp(bgi, "xrd.", 4)
   &&  strncmp(bgi, "xrdcl.", 4))
      {xlen = cgi - bgi;
       if (bP != Buff && Blen > 0) {*bP++ = '&'; Blen--;}
       if (xlen > Blen) xlen = Blen;
       strncpy(bP, bgi, xlen);
       bP   += xlen;
       Blen -= xlen;
      }
   while(*cgi && *cgi == '\t') cgi++;
  } while(*cgi && Blen > 2);

// Compute length and return
//
   *bP = 0;
// std::cerr <<"TPC cgi OT: " <<Buff <<" len=" <<(bP-Buff) <<'\n' <<std::flush;
   return bP - Buff;
}
