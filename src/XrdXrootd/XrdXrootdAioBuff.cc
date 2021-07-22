/******************************************************************************/
/*                                                                            */
/*                         X r d A i o B u f f . c c                          */
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

#include "Xrd/XrdBuffer.hh"
#include "XrdXrootd/XrdXrootdAioBuff.hh"
#include "XrdXrootd/XrdXrootdAioTask.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

#define TRACELINK reqP
#define ID ID()
 
/******************************************************************************/
/*                        G l o b a l   S t a t i c s                         */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

const char *XrdXrootdAioBuff::TraceID = "AioBuff";

namespace XrdXrootd
{
extern XrdBuffManager       *BPool;
}

using namespace XrdXrootd;
  
/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

namespace
{
XrdSysMutex       fqMutex;
XrdXrootdAioBuff *fqFirst = 0;
int               numFree = 0;

static const int  maxKeep =128; // Number of objects to keep sans buffer
}
  
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdAioBuff *XrdXrootdAioBuff::Alloc(XrdXrootdAioTask* arp)
{
   XrdXrootdAioBuff *aiobuff;
   XrdBuffer *bP;

// Obtain a buffer as we never hold on to them (unlike pgaio)
//
   if (!(bP = BPool->Obtain(XrdXrootdProtocol::as_segsize))) return 0;

// Obtain a preallocated aio object
//
   fqMutex.Lock();
   if ((aiobuff = fqFirst))
      {fqFirst = aiobuff->next;
       numFree--;
      }
   fqMutex.UnLock();

// If we have no object, create a new one.
//
   if (!aiobuff) aiobuff = new XrdXrootdAioBuff(arp, bP);
      else {aiobuff->reqP   = arp;
            aiobuff->buffP  = bP;
           }
    aiobuff->cksVec = 0;
    aiobuff->sfsAio.aio_buf = bP->buff;
    aiobuff->sfsAio.aio_nbytes = bP->bsize;

// Update aio counters
//
   arp->urProtocol()->aioUpdate(1);

// All done
//
   return aiobuff;
}

/******************************************************************************/
/*                              d o n e R e a d                               */
/******************************************************************************/
  
void XrdXrootdAioBuff::doneRead()
{
// Tell the request this data is available to be sent to the client
//
   reqP->Completed(this);
}

/******************************************************************************/
/*                             d o n e W r i t e                              */
/******************************************************************************/
  
void XrdXrootdAioBuff::doneWrite()
{
// Tell the request this data is has been dealth with
//
   reqP->Completed(this);
}
  
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdXrootdAioBuff::Recycle()
{

// Do some tracing
//
   TRACEI(FSAIO, "Recycle " <<sfsAio.aio_nbytes<<'@'
                            <<sfsAio.aio_offset<<" numF="<<numFree);

// Update aio counters
//
   reqP->urProtocol()->aioUpdate(-1);

// Recycle the buffer as we don't want to hold on to it
//
   if (buffP) {BPool->Release(buffP); buffP = 0;}

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
