/******************************************************************************/
/*                                                                            */
/*                         X r d C m s R e s p . c c                          */
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
  
#include "XrdCms/XrdCmsClientMsg.hh"
#include "XrdCms/XrdCmsParser.hh"
#include "XrdCms/XrdCmsResp.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"

using namespace XrdCms;
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdSysSemaphore         XrdCmsResp::isReady(0);
XrdSysMutex             XrdCmsResp::rdyMutex;
XrdCmsResp             *XrdCmsResp::First    =  0;
XrdCmsResp             *XrdCmsResp::Last     =  0;
XrdSysMutex             XrdCmsResp::myMutex;
XrdCmsResp             *XrdCmsResp::nextFree =  0;
int                     XrdCmsResp::numFree  =  0;
int                     XrdCmsResp::RepDelay =  5;
 
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdCmsResp *XrdCmsResp::Alloc(XrdOucErrInfo *erp, int msgid)
{
   XrdCmsResp *rp;

// Allocate a response object. We must be assured that the semaphore count
// is zero. This will be true for freshly allocated objects. For reused
// objects we will need to run down the count to zero as multiple calls
// to sem_init may produced undefined behaviour.
//
   myMutex.Lock();
   if (nextFree) 
      {rp = nextFree;
       nextFree = rp->next; 
       numFree--;
       rp->SyncCB.Init();
      }
      else if (!(rp = new XrdCmsResp())) 
              {myMutex.UnLock();
               return (XrdCmsResp *)0;
              }
   myMutex.UnLock();

// Initialize it. We also replace the callback object pointer with a pointer
// to the synchronization semaphore as we have taken over the object and must
// provide a callback synchronization path for the caller. The OucEI object
// must be setup with pointers to stable areas and we will relocate any data
// to allow for a message to be sent back without overwriting the data (usually
// the path related to this request). We do this manually as the assignment
// operator does a bit more and a bit less than what we really need to do here.
//
   strlcpy(rp->UserID, erp->getErrUser(), sizeof(rp->UserID));
   rp->setErrUser(rp->UserID);
   rp->setErrData(erp->getErrData(), XrdOucEI::Path_Offset);
   rp->setErrInfo(0, "");
   rp->setErrMid(erp->getErrMid());
   rp->ErrCB = erp->getErrCB(rp->ErrCBarg);
   erp->setErrCB((XrdOucEICB *)&rp->SyncCB);
   rp->myID   = msgid;
   rp->next   = 0;

// Return the response object
//
   return rp;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdCmsResp::Recycle()
{

// Recycle appendages
//
   if (myBuff) {myBuff->Recycle(); myBuff = 0;}

// We keep a stash of allocated response objects. If there are too many we
// simply delete this object.
//
   if (XrdCmsResp::numFree >= XrdCmsResp::maxFree) delete this;
      else {myMutex.Lock();
            next = nextFree;
            nextFree = this;
            numFree++;
            myMutex.UnLock();
           }
}

/******************************************************************************/
/*                                 R e p l y                                  */
/******************************************************************************/

// This version of reply simply queues the object for reply

void XrdCmsResp::Reply(const char *manp, CmsRRHdr &rrhdr, XrdOucBuffer *netbuff)
{

// Copy the data we need to have
//
   myRRHdr = rrhdr;
   myBuff  = netbuff;
   next    = 0;
   strlcpy(theMan, manp, sizeof(theMan));

// Now queue this object
//
   rdyMutex.Lock();
   if (Last) {Last->next = this; Last = this;}
      else    Last=First = this;
   rdyMutex.UnLock();

// Now indicate we have something to process
//
   isReady.Post();
}

/******************************************************************************/

// This version of Reply() dequeues queued replies for processing

void XrdCmsResp::Reply()
{
   XrdCmsResp *rp;

// Endless look looking for something to reply to
//
   while(1)
        {isReady.Wait();
         rdyMutex.Lock();
         if ((rp = First))
            {if (!(First = rp->next)) Last = 0;
             rdyMutex.UnLock();
             rp->ReplyXeq();
            } else rdyMutex.UnLock();
        }
}
  
/******************************************************************************/
/*                              R e p l y X e q                               */
/******************************************************************************/
  
void XrdCmsResp::ReplyXeq()
{
   EPNAME("Reply")
   XrdOucEICB *theCB;
   int Result;

// If there is no callback object, ignore this call. Eventually, we may wish
// to simulate a callback but this is rather complicated.
//
   if (!ErrCB)
      {DEBUG("No callback object for user " <<UserID <<" msgid="
             <<myRRHdr.streamid  <<' ' <<theMan);
       Recycle();
       return;
      }

// Get the values for the callback.
//
   Result = XrdCmsParser::Decode(theMan,myRRHdr,myBuff,(XrdOucErrInfo *)this);

// Translate the return code to what the caller's caller wanst to see. We
// should only receive the indicated codes at this point.
//
  if (Result != SFS_REDIRECT && Result != SFS_STALL
  &&  Result != SFS_DATA     && Result != SFS_ERROR)
     {char buff[16];
      sprintf(buff, "%d", Result);
      Say.Emsg("Reply", "Invalid call back result code", buff);
      setErrInfo(EINVAL,"Invalid call back response from redirector.");
      Result = SFS_ERROR;
     }

// Before invoking the callback we must be assured that the waitresp response
// has been sent to the client. We do this by waiting on a semaphore which is
// posted *after* the waitresp response is sent.
//
   SyncCB.Wait();

// We now must request a callback to recycle this object once the callback
// response is actually sent by setting the callback object pointer to us.
//
   theCB = ErrCB;
   ErrCB = (XrdOucEICB *)this;

// Invoke the callback
//
   theCB->Done(Result, (XrdOucErrInfo *)this, getErrData());
}

/******************************************************************************/
/*                           X r d O d c R e s p Q                            */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCmsRespQ::XrdCmsRespQ()
{
   memset(mqTab, 0, sizeof(mqTab));
}

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
void XrdCmsRespQ::Add(XrdCmsResp *rp)
{
   int i;

// Compute index and either add or chain the entry
//
   i = rp->myID % mqSize;
   myMutex.Lock();
   rp->next = (mqTab[i] ? mqTab[i] : 0);
   mqTab[i] = rp;
   myMutex.UnLock();
}

/******************************************************************************/
/*                                 P u r g e                                  */
/******************************************************************************/
  
void XrdCmsRespQ::Purge()
{
   XrdCmsResp *rp;
   int i;

   myMutex.Lock();
   for (i = 0; i < mqSize; i++)
       {while ((rp = mqTab[i])) {mqTab[i] = rp->next; delete rp;}}
   myMutex.UnLock();
}

/******************************************************************************/
/*                                   R e m                                    */
/******************************************************************************/
  
XrdCmsResp *XrdCmsRespQ::Rem(int msgid)
{
   int i;
   XrdCmsResp *rp, *pp = 0;

// Compute the index and find the entry
//
   i = msgid % mqSize;
   myMutex.Lock();
   rp = mqTab[i];
   while(rp && rp->myID != msgid) {pp = rp; rp = rp->next;}

// Remove the entry if we found it
//
   if (rp) {if (pp) pp->next = rp->next;
               else mqTab[i] = rp->next;
           }

// Return what we found
//
   myMutex.UnLock();
   return rp;
}
