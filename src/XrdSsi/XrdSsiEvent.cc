/******************************************************************************/
/*                                                                            */
/*                        X r d S s i E v e n t . c c                         */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSsi/XrdSsiEvent.hh"
#include "XrdSsi/XrdSsiTrace.hh"
#include "Xrd/XrdScheduler.hh"

using namespace XrdSsi;

/******************************************************************************/
/*                     S t a t i c s   &   G l o b a l s                      */
/******************************************************************************/
  
namespace
{
XrdSsiMutex             frMutex;
}

XrdSsiEvent::EventData *XrdSsiEvent::freeEvent = 0;

namespace XrdSsi
{
extern XrdScheduler *schedP;
}

/******************************************************************************/
/*                              A d d E v e n t                               */
/******************************************************************************/
  
void XrdSsiEvent::AddEvent(XrdCl::XRootDStatus *st, XrdCl::AnyObject *resp)
{
   EPNAME("AddEvent");
   XrdSsiMutexMon monMutex(evMutex);

// Indicate there is pending event here
//
   DEBUG("New event: isClear=" <<isClear <<" running=" <<running);
   isClear = false;

// If the base object has no status then we need to set it and schedule
// ourselves for processing if not already running.
//
   if (!thisEvent.status)
      {thisEvent.status   = st;
       thisEvent.response = resp;
       if (!running)
          {running = true;
           XrdSsi::schedP->Schedule(this);
          }
       return;
      }

// Allocate a new event object and chain it from the base event. This also
// implies that we doesn't need to be scheduled as it already was scheduled.
//
   frMutex.Lock();
   EventData *edP = freeEvent;
   if (!edP) edP = new EventData(st, resp);
      else {freeEvent     = edP->next;
            edP->status   = st;
            edP->response = resp;
            edP->next     = 0;
           }
   frMutex.UnLock();

// Establish the last event
//
   if (lastEvent) lastEvent->next = edP;
      else        thisEvent .next = edP;
   lastEvent = edP;
}

/******************************************************************************/
/*                              C l r E v e n t                               */
/******************************************************************************/
  
void XrdSsiEvent::ClrEvent(XrdSsiEvent::EventData *fdP)
{
   EPNAME("ClrEvent");
   EventData *xdP, *edP = fdP;

// This method may be safely called on a undeleted EventData object even if
// this event object has been deleted; as can happen in XeqEvent().
// Clear any chained events. This loop ends with edP pointing to the last event.
//
   while(edP->next)
        {edP = edP->next;
         delete edP->status;
         delete edP->response;
        }

// Place all chained elements, if any, in the free list
//
   if (fdP->next)
      {frMutex.Lock();
       xdP = fdP->next; edP->next = freeEvent; freeEvent = xdP;
       frMutex.UnLock();
       fdP->next = 0;
      }

// Clear the base event
//
   if (fdP->status)   {delete fdP->status;   fdP->status   = 0;}
   if (fdP->response) {delete fdP->response; fdP->response = 0;}

// If we are clearing our events then indicate we are not running. Note that
// this method is only called when cleaning up so we can't be running. We don't
// trace clears on event copies as they always occur.
//
   if (fdP == &thisEvent)
      {DEBUG("Self running=" <<running);
       lastEvent = 0;
       running   = false;
       isClear   = true;
      }
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdSsiEvent::DoIt()
{
   EPNAME("RunEvent");
   EventData *edP, myEvent;
   int rc;

// Process all of the events in our list. This is a tricky proposition because
// the event executor may delete us upon return. Hence we do not directly use
// any data members of this class, only copies. The return rc tells what to do.
// rc > 0: terminate event processing and conclude in a normal fashion.
// rc = 0: reflect next event.
// rc < 0: immediately return as this object has become invalid.
//
   evMutex.Lock();
do{thisEvent.Move2(myEvent);
   lastEvent = 0;
   isClear   = true;
   evMutex.UnLock();
   edP = &myEvent;

   do {if ((rc = XeqEvent(edP->status, &edP->response)) != 0) break;
       edP = edP->next;
      } while(edP);

   ClrEvent(&myEvent);

   if (rc)
      {DEBUG("XeqEvent requested " <<(rc < 0 ? "halt" : "flush"));
       if (rc < 0) return;
       evMutex.Lock();
       break;
      }

   evMutex.Lock();
  } while(thisEvent.status);

// Indicate we are no longer running
//
   running = false;
   evMutex.UnLock();

// The last thing we need to do is to tell the event handler that we are done
// as it may decide to delete this object if no more events will occur.
//
   XeqEvFin();
}
