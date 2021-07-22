/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d A i o F o b . h h                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdXrootd/XrdXrootdAioFob.hh"
#include "XrdXrootd/XrdXrootdAioTask.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

#define TRACELINK aioP->dataLink

/******************************************************************************/
/*                        G l o b a l   S t a t i c s                         */
/******************************************************************************/

namespace
{
const char *TraceID = "AioFob";
}

extern XrdSysTrace  XrdXrootdTrace;
  
namespace XrdXrootd
{
extern XrdSysError   eLog;
extern XrdScheduler *Sched;
}
using namespace XrdXrootd;

/******************************************************************************/
/*                                N o t i f y                                 */
/******************************************************************************/
  
void XrdXrootdAioFob::Notify(XrdXrootdAioTask *aioP, const char *what)
{
   TRACEI(FSAIO, what<<" aio "
          <<(aioP->aioState & XrdXrootdAioTask::aioPage ? "pgread " : "read ")
          <<aioP->dataLen<<'@'<<aioP->dataOffset<< " for "
          <<aioP->dataFile->FileKey);
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdXrootdAioFob::Reset()
{
   XrdXrootdAioTask *aioP;

// Recycle all outstanding aio tasks
//
   fobMutex.Lock();

   for (int i = 0; i < maxQ; i++)
       {while((aioP = aioQ[i].first))
             {aioQ[i].first = aioP->nextTask;
              if (TRACING(TRACE_FSAIO)) Notify(aioP, "Discarding");
              aioP->Recycle(true);
             }
        aioQ[i].last = 0;
        Running[i] = false;
       }

   fobMutex.UnLock();
}

/******************************************************************************/
  
void XrdXrootdAioFob::Reset(XrdXrootdProtocol *protP)
{
   XrdXrootdAioTask *aioP;
   int pathID = protP->getPathID();

// Recycle all outstanding aio tasks
//
   fobMutex.Lock();

   while((aioP = aioQ[pathID].first))
        {aioQ[pathID].first = aioP->nextTask;
         if (TRACING(TRACE_FSAIO)) Notify(aioP, "Discarding");
         aioP->Recycle(true);
        }
   aioQ[pathID].last = 0;
   Running[pathID] = false;

   fobMutex.UnLock();
}

/******************************************************************************/
/*                              S c h e d u l e                               */
/******************************************************************************/
  
void XrdXrootdAioFob::Schedule(XrdXrootdAioTask *aioP)
{
   int pathID = aioP->Protocol->getPathID();

// Run or queue this task
//
   fobMutex.Lock();

   if (Running[pathID])
      {if (aioQ[pathID].last) aioQ[pathID].last->nextTask = aioP;
          else aioQ[pathID].first = aioP;
       aioQ[pathID].last = aioP;
       aioP->nextTask = 0;
       if (maxQ <= pathID) maxQ = pathID+1;
       if (TRACING(TRACE_FSAIO)) Notify(aioP, "Queuing");
      } else {
       Sched->Schedule(aioP);
       Running[pathID] = true;
       if (TRACING(TRACE_FSAIO)) Notify(aioP, "Running");
      }

   fobMutex.UnLock();
}

/******************************************************************************/
  
void XrdXrootdAioFob::Schedule(XrdXrootdProtocol *protP)
{
   int pathID = protP->getPathID();

// Schedule the next task.
//
   fobMutex.Lock();

   if (aioQ[pathID].first)
      {XrdXrootdAioTask *aioP = aioQ[pathID].first;
       if (!(aioQ[pathID].first = aioP->nextTask)) aioQ[pathID].last = 0;
       aioP->nextTask = 0;
       Sched->Schedule(aioP);
       Running[pathID] = true;
       if (TRACING(TRACE_FSAIO)) Notify(aioP, "Running");
      } else  Running[pathID] = false;

   fobMutex.UnLock();
}
