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

namespace
{
// One integer: an optional '+', then digits. The whole value, so 144abc is
// not 144. Longer than this cannot be in 65..65535 and would overflow.
const size_t maxDigits = 18;

bool WholeFlow(const char *val, size_t n, long &flow)
{
   size_t i = 0;

   if (!n) return false;
   if (val[0] == '+')
      {i = 1;
       if (i == n) return false;
      }
   if (n - i > maxDigits) return false;

   long v = 0;
   for (; i < n; i++)
       {if (val[i] < '0' || val[i] > '9') return false;
        v = v * 10 + (val[i] - '0');
       }
   flow = v;
   return true;
}
}

/******************************************************************************/
/*                                 g e t E A                                  */
/******************************************************************************/

bool XrdNetPMark::getEA(const char *cgi, int &ecode, int &acode)
{
   static const char key[] = "scitag.flow";
   const size_t keyLen = sizeof(key) - 1;

   ecode = acode = 0;
   if (!cgi || !*cgi) return false;

// Walk &-separated pairs, the same way XrdOucEnv does. The name has to be
// scitag.flow: note=scitag.flow=1024 is a different key. The last one wins.
// A present value that is not one integer in 65..65535 is the 0,0 mark.
// A missing key is not a tag.
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
         const char *val = p;
         while (*p && *p != '&') p++;
         size_t vlen = static_cast<size_t>(p - val);
         if (!nlen || !vlen) continue;
         if (nlen != keyLen || memcmp(name, key, keyLen)) continue;

         found = true;
         ecode = acode = 0;
         long flow = 0;
         if (WholeFlow(val, vlen, flow)
         &&  flow >= minTotID && flow <= maxTotID)
            {ecode = static_cast<int>(flow) >> btsActID;
             acode = static_cast<int>(flow) & mskActID;
            }
        }
   return found;
}
