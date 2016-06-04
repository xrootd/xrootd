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

XrdSsiMutex                XrdSsiPacer::pMutex;
XrdSsiPacer                XrdSsiPacer::glbQ;
int                        XrdSsiPacer::glbN = 0;

/******************************************************************************/
/*                                  H o l d                                   */
/******************************************************************************/
  
void XrdSsiPacer::Hold(const char *reqID)
{
   XrdSsiMutexMon myLock(pMutex);

// If no request ID given then simply queue this on the global chain
//
   if (!reqID)
      {glbQ.Q_PushBack(this);
       lclQ = 0;
       glbN++;
       return;
      }

// See if we have a local anchor for this request, if not, add one.
//
   lclQ = reqMap.Find(reqID);
   if (!lclQ)
      {lclQ = new XrdSsiPacer;
       reqMap.Add(reqID, lclQ);
      }

// Chain the entry
//
   lclQ->Q_PushBack(this);
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdSsiPacer::Reset()
{
// If we are in a queue then remove ourselves
//
   if (!Singleton())
      {const char *reqID;
       pMutex.Lock();
       Q_Remove();
       if (!lclQ) glbN--;
          else if (lclQ->Singleton() && (reqID=RequestID())) reqMap.Del(reqID);
       pMutex.UnLock();
      }
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/
  
int XrdSsiPacer::Run(int num, const char *reqID)
{
   XrdSsiMutexMon myLock(pMutex);
   XrdSsiPacer   *anchor, *rItem;
   int numRestart = 0;

// Determine which anchor to use
//
   if (!reqID)   anchor = &glbQ;
      else if (!(anchor = reqMap.Find(reqID))) return 0;

// Check if only count wanted
//
   if (!num)
      {if (!reqID) return glbN;
       rItem = anchor->next;
       while(rItem != anchor) {numRestart++; rItem = rItem->next;}
       return numRestart;
      }

// Run as many as wanted
//
   if (num < 0) num = INT_MAX;
   for (int n = 1; n <= num && !(anchor->Singleton()); n++)
       {rItem = anchor->next;
        rItem->Q_Remove();
        rItem->lclQ = 0;
        XrdSsi::schedP->Schedule(rItem);
        numRestart++;
       }

// If this is a local queue, check if we removed the last element
//
   if (reqID && anchor->Singleton()) reqMap.Del(reqID);

// Return number of items we ran
//
   if (!reqID) glbN -= numRestart;
   return numRestart;
}
