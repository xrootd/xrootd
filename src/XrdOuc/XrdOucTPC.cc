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
  
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "XrdOuc/XrdOucTPC.hh"
#include "XrdSys/XrdSysDNS.hh"

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
const char *XrdOucTPC::tpcCks = "tpc.cks";
const char *XrdOucTPC::tpcDst = "tpc.dst";
const char *XrdOucTPC::tpcKey = "tpc.key";
const char *XrdOucTPC::tpcLfn = "tpc.lfn";
const char *XrdOucTPC::tpcOrg = "tpc.org";
const char *XrdOucTPC::tpcSrc = "tpc.src";
const char *XrdOucTPC::tpcTtl = "tpc.ttl";

/******************************************************************************/
/*                              c g i C 2 D s t                               */
/******************************************************************************/
  
const char *XrdOucTPC::cgiC2Dst(const char *cKey, const char *xSrc,
                                const char *xLfn, const char *xCks,
                                      char *Buff, int Blen)
{
   tpcInfo Info;
   char   *etext, *bP = Buff;
   int     n;

// Make sure we have the minimum amount of information here
//
   if (!cKey || !xSrc || Blen <= 0) return "!Invalid cgi parameters.";

// Generate the full name of the source
//
   Info.Data = XrdSysDNS::getHostName(xSrc, &etext);
   if (etext) return "!Unable to validate source.";

// Construct the cgi string
//
   n = snprintf(bP, Blen, "%s=%s&%s=%s", tpcKey, cKey, tpcSrc, Info.Data);
   if (xLfn)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcLfn, xLfn);
      }
   if (xCks)
      {bP += n; Blen -= n;
       if (Blen > 1) n = snprintf(bP, Blen, "&%s=%s", tpcCks, xCks);
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
   char   *etext, *bP = Buff;
   int     n;

// Make sure we have the minimum amount of information here
//
   if (!cKey || !xDst || Blen <= 0) return "!Invalid cgi parameters.";

// Generate the full name of the source
//
   Info.Data = XrdSysDNS::getHostName(xDst, &etext);
   if (etext) return "!Unable to validate destination.";

// Construct the cgi string
//
   n = snprintf(Buff, Blen, "%s=%s&%s=%s", tpcKey, cKey, tpcDst, Info.Data);
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
   char xbuff[512];
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
