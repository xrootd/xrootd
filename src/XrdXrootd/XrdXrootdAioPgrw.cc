/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d A i o P g r w . c c                    */
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
  
#include <unistd.h>
#include <arpa/inet.h>

#include "Xrd/XrdBuffer.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdAioPgrw.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

#define TRACELINK reqP
#define ID ID()
 
/******************************************************************************/
/*                        G l o b a l   S t a t i c s                         */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

namespace XrdXrootd
{
extern XrdBuffManager       *BPool;
}

using namespace XrdXrootd;
  
/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

const char *XrdXrootdAioPgrw::TraceID = "PgrwBuff";

namespace
{
XrdSysMutex       fqMutex;
XrdXrootdAioBuff *fqFirst = 0;
int               numFree = 0;

static const int  csLen = sizeof(uint32_t);

static const int  maxKeep = 64; // 4MB in the pocket
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdAioPgrw::XrdXrootdAioPgrw(XrdXrootdAioTask* tP, XrdBuffer *bP)
                 : XrdXrootdAioBuff(this, tP, bP)
{
   char *buff = bP->buff;
   uint32_t *csV = csVec;

// Fill out the iovec
//
   for (int i = 1; i <= acsSZ<<1; i+= 2)
       {ioVec[i  ].iov_base = csV;
        ioVec[i  ].iov_len  = csLen;
        ioVec[i+1].iov_base = buff;
        ioVec[i+1].iov_len  = XrdProto::kXR_pgPageSZ;
        csV++;
        buff += XrdProto::kXR_pgPageSZ;
       }

// Complete initialization
//
   Result = 0;
   iovReset = 0;
   cksVec = csVec;
   TIdent = "AioPgrw";
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdXrootdAioPgrw::~XrdXrootdAioPgrw()
{
// Recycle the buffer if we have one
//
   if (buffP) BPool->Release(buffP);
}
  
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdAioPgrw *XrdXrootdAioPgrw::Alloc(XrdXrootdAioTask* arp)
{
   XrdXrootdAioBuff *aiobuff;

// Obtain a preallocated aio object
//
   fqMutex.Lock();
   if ((aiobuff = fqFirst))
      {fqFirst = aiobuff->next;
       numFree--;
      }
   fqMutex.UnLock();

// If we have no object, create a new one. Otherwise initialize n old one
//
   if (!aiobuff)
      {XrdBuffer *bP = BPool->Obtain(aioSZ);
       if (!bP) return 0;
       aiobuff = new XrdXrootdAioPgrw(arp, bP);
      } else {
       aiobuff->Result = 0;
       aiobuff->cksVec = aiobuff->pgrwP->csVec;
       aiobuff->pgrwP->reqP = arp;
      }

// Update aio counters
//
   arp->urProtocol()->aioUpdate(1);

// All done
//
   return aiobuff->pgrwP;
}
  
/******************************************************************************/
/*                              i o v 4 R e c v                               */
/******************************************************************************/
  
struct iovec *XrdXrootdAioPgrw::iov4Recv(int &iovNum)
{
// Readjust ioVec as needed
//
   if (aioSZ != (int)sfsAio.aio_nbytes)
      {int fLen, lLen;
       csNum = XrdOucPgrwUtils::csNum(sfsAio.aio_offset, sfsAio.aio_nbytes,
                                      fLen, lLen);
       ioVec[2].iov_len = fLen;
       if (csNum > 1 && lLen != XrdProto::kXR_pgPageSZ)
          {iovReset = csNum<<1;
           ioVec[iovReset].iov_len = lLen;
          }
      } else csNum = acsSZ;

// Return the iovec reception args
//
   iovNum = (csNum<<1);
   return &ioVec[1];
}
  
/******************************************************************************/
/*                              i o v 4 S e n d                               */
/******************************************************************************/
  
struct iovec *XrdXrootdAioPgrw::iov4Send(int &iovNum, int &iovLen, bool cs2net)
{
   int fLen, lLen;

// Recalculate the iovec values for first and last read and summary values
//
   if (Result > 0)
      {csNum = XrdOucPgrwUtils::csNum(sfsAio.aio_offset, Result, fLen, lLen);
       iovNum = (csNum<<1) + 1;
       iovLen = Result + (csNum * sizeof(uint32_t));
       ioVec[2].iov_len = fLen;
       if (csNum > 1 && lLen != XrdProto::kXR_pgPageSZ)
          {iovReset = csNum<<1;
           ioVec[iovReset].iov_len = lLen;
          }
      } else csNum = 0;

// Convert checksums to net order if so requested
//
   if (cs2net) for (int i = 0; i < csNum; i++) csVec[i] = htonl(csVec[i]);

// Return the iovec
//
   return ioVec;
}
  
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdXrootdAioPgrw::Recycle()
{
// Do some tracing
//
   TRACE(FSAIO, " Recycle " <<sfsAio.aio_nbytes<<'@'
                            <<sfsAio.aio_offset<<" numF="<<numFree);

// Update aio counters
//
   reqP->urProtocol()->aioUpdate(-1);

// Place the object on the free queue if possible
//
   fqMutex.Lock();
   if (numFree >= maxKeep) 
      {fqMutex.UnLock();
       delete this;
      } else {
       next = fqFirst;
       fqFirst = this;
       numFree++;
       fqMutex.UnLock();
      }
}
  
/******************************************************************************/
/*                            S e t u p 2 R e c v                             */
/******************************************************************************/
  
int XrdXrootdAioPgrw::Setup2Recv(off_t offs, int dlen, const char *&eMsg)
{
   XrdOucPgrwUtils::Layout layout;

// Reset any truncated segement in the iov vector
//
   if (iovReset)
      {ioVec[iovReset].iov_len = XrdProto::kXR_pgPageSZ;
       iovReset = 0;
      }

// Get the layout for the iovec
//
   if (!(csNum = XrdOucPgrwUtils::recvLayout(layout, offs, dlen, aioSZ)))
      {eMsg = layout.eWhy;
       return 0;
      }
   eMsg = 0;

// Set the length of the first and last segments. Note that our iovec has
// an extra leading element meant for writing to the network.
//
   ioVec[2].iov_len = layout.fLen;
   if (csNum > 1 && layout.lLen < XrdProto::kXR_pgPageSZ)
      {iovReset = csNum<<1;
       ioVec[iovReset].iov_len = layout.lLen;
      }

// Setup for actual writing of the data
//
   sfsAio.aio_buf    = ioVec[2].iov_base =  buffP->buff + layout.bOffset;
   sfsAio.aio_nbytes = layout.dataLen;
   sfsAio.aio_offset = offs;

// Return how much we can read from the socket
//
   return layout.sockLen;
}
  
/******************************************************************************/
/*                            S e t u p 2 S e n d                             */
/******************************************************************************/
  
int XrdXrootdAioPgrw::Setup2Send(off_t offs, int dlen, const char *&eMsg)
{
   XrdOucPgrwUtils::Layout layout;

// Reset any truncated segement in the iov vector
//
   if (iovReset)
      {ioVec[iovReset].iov_len = XrdProto::kXR_pgPageSZ;
       iovReset = 0;
      }

// Get the layout for the iovec
//
   if (!(csNum = XrdOucPgrwUtils::sendLayout(layout, offs, dlen, aioSZ)))
      {eMsg = layout.eWhy;
       return 0;
      }
   eMsg = 0;

// Set the length of the first and last segments. Note that our iovec has
// an extra leading element meant for writing to the network.
//
   ioVec[2].iov_len = layout.fLen;
   if (csNum > 1 && layout.lLen < XrdProto::kXR_pgPageSZ)
      {iovReset = csNum<<1;
       ioVec[iovReset].iov_len = layout.lLen;
      }

// Setup for actual writing of the data
//
   sfsAio.aio_buf    = ioVec[2].iov_base =  buffP->buff + layout.bOffset;
   sfsAio.aio_nbytes = layout.dataLen;
   sfsAio.aio_offset = offs;

// Return how much we can write to the socket
//
   return layout.dataLen;
}
