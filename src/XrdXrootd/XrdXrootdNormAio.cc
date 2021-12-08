/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d N o r m A i o . c c                    */
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
#include <sys/uio.h>

#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdAioBuff.hh"
#include "XrdXrootd/XrdXrootdAioFob.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdNormAio.hh"
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
/*                       S t a t i c   M e m e b e r s                        */
/******************************************************************************/

const char *XrdXrootdNormAio::TraceID = "NormAio";
  
/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

namespace
{
XrdSysMutex       fqMutex;
XrdXrootdNormAio *fqFirst = 0;
int               numFree = 0;

static const int  maxKeep = 64; // Keep in reserve
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdNormAio *XrdXrootdNormAio::Alloc(XrdXrootdProtocol *protP,
                                          XrdXrootdResponse &resp,
                                          XrdXrootdFile     *fP)
{
   XrdXrootdNormAio *reqP;

// Obtain a preallocated aio request object
//
   fqMutex.Lock();
   if ((reqP = fqFirst))
      {fqFirst = reqP->nextNorm;
       numFree--;
      }
   fqMutex.UnLock();

// If we have no object, create a new one
//
   if (!reqP) reqP = new XrdXrootdNormAio;

// Initialize the object and return it
//
   reqP->Init(protP, resp, fP);
   reqP->nextNorm = 0;
   return reqP;
}
  
/******************************************************************************/
/* Private:                C o p y F 2 L _ A d d 2 Q                          */
/******************************************************************************/

bool XrdXrootdNormAio::CopyF2L_Add2Q(XrdXrootdAioBuff *aioP)
{
   int dlen, rc;
  
// Dispatch the requested number of aio requests if we have enough data
//
   if (dataLen > 0)
      {if (!aioP && !(aioP = XrdXrootdAioBuff::Alloc(this)))
          {if (inFlight) return true;
           SendError(ENOMEM, "insufficient memory");
           return false;
          }
       aioP->sfsAio.aio_offset = dataOffset;
       if (dataLen >= (int)aioP->sfsAio.aio_nbytes)
               dlen = aioP->sfsAio.aio_nbytes;
          else dlen = aioP->sfsAio.aio_nbytes = dataLen;

       if ((rc = dataFile->XrdSfsp->read((XrdSfsAio *)aioP)) != SFS_OK)
          {SendFSError(rc);
           aioP->Recycle();
           return false;
          }
       inFlight++;
       TRACEP(FSAIO, "aioR beg " <<dlen <<'@' <<dataOffset
                                 <<" inF=" <<int(inFlight));
       dataOffset += dlen;
       dataLen    -= dlen;
       if (dataLen <= 0)
          {dataFile->aioFob->Schedule(Protocol);
           aioState |= aioSchd;
          }
      }
   return true;
}
  
/******************************************************************************/
/* Private:                      C o p y F 2 L                                */
/******************************************************************************/
  
void XrdXrootdNormAio::CopyF2L()
{
   XrdXrootdAioBuff *aioP;
   bool aOK = true;

// Pick a finished element off the pendQ. Wait for an oustanding buffer if we
// reached our buffer limit. Otherwise, ask for a return if we can start anew.
// Note: We asked getBuff() if it returns nil to not release the lock.
//
do{bool doWait = dataLen <= 0 || inFlight >= XrdXrootdProtocol::as_maxperreq;
   if (!(aioP = getBuff(doWait)))
      {if (isDone || !CopyF2L_Add2Q()) break;
       continue;
      }

// Step 1: do some tracing
//
   TRACEP(FSAIO,"aioR end "<<aioP->sfsAio.aio_nbytes
              <<'@'<<aioP->sfsAio.aio_offset
              <<" result="<<aioP->Result<<" D-S="<<isDone<<'-'<<int(Status)
              <<" inF="<<int(inFlight));

// Step 2: Validate this buffer
//
   if (!Validate(aioP))
      {if (aioP != finalRead) aioP->Recycle();
       continue;
      }

// Step 3: Since block may come back out of order we need to make sure we are
//         sending then in proper order with no gaps.
//
   if (aioP->sfsAio.aio_offset != sendOffset && !isDone)
      {XrdXrootdAioBuff *bP = sendQ, *bPP = 0;
       while(bP)
            {if (aioP->sfsAio.aio_offset < bP->sfsAio.aio_offset) break;
             bPP = bP; bP = bP->next;
            }
       aioP->next = bP;
       if (bPP) bPP->next = aioP;
          else  sendQ = aioP;
       reorders++;
       TRACEP(FSAIO,"aioR inQ "<<aioP->Result<<'@'<<aioP->sfsAio.aio_offset);
       continue;
      }

// Step 4: If this is the last block to be read then establish the actual
//         last block to be used for final status to avoid an extra response.
//
   if (inFlight == 0 && dataLen == 0 && !finalRead)
      {if (!sendQ)
          {finalRead = aioP;
           break;
          } else {
           XrdXrootdAioBuff *bP = sendQ, *bPP = 0;
           while(bP->next) {bPP = bP; bP = bP->next;}
           if (bPP) {finalRead = bP; bPP->next = 0;}
              else  {finalRead = sendQ; sendQ = 0;}
          }
      }

// Step 5: Send the data to the client and if successful, see if we need to
//         schedule more data to be read from the data source.
//
   if (isDone || !Send(aioP) || dataLen <= 0) aioP->Recycle();
      else if (!CopyF2L_Add2Q(aioP)) break;

// Step 6: Now send any queued messages that are eligible to be sent
//
   while(sendQ && sendQ->sfsAio.aio_offset == sendOffset && aOK)
      {aioP  = sendQ;
       sendQ = sendQ->next;
       TRACEP(FSAIO,"aioR deQ "<<aioP->Result<<'@'<<aioP->sfsAio.aio_offset);
       if (!isDone && Send(aioP) && dataLen) aOK = CopyF2L_Add2Q(aioP);
          else aioP->Recycle();
      }

   } while(inFlight > 0 && aOK);

// If we are here then the request has finished. If all went well,
// fire off the final response.
//                                                                               .
   if (!isDone)
      {if (sendQ)
          {char ebuff[80];
           snprintf(ebuff, sizeof(ebuff), "aio read failed at offset %lld; "
                    "missing data", static_cast<long long>(sendOffset));
           SendError(ENODEV, ebuff);
          } else Send(finalRead, true);
      }

// Cleanup anything left over
//
   if (finalRead) finalRead->Recycle();
   while((aioP = sendQ)) {sendQ = sendQ->next; aioP->Recycle();}

// If we encountered a fatal link error then cancel any pending aio reads on
// this link. Otherwise if we have not yet scheduled the next aio, do so.
//
   if (aioState & aioDead) dataFile->aioFob->Reset(Protocol);
      else if (!(aioState & aioSchd)) dataFile->aioFob->Schedule(Protocol);

// Do a quick drain if something is still in flight for logging purposes.
// If the quick drain wasn't successful, then draining will be done in
// the background; which, of course, might never complete. Otherwise, recycle.
//
   if (!inFlight) Recycle(true);
      else Recycle(Drain());
}
  
/******************************************************************************/
/* Private:                      C o p y L 2 F                                */
/******************************************************************************/
  
int XrdXrootdNormAio::CopyL2F()
{
   XrdXrootdAioBuff *aioP;
   int dLen, rc;

// Pick a finished element off the pendQ. If there are no elements then get
// a new one if we can. Otherwise, we will have to wait for one to come back.
// Unlike read() writes are bound to a socket and we cannot reliably
// give up the thread by returning to level 0.
//
do{bool doWait = dataLen <= 0 || inFlight >= XrdXrootdProtocol::as_maxperreq;
   if (!(aioP = getBuff(doWait)))
      {if (isDone) return 0;
       if (!(aioP = XrdXrootdAioBuff::Alloc(this)))
          {SendError(ENOMEM, "insufficient memory");
           return 0;
          }
      } else {

       TRACEP(FSAIO, "aioW end "<<aioP->sfsAio.aio_nbytes<<'@'
                   <<aioP->sfsAio.aio_offset<<" result="<<aioP->Result
                   <<" D-S="<<isDone<<'-'<<int(Status)<<" inF="<<int(inFlight));

// If the aio failed, send an error
//
   if (aioP->Result <= 0)
      {SendError(-aioP->Result, 0);
       aioP->Recycle();
       return 0;  // Caller will drain
      }

// If we have no data or status was posted, ignore the result
//
   if (dataLen <= 0 || isDone)
      {aioP->Recycle();
       continue;
      }
      }

// Setup the aio object
//
   aioP->sfsAio.aio_offset = dataOffset;
   if (dataLen >= (int)aioP->sfsAio.aio_nbytes)
           dLen = aioP->sfsAio.aio_nbytes;
      else dLen = aioP->sfsAio.aio_nbytes = dataLen;
   dataOffset += dLen;
   dataLen    -= dLen;

// Issue the read to get the data into the buffer
//
   if ((rc = Protocol->getData(this,"aiowr",(char *)aioP->sfsAio.aio_buf,dLen)))
      {if (rc > 0) pendWrite = aioP;
          else {aioP->Recycle();  // rc must be < 0!
                dataLen = 0;
               }
       return rc;
      }

// Complete the write operation
//
   if (!CopyL2F(aioP)) return 0;

  } while(inFlight);

// If we finished successfully, send off final response otherwise its an error.
//
   if (!isDone)
      {if (!dataLen) return (Send(0) ? 0 : -1);
       SendError(EIDRM, "aioWrite encountered an impossible condition");
       eLog.Emsg("NormAio", "write logic error for",
                            dataLink->ID, dataFile->FileKey);
      }

// Cleanup as we don't know where we will return
//
   return 0;
}

/******************************************************************************/
  
bool XrdXrootdNormAio::CopyL2F(XrdXrootdAioBuff *aioP)
{

// Write out the data
//
   int rc = dataFile->XrdSfsp->write((XrdSfsAio *)aioP);
   if (rc != SFS_OK)
      {SendFSError(rc);
       aioP->Recycle();
       return false;
      }

// Do some tracing and return
//
   inFlight++;
   TRACEP(FSAIO,"aioW beg "<<aioP->sfsAio.aio_nbytes <<'@'
                           <<aioP->sfsAio.aio_offset <<" inF=" <<int(inFlight));
   return true;
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

// This method is invoked when we have run out of aio objects but have inflight
// objects during reading. In that case, we must relinquish the thread. When an
// aio object completes it will reschedule this object on a new thread.
  
void XrdXrootdNormAio::DoIt()
{
// Reads run disconnected as they will never read from the link.
//
   if (aioState & aioRead) CopyF2L();
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

void XrdXrootdNormAio::Read(long long offs, int dlen)
{

// Setup the copy from the file to the network
//
   dataOffset = highOffset = sendOffset = offs;
   dataLen    = dlen;
   aioState   = aioRead;

// Reads run disconnected and are self-terminating, so we need to increase the
// refcount for the link we will be using to prevent it from disapearing.
// Recycle will decrement it but does so only for reads. We always update
// the file refcount and increase the request count.
//
   dataLink->setRef(1);
   dataFile->Ref(1);
   Protocol->aioUpdReq(1);

// Schedule ourselves to run this asynchronously and return
//
   dataFile->aioFob->Schedule(this);
}
  
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdXrootdNormAio::Recycle(bool release)
{
// Update request count, file and link reference count
//
   if (!(aioState & aioHeld))
      {Protocol->aioUpdReq(-1);
       if (aioState & aioRead)
          {dataFile->Ref(-1);
           dataLink->setRef(-1);
          }
       aioState |= aioHeld;
      }

// Do some tracing and reset reorder counter
//
   TRACEP(FSAIO,"aio"<<(aioState & aioRead ? 'R' : 'W')<<" recycle"
                     <<(release ? "" : " hold")<<"; reorders="<<reorders
                     <<" D-S="<<isDone<<'-'<<int(Status));
   reorders = 0;

// Place the object on the free queue if possible
//
   if (release)
      {fqMutex.Lock();
       if (numFree >= maxKeep)
          {fqMutex.UnLock();
           delete this;
          } else {
           nextNorm = fqFirst;
           fqFirst = this;
           numFree++;
           fqMutex.UnLock();
          }
      }
}

/******************************************************************************/
/* Private:                         S e n d                                   */
/******************************************************************************/

bool XrdXrootdNormAio::Send(XrdXrootdAioBuff *aioP, bool final)
{
   XResponseType code = (final ? kXR_ok : kXR_oksofar);
   int rc;

// Send the data (note that no data means it's a finalresponse)
//
   if (aioP)
      {rc = Response.Send(code,(void*)aioP->sfsAio.aio_buf,aioP->Result);
       sendOffset = aioP->sfsAio.aio_offset + aioP->Result;
      } else rc = Response.Send();

// Diagnose any errors
//
   if (rc || final)
      {isDone = true;
       dataLen = 0;
       if (rc) aioState |= aioDead;
      }
   return rc == 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/

int XrdXrootdNormAio::Write(long long offs, int dlen)
{
// Update request count. Note that dataLink and dataFile references are
// handled outboard as writes are inextricably tied to the data link.
//
   Protocol->aioUpdReq(1);

// Setup the copy from the network to the file
//
   aioState  &= ~aioRead;
   dataOffset = highOffset = offs;
   dataLen    = dlen;

// Since this thread can't do anything else since it's blocked by the socket
// we simply initiate the write operation via a simulated getData() callback.
//
   return gdDone();
}
