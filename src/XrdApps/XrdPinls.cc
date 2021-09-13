/******************************************************************************/
/*                                                                            */
/*                           X r d P i n l s . c c                            */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This utility prints plugin version requirements. Syntax:

   xrdpinls

*/

/******************************************************************************/
/*                         i n c l u d e   f i l e s                          */
/******************************************************************************/
  
#include <cctype>
#include <iostream>
#include <map>
#include <cstdio>
#include <cstring>

#include "XrdVersionPlugin.hh"

/******************************************************************************/
/*                         L o c a l   O b j e c t s                          */
/******************************************************************************/
  
namespace
{
struct cmp_str
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};
}

/******************************************************************************/
/*                               D i s p l a y                                */
/******************************************************************************/
  
void Display(const char *drctv, XrdVersionPlugin *vP)
{
   const char *vType = "Unknown";
   char buff[80];

// First determine what kind of rule this is
//
        if (vP->vProcess == XrdVERSIONPLUGIN_DoNotChk) vType = "Untested";
   else if (vP->vProcess == XrdVERSIONPLUGIN_Optional) vType = "Optional";
   else if (vP->vProcess == XrdVERSIONPLUGIN_Required) vType = "Required";

// Establish minimum version
//
   if (vP->vMinLow < 0) snprintf(buff, sizeof(buff), "%2d.x ", vP->vMajLow);
      else snprintf(buff, sizeof(buff), "%2d.%-2d", vP->vMajLow, vP->vMinLow);

// Output the line
//
   std::cout <<vType <<" >= "<<buff <<' ' <<drctv <<std::endl;
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   XrdVersionPlugin vInfo[] = {XrdVERSIONPLUGINRULES};
   XrdVersionMapD2P dInfo[] = {XrdVERSIONPLUGINMAPD2P};
   std::map<const char *, XrdVersionPlugin*, cmp_str> vRules;
   std::map<const char *, XrdVersionPlugin*, cmp_str> dRules;
   std::map<const char *, XrdVersionPlugin*, cmp_str>::iterator itD, itV;
   int i;

// Map all of plugin rules by plugin object creator
//
   i = 0;
   while(vInfo[i].pName)
        {vRules[vInfo[i].pName] = &vInfo[i];
         i++;
        }

// Now for each directive, find the matching rule
//
   i = 0;
   while(dInfo[i].dName)
        {itV = vRules.find(dInfo[i].pName);
         dRules[dInfo[i].dName] = (itV != dRules.end() ? itV->second : 0);
         i++;
        }

// Now display the results
//
   for (itD = dRules.begin(); itD != dRules.end(); itD++)
       {if (itD->second) Display(itD->first, itD->second);
           else std::cout <<"No version rule present for " <<itD->first
                          <<std::endl;
       }

// All done
//
   return(0);
}
