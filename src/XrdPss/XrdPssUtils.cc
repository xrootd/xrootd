/******************************************************************************/
/*                                                                            */
/*                        X r d P s s U t i l s . c c                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <strings.h>

#include "XrdPss/XrdPssUtils.hh"
  
/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

namespace
{
   struct pEnt {const char *pname; int pnlen;} pTab[] =
               {{ "https://", 8},  { "http://", 7},
                { "roots://", 8},  { "root://", 7},
                {"xroots://", 9},  {"xroot://", 8}
               };
   int pTNum = sizeof(pTab)/sizeof(pEnt);
   int xrBeg = 2;
}
  
/******************************************************************************/
/*                             g e t D o m a i n                              */
/******************************************************************************/

const char *XrdPssUtils::getDomain(const char *hName)
{
   const char *dot = index(hName, '.');

   if (dot) return dot+1;
   return hName;
}

/******************************************************************************/
/*                             i s 4 X r o o t d                              */
/******************************************************************************/

bool XrdPssUtils::is4Xrootd(const char *pname)
{
// Find out of protocol is for xroot protocol
//
   if (*pname == 'x' || *pname == 'r')
      for (int i = xrBeg; i < pTNum; i++)
          if (!strncmp(pname, pTab[i].pname, pTab[i].pnlen)) return true;
   return false;
}
  
/******************************************************************************/
/*                               v a l P r o t                                */
/******************************************************************************/

const char *XrdPssUtils::valProt(const char *pname, int &plen, int adj)
{
   int i;

// Find a match
//
   for (i = 0; i < pTNum; i++)
       {if (!strncmp(pname, pTab[i].pname, pTab[i].pnlen-adj)) break;}
   if (i >= pTNum) return 0;
   plen = pTab[i].pnlen-adj;
   return pTab[i].pname;
}
  
/******************************************************************************/
/*                             V e c t o r i z e                              */
/******************************************************************************/

bool XrdPssUtils::Vectorize(char *str, std::vector<char *> &vec, char sep)
{
   char *seppos;

// Get each element and place it in the vecor. Null elements are not allowed.
//
   do {seppos = index(str, sep);
       if (seppos)
          {if (!(*(seppos+1))) return false;
           *seppos = '\0';
          }
       if (!strlen(str)) return false;
       vec.push_back(str);
       str = seppos+1;
      } while(seppos && *str);
   return true;
}
