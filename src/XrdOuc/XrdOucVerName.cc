/******************************************************************************/
/*                                                                            */
/*                      X r d O u c V e r N a m e . c c                       */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdVersionPlugin.hh"

#include "XrdOuc/XrdOucVerName.hh"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
namespace
{
static const char *StrictName[] = XrdVERSIONPLUGINSTRICT;
}

/******************************************************************************/
/*                               V e r s i o n                                */
/******************************************************************************/

int XrdOucVerName::Version(const char *piVers, const char *piPath, bool &noFBK,
                                 char *buff,         int   blen)
{
   const char *Dot, *Slash, *fName;
   int         n, pLen;

// Find the markers in the passed path
//
   if ((Slash = rindex(piPath, '/')))
           {pLen = Slash-piPath+1; Dot = rindex(Slash+1, '.'); fName = Slash+1;}
      else {pLen = 0;              Dot = rindex(piPath,  '.'); fName = piPath;}
   if (Dot) pLen += Dot-fName;
      else {pLen += strlen(fName); Dot = "";}

// Test strict naming and return result
//
   n = 0;
   while(StrictName[n] && strcmp(fName, StrictName[n])) n++;
   noFBK = (StrictName[n] != 0);

// Format the versioned name
//
   n = snprintf(buff, blen-1, "%.*s-%s%s", pLen, piPath, piVers, Dot);

// Return result
//
   return (n < blen ? n : 0);
}
