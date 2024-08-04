/******************************************************************************/
/*                                                                            */
/*                          X r d S s i C m s . c c                           */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>

#include "XrdOuc/XrdOucTList.hh"

#include "XrdSsi/XrdSsiCms.hh"
#include "XrdSsi/XrdSsiStats.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSsiStats    Stats;
}
using namespace XrdSsi;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSsiCms::XrdSsiCms(XrdCmsClient *cmsP) : theCms(cmsP)
{  
   XrdOucTList *tP, *stP = cmsP->Managers();
   char buff[1024];
   int i;

// Count up the number of entries in the manager list
//
   manNum = 0;
   tP     = stP;
   while(tP) {manNum++; tP = tP->next;}

// Allocate an array of the right size
//
   manList = new char*[manNum];

// Format out the managers
//
   for (i = 0; i < manNum; i++)
       {sprintf(buff, "%s:%d", stP->text, stP->val);
        manList[i] = strdup(buff);
        stP = stP->next;
       }
}

/******************************************************************************/
/*                                 A d d e d                                  */
/******************************************************************************/

void XrdSsiCms::Added(const char *name, bool pend)
{
// Do statistics
//
   Stats.Bump(Stats.ResAdds);

// Perform action
//
   if (theCms) theCms->Added(name, pend);
}
  
/******************************************************************************/
/*                               R e m o v e d                                */
/******************************************************************************/

void XrdSsiCms::Removed(const char *name)
{
// Do statistics
//
   Stats.Bump(Stats.ResRems);

// Perform action
//
   if (theCms) theCms->Removed(name);
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiCms::~XrdSsiCms()
{
   int i;

   for (i = 0; i < manNum; i++) free(manList[i]);

   delete[] manList;
}
