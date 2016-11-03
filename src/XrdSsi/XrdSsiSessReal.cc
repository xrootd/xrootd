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
#include <sys/types.h>
#include <netinet/in.h>
  
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiRRInfo.hh"
#include "XrdSsi/XrdSsiServReal.hh"
#include "XrdSsi/XrdSsiSessReal.hh"
#include "XrdSsi/XrdSsiTrace.hh"
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
/*                           I n i t S e s s i o n                            */
/******************************************************************************/
  
void XrdSsiSessReal::InitSession(XrdSsiServReal *servP, const char *sName)
{
   resource  = 0;
   attBase   = 0;
   freeTask  = 0;
   pendTask  = 0;
   myService = servP;
   nextTID   = 0;
   alocLeft  = XrdSsiRRInfo::maxID;
   doUnProv  = false;
   stopping  = false;
   if (sName)
      {if (sessName) free(sessName);
       sessName = strdup(sName);
      }
}

/******************************************************************************/
/* Private:                       M a p E r r                                 */
/******************************************************************************/

int XrdSsiSessReal::MapErr(int xEnum)
{
    switch(xEnum)
       {case kXR_NotFound:      return ENOENT;
        case kXR_NotAuthorized: return EACCES;
        case kXR_IOError:       return EIO;
        case kXR_NoMemory:      return ENOMEM;
        case kXR_NoSpace:       return ENOSPC;
        case kXR_ArgTooLong:    return ENAMETOOLONG;
        case kXR_noserver:      return EHOSTUNREACH;
        case kXR_NotFile:       return ENOTBLK;
        case kXR_isDirectory:   return EISDIR;
        case kXR_FSError:       return ENOSYS;
        default:                return ECANCELED;
       }
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
bool XrdSsiSessReal::Open(XrdSsiService::Resource *resP,
                          const char              *epURL,
                          unsigned short           tOut,
                          bool                     finup)
{
   EPNAME("SessOpen");
   XrdCl::XRootDStatus epStatus;

// Set resource, we will need to call ProvisionDone() on it later
//
   resource = resP;
   doUnProv = finup;

// Issue the open and if the open was started, return success.
//
   DEBUG("Opening " <<epURL);
   epStatus = epFile.Open((const std::string)epURL,
                          XrdCl::OpenFlags::Read, (XrdCl::Access::Mode)0,
                          (XrdCl::ResponseHandler *)this, tOut);
   if (epStatus.IsOK()) return true;

// We failed.
//
   SetErr(epStatus, resP->eInfo);
   return false;
}

/******************************************************************************/
/*                        P r o c e s s R e q u e s t                         */
/******************************************************************************/
  
void XrdSsiSessReal::ProcessRequest(XrdSsiRequest *reqP, unsigned short tOut)
{
   EPNAME("SessProcReq");
   XrdCl::XRootDStatus Status;
   XrdSsiRRInfo        rrInfo;
   XrdSsiTaskReal     *tP, *ptP;
   char               *reqBuff;
   int                 reqBlen;

// Make sure the file is open
//
   if (!epFile.IsOpen())
      {RequestFailed(reqP, "Session not provisioned.", ENOTCONN); return;}

// Get the request information
//
   reqBuff = reqP->GetRequest(reqBlen);

// Remainder of the code here must have the mutex to hold off stops
//
   myMutex.Lock();

// Allocate a task object for this request
//
   if ((tP = freeTask)) freeTask = tP->attList.next;
      else {if (!alocLeft || !(tP = new XrdSsiTaskReal(this, nextTID)))
               {myMutex.UnLock();
                RequestFailed(reqP, "Too many active requests.", EMLINK);
                return;
               }
            alocLeft--; nextTID++;
           }

// Initialize the task
//
   tP->Init(reqP, tOut);
   DEBUG("Task=" <<tP <<" processing id=" <<nextTID-1);

// Construct the info for this request
//
   rrInfo.Id(tP->ID());
   rrInfo.Size(reqBlen);

// Issue the write
//
   Status = epFile.Write(rrInfo.Info(), (uint32_t)reqBlen, reqBuff,
                         (XrdCl::ResponseHandler *)tP, tOut);

// Determine ending status. If OK, place task on attached list and bind the
// request to ourselves indicating that the task will be the responder.
//
   if (Status.IsOK())
      {if ((ptP = attBase)) {INSERT(attList, ptP, tP);}
           else attBase = tP;
       BindRequest(reqP, (XrdSsiSession *)this, (XrdSsiResponder *)tP);
       myMutex.UnLock();
      } else {
       const char *eText;
       int eNum;
       RelTask(tP);
       myMutex.UnLock();
       SetErr(Status, eNum, &eText);
       RequestFailed(reqP, eText, eNum);
      }
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdSsiSessReal::Recycle(XrdSsiTaskReal *tP) // myMutex locked!
{
   EPNAME("SessRecycle");
   XrdSsiTaskReal *ptP = 0, *ntP = pendTask;

// This is called from the msg handler for delayed cancelled tasks. We must
// remove the task from the pend list before releasing it. If we don't find
// it (unlikely) we simply delete it.
//
   while(ntP && ntP != tP) {ptP = ntP; ntP = ntP->attList.next;}

// If we found it unchain and release it
//
   if (ntP)
      {if (ptP) ptP->attList.next = ntP->attList.next;
          else  pendTask          = ntP->attList.next;
       RelTask(tP);
      } else {
       DEBUG("Task=" <<tP <<" not found; id="<<" id="<<tP->ID());
       delete tP;
      }

// If we are stopping then we may need to close the file as we caould not if
// any tasks were actually pending.
//
   if (stopping)
      {numPT--;
       if (numPT < 1)
          {XrdCl::XRootDStatus epStatus;
           epStatus = epFile.Close();
           Shutdown(epStatus);
          }
      }
}
  
/******************************************************************************/
/*                               R e l T a s k                                */
/******************************************************************************/
  
void XrdSsiSessReal::RelTask(XrdSsiTaskReal *tP) // myMutex locked!
{
   EPNAME("SessRelTask");

// Delete this task or place it on the free list
//
   DEBUG("dodel="<<stopping<<" id="<<tP->ID());
   if (stopping) delete tP;
      else {tP->ClrEvent();
            tP->attList.next = freeTask;
            freeTask = tP;
           }
}

/******************************************************************************/
/*                         R e q u e s t F a i l e d                          */
/******************************************************************************/

void XrdSsiSessReal::RequestFailed(XrdSsiRequest *rqstP,
                                   const char    *eText,
                                   int            eCode)
{

// Bind the request to outselves as we will be responding
//
   BindRequest(rqstP, this);

// Set the error response, this will call ProcessResponse()
//
   SetErrResponse(eText, eCode);
}

  
/******************************************************************************/
/*                       R e q u e s t F i n i s h e d                        */
/******************************************************************************/
  
void XrdSsiSessReal::RequestFinished(XrdSsiRequest        *rqstP,
                                     const XrdSsiRespInfo &rInfo, bool cancel)
{
   EPNAME("SessReqFin");
   XrdSysMutexHelper rHelp(&myMutex);
   XrdSsiResponder *tP = rqstP->GetResponder();
   XrdSsiTaskReal  *rtP;
   void            *objHandle;

// If we have no task then we are really done here (we may need to unprovision)
//
   DEBUG("Request="<<rqstP<<" cancel="<<cancel<<" task="<<tP);
   if (!tP)
      {if (doUnProv) Unprovision();
       return;
      }

// Since only we can set the task pointer we are allowed to down-cast it
// to it's actual implementation. This is actualy a safe operation.
//
   rtP = static_cast<XrdSsiTaskReal *>(tP->GetObject(objHandle));

// Remove task from the task list if it's in it
//
   if (rtP == attBase || rtP->attList.next != rtP)
      {REMOVE(attBase, attList, rtP);}

// If we can kill the task right now, clean up
//
   if (rtP->Kill()) RelTask(rtP);
      else {rtP->attList.next = pendTask; pendTask = rtP;
            DEBUG("Removal deferred; Task="<<tP<<" id=" <<nextTID-1);
           }

// Finally, if this is a single session run then unprovision here
//
   if (doUnProv) Unprovision();
}

/******************************************************************************/
/*                                S e t E r r                                 */
/******************************************************************************/
  
void XrdSsiSessReal::SetErr(XrdCl::XRootDStatus &Status, XrdSsiErrInfo &eInfo)
{

// If this is an xrootd error then get the xrootd generated error
//
   if (Status.code == XrdCl::errErrorResponse)
      {eInfo.Set(Status.GetErrorMessage().c_str(), MapErr(Status.errNo));
      } else {
       eInfo.Set(Status.ToStr().c_str(), (Status.errNo ? Status.errNo:EFAULT));
      }
}

/******************************************************************************/
  
void XrdSsiSessReal::SetErr(XrdCl::XRootDStatus &Status,
                            int &eNum, const char **eText)
{

// If this is an xrootd error then get the xrootd generated error
//
   if (Status.code == XrdCl::errErrorResponse)
      {*eText = Status.GetErrorMessage().c_str();
       eNum = MapErr(Status.errNo);
      } else {
       *eText = Status.ToStr().c_str(),
       eNum =  (Status.errNo ? Status.errNo : EFAULT);
      }
}

/******************************************************************************/
/* Private:                     S h u t d o w n                               */
/******************************************************************************/
  
void XrdSsiSessReal::Shutdown(XrdCl::XRootDStatus &epStatus)
{

// If the close failed then we cannot recycle this object as it is not reusable
//
   if (!epStatus.IsOK())
      {XrdSsiErrInfo  eInfo;
       const char    *eText;
       int            eNum;
       SetErr(epStatus, eInfo);
       eText = eInfo.Get(eNum);
       cerr <<"Unprovision "<<sessName<<'@'<<sessNode<<" error; "<<eText<<endl;
      } else {
       if (sessName) {free(sessName); sessName = 0;}
       if (sessNode) {free(sessNode); sessNode = 0;}
       myService->Recycle(this);
      }
}

/******************************************************************************/
/*                           U n p r o v i s i o n                            */
/******************************************************************************/
  
bool XrdSsiSessReal::Unprovision(bool forced)
{
   XrdSysMutexHelper   rHelp(&myMutex);
   XrdCl::XRootDStatus epStatus;
   XrdSsiTaskReal *tP = pendTask;

// Make sure there are no outstanding requests
//
   if (attBase) return false;

// Make sure we are bing called only once
//
   if (stopping) return true;
   stopping = true;

// If we have any pending tasks then detach them, they will be deleted later
//
   numPT = 0;
   while(tP) {tP->Detach(true); tP = tP->attList.next; numPT++;}
   pendTask = 0;

// Close the file associated with this session if we have no pending tasks. If
// error occurs, the move ahead to Shutdown(). We cannot be holding our mutex!
//
   if (!numPT)
      {epStatus = epFile.Close((XrdCl::ResponseHandler *)this);
       if (!epStatus.IsOK()) {rHelp.UnLock(); Shutdown(epStatus);}
      }

// All done
//
   return true;
}

/******************************************************************************/
/*                              X e q E v e n t                               */
/******************************************************************************/
  
bool XrdSsiSessReal::XeqEvent(XrdCl::XRootDStatus *status,
                              XrdCl::AnyObject   **respP)
{
   XrdSysMutexHelper rHelp(&myMutex);
   XrdSsiSessReal *sObj = 0;

// If we are stopping then simply recycle ourselves. We must not be holding
// our mutex when we do so!
//
   if (stopping)
      {rHelp.UnLock();
       Shutdown(*status);
       return false;
      }

// We are here because the open finally completed. Check for errors.
//
   if (status->IsOK())
      {std::string currNode;
       if (epFile.GetProperty(dsProperty, currNode))
          {if (sessNode) free(sessNode);
           sessNode = strdup(currNode.c_str());
           sObj = this;
          } else resource->eInfo.Set("Unable to get node name!",EADDRNOTAVAIL);
      } else SetErr(*status, resource->eInfo);

// Do appropriate callback. Be careful, as the below is set up to allow the
// callback to implicitly delete us should it unprovision the session. So,
// we drop out lock at this point so we neither deadlock nor get SEGV.
//
   rHelp.UnLock();
   resource->ProvisionDone(sObj);
   return stopping;
}
