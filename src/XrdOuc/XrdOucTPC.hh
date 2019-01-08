#ifndef __XRDOUCTPC_HH__
#define __XRDOUCTPC_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c T P C . h h                           */
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

#include <stdlib.h>

class XrdOucTPC
{
public:

static
const char *cgiC2Dst(const char *cKey, const char *xSrc, const char *xLfn,
                     const char *xCks,       char *Buff, int Blen, int strms=0,
                     const char *iHst=0, const char *sprt=0, const char *tprt=0,
                     bool push=false);

static
const char *cgiC2Src(const char *cKey, const char *xDst, int xTTL,
                           char *Buff, int Blen);

static
const char *cgiD2Src(const char *cKey, const char *cOrg,
                           char *Buff, int Blen);

static const char *tpcCks;
static const char *tpcDlg;
static const char *tpcDst;
static const char *tpcKey;
static const char *tpcLfn;
static const char *tpcOrg;
static const char *tpcPsh;
static const char *tpcSpr;
static const char *tpcSrc;
static const char *tpcStr;
static const char *tpcTpr;
static const char *tpcTtl;

            XrdOucTPC() {}
           ~XrdOucTPC() {}
private:

struct tpcInfo
      {const char *uName;
             char *hName;
       const char *pName;
             char  User[256];

             tpcInfo() : uName(""), hName(0), pName("") {}
            ~tpcInfo() {if (hName) free(hName);}
      };

static bool cgiHost(tpcInfo &Info, const char *hSpec);
};
#endif
