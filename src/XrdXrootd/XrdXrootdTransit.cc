/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d T r a n s i t . c c                    */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>
#include <unistd.h>
#include <sys/uio.h>

#include "XProtocol/XProtocol.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdTransit.hh"
#include "XrdXrootd/XrdXrootdTransPend.hh"
#include "XrdXrootd/XrdXrootdTransSend.hh"

/******************************************************************************/
/*                        C l o b a l   S y m b o l s                         */
/******************************************************************************/
  
extern  XrdOucTrace *XrdXrootdTrace;

#undef  TRACELINK
#define TRACELINK Link

#define XRD_GETNUM(x)\
        ntohl(*(static_cast<unsigned int *>(static_cast<void *>(x))))

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
const char *XrdXrootdTransit::reqTab = XrdXrootdTransit::ReqTable();

XrdObjectQ<XrdXrootdTransit>
           XrdXrootdTransit::ProtStack("ProtStack",
                                       "transit protocol anchor");

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdTransit *XrdXrootdTransit::Alloc(XrdXrootd::Bridge::Result *rsltP,
                                          XrdLink                   *linkP,
                                          XrdSecEntity              *seceP,
                                          const char                *nameP,
                                          const char                *protP
                                         )
{
   XrdXrootdTransit *xp;

// Simply return a new transit object masquerading as a bridge
//
   if (!(xp = ProtStack.Pop())) xp = new XrdXrootdTransit();
   xp->Init(rsltP, linkP, seceP, nameP, protP);
   return xp;
}

/******************************************************************************/
/*                                  A t t n                                   */
/******************************************************************************/
  
int XrdXrootdTransit::Attn(XrdLink *lP, short *theSID, int rcode,
                           const struct iovec *ioV, int ioN, int ioL)
{
   XrdXrootdTransPend *tP;

// Find the request
//
   if (!(tP = XrdXrootdTransPend::Remove(lP, *theSID)))
      {TRACE(REQ, "Unable to find request for " <<lP->ID <<" sid=" <<*theSID);
       return 0;
      }

// Resume the request as we have been waiting for the response.
//
   return tP->bridge->AttnCont(tP, rcode, ioV, ioN, ioL);
}

/******************************************************************************/
/*                              A t t n C o n t                               */
/******************************************************************************/

int XrdXrootdTransit::AttnCont(XrdXrootdTransPend *tP,  int rcode,
                               const struct iovec *ioV, int ioN, int ioL)
{
   int rc;

// Refresh the request structure
//
   memcpy(&Request, &(tP->Pend.Request), sizeof(Request));
   delete tP;
   runWait = 0;

// Reissue the request if it's a wait 0 response.
//
   if (rcode==kXR_wait
   &&  (!ioN || XRD_GETNUM(ioV[0].iov_base) == 0))
      {Sched->Schedule((XrdJob *)&waitJob);
       return 0;
      }

// Send off the defered response
//
   rc = Send(rcode, ioV, ioN, ioL);

// If no wait needed, enable the link. Otherwise, handle the wait (rare)
//
   if (rc >= 0)
      {if (runDone && !runWait)
          {AtomicBeg(runMutex);
           AtomicZAP(runStatus);
           AtomicEnd(runMutex);
           tP->link->Enable();
          } else {
           if (runWait >= 0)
              Sched->Schedule((XrdJob *)&waitJob, time(0)+runWait);
          }
      }

// All done
//
   return rc;
}
  
/******************************************************************************/
/*                                  D i s c                                   */
/******************************************************************************/
  
bool XrdXrootdTransit::Disc()
{
   char buff[128];
   int rc;

// We do not allow disconnection while we are active
//
   AtomicBeg(runMutex);
   rc = AtomicInc(runStatus);
   AtomicEnd(runMutex);
   if (rc) return false;

// Reconnect original protocol to the link
//
   Link->setProtocol(realProt);

// Now we need to recycle our xrootd part
//
   sprintf(buff, "%s disconnection", pName);
   XrdXrootdProtocol::Recycle(Link, time(0)-cTime, buff);

// Now just free up our object.
//
   ProtStack.Push(&ProtLink);
   return true;
}
  
/******************************************************************************/
/* Private:                         F a i l                                   */
/******************************************************************************/
  
bool XrdXrootdTransit::Fail(int ecode, const char *etext)
{
   runError = ecode;
   runEText = etext;
   return true;
}
  
/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

int XrdXrootdTransit::Fatal(int rc)
{
   XrdXrootd::Bridge::Context rInfo(Link, Request.header.streamid,
                                          Request.header.requestid);

   return (respObj->Error(rInfo, runError, runEText) ? rc : -1);
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/

void XrdXrootdTransit::Init(XrdXrootd::Bridge::Result *respP,
                            XrdLink                   *linkP,
                            XrdSecEntity              *seceP,
                            const char                *nameP,
                            const char                *protP
                           )
{
   static XrdSysMutex myMutex;
   static int bID = 0;
   XrdNetAddrInfo *addrP;
   const char *who;
   char uname[sizeof(Request.login.username)+1];
   int pID, n;

// Set standard stuff
//
   runArgs   = 0;
   runALen   = 0;
   runABsz   = 0;
   runError  = 0;
   runStatus = 0;
   runWait   = 0;
   runWTot   = 0;
   runWMax   = 3600;
   runWCall  = false;
   runDone   = false;
   reInvoke  = false;
   wBuff     = 0;
   wBLen     = 0;
   respObj   = respP;
   pName     = protP;

// Bind the protocol to the link
//
   SI->Bump(SI->Count);
   Link = linkP;
   Response.Set(linkP);
   Response.Set(this);
   strcpy(Entity.prot, "host");
   Entity.host = (char *)linkP->Host();

// Develop a trace identifier
//
   myMutex.Lock(); pID = ++bID; myMutex.UnLock();
   n = strlen(nameP);
   if (n >= int(sizeof(uname))) n = sizeof(uname)-1;
   strncpy(uname, nameP, n);
   uname[n] = 0;
   linkP->setID(uname, pID);

// Indicate that this brige supports asynchronous responses
//
   CapVer = kXR_asyncap | kXR_ver002;

// Mark the client as IPv4 if they came in as IPv4 or mapped IPv4
//
   addrP = Link->AddrInfo();
   if (addrP->isIPType(XrdNetAddrInfo::IPv4) || addrP->isMapped())
      clientPV |= XrdOucEI::uIPv4;

// Mark the client as being on a private net if the address is private
//
   if (addrP->isPrivate()) {clientPV |= XrdOucEI::uPrip; rdType = 1;}
      else rdType = 0;

// Now tie the security information
//
   Client = (seceP ? seceP : &Entity);

// Allocate a monitoring object, if needed for this connection and record login
//
   if (Monitor.Ready())
      {Monitor.Register(linkP->ID, linkP->Host(), protP);
       if (Monitor.Logins())
          {if (Monitor.Auths() && seceP) MonAuth();
              else Monitor.Report(Monitor.Auths() ? "" : 0);
          }
      }

// Complete the request ID object
//
   ReqID.setID(Request.header.streamid, linkP->FDnum(), linkP->Inst());

// Substitute our protocol for the existing one
//
   realProt = linkP->setProtocol(this);
   linkP->armBridge();

// Document this login
//
   who = (seceP && seceP->name ? seceP->name : "nobody");
   eDest.Log(SYS_LOG_01, "Bridge", Link->ID, "login as", who);

// All done, indicate we are logged in
//
   Status = XRD_LOGGEDIN;
   cTime = time(0);
}
  
/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/
  
int XrdXrootdTransit::Process(XrdLink *lp)
{
   int rc, bridgeActive;

// This entry is serialized via link processing. First, get the run status.
//
   AtomicBeg(runMutex);
   bridgeActive = AtomicGet(runStatus);
   AtomicEnd(runMutex);

// If we are running then we need to reflect this to the xrootd protocol as
// data is now available. One of the following will be returned.
//
// < 0 -> Stop getting requests,
//        -EINPROGRESS leave link disabled but otherwise all is well
//        -n           Error, disable and close the link
// = 0 -> OK, get next request, if allowed, o/w enable the link
// > 0 -> Slow link, stop getting requests  and enable the link
//
   if (bridgeActive)
      {rc = XrdXrootdProtocol::Process(lp);
       if (rc < 0) return rc;
       if (runWait)
          {if (runWait >= 0)
              Sched->Schedule((XrdJob *)&waitJob, time(0)+runWait);
           return -EINPROGRESS;
          }
       if (!runDone) return rc;
       AtomicBeg(runMutex);
       AtomicZAP(runStatus);
       AtomicEnd(runMutex);
       if (!reInvoke) return 1;
      }

// Reflect data is present to the underlying protocol and if Run() has been
// called we need to dispatch that request. This may be iterative.
//
do{rc = realProt->Process((reInvoke ? 0 : lp));
   if (rc >= 0 && runStatus)
      {reInvoke = (rc == 0);
       if (runError) rc = Fatal(rc);
          else {runDone = false;
                rc = (Resume ? XrdXrootdProtocol::Process(lp) : Process2());
                if (rc >= 0)
                   {if (runWait)
                       {if (runWait >= 0)
                           Sched->Schedule((XrdJob *)&waitJob, time(0)+runWait);
                        return -EINPROGRESS;
                       }
                    if (!runDone) return rc;
                    AtomicBeg(runMutex);
                    AtomicZAP(runStatus);
                    AtomicEnd(runMutex);
                   }
               }
      } else reInvoke = false;
   } while(rc >= 0 && reInvoke);

// Make sure that we indicate that we are no longer active
//
   if (runStatus)
      {AtomicBeg(runMutex);
       AtomicZAP(runStatus);
       AtomicEnd(runMutex);
      }

// All done
//
   return (rc ? rc : 1);
}

/******************************************************************************/
  
int XrdXrootdTransit::Process()
{
   static int  eCode         = htonl(kXR_NoMemory);
   static char eText[]       = "Insufficent memory to re-issue request";
   static struct iovec ioV[] = {{(char *)&eCode,sizeof(eCode)},
                                {(char *)&eText,sizeof(eText)}};
   int rc;

// Update wait statistics
//
   runWTot += runWait;
   runWait = 0;

// While we are running asynchronously, there is no way that this object can
// be deleted while a timer is outstanding as the link has been disabled. So,
// we can reissue the request with little worry.
//
   if (!runALen || RunCopy(runArgs, runALen)) rc = Process2();
      else rc = Send(kXR_error, ioV, 2, 0);

// Defer the request if need be
//
   if (rc >= 0 && runWait)
      {if (runWait > 0) Sched->Schedule((XrdJob *)&waitJob, time(0)+runWait);
       return 0;
      }
   runWTot = 0;

// Indicate we are no longer active
//
   if (runStatus)
      {AtomicBeg(runMutex);
       AtomicZAP(runStatus);
       AtomicEnd(runMutex);
      }

// If the link needs to be terminated, terminate the link. Otherwise, we can
// enable the link for new requests at this point.
//
   if (rc < 0) Link->Close();
      else     Link->Enable();

// All done
//
   return 0;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdXrootdTransit::Recycle(XrdLink *lp, int consec, const char *reason)
{

// Set ourselves as active so we can't get more requests
//
   AtomicBeg(runMutex);
   AtomicInc(runStatus);
   AtomicEnd(runMutex);

// If we were active then we will need to quiesce before dismantling ourselves.
// Note that Recycle() can only be called if the link is enabled. So, this bit
// of code is improbable but we check it anyway.
//
   if (runWait > 0) Sched->Cancel(&waitJob);

// First we need to recycle the real protocol
//
   if (realProt) realProt->Recycle(lp, consec, reason);

// Now we need to recycle our xrootd part
//
   XrdXrootdProtocol::Recycle(lp, consec, reason);

// Release the argument buffer
//
   if (runArgs) {free(runArgs); runArgs = 0;}

// Delete all pending requests
//
   XrdXrootdTransPend::Clear(this);

// Now just free up our object.
//
   ProtStack.Push(&ProtLink);
}

/******************************************************************************/
/*                              R e q T a b l e                               */
/******************************************************************************/

#define KXR_INDEX(x) x-kXR_auth
  
const char *XrdXrootdTransit::ReqTable()
{
   static char rTab[kXR_truncate-kXR_auth+1];

// Initialize the table
//
   memset(rTab, 0, sizeof(rTab));
   rTab[KXR_INDEX(kXR_chmod)]     = 1;
   rTab[KXR_INDEX(kXR_close)]     = 1;
   rTab[KXR_INDEX(kXR_dirlist)]   = 1;
   rTab[KXR_INDEX(kXR_locate)]    = 1;
   rTab[KXR_INDEX(kXR_mkdir)]     = 1;
   rTab[KXR_INDEX(kXR_mv)]        = 1;
   rTab[KXR_INDEX(kXR_open)]      = 1;
   rTab[KXR_INDEX(kXR_prepare)]   = 1;
   rTab[KXR_INDEX(kXR_protocol)]  = 1;
   rTab[KXR_INDEX(kXR_query)]     = 1;
   rTab[KXR_INDEX(kXR_read)]      = 2;
   rTab[KXR_INDEX(kXR_readv)]     = 2;
   rTab[KXR_INDEX(kXR_rm)]        = 1;
   rTab[KXR_INDEX(kXR_rmdir)]     = 1;
   rTab[KXR_INDEX(kXR_set)]       = 1;
   rTab[KXR_INDEX(kXR_stat)]      = 1;
   rTab[KXR_INDEX(kXR_statx)]     = 1;
   rTab[KXR_INDEX(kXR_sync)]      = 1;
   rTab[KXR_INDEX(kXR_truncate)]  = 1;
   rTab[KXR_INDEX(kXR_write)]     = 2;

// Now return the address
//
   return rTab;
}

/******************************************************************************/
/* Private:                     R e q W r i t e                               */
/******************************************************************************/
  
bool XrdXrootdTransit::ReqWrite(char *xdataP, int xdataL)
{

// Make sure we always transit to the resume point
//
   myBlen = 0;

// If nothing was read, then this is a straight-up write
//
   if (!xdataL || !xdataP || !Request.header.dlen)
      {Resume = 0; wBuff = xdataP; wBLen = xdataL;
       return true;
      }

// Partial data was read, we may have to split this between a direct write
// and a network read/write -- somewhat complicated.
//
   myBuff  = wBuff = xdataP;
   myBlast = wBLen = xdataL;
   Resume = &XrdXrootdProtocol::do_WriteSpan;
   return true;
}
  
/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/
  
bool XrdXrootdTransit::Run(const char *xreqP, char *xdataP, int xdataL)
{
   int movLen, rc;

// We do not allow re-entry if we are curently processing a request.
// It will be reset, as need, when a response is effected.
//
   AtomicBeg(runMutex);
   rc = AtomicInc(runStatus);
   AtomicEnd(runMutex);
   if (rc) return false;

// Copy the request header
//
   memcpy((void *)&Request, (void *)xreqP, sizeof(Request));

// Validate that we can actually handle this request
//
   Request.header.requestid = ntohs(Request.header.requestid);
   if (Request.header.requestid & 0x8000
   || Request.header.requestid > static_cast<kXR_unt16>(kXR_truncate)
   || !reqTab[Request.header.requestid - kXR_auth])
      return Fail(kXR_Unsupported, "Unsupported bridge request");

// Validate the data length
//
   Request.header.dlen      = ntohl(Request.header.dlen);
   if (Request.header.dlen < 0)
      return Fail(kXR_ArgInvalid, "Invalid request data length");

// Copy the stream id and trace this request
//
   Response.Set(Request.header.streamid);
   TRACEP(REQ, "Bridge req=" <<Request.header.requestid
               <<" dlen=" <<Request.header.dlen <<" blen=" <<xdataL);

// If this is a write request, we will need to do a lot more
//
   if (Request.header.requestid == kXR_write) return ReqWrite(xdataP, xdataL);

// Obtain any needed buffer and handle any existing data arguments. Also, we
// need to keep a shadow copy of the request arguments should we get a wait
// and will need to re-issue the request (the server mangles the args).
//
   if (Request.header.dlen)
      {movLen = (xdataL < Request.header.dlen ? xdataL : Request.header.dlen);
       if (!RunCopy(xdataP, movLen)) return true;
       if (!runArgs || movLen > runABsz)
          {if (runArgs) free(runArgs);
           if (!(runArgs = (char *)malloc(movLen)))
              return Fail(kXR_NoMemory, "Insufficient memory");
           runABsz = movLen;
          }
       memcpy(runArgs, xdataP, movLen); runALen = movLen;
       if ((myBlen = Request.header.dlen - movLen))
          {myBuff = argp->buff + movLen;
           Resume = &XrdXrootdProtocol::Process2;
           return true;
          }
      } else runALen = 0;

// If we have all the data, indicate request accepted.
//
   runError = 0;
   Resume   = 0;
   return true;
}

/******************************************************************************/
/* Privae:                       R u n C o p y                                */
/******************************************************************************/
  
bool XrdXrootdTransit::RunCopy(char *buffP, int buffL)
{

// Allocate a buffer if we do not have one or it is too small
//
   if (!argp || Request.header.dlen+1 > argp->bsize)
      {if (argp) BPool->Release(argp);
       if (!(argp = BPool->Obtain(Request.header.dlen+1)))
          {Fail(kXR_ArgTooLong, "Request argument too long"); return false;}
       hcNow = hcPrev; halfBSize = argp->bsize >> 1;
      }

// Copy the arguments to the buffer
//
   memcpy(argp->buff, buffP, buffL);
   argp->buff[buffL] = 0;
   return true;
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdXrootdTransit::Send(int rcode, const struct iovec *ioV, int ioN, int ioL)
{
   XrdXrootd::Bridge::Context rInfo(Link, Request.header.streamid,
                                          Request.header.requestid);
   const char *eMsg;
   int         rc;
   bool        aOK;

// Invoke the result object (we initially assume this is the final result)
//
   runDone = true;
   switch(rcode)
         {case kXR_error:
                   rc = XRD_GETNUM(ioV[0].iov_base);
                   eMsg = (ioN < 2 ? "" : (const char *)ioV[1].iov_base);
                   if (wBuff) respObj->Free(rInfo, wBuff, wBLen);
                   aOK = respObj->Error(rInfo, rc, eMsg);
                   break;
          case kXR_ok:
                   if (wBuff)  respObj->Free(rInfo, wBuff, wBLen);
                   aOK = (ioN ? respObj->Data(rInfo, ioV, ioN, ioL, true)
                              : respObj->Done(rInfo));
                   break;
          case kXR_oksofar:
                   aOK = respObj->Data(rInfo, ioV, ioN, ioL, false);
                   runDone = false;
                   break;
          case kXR_redirect:
                   if (wBuff) respObj->Free(rInfo, wBuff, wBLen);
                   rc = XRD_GETNUM(ioV[0].iov_base);
                   aOK = respObj->Redir(rInfo,rc,(const char *)ioV[1].iov_base);
                   break;
          case kXR_wait:
                   return Wait(rInfo, ioV, ioN, ioL);
                   break;
          case kXR_waitresp:
                   return WaitResp(rInfo, ioV, ioN, ioL);
                   break;
          default: if (wBuff) respObj->Free(rInfo, wBuff, wBLen);
                   aOK = respObj->Error(rInfo, kXR_ServerError,
                                       "internal logic error");
                   break;
         };

// All done
//
   return (aOK ? 0 : -1);
}

/******************************************************************************/

int XrdXrootdTransit::Send(long long offset, int dlen, int fdnum)
{
   XrdXrootdTransSend sfInfo(Link, Request.header.streamid,
                                   Request.header.requestid,
                                   offset, dlen, fdnum);

// Effect callback (this is always a final result)
//
   runDone = true;
   return (respObj->File(sfInfo, dlen) ? 0 : -1);
}

/******************************************************************************/

int XrdXrootdTransit::Send(XrdOucSFVec *sfvec, int sfvnum, int dlen)
{
   XrdXrootdTransSend sfInfo(Link, Request.header.streamid,
                                   Request.header.requestid,
                                   sfvec, sfvnum, dlen);

// Effect callback (this is always a final result)
//
   runDone = true;
   return (respObj->File(sfInfo, dlen) ? 0 : -1);
}

/******************************************************************************/
/* Private:                         W a i t                                   */
/******************************************************************************/
  
int XrdXrootdTransit::Wait(XrdXrootd::Bridge::Context &rInfo,
                           const struct iovec *ioV, int ioN, int ioL)
{
   const char *eMsg;

// Trace this request if need be
//
   runWait = XRD_GETNUM(ioV[0].iov_base);
   eMsg = (ioN < 2 ? "reason unknown" : (const char *)ioV[1].iov_base);

// Check if the protocol wants to handle all waits
//
   if (runWMax <= 0)
      {int wtime = runWait;
       runWait = 0;
       return (respObj->Wait(rInfo, wtime, eMsg) ? 0 : -1);
      }

// Check if we have exceeded the maximum wait time
//
   if (runWTot >= runWMax)
      {runDone = true;
       runWait = 0;
       return (respObj->Error(rInfo, kXR_Cancelled, eMsg) ? 0 : -1);
      }

// Readjust wait time
//
   if (runWait > runWMax) runWait = runWMax;

// Check if the protocol wants a wait notification
//
   if (runWCall && !(respObj->Wait(rInfo, runWait, eMsg))) return -1;

// All done, the process driver will effect the wait
//
   TRACEP(REQ, "Bridge delaying request " <<runWait <<" sec (" <<eMsg <<")");
   return 0;
}

/******************************************************************************/
/* Private:                     W a i t R e s p                               */
/******************************************************************************/
  
int XrdXrootdTransit::WaitResp(XrdXrootd::Bridge::Context &rInfo,
                               const struct iovec *ioV, int ioN, int ioL)
{
   XrdXrootdTransPend *trP;
   const char *eMsg;
   int wTime;

// Trace this request if need be
//
   wTime = XRD_GETNUM(ioV[0].iov_base);
   eMsg = (ioN < 2 ? "reason unknown" : (const char *)ioV[1].iov_base);
   TRACEP(REQ, "Bridge waiting for resp; sid=" <<rInfo.sID.num
               <<" wt=" <<wTime <<" (" <<eMsg <<")");

// We would issue callback to see how we should handle this. However, we can't
// predictably handle a waitresp. So that means we will just wait for a resp.
//
// XrdXrootd::Bridge::Result *newCBP = respObj->WaitResp(rInfo, runWait, eMsg);

// Save the current state
//
   trP = new XrdXrootdTransPend(Link, this, &Request);
   trP->Queue();

// Effect a wait
//
   runWait = -1;
   return 0;
}
