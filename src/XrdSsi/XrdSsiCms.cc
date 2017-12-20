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

#include <stdio.h>

#include "XrdOuc/XrdOucTList.hh"

#include "XrdSsi/XrdSsiCms.hh"

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

// Allocate an array of teh right size
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
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiCms::~XrdSsiCms()
{
   int i;

   for (i = 0; i < manNum; i++) free(manList[i]);

   delete[] manList;
}
