/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d P g r w A i o . c c                    */
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
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdAioFob.hh"
#include "XrdXrootd/XrdXrootdAioPgrw.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdPgrwAio.hh"
#include "XrdXrootd/XrdXrootdPgwBadCS.hh"
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

const char *XrdXrootdPgrwAio::TraceID = "PgrwAio";
  
/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

namespace
{
XrdSysMutex       fqMutex;
XrdXrootdPgrwAio *fqFirst = 0;
int               numFree = 0;

static const int  maxKeep = 64; // 4 MB to keep in reserve
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdPgrwAio *XrdXrootdPgrwAio::Alloc(XrdXrootdProtocol *protP,
                                          XrdXrootdResponse &resp,
                                          XrdXrootdFile     *fP,
                                          XrdXrootdPgwBadCS *bcsP)
{
   XrdXrootdPgrwAio *reqP;

// Obtain a preallocated aio request object
//
   fqMutex.Lock();
   if ((reqP = fqFirst))
      {fqFirst = reqP->nextPgrw;
       numFree--;
      }
   fqMutex.UnLock();

// If we have no object, create a new one
//
   if (!reqP) reqP = new XrdXrootdPgrwAio;

// Initialize the object and return it
//
   reqP->Init(protP, resp, fP);
   reqP->nextPgrw = 0;
   reqP->badCSP   = bcsP;
   return reqP;
}

/******************************************************************************/
/* Private:                C o p y F 2 L _ A d d 2 Q                          */
/******************************************************************************/

bool XrdXrootdPgrwAio::CopyF2L_Add2Q(XrdXrootdAioPgrw *aioP)
{
   const char *eMsg;
   int dlen, rc;
  
// Dispatch the requested number of aio requests if we have enough data
//
   if (dataLen > 0)
      {if (!aioP && !(aioP = XrdXrootdAioPgrw::Alloc(this)))
          {if (inFlight) return true;
           SendError(ENOMEM, "insufficient memory");
           return false;
          }
       if (!(dlen = aioP->Setup2Send(dataOffset, dataLen, eMsg)))
          {SendError(EINVAL, eMsg);
           aioP->Recycle();
           return false;
          }
       if ((rc = dataFile->XrdSfsp->pgRead((XrdSfsAio *)aioP)) != SFS_OK)
          {SendFSError(rc);
           aioP->Recycle();
           return false;
          }
       inFlight++;
       TRACEP(FSAIO, "pgrd beg " <<dlen <<'@' <<dataOffset
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
  
void XrdXrootdPgrwAio::CopyF2L()
{
   XrdXrootdAioBuff *bP;
   XrdXrootdAioPgrw *aioP;

// Pick a finished element off the pendQ. Wait for an oustanding buffer if we
// reached our buffer limit. Otherwise, ask for a return if we can start anew.
// Note: We asked getBuff() if it returns nil to not release the lock.
//
do{bool doWait = dataLen <= 0 || inFlight >= XrdXrootdProtocol::as_maxperreq;
   if (!(bP = getBuff(doWait)))
      {if (isDone || !CopyF2L_Add2Q()) break;
       continue;
      }

// Step 1: do some tracing
//
   TRACEP(FSAIO,"pgrd end "<<bP->sfsAio.aio_nbytes<<'@'<<bP->sfsAio.aio_offset
                 <<" result="<<bP->Result<<" D-S="<<isDone<<'-'<<int(Status)
                 <<" inF="<<int(inFlight));

// Step 2: Validate this buffer
//
   if (!Validate(bP))
      {if (bP != finalRead) bP->Recycle();
       continue;
      }

// Step 3: Get a pointer to the derived type (we avoid dynamic cast)
//
   aioP = bP->pgrwP;

// Step 4: If this aio request was simulated (indicated by cksVec being nil)
// we have to compute the checksums and reset the pointer via noChkSums().
//
   if (aioP->noChkSums() && aioP->Result > 0)
      XrdOucPgrwUtils::csCalc((char *)aioP->sfsAio.aio_buf,
                       aioP->sfsAio.aio_offset, aioP->Result, aioP->cksVec);

// Step 5: If this is the last block to be read then save it for final status
//
   if (inFlight == 0 && dataLen == 0 && !finalRead)
      {finalRead = aioP;
       break;
      }

// Step 8: Send the data to the client and if successful, see if we need to
//         schedule more data to be read from the data source.
//
   if (!isDone && SendData(aioP) && dataLen) {if (!CopyF2L_Add2Q(aioP)) break;}
      else aioP->Recycle();

   } while(inFlight > 0);

// If we are here then the request has finished. If all went well,
// fire off the final response.
//
   if (!isDone) SendData(finalRead, true);
   if (finalRead) finalRead->Recycle();

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
  
int XrdXrootdPgrwAio::CopyL2F()
{
   XrdXrootdAioBuff *bP;
   XrdXrootdAioPgrw *aioP;
   const char *eMsg;
   int dLen, ioVNum, rc;

// Pick a finished element off the pendQ. If there are no elements then get
// a new one if we can. Otherwise, we will have to wait for one to come back.
// Unlike read() writes are bound to a socket and we cannot reliably
// give up the thread by returning to level 0.
//
do{bool doWait = dataLen <= 0 || inFlight >= XrdXrootdProtocol::as_maxperreq;
   if (!(bP = getBuff(doWait)))
      {if (isDone) return 0;
       if (!(aioP = XrdXrootdAioPgrw::Alloc(this)))
          {SendError(ENOMEM, "insufficient memory");
           return 0;
          }
      } else {
       aioP = bP->pgrwP;

       TRACEP(FSAIO,"pgwr end "<<aioP->sfsAio.aio_nbytes<<'@'
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
   dLen = aioP->Setup2Recv(dataOffset, dataLen, eMsg);
   if (!dLen)
      {SendError(EINVAL, eMsg);
       aioP->Recycle();
       return 0;
      }
   dataOffset += aioP->sfsAio.aio_nbytes;
   dataLen    -= dLen;

// Get the iovec information
//
   struct iovec *ioV = aioP->iov4Recv(ioVNum);

// Issue the read to get the data into the buffer
//
   if ((rc = Protocol->getData(this, "pgWrite", ioV, ioVNum)))
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
      {if (!dataLen) return SendDone();
       SendError(EIDRM, "pgWrite encountered an impossible condition");
       eLog.Emsg("PgrwAio", "pgWrite logic error for",
                            dataLink->ID, dataFile->FileKey);
      }

// Cleanup as we don't know where we will return
//
   return 0;
}

/******************************************************************************/
  
bool XrdXrootdPgrwAio::CopyL2F(XrdXrootdAioBuff *bP)
{

// Verify the checksums. Upon success, write out the data.
//
   if (VerCks(bP->pgrwP))
      {int rc = dataFile->XrdSfsp->pgWrite((XrdSfsAio *)bP);
       if (rc != SFS_OK) {SendFSError(rc); bP->Recycle();}
          else {inFlight++;
                TRACEP(FSAIO, "pgwr beg " <<bP->sfsAio.aio_nbytes <<'@'
                                          <<bP->sfsAio.aio_offset
                                          <<" inF=" <<int(inFlight));
                return true;
               }
      }
   return false;
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

// This method is invoked when we have run out of aio objects but have inflight
// objects during reading. In that case, we must relinquish the thread. When an
// aio object completes it will reschedule this object on a new thread.
  
void XrdXrootdPgrwAio::DoIt()
{
// Reads run disconnected as they will never read from the link.
//
   if (aioState & aioRead) CopyF2L();
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

void XrdXrootdPgrwAio::Read(long long offs, int dlen)
{

// Setup the copy from the file to the network
//
   dataOffset = highOffset = offs;
   dataLen    = dlen;
   aioState   = aioRead | aioPage;

// Reads run disconnected and are self-terminating, so we need to inclreas the
// refcount for the link we will be using to prevent it from disaapearing.
// Recycle will decrement it but does so only for reads. We always up the file
// refcount and number of requests.
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

void XrdXrootdPgrwAio::Recycle(bool release)
{
// Update request count, file and link reference count
//
   if (!(aioState & aioHeld))
       {Protocol->aioUpdReq(-1);
        if (aioState & aioRead)
           {dataLink->setRef(-1);
            dataFile->Ref(-1);
           }
        aioState |= aioHeld;
       }

// Do some traceing
//
   TRACEP(FSAIO,"pgrw recycle "<<(release ? "" : "hold ")
                <<(aioState & aioRead ? 'R' : 'W')<<" D-S="
                <<isDone<<'-'<<int(Status));

// Place the object on the free queue if possible
//
   if (release)
      {fqMutex.Lock();
       if (numFree >= maxKeep)
          {fqMutex.UnLock();
           delete this;
          } else {
           nextPgrw = fqFirst;
           fqFirst = this;
           numFree++;
           fqMutex.UnLock();
          }
      }
}

/******************************************************************************/
/* Private:                     S e n d D a t a                               */
/******************************************************************************/

bool XrdXrootdPgrwAio::SendData(XrdXrootdAioBuff *bP, bool final)
{
   static const int infoLen = sizeof(kXR_int64);
   struct pgReadResponse
         {ServerResponseStatus rsp;
          kXR_int64            ofs;
         } pgrResp;
   int rc;

// Preinitialize the header
//
   pgrResp.rsp.bdy.requestid = kXR_pgread - kXR_1stRequest;
   pgrResp.rsp.bdy.resptype  = (final ? XrdProto::kXR_FinalResult
                                      : XrdProto::kXR_PartialResult);
   memset(pgrResp.rsp.bdy.reserved, 0, sizeof(pgrResp.rsp.bdy.reserved));

// Send the data; we might not have any (typically in a final response)
//
   if (bP)
      {int iovLen, iovNum;
       struct iovec *ioVec = bP->pgrwP->iov4Send(iovNum, iovLen, true);
       pgrResp.ofs = htonll(bP->sfsAio.aio_offset);
       rc = Response.Send(pgrResp.rsp, infoLen, ioVec, iovNum, iovLen);
      } else {
       pgrResp.rsp.bdy.dlen = 0;
       pgrResp.ofs          = htonll(dataOffset);
       rc = Response.Send(pgrResp.rsp, infoLen);
      }

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
/* Private:                     S e n d D o n e                               */
/******************************************************************************/

int XrdXrootdPgrwAio::SendDone()
{
   static const int infoLen = sizeof(kXR_int64);
   struct {ServerResponseStatus       rsp;
           ServerResponseBody_pgWrite info;  // info.offset
          } pgwResp;
   char *buff;
   int n, rc;

// Preinitialize the header
//
   pgwResp.rsp.bdy.requestid = kXR_pgwrite - kXR_1stRequest;
   pgwResp.rsp.bdy.resptype  = XrdProto::kXR_FinalResult;
   pgwResp.info.offset       = htonll(highOffset);
   memset(pgwResp.rsp.bdy.reserved, 0, sizeof(pgwResp.rsp.bdy.reserved));

// Get any checksum correction information we should turn
//
   buff = badCSP->boInfo(n);

// Send the final response
//
   if ((rc = Response.Send(pgwResp.rsp, infoLen, buff, n))) dataLen = 0;
   isDone = true;
   if (rc) aioState |= aioDead;
   return rc;
}

/******************************************************************************/
/*                                V e r C k s                                 */
/******************************************************************************/

bool XrdXrootdPgrwAio::VerCks(XrdXrootdAioPgrw *aioP)
{
   off_t     dOffset = aioP->sfsAio.aio_offset;
   uint32_t *csVec, *csVP, csVal;
   int       ioVNum, dLen;

// Get the iovec information as this will drive the checksum
//
   struct iovec *ioV = aioP->iov4Data(ioVNum);
   csVP = csVec = (uint32_t*)ioV[0].iov_base;

// Verify each page or page segment
//
   for (int i = 1; i < ioVNum; i +=2)
       {dLen = ioV[i].iov_len;
        csVal = ntohl(*csVP); *csVP++ = csVal;
        if (!XrdOucCRC::Ver32C(ioV[i].iov_base, dLen, csVal))
           {const char *eMsg = badCSP->boAdd(dataFile, dOffset, dLen);
            if (eMsg) {SendError(ETOOMANYREFS, eMsg);
                       aioP->Recycle();
                       return false;
                      }
           }
        dOffset += dLen;
       }

// All done, while we may have checksum error there is nothing we can do about
// it and it's up to the client to send corrections.
//
   return true;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/

int XrdXrootdPgrwAio::Write(long long offs, int dlen)
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
