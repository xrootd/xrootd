#ifndef __XRDSSIEVENT_HH__
#define __XRDSSIEVENT_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i E v e n t . h h                         */
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

#include "Xrd/XrdJob.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdSsi/XrdSsiAtomics.hh"

class XrdSsiEvent : public XrdJob, public XrdCl::ResponseHandler
{
public:

        void AddEvent(XrdCl::XRootDStatus *st, XrdCl::AnyObject *resp);

        void ClrEvent() {evMutex.Lock();ClrEvent(&thisEvent);evMutex.UnLock();}

virtual void DoIt();

virtual void HandleResponse(XrdCl::XRootDStatus *status,
                            XrdCl::AnyObject *response)
                           {AddEvent(status, response);}

virtual int  XeqEvent(XrdCl::XRootDStatus *st, XrdCl::AnyObject **resp) = 0;

virtual void XeqEvFin() = 0;

             XrdSsiEvent() : XrdJob(tident),  lastEvent(0),
                             running(false),  isClear(true)
                             {*tident = 0;}

            ~XrdSsiEvent() {if (!isClear) ClrEvent(&thisEvent);}

protected:

char   tident[24]; //"c %u#%u" with %u max 10 digits

private:
struct EventData
      {XrdCl::XRootDStatus *status;
       XrdCl::AnyObject    *response;
       EventData           *next;

       void Move2(EventData &dest) {dest.status   = status;   status   = 0;
                                    dest.response = response; response = 0;
                                    dest.next     = next;     next     = 0;
                                   }

       EventData(XrdCl::XRootDStatus *st=0, XrdCl::AnyObject *resp=0)
                : status(st), response(resp), next(0) {}
      ~EventData() {}
      };

void          ClrEvent(EventData *fdP);

XrdSsiMutex   evMutex;
EventData     thisEvent;
EventData    *lastEvent;
bool          running;
bool          isClear;
static
EventData    *freeEvent;
};
#endif
