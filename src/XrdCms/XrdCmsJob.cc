/******************************************************************************/
/*                                                                            */
/*                          X r d C m s J o b . c c                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
 
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdSys/XrdSysHeaders.hh"

#include "XrdCms/XrdCmsJob.hh"
#include "XrdCms/XrdCmsProtocol.hh"
#include "XrdCms/XrdCmsRRData.hh"
#include "XrdCms/XrdCmsTrace.hh"

using namespace XrdCms;

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdCms
{
extern XrdScheduler               *Sched;
};

       XrdSysMutex      XrdCmsJob::JobMutex;
       XrdCmsJob       *XrdCmsJob::JobStack = 0;

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdCmsJob *XrdCmsJob::Alloc(XrdCmsProtocol *Proto, XrdCmsRRData *Data)
{
   XrdCmsJob *jp;

// Grab a protocol object and, if none, return a new one
//
   JobMutex.Lock();
   if ((jp = JobStack)) JobStack = jp->JobLink;
      else jp = new XrdCmsJob();
   JobMutex.UnLock();

// Copy relevant sections to the newly allocated protocol object
//
   if (jp)
      {jp->theProto = Proto;
       jp->theData  = Data;
       jp->Comment  = Proto->myRole;
       Proto->Link->setRef(1);
      } else Say.Emsg("Job","No more job objects to serve",Proto->Link->Name());

// All done
//
   return jp;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdCmsJob::DoIt()
{
   int rc;

// Simply execute the method on the data. If operation started and we have to
// wait foir it, simply reschedule ourselves for a later time.
//
   if ((rc = theProto->Execute(*theData)))
      if (rc == -EINPROGRESS)
         {Sched->Schedule((XrdJob *)this, theData->waitVal+time(0)); return;}
   theProto->Ref(-1);
   Recycle();
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdCmsJob::Recycle()
{

// Dereference the link at this point
//
   theProto->Link->setRef(-1);

// Release the data buffer
//
   if (theData) {XrdCmsRRData::Objectify(theData); theData = 0;}

// Push ourselves on the stack
//
   JobMutex.Lock();
   JobLink  = JobStack;
   JobStack = this;
   JobMutex.UnLock();
}
