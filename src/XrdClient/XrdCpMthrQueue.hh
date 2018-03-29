#ifndef XRDCPMTHRQ__HH
#define XRDCPMTHRQ__HH
/******************************************************************************/
/*                                                                            */
/*                   X r d C p M t h r Q u e u e . h h                        */
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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdClient/XrdClientVector.hh"
#include "XrdSys/XrdSysSemWait.hh"
#include "XrdSys/XrdSysHeaders.hh"

using namespace std;

struct XrdCpMessage {
   void *buf;
   long long offs;
   int len;
};

// The max allowed size for this queue
// If this value is reached, then the writer has to wait...
#define CPMTQ_BUFFSIZE            50000000

class XrdCpMthrQueue {
 private:
   long                           fTotSize;
   XrdClientVector<XrdCpMessage*>            fMsgQue;      // queue for incoming messages
   int                                       fMsgIter;     // an iterator on it
   int                                       fWrWait;      // Write waiters

   XrdSysRecMutex                        fMutex;       // mutex to protect data structures

   XrdSysSemWait                      fReadSem;     // variable to make the reader wait
                                                    // until some data is available
   XrdSysSemaphore                    fWriteSem;    // variable to make the writer wait
                                                    // if the queue is full
 public:

   XrdCpMthrQueue();
   ~XrdCpMthrQueue() {}

   int PutBuffer(void *buf, long long offs, int len);
   int GetBuffer(void **buf, long long &offs, int &len);
   int GetLength() { return fMsgQue.GetSize(); }
   void Clear();
};
#endif
