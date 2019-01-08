/******************************************************************************/
/*                                                                            */
/*                     X r d S s i S e s s R e a l . c c                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <netinet/in.h>
  
#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiRRAgent.hh"
#include "XrdSsi/XrdSsiRRInfo.hh"
#include "XrdSsi/XrdSsiScale.hh"
#include "XrdSsi/XrdSsiServReal.hh"
#include "XrdSsi/XrdSsiSessReal.hh"
#include "XrdSsi/XrdSsiTaskReal.hh"
#include "XrdSsi/XrdSsiTrace.hh"
#include "XrdSsi/XrdSsiUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "Xrd/XrdScheduler.hh"

using namespace XrdSsi;

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define SINGLETON(dlvar, theitem)\
               theitem ->dlvar .next == theitem
  
#define INSERT(dlvar, curitem, newitem) \
               newitem ->dlvar .next = curitem; \
               newitem ->dlvar .prev = curitem ->dlvar .prev; \
               curitem ->dlvar .prev-> dlvar .next = newitem; \
               curitem ->dlvar .prev = newitem

#define REMOVE(dlbase, dlvar, curitem) \
               if (dlbase == curitem) dlbase = (SINGLETON(dlvar,curitem) \
                                             ? 0   : curitem ->dlvar .next);\
               curitem ->dlvar .prev-> dlvar .next = curitem ->dlvar .next;\
               curitem ->dlvar .next-> dlvar .prev = curitem ->dlvar .prev;\
               curitem ->dlvar .next = curitem;\
               curitem ->dlvar .prev = curitem

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
   std::string dsProperty("DataServer");
   const char *tident = 0;
}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace XrdSsi
{
extern XrdScheduler *schedP;

extern XrdSysError   Log;
extern XrdSsiScale   sidScale;
}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
class CleanUp : public XrdJob
{
public:

void  DoIt() {sessP->Lock();
              sessP->Unprovision();
              delete this;
             }

      CleanUp(XrdSsiSessReal *sP) : sessP(sP) {}
     ~CleanUp() {}

private:
XrdSsiSessReal *sessP;
};
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdSsiSessReal::~XrdSsiSessReal()
{
   XrdSsiTaskReal *tP;

   if (resKey)   free(resKey);
   if (sessName) free(sessName);
   if (sessNode) free(sessNode);

   while((tP = freeTask)) {freeTask = tP->attList.next; delete tP;}
}

/******************************************************************************/
/*                           I n i t S e s s i o n                            */
/******************************************************************************/
  
void XrdSsiSessReal::InitSession(XrdSsiServReal *servP, const char *sName,
                                 int uent, bool hold)
{
   requestP  = 0;
   uEnt      = uent;
   attBase   = 0;
   freeTask  = 0;
   myService = servP;
   nextTID   = 0;
   alocLeft  = XrdSsiRRInfo::idMax;
   isHeld    = hold;
   inOpen    = false;
   noReuse   = false;
   if (resKey) {free(resKey); resKey = 0;}
   if (sessName) free(sessName);
   sessName  = (sName ? strdup(sName) : 0);
   if (sessNode) free(sessNode);
   sessNode  = 0;
}

/******************************************************************************/
/* Private:                      N e w T a s k                                */
/******************************************************************************/

// Must be called with sessMutex locked!
  
XrdSsiTaskReal *XrdSsiSessReal::NewTask(XrdSsiRequest *reqP)
{
   EPNAME("NewTask");
   XrdSsiTaskReal *ptP, *tP;

// Allocate a task object for this request
//
   if ((tP = freeTask)) freeTask = tP->attList.next;
      else {if (!alocLeft || !(tP = new XrdSsiTaskReal(this)))
               {XrdSsiUtils::RetErr(*reqP, "Too many active requests.", EMLINK);
                return 0;
               }
            alocLeft--;
           }

// We always set a new task ID to avoid ID collisions. his is good for over
// 194 days if we have 1 request/second. In practice. this will work for a
// couple of years before wrapping. By then the ID's should be free.
//
   tP->SetTaskID(nextTID++);
   nextTID &= XrdSsiRRInfo::idMax;

// Initialize the task and return its pointer
//
   tP->Init(reqP, reqP->GetTimeOut());
   DEBUG("Task=" <<tP <<" processing id=" <<nextTID-1);

// Insert the task into our list of tasks
//
   if ((ptP = attBase)) {INSERT(attList, ptP, tP);}
      else attBase = tP;

// We will be using the session mutex for serialization. Afterwards, bind the
// task to the request and return the task pointer.
//
   XrdSsiRRAgent::SetMutex(reqP, &sessMutex);
   tP->BindRequest(*reqP);
   return tP;
}

/******************************************************************************/
/*                             P r o v i s i o n                              */
/******************************************************************************/

bool XrdSsiSessReal::Provision(XrdSsiRequest *reqP, const char *epURL)
{
   EPNAME("Provision");
   XrdCl::XRootDStatus epStatus;
   XrdSsiMutexMon rHelp(&sessMutex);

// Issue the open and if the open was started, return success.
//
   DEBUG("Provisioning " <<epURL);
   epStatus = epFile.Open((const std::string)epURL,
                          XrdCl::OpenFlags::Read, (XrdCl::Access::Mode)0,
                          (XrdCl::ResponseHandler *)this,
                          reqP->GetTimeOut());

// If there was an error, scuttle the request. Note that errors will be returned
// on a separate thread to avoid hangs here.
//
   if (!epStatus.IsOK())
      {std::string eTxt;
       int         eNum = XrdSsiUtils::GetErr(epStatus, eTxt);
       XrdSsiUtils::RetErr(*reqP, eTxt.c_str(), eNum);
       XrdSsi::sidScale.retEnt(uEnt);
       return false;
      }

// Queue a new task and indicate our state
//
   NewTask(reqP);
   inOpen = true;
   return true;
}

/******************************************************************************/
/* Private:                      R e l T a s k                                */
/******************************************************************************/
  
void XrdSsiSessReal::RelTask(XrdSsiTaskReal *tP) // sessMutex locked!
{
   EPNAME("RelTask");

// Do some debugging here
//
   DEBUG((isHeld ? "Recycling" : "Deleting")<<" task="<<tP<<" id=" <<tP->ID());

// Delete this task or place it on the free list
//
   if (!isHeld) delete tP;
      else {tP->attList.next = freeTask;
            freeTask = tP;
           }
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/

bool XrdSsiSessReal::Run(XrdSsiRequest *reqP)
{
   XrdSsiMutexMon sessMon(sessMutex);
   XrdSsiTaskReal *tP;

// If we are not allowed to be reused, return to indicated try someone else
//
   if (noReuse) return false;

// Reserve a stream ID. If we cannot then indicate we cannot be reused
//
   if (!XrdSsi::sidScale.rsvEnt(uEnt)) return false;

// Queue a new task
//
   tP = NewTask(reqP);

// If we are already open and we have a task, send off the request
//
   if (!inOpen && tP && !tP->SendRequest(sessNode)) noReuse = true;
   return true;
}
  
/******************************************************************************/
/* Private:                     S h u t d o w n                               */
/******************************************************************************/

// Called with sessMutex locked and return with it unlocked
  
void XrdSsiSessReal::Shutdown(XrdCl::XRootDStatus &epStatus, bool onClose)
{
   XrdSsiTaskReal *tP, *ntP = freeTask;

// Delete all acccumulated tasks
//
   while((tP = ntP)) {ntP = tP->attList.next; delete tP;}
   freeTask = 0;

// If the close failed then we cannot recycle this object as it is not reusable
//
   if (onClose && !epStatus.IsOK())
      {std::string  eText;
       int          eNum = XrdSsiUtils::GetErr(epStatus, eText);
       char         mBuff[1024];
       snprintf(mBuff, sizeof(mBuff), "Unprovision: %s@%s error; %d",
                       sessName, sessNode, eNum);
       Log.Emsg("Shutdown", mBuff, eText.c_str());
       sessMutex.UnLock();
       myService->Recycle(this, false);
      } else {
       if (sessName) {free(sessName); sessName = 0;}
       if (sessNode) {free(sessNode); sessNode = 0;}
       sessMutex.UnLock();
       myService->Recycle(this, !noReuse);
      }
}
  
/******************************************************************************/
/*                          T a s k F i n i s h e d                           */
/******************************************************************************/
  
void XrdSsiSessReal::TaskFinished(XrdSsiTaskReal *tP)
{
// Lock our mutex
//
   sessMutex.Lock();

// Remove task from the task list if it's in it
//
   if (tP == attBase || tP->attList.next != tP)
      {REMOVE(attBase, attList, tP);}

// Clear any pending task events and decrease active count
//
   tP->ClrEvent();

// Return the request entry number
//
   XrdSsi::sidScale.retEnt(uEnt);

// Place the task on the free list. If we can shutdown, then unprovision which
// will drive a shutdown. The returns without the sessMutex, otherwise we must
// unlock it before we return.
//
   RelTask(tP);
   if (!isHeld && !attBase) Unprovision();
      else sessMutex.UnLock();
}

/******************************************************************************/
/*                                U n H o l d                                 */
/******************************************************************************/
  
void XrdSsiSessReal::UnHold(bool cleanup)
{
   XrdSsiMutexMon sessMon(sessMutex);

// Immediately stopo reuse of this object
//
   if (isHeld && resKey && myService) myService->StopReuse(resKey);

// Turn off the hold flag and if we have no attached tasks, schedule shutdown
//
   isHeld = false;
   if (cleanup && !attBase) XrdSsi::schedP->Schedule(new CleanUp(this));
}

/******************************************************************************/
/* Private:                  U n p r o v i s i o n                            */
/******************************************************************************/

// Called with sessMutex locked and returns with it unlocked
  
void XrdSsiSessReal::Unprovision() // Called with sessMutex locked!
{
   EPNAME("Unprovision");
   XrdCl::XRootDStatus uStat;

// Clear any pending events
//
   DEBUG("Closing " <<sessName);
   ClrEvent();

// If the file is not open (it might be due to an open error) then do a
// shutdown right away. Otherwise, try to close if successful the event
// handler will do the shutdown, Otherwise, we do a Futterwacken dance.
//
   if (!epFile.IsOpen()) Shutdown(uStat, false);
      else {uStat = epFile.Close((XrdCl::ResponseHandler *)this);
            if (!uStat.IsOK()) Shutdown(uStat, true);
               else sessMutex.UnLock();
           }
}

/******************************************************************************/
/*                              X e q E v e n t                               */
/******************************************************************************/
  
bool XrdSsiSessReal::XeqEvent(XrdCl::XRootDStatus *status,
                              XrdCl::AnyObject   **respP)
{
// Lock out mutex. Note that events like shutdown unlock the mutex
//
   sessMutex.Lock();
   XrdSsiTaskReal *ztP, *ntP, *tP = attBase;

// If we are not in the open phase then this is due to a close event. Simply
// do a shutdown and return to stop event processing.
//
   if (!inOpen)
      {Shutdown(*status, true); // sessMutex gets unlocked!
       return false;
      }

// We are no longer in open. However, if open encounetered an error then this
// session cannot be reused because the file object is in a bad state.
//
   inOpen  = false;
   noReuse = !status->IsOK();

// If we have no requests then we may want to simply shoutdown.
// Note that shutdown and unprovision unlock the sessMutex.
//
   if (!tP)
      {if (isHeld)
          {sessMutex.UnLock();
           return false;
          }
       if (!status->IsOK()) Shutdown(*status, false);
          else {if (!isHeld) Unprovision();
                   else sessMutex.UnLock();
               }
       return false;
      }

// We are here because the open finally completed. If the open failed, then
// schedule an error for all pending tasks. The Finish() call on each will
// drive the cleanup of this session.
//
   if (!status->IsOK())
      {XrdSsiErrInfo eInfo;
       XrdSsiUtils::SetErr(*status, eInfo);
       do {tP->SchedError(&eInfo); tP = tP->attList.next;}
          while(tP != attBase);
       sessMutex.UnLock();
       return false;
      }

// Obtain the endpoint name
//
   std::string currNode;
   if (epFile.GetProperty(dsProperty, currNode))
      {if (sessNode) free(sessNode);
       sessNode = strdup(currNode.c_str());
      } else sessNode = strdup("Unknown!");

// Execute each pending request. Make sure not to reference the task object
// chain pointer after invoking SendRequest() as it may become invalid.
//
   ztP = attBase;
   do {ntP = tP->attList.next;
       if (!tP->SendRequest(sessNode)) noReuse = true;
       tP = ntP;
      } while(tP != ztP);

// We are done, field the next event
//
   sessMutex.UnLock();
   return true;
}
