/******************************************************************************/
/*                                                                            */
/*                        X r d S s i P a c e r . c c                         */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <limits.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSsi/XrdSsiPacer.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace XrdSsi
{
extern XrdScheduler *schedP;
}
  
/******************************************************************************/
/*                         L o c a l   O b j e c t s                          */
/******************************************************************************/
  
namespace
{
XrdOucHash<XrdSsiPacer>    reqMap;
}

XrdSsiMutex                XrdSsiPacer::pMutex(XrdSsiMutex::Recursive);
XrdSsiPacer                XrdSsiPacer::glbQ;

/******************************************************************************/
/*                                  H o l d                                   */
/******************************************************************************/
  
void XrdSsiPacer::Hold(const char *reqID)
{
   XrdSsiMutexMon myLock(pMutex);

// Establish correct anchor
//
   if (!reqID) theQ = &glbQ;
      else if (!(theQ = reqMap.Find(reqID)))
              {theQ = new XrdSsiPacer;
               reqMap.Add(reqID, theQ);
              }

// Before queing, check we can actually run this right away
//
   if (theQ->aCnt)
      {XrdSsi::schedP->Schedule(this);
       theQ->aCnt--;
       if (reqID && theQ->Singleton() && theQ->aCnt == 0) reqMap.Del(reqID);
      } else theQ->Q_PushBack(this);
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdSsiPacer::Reset()
{
   XrdSsiMutexMon myLock(pMutex);

// If we are in a queue then remove ourselves
//
   if (!Singleton())
      {Q_Remove();
       if (theQ && theQ != &glbQ)
          {const char *reqID = RequestID();
           if (reqID && theQ->Singleton() && theQ->aCnt == 0) reqMap.Del(reqID);
          }
      }
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/
  
void XrdSsiPacer::Run(XrdSsiRequest::RDR_Info &rInfo,
                      XrdSsiRequest::RDR_How   rHow, const char *reqID)
{
   XrdSsiMutexMon myLock(pMutex);
   XrdSsiPacer   *anchor, *rItem;
   int allowed;

// Determine which anchor to use
//
        if (!reqID) anchor = &glbQ;
   else if ((anchor = reqMap.Find(reqID))) {}
   else if (rHow == XrdSsiRequest::RDR_One || rHow == XrdSsiRequest::RDR_Post)
           {anchor = new XrdSsiPacer;
            reqMap.Add(reqID, anchor);
           }
   else return;

// Preset the information we will return
//
   rInfo.iAllow = allowed = anchor->aCnt;

// Process as request
//
   switch(rHow)
         {case XrdSsiRequest::RDR_All:
               allowed = anchor->qCnt;
               break;
          case XrdSsiRequest::RDR_Hold:
               rInfo.qCount = anchor->qCnt;
               rInfo.fAllow = 0;
               anchor->aCnt = 0;
               return;
               break;
          case XrdSsiRequest::RDR_Immed:
               allowed = 1;
               break;
          case XrdSsiRequest::RDR_Query:
               rInfo.fAllow = rInfo.iAllow;
               rInfo.qCount = anchor->qCnt;
               return;
               break;
          case XrdSsiRequest::RDR_One:
               allowed = 1;
               break;
          case XrdSsiRequest::RDR_Post:
               allowed++;
               break;
          default: return; break;
         }

// Run responses
//
   while(allowed && anchor->qCnt)
        {rItem = anchor->next;
         rItem->Q_Remove();
         XrdSsi::schedP->Schedule(rItem);
         rInfo.rCount++;
         allowed--;
        }

// Set returned information
//
   rInfo.qCount = anchor->qCnt;
   if (rHow != XrdSsiRequest::RDR_Immed) anchor->aCnt = allowed;
   rInfo.fAllow = anchor->aCnt;

// If this is a local queue, check if we removed the last element
//
   if (reqID && anchor->Singleton() && anchor->aCnt == 0) reqMap.Del(reqID);
}
