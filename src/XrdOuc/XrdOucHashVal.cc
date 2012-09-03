/******************************************************************************/
/*                                                                            */
/*                      X r d O u c H a s h V a l . c c                       */
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

#include "string.h"
#ifndef WIN32
#include "strings.h"
#endif

unsigned long XrdOucHashVal(const char *KeyVal)
         {extern unsigned long XrdOucHashVal2(const char *, int);
          return XrdOucHashVal2(KeyVal, strlen(KeyVal));
         }

unsigned long XrdOucHashVal2(const char *KeyVal, int KeyLen)
{  int j;
   unsigned long *lp, lword, hval = 0;
   int hl = sizeof(hval);

// If name is shorter than the hash length, use the name.
//
   if (KeyLen <= hl)
      {memcpy(&hval, KeyVal, (size_t)KeyLen);
       return hval;
      }

// Compute the length of the name and develop starting hash.
//
   hval = KeyLen;
   j = KeyLen % hl; KeyLen /= hl;
   if (j) 
      {memcpy(&lword, KeyVal, (size_t)hl);
       hval ^= lword;
      }
   lp = (unsigned long *)&KeyVal[j];

// Compute and return the full hash.
//
   while(KeyLen--)
        {memcpy(&lword, lp++, (size_t)hl);
         hval ^= lword;
        }
   return (hval ? hval : 1);
}
