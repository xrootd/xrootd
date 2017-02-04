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
#include "XrdSsi/XrdSsiRRInfo.hh"
#include "XrdSsi/XrdSsiUtils.hh"
#include "XrdSsi/XrdSsiScale.hh"
#include "XrdSsi/XrdSsiServReal.hh"
#include "XrdSsi/XrdSsiSessReal.hh"
#include "XrdSsi/XrdSsiTaskReal.hh"
#include "XrdSsi/XrdSsiTrace.hh"
#include "XrdSsi/XrdSsiUtils.hh"

#include "XrdSys/XrdSysHeaders.hh"

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
extern XrdSsiScale sidScale;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdSsiSessReal::~XrdSsiSessReal()
{
   XrdSsiTaskReal *tP;

   if (sessName) free(sessName);
   if (sessNode) free(sessNode);

   while((tP = freeTask)) {freeTask = tP->attList.next; delete tP;}
}

/******************************************************************************/
/* Private:                      E x e c u t e                                */
/******************************************************************************/
  
// Called with sessMutex locked!

bool XrdSsiSessReal::Execute(XrdSsiRequest *reqP)
{
   XrdCl::XRootDStatus Status;
   XrdSsiRRInfo        rrInfo;
   XrdSsiTaskReal     *tP, *ptP;
   char               *reqBuff;
   int                 reqBlen;

// Get the request information
//
   reqBuff = reqP->GetRequest(reqBlen);

// Allocate a task object for this request
//
   if (!(tP = NewTask(reqP))) return false;

// Construct the info for this request
//
   rrInfo.Id(tP->ID());
   rrInfo.Size(reqBlen);

// Issue the write
//
   Status = epFile.Write(rrInfo.Info(), (uint32_t)reqBlen, reqBuff,
                         (XrdCl::ResponseHandler *)tP, reqP->GetTimeOut());

// Determine ending status. If it's bad, return an error.
//
   if (!Status.IsOK())
      {std::string eText;
       int eNum = XrdSsiUtils::GetErr(Status, eText);
       RelTask(tP);
       SetErrResponse(eText.c_str(), eNum);
       return false;
      }

// Insert the task into our list of tasks
//
   if ((ptP = attBase)) {INSERT(attList, ptP, tP);}
      else attBase = tP;

// We now need to change the binding to the task.
//
   tP->BindRequest(*reqP);
   numAT++;
   return true;
}
  
/******************************************************************************/
/*                              F i n i s h e d                               */
/******************************************************************************/

// Note that if we are called then Finished() must have been called while we
// were still in the open phase or in the task dispatch phase.
  
void XrdSsiSessReal::Finished(XrdSsiRequest        &rqstR,
                              const XrdSsiRespInfo &rInfo, bool cancel)
{
   EPNAME("SessReqFin");

// Document this call as it rarely happens at this point
//
   DEBUG("Request="<<&rqstR<<" cancel="<<cancel);

// Get the session lock
//
   sessMutex.Lock();

// If a task is already attached then we need to forward this finish to it.
// Otherwise, clear the pending request pointer to scuttle this.
//
   if (numAT) attBase->Finished(rqstR, rInfo, cancel);
      else {if (requestP) XrdSsi::sidScale.retEnt(uEnt);
            requestP = 0;
            if (doStop)
               {XrdCl::XRootDStatus zStat;
                epFile.IsOpen() ? Unprovision() : Shutdown(zStat);
                return;
               }
           }

// Unlock our mutex as we are done here
//
   sessMutex.UnLock();
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
   alocLeft  = XrdSsiRRInfo::maxID;
   isHeld    = hold;
   doStop    = false;
   inOpen    = false;
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
   XrdSsiTaskReal *tP;

// Allocate a task object for this request
//
   if ((tP = freeTask)) freeTask = tP->attList.next;
      else {if (!alocLeft || !(tP = new XrdSsiTaskReal(this, nextTID)))
               {SetErrResponse("Too many active requests.", EMLINK);
                return 0;
               }
            alocLeft--; nextTID++;
           }

// Initialize the task and return its pointer
//
   tP->Init(reqP, reqP->GetTimeOut());
   DEBUG("Task=" <<tP <<" processing id=" <<nextTID-1);
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

// If there was an error, scuttle the request
//
   if (!epStatus.IsOK())
      {std::string eTxt;
       int         eNum = XrdSsiUtils::GetErr(epStatus, eTxt);
       XrdSsiUtils::RetErr(*requestP, eTxt.c_str(), eNum);
       XrdSsi::sidScale.retEnt(uEnt);
       return false;
      }

// We succeeded. So, bind to this request so we can respond with any errors
//
   inOpen   = true;
   requestP = reqP;
   BindRequest(*reqP);
   return true;
}

/******************************************************************************/
/* Private:                      R e l T a s k                                */
/******************************************************************************/
  
void XrdSsiSessReal::RelTask(XrdSsiTaskReal *tP) // sessMutex locked!
{

// Delete this task or place it on the free list
//
   if (!isHeld) delete tP;
      else {tP->attList.next = freeTask;
            freeTask = tP;
           }
}

/******************************************************************************/
/* Private:                     S h u t d o w n                               */
/******************************************************************************/

// Called with sessMutex locked and return with it unlocked
  
void XrdSsiSessReal::Shutdown(XrdCl::XRootDStatus &epStatus)
{

// If the close failed then we cannot recycle this object as it is not reusable
//?? Future: notify service of this if we are being held.
//
   if (!epStatus.IsOK() && !inOpen)
      {std::string  eText;
       int          eNum = XrdSsiUtils::GetErr(epStatus, eText);

       cerr <<"Unprovision "<<sessName<<'@'<<sessNode<<" error; "<<eNum
            <<' ' <<eText<<"\n"<<flush;
       sessMutex.UnLock();
      } else {
       if (sessName) {free(sessName); sessName = 0;}
       if (sessNode) {free(sessNode); sessNode = 0;}
       sessMutex.UnLock();
       myService->Recycle(this);
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

// Clear asny pending task events and decrease active count
//
   tP->ClrEvent();
   numAT--;

// Return the request entry number
//
   XrdSsi::sidScale.retEnt(uEnt);

// Place the task on the free list. If we can shutdown, then unprovision which
// will drive a shutdown. The returns without the sessMutex, otherwise we must
// unlock it before we return.
//
   RelTask(tP);
   if (!isHeld && numAT < 1) Unprovision();
      else sessMutex.UnLock();
}

/******************************************************************************/
/* Private:                  U n p r o v i s i o n                            */
/******************************************************************************/

// Called with sessMutex locked and returns with it unlocked
  
void XrdSsiSessReal::Unprovision() // Called with sessMutex locked!
{
   XrdCl::XRootDStatus uStat;

// Close the file this will schedule a shutdown if successful
//
   ClrEvent();
   uStat = epFile.Close((XrdCl::ResponseHandler *)this);

// If this was not successful then we can do the shutdown right now. Note that
// Shutdown() unlocks the sessMutex.
//
   if (!uStat.IsOK()) Shutdown(uStat);
      else sessMutex.UnLock();
}

/******************************************************************************/
/*                              X e q E v e n t                               */
/******************************************************************************/
  
bool XrdSsiSessReal::XeqEvent(XrdCl::XRootDStatus *status,
                              XrdCl::AnyObject   **respP)
{

// Lock the session. We keep the lock if there is going to any continuation
// via the event handler. Otherwise, drop the lock.
//
   sessMutex.Lock();

// If we are not in the open phase then this is due to a close event. Simply
// do a shutdown and return to stop event processing.
//
   if (!inOpen)
      {Shutdown(*status);
       return false;
      }
   inOpen = false;

// Check if the request that triggered the open was cancelled. If so, bail.
// Note that shutdown and unprovision unlock the sessMutex.
//
   if (!requestP)
      {if (!status->IsOK()) Shutdown(*status);
          else {if (!isHeld) Unprovision();}
       return false;
      }

// We are here because the open finally completed. If the open failed, then
// tell Finish() to do a shutdown and post an error response.
//
   if (!status->IsOK())
      {std::string eTxt;
       int         eNum = XrdSsiUtils::GetErr(*status, eTxt);
       doStop = true;
       sessMutex.UnLock();
       SetErrResponse(eTxt.c_str(), eNum);
       return false;
      }

// Obtain the endpoint name
//
   std::string currNode;
   if (epFile.GetProperty(dsProperty, currNode))
      {if (sessNode) free(sessNode);
       sessNode = strdup(currNode.c_str());
      } else sessNode = strdup("Unknown!");

// Execute the request. If we failed and this is a single request session then
// we need to disband he session. We delay this until Finish() is called.
//
   if (!Execute(requestP) && !isHeld)
      {if (!requestP) Unprovision();
          else {doStop = true;
                sessMutex.UnLock();
               }
      } else sessMutex.UnLock();
   return false;
}
