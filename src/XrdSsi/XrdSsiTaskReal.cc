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
#include "XrdSsi/XrdSsiTrace.hh"
#include "XrdSys/XrdSysHeaders.hh"

using namespace XrdSsi;

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/
  
namespace
{
const char *statName[] = {"isWrite", "isSync", "isReady",
                          "isDone",  "isDead"};

XrdSsiSessReal voidSession(0, "voidSession");

char        zedData = 0;
const char *tident  = 0;
}
  
/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/

void XrdSsiTaskReal::Detach(bool force)
{   tStat = isDead;
    if (force) sessP = &voidSession;
}

/******************************************************************************/
/* Private:                      G e t R e s p                                */
/******************************************************************************/
  
XrdSsiTaskReal::respType XrdSsiTaskReal::GetResp(XrdCl::AnyObject **respP,
                                                 char *&dbuff, int &dbL)
{
   EPNAME("GetResp");
   XrdCl::AnyObject *response = *respP;
   XrdCl::Buffer    *buffP;
   XrdSsiRRInfoAttn *mdP;
   char             *cdP;
   unsigned int      mdL, pxL, n;
   respType          xResp;

// Make sure that the [meta]data is actually present
//
   response->Get(buffP);
   if (!buffP || !(cdP = buffP->GetBuffer()))
      {DEBUG("Responding with stream id=" <<tskID);
       return isStream;
      }

// Validate the header
//
   if ((n=buffP->GetSize()) <= sizeof(XrdSsiRRInfoAttn)) return isBad;
   mdP = (XrdSsiRRInfoAttn *)cdP;
   mdL = ntohl(mdP->mdLen);
   pxL = ntohs(mdP->pfxLen);
   dbL = n - mdL - pxL;
   if (pxL < sizeof(XrdSsiRRInfoAttn) || dbL < 0) return isBad;

// Extract out the metadata
//
   if (mdL)
      {DEBUG("Adding metadata id=" <<tskID);
       SetMetadata(cdP+pxL, mdL);
      }

// Extract out the data
//
        if (mdP->tag == XrdSsiRRInfoAttn::fullResp)
           {dbuff = (dbL ? cdP+mdL+pxL : &zedData);
            xResp = isData;
            DEBUG("Responding with data id=" <<tskID);
           }
   else    {xResp = isStream;
            DEBUG("Responding with stream id=" <<tskID);
           }

// Save the response buffer if we will be referecing it later
//
   if (mdL || dbL)
      {mdResp = response;
       *respP = 0;
      }

// All done
//
   return xResp;
}

/******************************************************************************/
/*                                  K i l l                                   */
/******************************************************************************/

bool XrdSsiTaskReal::Kill() // Called with session mutex locked!
{
   EPNAME("TaskKill");
   XrdSsiRRInfo rInfo;

// Do some debugging
//
   DEBUG("Status = "<<statName[tStat]<<" mhPend="<<mhPend <<" id=" <<tskID);

// Regardless of the state, remove this task from the hold queue if there
//
   Reset();

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
   DEBUG("Sending cancel request id=" <<tskID);
   sessP->epFile.Truncate(rInfo.Info(), tmOut);

// If we are in the message handler or if we have a message pending, then
// the message handler will dispose of the task.
//
   tStat = isDead;
   return !mhPend;
}
  
/******************************************************************************/
/*                               R e d r i v e                                */
/******************************************************************************/
  
void XrdSsiTaskReal::Redrive()
{
   EPNAME("TaskRedrive");
   XrdSsiRequest::PRD_Xeq prdVal;
   bool last = tStat == isDone;

// Simply call data response method again
//
   sessP->UnLock();
   DEBUG("Redriving ProcessResponseData; len="<<dataRlen<<" last="<<last);
   prdVal = rqstP->ProcessResponseData(dataBuff, dataRlen, last);
   switch(prdVal)
         {case XrdSsiRequest::PRD_Normal:                      break;
          case XrdSsiRequest::PRD_Hold:    Hold(0);            break;
          case XrdSsiRequest::PRD_HoldLcl: Hold(rqstP->reqID); break;
          default: cerr <<"Redrive: ProcessResponseData() return invalid enum "
                          " - " <<prdVal <<endl;
         }
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
   EPNAME("TaskSetBuff");
   XrdSysMutexHelper rHelp(sessP->MutexP());
   XrdCl::XRootDStatus epStatus;
   XrdSsiRRInfo rrInfo;
   union {uint32_t ubRead; int ibRead;};

// Check if this is a proper call or we have reached EOF
//
   DEBUG("Sync Status=" <<statName[tStat] <<" id=" <<tskID);
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
   DEBUG("Task Sync SetBuff error id=" <<tskID);
   return -1;
}

/******************************************************************************/
  
bool XrdSsiTaskReal::SetBuff(XrdSsiRequest *reqP, char *buff, int blen)
{
   EPNAME("TaskSetBuff");
   XrdSysMutexHelper rHelp(sessP->MutexP());
   XrdCl::XRootDStatus epStatus;
   XrdSsiRRInfo rrInfo;

// Check if this is a proper call or we have reached EOF
//
   DEBUG("Async Status=" <<statName[tStat] <<" id=" <<tskID);
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

// Make sure the buffer length is valid
//
   if (blen <= 0)
      {reqP->eInfo.Set("Buffer length invalid", EINVAL);
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
   DEBUG("Task Async SetBuff error id=" <<tskID);
   return false;
}
  
/******************************************************************************/
/*                              X e q E v e n t                               */
/******************************************************************************/
  
bool XrdSsiTaskReal::XeqEvent(XrdCl::XRootDStatus *status,
                              XrdCl::AnyObject   **respP)
{
   EPNAME("TaskXeqEvent");
   static std::string pName("ReadRecovery");
   static std::string pValue("false");

   XrdCl::XRootDStatus epStatus;
   XrdCl::AnyObject   *response = *respP;
   XrdSsiRRInfo        rInfo;
   char *dBuff;
   XrdCl::Buffer qBuff(sizeof(unsigned long long));
   union {uint32_t ubRead; int ibRead;};
   int dLen;
   XrdSsiRequest::PRD_Xeq prdVal;
   bool last, aOK = status->IsOK();

// Affect proper response
//
   sessP->Lock();
   DEBUG(" sess="<<(sessP==&voidSession?"no":"ok") <<" id=" <<tskID
              <<" Status="<<aOK<<' '<<statName[tStat]
              <<" clrT=" <<Xrd::hex <<myCaller
              <<" xeqT=" <<pthread_self() <<Xrd::dec);

   switch(tStat)
         {case isWrite:
               if (!aOK) {RespErr(status); return true;}
               tStat = isSync;
               sessP->epFile.SetProperty(pName, pValue);
               rInfo.Id(tskID); rInfo.Cmd(XrdSsiRRInfo::Rwt);
               DEBUG("Calling RelBuff id=" <<tskID);
               ReleaseRequestBuffer();
               DEBUG("Calling fcntl id=" <<tskID);
               memcpy(qBuff.GetBuffer(), rInfo.Data(), sizeof(long long));
               epStatus = sessP->epFile.Fcntl(qBuff,
                                             (ResponseHandler *)this,
                                             tmOut);
               if (!epStatus.IsOK()) {RespErr(&epStatus); return true;}
               sessP->UnLock();
               return true; break;
          case isSync:
               if (!aOK) {RespErr(status); return true;}
               mhPend = false;
               if (response) switch(GetResp(respP, dBuff, dLen))
                  {case isData:   tStat = isDone;  sessP->UnLock();
                                  SetResponse(dBuff, dLen);
                                  break;
                   case isStream: tStat = isReady; sessP->UnLock();
                                  SetResponse((XrdSsiStream *)this);
                                  break;
                   default:       tStat = isDone;  sessP->UnLock();
                                  SetErrResponse("Invalid response", EFAULT);
                                  break;
                  }
               return true; break;
          case isReady:
               break;
          case isDead:
               if (sessP != &voidSession)
                  {DEBUG("Task Handler calling Recycle.");
                   sessP->Recycle(this);
                   sessP->UnLock();
                  } else {
                   DEBUG("Deleting task.");
                   sessP->UnLock();
                   delete this;
                  }
               return false; break;
          default: cerr <<"XrdSsiTaskReal: Invalid state " <<tStat <<endl;
               return false;
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
// reference this object after the UnLock() as Finished() might be called.
//
   if (ibRead < dataRlen) {tStat = isDone; dataRlen = ibRead;}
   dBuff = dataBuff;
   mhPend = false;
   last = tStat == isDone;
   sessP->UnLock();
   DEBUG("Calling ProcessResponseData; len="<<ibRead<<" last="<<last);
   prdVal = rqstP->ProcessResponseData(dBuff, ibRead, last);

// The processor requested a hold on further action for this request
//
   switch(prdVal)
         {case XrdSsiRequest::PRD_Normal:                      break;
          case XrdSsiRequest::PRD_Hold:    Hold(0);            break;
          case XrdSsiRequest::PRD_HoldLcl: Hold(rqstP->reqID); break;
          default: cerr <<"XeqEvent: ProcessResponseData() return invalid enum "
                          " - " <<prdVal <<endl;
         }
   return true;
}
