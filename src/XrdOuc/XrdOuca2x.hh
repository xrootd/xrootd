#ifndef __OOUC_A2X__
#define __OOUC_A2X__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c a 2 x . h h                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysError.hh"

// This class is a holding area for various conversion utility routines
//

class XrdOuca2x
{
public:
static int a2i( XrdSysError &, const char *emsg, const char *item, int *val, int minv=-1, int maxv=-1);
static int a2ll(XrdSysError &, const char *emsg, const char *item, long long *val, long long minv=-1, long long maxv=-1);
static int a2fm(XrdSysError &, const char *emsg, const char *item, int *val, int minv);
static int a2fm(XrdSysError &, const char *emsg, const char *item, int *val, int minv, int maxv);
static int a2sn(XrdSysError &Eroute, const char *emsg, const char *item,
                int *val, int nScale, int minv=-1, int maxv=-1);
static int a2sp(XrdSysError &, const char *emsg, const char *item, long long *val, long long minv=-1, long long maxv=-1);
static int a2sz(XrdSysError &, const char *emsg, const char *item, long long *val, long long minv=-1, long long maxv=-1);
static int a2tm(XrdSysError &, const char *emsg, const char *item, int *val, int minv=-1, int maxv=-1);
static int a2vp(XrdSysError &, const char *emsg, const char *item, int *val, int minv=-1, int maxv=-1);

static int b2x(const unsigned char* src, int slen, char* dst, int dlen);
static int x2b(const char* src, int slen, unsigned char* dst, int dlen,
               bool radj=false);

private:
static int Emsg(XrdSysError &Eroute, const char *etxt1, const char *item,
                                     const char *etxt2, double    val);
static int Emsg(XrdSysError &Eroute, const char *etxt1, const char *item,
                                     const char *etxt2, int       val);
static int Emsg(XrdSysError &Eroute, const char *etxt1, const char *item,
                                     const char *etxt2, long long val);
};

#endif
