/******************************************************************************/
/*                                                                            */
/*                     X r d S s i T a s k R e a l . c c                      */
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

#include "XrdSsi/XrdSsiRRInfo.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiSessReal.hh"
#include "XrdSsi/XrdSsiTaskReal.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
#define DBG(x) cerr <<x <<endl;

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/
  
namespace
{
const char *statName[] = {"isWrite", "isSync", "isReady",
                          "isDone",  "isDead"};

XrdSsiSessReal voidSession(0, "voidSession");
}

/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/

void XrdSsiTaskReal::Detach(bool force)
{   tStat = isDead;
    if (force) sessP = &voidSession;
}
  
/******************************************************************************/
/*                        H a n d l e R e s p o n s e                         */
/******************************************************************************/
  
void XrdSsiTaskReal::HandleResponse(XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *response)
{
   class delRsp
        {public: delRsp(XrdCl::XRootDStatus *sP, XrdCl::AnyObject *rP)
                       : ssp(sP), rsp(rP) {}
                ~delRsp() {if (ssp) delete ssp;
                           if (rsp) delete rsp;
                          }
         XrdCl::XRootDStatus *ssp;
         XrdCl::AnyObject    *rsp;
        };
   delRsp rspHelper(status, response);
   XrdCl::XRootDStatus epStatus;
   XrdSsiRRInfo        rInfo;
   char *dBuff;
   union {uint32_t ubRead; int ibRead;};
   bool aOK = status->IsOK();

// Affect proper response
//
   sessP->Lock();
   DBG("Task "<<hex<<this<<dec<<" sess="<<(sessP==&voidSession?"no":"ok")
              <<" Status = "<<aOK<<' '<<statName[tStat]);

   switch(tStat)
         {case isWrite:
               if (!aOK) {RespErr(status); return;}
               tStat = isSync;
               rInfo.Id(tskID); rInfo.Cmd(XrdSsiRRInfo::Rwt);
               DBG("Task Handler calling RelBuff.");
               ReleaseRequestBuffer();
               DBG("Task Handler calling trunc.");
               epStatus = sessP->epFile.Truncate(rInfo.Info(),
                                                 (ResponseHandler *)this,
                                                 tmOut);
               if (!epStatus.IsOK()) {RespErr(&epStatus); return;}
               sessP->UnLock();
               return; break;
          case isSync:
               if (!aOK) {RespErr(status); return;}
               tStat = isReady;
               mhPend = false;
               sessP->UnLock();
               DBG("Task Handler responding with stream.");
               SetResponse((XrdSsiStream *)this);
               return; break;
          case isReady:
               break;
          case isDead:
               if (sessP != &voidSession)
                  {DBG("Task Handler calling Recycle.");
                   sessP->Recycle(this);
                   sessP->UnLock();
                  } else {
                   DBG("Task Handler deleting task.");
                   sessP->UnLock();
                   delete this;
                  }
               return; break;
          default: cerr <<"XrdSsiTaskReal: Invalid state " <<tStat <<endl;
               return;
         }

// Handle incomming response data
//
   if (!aOK || !response)
      {ibRead = -1;
       if (!aOK) XrdSsiSessReal::SetErr(*status, rqstP->eInfo);
          else   rqstP->eInfo.Set("Missing response", EFAULT);
      } else {
       XrdCl::ChunkInfo *cInfo = 0;
       response->Get(cInfo);
       ubRead = (cInfo ? cInfo->length : 0);
      }

// Reflect the response to the request as this was an async receive. We may not
// reference this object after the UnLock() as Complete() might be called.
//
   if (ibRead < dataRlen) tStat = isDone;
   dBuff = dataBuff;
   mhPend = false;
   sessP->UnLock();
   DBG("Task Handler calling ProcessResponseData.");
   rqstP->ProcessResponseData(dBuff, ibRead, tStat == isDone);
}

/******************************************************************************/
/*                                  K i l l                                   */
/******************************************************************************/

bool XrdSsiTaskReal::Kill() // Called with session mutex locked!
{
   XrdSsiRRInfo rInfo;

// Do some debugging
//
   DBG("Task "<<hex <<this <<dec<<" Kill status = "<<statName[tStat]
             <<" mhpend=" <<mhPend);

// Affect proper procedure
//
   switch(tStat)
         {case isWrite: break;
          case isSync:  break;
          case isReady: break;
          case isDone:  tStat = isDead;
                        return true;
                        break;
          case isDead:  return false;
                        break;
          default: cerr <<"XrdSsiTaskReal: Invalid state " <<tStat <<endl;
               tStat = isDead;
               return false;
               break;
         }

// If we are here then the request is potentially still active at the server.
// We will send a synchronous cancel request. It shouldn't take long.
//
   rInfo.Id(tskID); rInfo.Cmd(XrdSsiRRInfo::Can);
   DBG("Kill cancelling request.");
   sessP->epFile.Truncate(rInfo.Info(), tmOut);

// If we are in the message handler or if we have a message pending, then
// the message handler will dispose of the task.
//
   tStat = isDead;
   return !mhPend;
}
  
/******************************************************************************/
/* Private:                      R e s p E r r                                */
/******************************************************************************/
  
void XrdSsiTaskReal::RespErr(XrdCl::XRootDStatus *status) // Session is locked!
{
   XrdSsiErrInfo eInfo;
   const char *eText;
   int   eNum;

// Get the error information
//
   XrdSsiSessReal::SetErr(*status, eInfo);

// Indicate we are done and unlock the session. We also indicate we are no
// longer in the message handler, even though we might be in ProcessResponse()
// when Complete() is called. That's OK since we will not be referencing this
// object no matter what so all is well
//
   tStat = isDone;
   mhPend = false;
   if (sessP) sessP->UnLock();

// Reflect an error to the request object.
//
   eText = eInfo.Get(eNum);
   SetErrResponse(eText, eNum);
}

/******************************************************************************/
/*                               S e t B u f f                                */
/******************************************************************************/
  
int XrdSsiTaskReal::SetBuff(XrdSsiErrInfo &eInfo,
                            char *buff, int blen, bool &last)
{
   XrdSysMutexHelper rHelp(sessP->MutexP());
   XrdCl::XRootDStatus epStatus;
   XrdSsiRRInfo rrInfo;
   union {uint32_t ubRead; int ibRead;};

// Check if this is a proper call or we have reached EOF
//
   DBG("Task "<<hex<<this<<dec<<" SetBuff Sync Status=" <<statName[tStat]);
   if (tStat != isReady)
      {if (tStat == isDone) return 0;
       eInfo.Set("Stream is not active", ENODEV);
       return -1;
      }

// Prepare to issue the read
//
   rrInfo.Id(tskID);

// Issue a read
//
   epStatus = sessP->epFile.Read(rrInfo.Info(),(uint32_t)blen,buff,ubRead,tmOut);
   if (epStatus.IsOK())
      {if (ibRead < blen) {tStat = isDone; last = true;}
       return ibRead;
      }

// We failed, return an error
//
   XrdSsiSessReal::SetErr(epStatus, eInfo);
   tStat = isDone;
   DBG("Task Sync SetBuff error");
   return -1;
}

/******************************************************************************/
  
bool XrdSsiTaskReal::SetBuff(XrdSsiRequest *reqP, char *buff, int blen)
{
   XrdSysMutexHelper rHelp(sessP->MutexP());
   XrdCl::XRootDStatus epStatus;
   XrdSsiRRInfo rrInfo;

// Check if this is a proper call or we have reached EOF
//
   DBG("Task "<<hex<<this<<dec<<" SetBuff Async Status=" <<statName[tStat]);
   if (tStat != isReady)
      {reqP->eInfo.Set("Stream is not active", ENODEV);
       return false;
      }

// We only support (for now) one read at a time
//
   if (mhPend)
      {reqP->eInfo.Set("Stream is already active", EINPROGRESS);
       return false;
      }

// Prepare to issue the read
//
   rrInfo.Id(tskID);

// Issue a read
//
   dataBuff = buff; dataRlen = blen; rqstP = reqP;
   epStatus = sessP->epFile.Read(rrInfo.Info(), (uint32_t)blen, buff,
                                (XrdCl::ResponseHandler *)this, tmOut);

// If success then indicate we are pending and return
//
   if (epStatus.IsOK()) {mhPend = true; return true;}

// We failed, return an error
//
   XrdSsiSessReal::SetErr(epStatus, reqP->eInfo);
   tStat = isDone;
   DBG("Task Async SetBuff error");
   return false;
}
