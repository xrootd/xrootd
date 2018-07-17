/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d C a l l B a c k . c c                   */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include "Xrd/XrdScheduler.hh"
#include "XProtocol/XProtocol.hh"
#include "XProtocol/XPtypes.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdXrootd/XrdXrootdCallBack.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdReqID.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdXrootdCBJob : XrdJob
{
public:

static XrdXrootdCBJob *Alloc(XrdXrootdCallBack *cbF, XrdOucErrInfo *erp,
                             const char *Path,       int rval);

       void            DoIt();

inline void            Recycle(){myMutex.Lock();
                                 Next = FreeJob;
                                 FreeJob = this;
                                 myMutex.UnLock();
                                }

                       XrdXrootdCBJob(XrdXrootdCallBack *cbp,
                                      XrdOucErrInfo     *erp,
                                      const char        *path,
                                      int                rval)
                                     : XrdJob("async response"),
                                       cbFunc(cbp), eInfo(erp), Path(path),
                                       Result(rval) {}

                      ~XrdXrootdCBJob() {}

private:
void DoClose(XrdOucErrInfo *eInfo);
void DoStatx(XrdOucErrInfo *eInfo);
static XrdSysMutex         myMutex;
static XrdXrootdCBJob     *FreeJob;

XrdXrootdCBJob            *Next;
XrdXrootdCallBack         *cbFunc;
XrdOucErrInfo             *eInfo;
const char                *Path;
int                        Result;
};

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdOucTrace       *XrdXrootdTrace;

namespace
{
       XrdSysError       *eDest;
       XrdXrootdStats    *SI;
       XrdScheduler      *Sched;
       int                Port;
}

       XrdSysMutex        XrdXrootdCBJob::myMutex;
       XrdXrootdCBJob    *XrdXrootdCBJob::FreeJob;

/******************************************************************************/
/*                        X r d X r o o t d C B J o b                         */
/******************************************************************************/
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdCBJob *XrdXrootdCBJob::Alloc(XrdXrootdCallBack *cbF,
                                      XrdOucErrInfo     *erp,
                                      const char        *Path,
                                      int                rval)
{
   XrdXrootdCBJob *cbj;

// Obtain a call back object by trying to avoid new()
//
   myMutex.Lock();
   if (!(cbj = FreeJob)) cbj = new XrdXrootdCBJob(cbF, erp, Path, rval);
      else {cbj->cbFunc = cbF, cbj->eInfo = erp; 
            cbj->Result = rval;cbj->Path  = Path;
            FreeJob = cbj->Next;
           }
   myMutex.UnLock();

// Return the new object
//
   return cbj;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdXrootdCBJob::DoIt()
{
   static const char *TraceID = "DoIt";

// Do some tracing here
//
   TRACE(RSP, eInfo->getErrUser() <<' ' <<cbFunc->Func() <<" async callback");

// Some operations differ in  the way we handle them. For instance, for open()
// if it succeeds then we must force the client to retry the open request
// because we can't attach the file to the client here. We do this by asking
// the client to wait zero seconds. Protocol demands a client retry. Close
// operations are always final and we need to do some cleanup.
//
        if (*(cbFunc->Func()) == 'c') DoClose(eInfo);
   else if (SFS_OK == Result)
          {if (*(cbFunc->Func()) == 'o')
              {int rc = 0; cbFunc->sendResp(eInfo, kXR_wait, &rc);}
              else {if (*(cbFunc->Func()) == 'x') DoStatx(eInfo);
                       cbFunc->sendResp(eInfo, kXR_ok, 0, eInfo->getErrText(),
                                               eInfo->getErrTextLen());
                   }
          }
   else cbFunc->sendError(Result, eInfo, Path);

// Tell the requestor that the callback has completed
//
   if (eInfo->getErrCB()) eInfo->getErrCB()->Done(Result, eInfo);
      else delete eInfo;
   eInfo = 0;
   Recycle();
}
  
/******************************************************************************/
/*                               D o C l o s e                                */
/******************************************************************************/
  
void XrdXrootdCBJob::DoClose(XrdOucErrInfo *eInfo)
{
   XrdXrootdFile *fP = (XrdXrootdFile *)eInfo->getErrArg();

// For close the main argument is the file pointer. Set the main arg to
// be the request identifier which is saved in he file object.
//
   eInfo->setErrArg(fP->cbArg);

// Responses to close() must be final; otherwise it's a systm error.
//
   if (Result != SFS_OK && Result != SFS_ERROR)
      {char buff[64];
       SI->errorCnt++;
       sprintf(buff, "Invalid close() callcback result of %d for", Result);
       eDest->Emsg("DoClose", buff, Path);
       Result = SFS_ERROR;
       eInfo->setErrInfo(kXR_FSError, "Internal error; file close forced");
      }

// Send appropriate response (OK or error)
//
   if (Result == SFS_OK) cbFunc->sendResp(eInfo, kXR_ok);
      else               cbFunc->sendError(Result, eInfo, Path);

// Delete he file object
//
   delete fP;
}
  
/******************************************************************************/
/*                               D o S t a t x                                */
/******************************************************************************/
  
void XrdXrootdCBJob::DoStatx(XrdOucErrInfo *einfo)
{
   const char *tp = einfo->getErrText();
   char cflags[2];
   int flags;

// Skip to the third token
//
   while(*tp && *tp == ' ') tp++;
   while(*tp && *tp != ' ') tp++; // 1st
   while(*tp && *tp == ' ') tp++;
   while(*tp && *tp != ' ') tp++; // 2nd

// Convert to flags
//
   flags = atoi(tp);

// Convert to proper indicator
//
        if (flags & kXR_offline) cflags[0] = (char)kXR_offline;
   else if (flags & kXR_isDir)   cflags[0] = (char)kXR_isDir;
   else                          cflags[0] = (char)kXR_file;

// Set the new response
//
   cflags[1] = '\0';
   einfo->setErrInfo(0, cflags);
}

/******************************************************************************/
/*                     X r d X r o o t d C a l l B a c k                      */
/******************************************************************************/
/******************************************************************************/
/*                                  D o n e                                   */
/******************************************************************************/
  
void XrdXrootdCallBack::Done(int           &Result,   //I/O: Function result
                             XrdOucErrInfo *eInfo,    // In: Error information
                             const char    *Path)     // In: Path related
{
   XrdXrootdCBJob *cbj;

// Sending an async response may take a long time. So, we schedule the task
// to run asynchronously from the forces that got us here.
//
   if (!(cbj = XrdXrootdCBJob::Alloc(this, eInfo, Path, Result)))
      {eDest->Emsg("Done",ENOMEM,"get call back job; user",eInfo->getErrUser());
       if (eInfo->getErrCB()) eInfo->getErrCB()->Done(Result, eInfo);
          else delete eInfo;
      } else Sched->Schedule((XrdJob *)cbj);
}

/******************************************************************************/
/*                                  S a m e                                   */
/******************************************************************************/
  
int XrdXrootdCallBack::Same(unsigned long long arg1, unsigned long long arg2)
{
   XrdXrootdReqID ReqID1(arg1), ReqID2(arg2);
   unsigned char sid1[2], sid2[2];
   unsigned int  inst1, inst2;
            int  lid1, lid2;

   ReqID1.getID(sid1, lid1, inst1);
   ReqID2.getID(sid2, lid2, inst2);
   return lid1 == lid2;
}

/******************************************************************************/
/*                             s e n d E r r o r                              */
/******************************************************************************/
  
void XrdXrootdCallBack::sendError(int            rc,
                                  XrdOucErrInfo *eInfo,
                                  const char    *Path)
{
   static const char *TraceID = "fsError";
   static int Xserr = kXR_ServerError;
   int ecode;
   const char *eMsg = eInfo->getErrText(ecode);
   const char *User = eInfo->getErrUser();

// Process the data response vector (we need to do this here)
//
   if (rc == SFS_DATAVEC)
      {if (ecode > 1) sendVesp(eInfo, kXR_ok, (struct iovec *)eMsg, ecode);
         else         sendResp(eInfo, kXR_ok, 0);
       return;
      }

// Optimize error message handling here
//
   if (eMsg && !*eMsg) eMsg = 0;

// Process standard errors
//
   if (rc == SFS_ERROR)
      {SI->errorCnt++;
       rc = XProtocol::mapError(ecode);
       sendResp(eInfo, kXR_error, &rc, eMsg, eInfo->getErrTextLen()+1);
       return;
      }

// Process the redirection (error msg is host:port)
//
   if (rc == SFS_REDIRECT)
      {SI->redirCnt++;
       if (ecode <= 0) ecode = (ecode ? -ecode : Port);
       TRACE(REDIR, User <<" async redir to " << eMsg <<':' <<ecode <<' '
                         <<(Path ? Path : ""));
       sendResp(eInfo, kXR_redirect, &ecode, eMsg, eInfo->getErrTextLen());
       if (XrdXrootdMonitor::Redirect() && Path)
           XrdXrootdMonitor::Redirect(eInfo->getErrMid(),eMsg,ecode,Opcode,Path);
       return;
      }

// Process the deferal
//
   if (rc >= SFS_STALL)
      {SI->stallCnt++;
       TRACE(STALL, "Stalling " <<User <<" for " <<rc <<" sec");
       sendResp(eInfo, kXR_wait, &rc, eMsg, eInfo->getErrTextLen()+1);
       return;
      }

// Process the data response
//
   if (rc == SFS_DATA)
      {if (ecode) sendResp(eInfo, kXR_ok, 0, eMsg, ecode);
         else     sendResp(eInfo, kXR_ok, 0);
       return;
      }

// Unknown conditions, report it
//
   {char buff[64];
    SI->errorCnt++;
    ecode = sprintf(buff, "Unknown sfs response code %d", rc);
    eDest->Emsg("sendError", buff);
    sendResp(eInfo, kXR_error, &Xserr, buff, ecode+1);
    return;
   }
}

/******************************************************************************/
/*                              s e n d R e s p                               */
/******************************************************************************/
  
void XrdXrootdCallBack::sendResp(XrdOucErrInfo  *eInfo,
                                 XResponseType   Status,
                                 int            *Data,
                                 const char     *Msg,
                                 int             Mlen)
{
   static const char *TraceID = "sendResp";
   struct iovec       rspVec[4];
   XrdXrootdReqID     ReqID;
   int                dlen = 0, n = 1;
   kXR_int32          xbuf;

   if (Data)
      {xbuf = static_cast<kXR_int32>(htonl(*Data));
               rspVec[n].iov_base = (caddr_t)(&xbuf);
       dlen  = rspVec[n].iov_len  = sizeof(xbuf); n++;           // 1
      }
    if (Msg && *Msg)
       {        rspVec[n].iov_base = (caddr_t)Msg;
        dlen += rspVec[n].iov_len  = Mlen; n++;                  // 2
       }

// Set the destination
//
   ReqID.setID(eInfo->getErrArg());

// Send the async response
//
   if (XrdXrootdResponse::Send(ReqID, Status, rspVec, n, dlen) < 0)
      eDest->Emsg("sendResp", eInfo->getErrUser(), Opname, 
                  "async resp aborted; user gone.");
      else if (TRACING(TRACE_RSP))
              {XrdXrootdResponse theResp;
               theResp.Set(ReqID.Stream());
               TRACE(RSP, eInfo->getErrUser() <<" async " <<theResp.ID()
                          <<' ' <<Opname <<" status " <<Status);
              }

// Release any external buffer from the errinfo object
//
   if (eInfo->extData()) eInfo->Reset();
}

/******************************************************************************/
/*                              s e n d V e s p                               */
/******************************************************************************/
  
void XrdXrootdCallBack::sendVesp(XrdOucErrInfo  *eInfo,
                                 XResponseType   Status,
                                 struct iovec   *ioV,
                                 int             ioN)
{
   static const char *TraceID = "sendVesp";
   XrdXrootdReqID     ReqID;
   int                dlen = 0;

// Calculate the amount of data being sent
//
   for (int i = 1; i < ioN; i++) dlen += ioV[i].iov_len;

// Set the destination
//
   ReqID.setID(eInfo->getErrArg());

// Send the async response
//
   if (XrdXrootdResponse::Send(ReqID, Status, ioV, ioN, dlen) < 0)
      eDest->Emsg("sendResp", eInfo->getErrUser(), Opname, 
                  "async resp aborted; user gone.");
      else if (TRACING(TRACE_RSP))
              {XrdXrootdResponse theResp;
               theResp.Set(ReqID.Stream());
               TRACE(RSP, eInfo->getErrUser() <<" async " <<theResp.ID()
                          <<' ' <<Opname <<" status " <<Status);
              }

// Release any external buffer from the errinfo object
//
   if (eInfo->extData()) eInfo->Reset();
}

/******************************************************************************/
/*                               S e t V a l s                                */
/******************************************************************************/
  
void XrdXrootdCallBack::setVals(XrdSysError    *erp,
                                XrdXrootdStats *SIp,
                                XrdScheduler   *schp,
                                int             port)
{
// Set values into out unnamed static space
//
   eDest = erp;
   SI    = SIp;
   Sched = schp;
   Port  = port;
}
