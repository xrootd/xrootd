/******************************************************************************/
/*                                                                            */
/*                   X r d C p M t h r Q u e u e . c c                        */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// A thread safe queue to be used for multithreaded producers-consumers //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdCpMthrQueue.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdClient/XrdClientDebug.hh"

XrdCpMthrQueue::XrdCpMthrQueue(): fWrWait(0), fReadSem(0), fWriteSem(0) {
   // Constructor

   fMsgQue.Clear();
   fTotSize = 0;
}

int XrdCpMthrQueue::PutBuffer(void *buf, long long offs, int len) {
   XrdCpMessage *m;

   fMutex.Lock();
   if (len && fTotSize > CPMTQ_BUFFSIZE)
      {fWrWait++;
       fMutex.UnLock();
       fWriteSem.Wait();
      }

   m = new XrdCpMessage;
   m->offs = offs;
   m->buf = buf;
   m->len = len;

   // Put message in the list
   //
      fMsgQue.Push_back(m);
      fTotSize += len;
    
   fMutex.UnLock();
   fReadSem.Post(); 

   return 0;
}

int XrdCpMthrQueue::GetBuffer(void **buf, long long &offs, int &len) {
   XrdCpMessage *res;

   res = 0;
 
   // If there is no data for one hour, then give up with an error
   if (!fReadSem.Wait(3600))
      {fMutex.Lock();
      // If there are messages to dequeue, we pick the oldest one
       if (fMsgQue.GetSize() > 0)
          {res = fMsgQue.Pop_front();
           if (res) {fTotSize -= res->len;
                     if (fWrWait) {fWrWait--; fWriteSem.Post();}
                    }
          }
       fMutex.UnLock();
      }


   if (res) {
      *buf = res->buf;
      len = res->len;
      offs = res->offs;
      delete res;
   }

   return (res != 0);
}


void XrdCpMthrQueue::Clear() {
   void *buf;
   int len;
   long long offs;

   while (fMsgQue.GetSize() && GetBuffer(&buf, offs, len)) {
      free(buf);
   }

   fTotSize = 0;

}

   
