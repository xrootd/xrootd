/******************************************************************************/
/*                                                                            */
/*                        X r d N e t P M a r k . c c                         */
/*                                                                            */
/* (c) 2022 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <string.h>

#include "XrdNet/XrdNetPMark.hh"

/******************************************************************************/
/*                               F i n d C g i                                */
/******************************************************************************/

bool XrdNetPMark::FindCgi(const char *cgi, const char *key,
                          const char *&val, size_t &n)
{
   size_t keyLen;

   val = 0;
   n = 0;
   if (!cgi || !*cgi || !key || !(keyLen = strlen(key))) return false;

// The same scan as XrdOucEnv, except an empty value is kept. XrdOucEnv drops
// one, so scitag.flow= would look like a missing key. The name is the whole
// token: note=scitag.flow=1024 is a different key. The last one wins.
//
   bool found = false;
   const char *p = cgi;
   while (*p)
        {while (*p == '&') p++;
         if (!*p) break;
         const char *name = p;
         while (*p && *p != '=' && *p != '&') p++;
         if (*p != '=') continue;
         size_t nlen = static_cast<size_t>(p - name);
         p++;
         const char *v = p;
         while (*p && *p != '&') p++;
         if (nlen == keyLen && !memcmp(name, key, keyLen))
            {found = true;
             val = v;
             n = static_cast<size_t>(p - v);
            }
        }
   return found;
}

/******************************************************************************/
/*                                F l o w I D                                 */
/******************************************************************************/

bool XrdNetPMark::FlowID(const char *val, size_t n, int &flow)
{
   size_t i = 0;

   flow = 0;
   if (!val || !n) return false;
   if (val[0] == '+')
      {if (n == 1) return false;
       i = 1;
      }
   if (val[i] < '0' || val[i] > '9') return false;

// Leading zeros do not change the integer. What remains is at most five
// digits: 65535 is the top of the range, and a longer significand cannot be
// inside it. Stopping there also keeps the multiply inside an int.
//
   while (i < n && val[i] == '0') i++;
   if (i == n || n - i > 5) return false;

   int v = 0;
   for (; i < n; i++)
       {if (val[i] < '0' || val[i] > '9') return false;
        v = v * 10 + (val[i] - '0');
       }
   if (v < minTotID || v > maxTotID) return false;
   flow = v;
   return true;
}

/******************************************************************************/
/*                                 g e t E A                                  */
/******************************************************************************/

bool XrdNetPMark::getEA(const char *cgi, int &ecode, int &acode)
{
   const char *val = 0;
   size_t n = 0;
   int flow = 0;

   ecode = acode = 0;
// A missing key is not a tag. A present value that is not one integer in
// 65..65535, including an empty value, is the 0,0 mark.
//
   if (!FindCgi(cgi, "scitag.flow", val, n)) return false;
   if (FlowID(val, n, flow))
      {ecode = flow >> btsActID;
       acode = flow & mskActID;
      }
   return true;
}
