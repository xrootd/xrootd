/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d A i o T a s k . c c                    */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#include <cstdio>
#include <ctime>
#include <sys/uio.h>
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdXrootd/XrdXrootdAioBuff.hh"
#include "XrdXrootd/XrdXrootdAioFob.hh"
#include "XrdXrootd/XrdXrootdAioTask.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

#define TRACELINK dataLink
 
/******************************************************************************/
/*                        G l o b a l   S t a t i c s                         */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

namespace XrdXrootd
{
extern XrdSysError   eLog;
extern XrdScheduler *Sched;
}
using namespace XrdXrootd;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

const char *XrdXrootdAioTask::TraceID = "AioTask";
  
/******************************************************************************/
/*                             C o m p l e t e d                              */
/******************************************************************************/
  
void XrdXrootdAioTask::Completed(XrdXrootdAioBuff *aioP)
{
// Lock this code path
//
   aioMutex.Lock();

// If this request is not running and completed then take a shortcut.
//
   if (Status == Offline && isDone)
      {aioP->Recycle();
       inFlight--;
       aioMutex.UnLock();
       if (inFlight <= 0) Recycle(true);
       return;
      }

// Add this element to the end of the queue
//
   aioP->next = 0;
   if (!pendQ) pendQEnd = pendQ = aioP;
      else {pendQEnd->next = aioP;
            pendQEnd = aioP;
           }

// Check if the request is waiting for our buffer tell it now has one. Otherwise,
// if the task is offline then it cannot be done (see above); so schedule it.
//
   if (Status != Running)
      {if (Status == Waiting) aioReady.Signal();
          else Sched->Schedule(this);
       Status = Running;
      }

   aioMutex.UnLock();
}

/******************************************************************************/
/* Protected:                      D r a i n                                  */
/******************************************************************************/

bool XrdXrootdAioTask::Drain()
{
   XrdXrootdAioBuff *aioP;
   int maxWait = 6;          // Max seconds to wait for outstanding requests

// Reap as many aio object as you can
//
   aioMutex.Lock();
   while(inFlight > 0)
        {while((aioP = pendQ))
            {if (!(pendQ = aioP->next)) pendQEnd = 0;
             aioMutex.UnLock(); // Open a window of opportunity
             inFlight--;
             aioP->Recycle();
             aioMutex.Lock();
            }
         if (inFlight <= 0 || !Wait4Buff(maxWait)) break;
        }

// If there are still in flight requets, issue message and we will run the
// drain in the background.
//
   if (inFlight > 0)
      {char buff[128];
       snprintf(buff, sizeof(buff),
                      "aio%c overdue %d inflight request%s for",
                      (aioState & aioRead ? 'R' : 'W'), int(inFlight),
                      (inFlight > 1 ? "s" : ""));
       eLog.Emsg("AioTask", buff, dataLink->ID, dataFile->FileKey);
      }

// Indicate we are going offline and tell the caller if we need to stay
// alive to drain the tardy requests in the background.
//
   Status = Offline;
   isDone = true;
   aioMutex.UnLock();
   return inFlight <= 0;
}
  
/******************************************************************************/
/* Private:                       g d D o n e                                 */
/******************************************************************************/

int XrdXrootdAioTask::gdDone() // Only called for link to file transfers!
{
   XrdXrootdAioBuff *bP = pendWrite;
   int rc;

// Do some debugging
//
   TRACEP(DEBUG,"gdDone: "<<(void *)this<<" pendWrite "
                <<(pendWrite != 0 ? "set":"not set"));

// This is a callback indicating the pending aio object has all of the data.
// Resume sending data to the destination.
//
   pendWrite = 0;
   if (!bP) rc = CopyL2F();
      else {if (CopyL2F(bP) && (inFlight || !isDone)) rc = CopyL2F();
               else rc = 0;
           }

// Do some debugging
//
   TRACEP(DEBUG,"gdDone: "<<(void *)this<<" ending rc="<<rc);

// If we are not pausing for data to be delivered. Drain any oustanding aio
// requests and discard left over bytes, if any. Note we must copy the left
// over length as we may recycle before discarding as discard must be last.
//
   if (rc <= 0)
      {XrdXrootdProtocol* prot = Protocol;
       int dlen = dataLen;
       if (!inFlight) Recycle(true);
          else Recycle(Drain());
       if (!rc && dlen) return prot->getDump(Comment, dlen);
      }
   return rc;
}

/******************************************************************************/
/* Private:                       g d F a i l                                 */
/******************************************************************************/

void XrdXrootdAioTask::gdFail()
{
   char eBuff[512];

// Do some tracing
//
   TRACEP(DEBUG,"gdFail: "<<(void *)this);

// Format message for display
//
   snprintf(eBuff, sizeof(eBuff), "link error aborted %s for", Comment);
   eLog.Emsg("AioTask", eBuff, dataLink->ID, dataFile->FileKey);

// This is a callback indicating the link is dead. Terminate this operation.
//
   isDone = true;
   aioState |= aioDead;
   dataLen = 0;
   if (pendWrite) {pendWrite->Recycle(); pendWrite = 0;}

// If this is a read, cancel all queued read requests
//
   if (aioState & aioRead) dataFile->aioFob->Reset(Protocol);

// If we still have any requests in flight drain them.
//
   if (!inFlight) Recycle(true);
      else Recycle(Drain());
}

/******************************************************************************/
/* Protected:                    g e t B u f f                                */
/******************************************************************************/

XrdXrootdAioBuff* XrdXrootdAioTask::getBuff(bool wait)
{
   XrdXrootdAioBuff* aioP;

// Try to get the next buffer
//
   aioMutex.Lock();
do{if ((aioP = pendQ))
      {if (!(pendQ = aioP->next)) pendQEnd = 0;
       aioMutex.UnLock();
       inFlight--;
       return aioP;
      }

// If the caller does not want to wait or if there is nothing in flight, return
//
   if (!wait || !inFlight)
      {aioMutex.UnLock();
       return 0;
      }

// So, wait for a buffer to arrive
//
  } while(Wait4Buff());

// We timed out and this is considered an error
//
   aioMutex.UnLock();
   SendError(ETIMEDOUT, (aioState & aioRead ? "aio file read timed out"
                                            : "aio file write timed out"));
   return 0;
}
  
/******************************************************************************/
/*                                    I D                                     */
/******************************************************************************/

const char *XrdXrootdAioTask::ID() {return dataLink->ID;}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

void XrdXrootdAioTask::Init(XrdXrootdProtocol *protP,
                            XrdXrootdResponse &resp,
                            XrdXrootdFile     *fP)
{

// Reset the object
//
   pendQEnd   = pendQ = 0;
   finalRead  = 0; // Also sets pendWrite
   Protocol   = protP;
   dataLink   = resp.theLink();
   Response   = resp;
   dataFile   = fP;
   aioState   = 0;
   inFlight   = 0;
   isDone     = false;
   Status     = Running;
}
  
/******************************************************************************/
/* Protected:                  S e n d E r r o r                              */
/******************************************************************************/
  
void XrdXrootdAioTask::SendError(int rc, const char *eText)
{
   char eBuff[1024];

// If there is no error text, use the rc
//
   if (!eText) eText = (rc ? XrdSysE2T(rc) : "invalid error code");

// For message for display
//
   snprintf(eBuff, sizeof(eBuff), "async %s failed for %s;",
            (aioState & aioRead ? "read" : "write"), dataLink->ID);
   eLog.Emsg("AioTask", eBuff, eText, dataFile->FileKey);

// If this request is still active, send the error to the client
//
   if (!isDone)
      {XErrorCode eCode = (XErrorCode)XProtocol::mapError(rc);
       if (Response.Send(eCode, eText))
          {aioState |= aioDead;
           dataLen = 0;
          } else if (aioState & aioRead) dataLen = 0;
       isDone = true;
      }
}

/******************************************************************************/
/* Protected:                S e n d F S E r r o r                            */
/******************************************************************************/
  
void XrdXrootdAioTask::SendFSError(int rc)
{
   XrdOucErrInfo &myError = dataFile->XrdSfsp->error;
   int eCode;

// We can only handle actual errors. Under some conditions a redirect (e.g.
// Xcache) can return other error codes. We treat these as server errors.
//
   if (rc != SFS_ERROR)
      {char eBuff[256];
       snprintf(eBuff, sizeof(eBuff), "fs returned unexpected rc %d", rc);
       SendError(EFAULT, eBuff);
       if (myError.extData()) myError.Reset();
       return;
      }

// Handle file system error but only if we are still alive
//
   if (!isDone)
      {const char *eMsg = myError.getErrText(eCode);
       eLog.Emsg("AioTask", dataLink->ID, eMsg, dataFile->FileKey);
       int rc = XProtocol::mapError(eCode);
       if (Response.Send((XErrorCode)rc, eMsg))
          {aioState |= aioDead;
           dataLen = 0;
          } else if (aioState & aioRead) dataLen = 0;
       isDone = true;
      }

// Clear error message and recycle aio object if need be
//
   if (myError.extData()) myError.Reset();
}

/******************************************************************************/
/* Protected:                   V a l i d a t e                               */
/******************************************************************************/

bool XrdXrootdAioTask::Validate(XrdXrootdAioBuff* aioP)
{
   ssize_t aioResult = aioP->Result;
   off_t   aioOffset = aioP->sfsAio.aio_offset;
   int     aioLength = aioP->sfsAio.aio_nbytes;

// Step 1: Check if this request is already completed. This may be the case
//         if we had a previous error.
//
   if (isDone) return false;

// Step 2: Check if an error occurred as this will terminate the request even
//         if we have not sent all of the data.
//
   if (aioP->Result < 0)
      {SendError(-aioP->Result, 0);
       return false;
      }

// Step 3: Check for a short read which signals that no more data past this
//         offset is forthcomming. Save it as we will send a final response
//         using this element. We discard zero length reads. It's an error if we
//         get more than one short read with data or if its offset is less than
//         the highest full read element.
//
   if (aioResult < aioLength)
      {dataLen = 0;
       if (!aioResult)
          {if ((finalRead && aioOffset < finalRead->sfsAio.aio_offset)
           ||  aioOffset < highOffset) SendError(EFAULT, "embedded null block");
           return false;
          } else {
           if (aioOffset < highOffset)
              {SendError(ENODEV, "embedded short block");
               return false;
              } else {
               if (finalRead) SendError(ENODEV, "multiple short blocks");
                  else {finalRead = aioP;
                        highOffset = aioOffset;
                       }
              }
          }
       return false;
      }

// Step 4: This is a full read and its offset must be lower than the offset of
//         any short read we have encountered.
//
   if (finalRead && aioOffset >= finalRead->sfsAio.aio_offset)
      {SendError(ENODEV, "read offset past EOD");
       return false;
      }
   if (aioOffset > highOffset) highOffset = aioOffset;
   return true;
}
  
/******************************************************************************/
/* Private:                    W a i t 4 B u f f                              */
/******************************************************************************/
  
// Called with with aioMutex locked and returns it locked.

bool XrdXrootdAioTask::Wait4Buff(int maxWait)
{
   static const int msgInterval = 30;
   time_t begWait;
   int aioWait, msgWait = msgInterval, totWait = 0;

// Return success if somehow we got a buffer
//
   if (pendQ) return true;

// Make sure that something will actually arrive but issue a warning
// message and sleep a bit to avoid a loop as there is clearly a logic error.
//
   if (!inFlight)
      {eLog.Emsg("Wait4Buff", dataLink->ID, "has nothing inflight for",
                 dataFile->FileKey);
       XrdSysTimer::Snooze(30);
       return false;
      }

// Calculate wait time and when we should produce a message
//
   if (maxWait <= 0)  maxWait = XrdXrootdProtocol::as_timeout;
   aioWait = (maxWait > msgInterval ? msgInterval : maxWait);

// Wait for a buffer to arrive.
//
   begWait = time(0);
   while(totWait < maxWait)
        {Status = Waiting;
         aioReady.Wait(aioWait);
         if (pendQ) break;
         totWait = (time(0) - begWait); // Spurious wakeup
         int tmpWait = maxWait - totWait;
         if (tmpWait > 0 && tmpWait < aioWait) aioWait = tmpWait;
         if (totWait >= msgWait)
            {char buff[80];
             int inF = inFlight;
             msgWait += aioWait;
             snprintf(buff, sizeof(buff), "%d tardy aio%c requests for",
                            inF, (aioState & aioRead ? 'R' : 'W'));
             eLog.Emsg("Wait4Buff", dataLink->ID, buff, dataFile->FileKey);
            }
        }

// If we are here either we actually have a buffer available or timed out.
//
   Status = Running;
   return (pendQ != 0);
}
