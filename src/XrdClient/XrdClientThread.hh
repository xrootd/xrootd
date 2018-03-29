#ifndef XRC_THREAD_H
#define XRC_THREAD_H
/******************************************************************************/
/*                                                                            */
/*                  X r d C l i e n t T h r e a d . h h                       */
/*                                                                            */
/* Author: F.Furano (INFN, 2005)                                              */
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
// An user friendly thread wrapper                                      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdSys/XrdSysPthread.hh"

void * XrdClientThreadDispatcher(void * arg);

class XrdClientThread {
private:
   pthread_t fThr;

   typedef void *(*VoidRtnFunc_t)(void *, XrdClientThread *);
   VoidRtnFunc_t ThreadFunc;
   friend void *XrdClientThreadDispatcher(void *);

 public:
   struct XrdClientThreadArgs {
      void *arg;
      XrdClientThread *threadobj;
   } fArg;
   
   
   XrdClientThread(VoidRtnFunc_t fn) {
#ifndef WIN32
      fThr = 0;
#endif
      ThreadFunc = fn;
   };

   virtual ~XrdClientThread() {

//      Cancel();
   };

   int Cancel() {
      return XrdSysThread::Cancel(fThr);
   };

   int Run(void *arg = 0) {
      fArg.arg = arg;
      fArg.threadobj = this;
      return XrdSysThread::Run(&fThr, XrdClientThreadDispatcher, (void *)&fArg,
			       XRDSYSTHREAD_HOLD, "");
   };

   int Detach() {
      return XrdSysThread::Detach(fThr);
   };

   int Join(void **ret = 0) {
      return XrdSysThread::Join(fThr, ret);
   };

   // these funcs are to be called only from INSIDE the thread loop
   int     SetCancelOn() {
      return XrdSysThread::SetCancelOn();
   };
   int     SetCancelOff() {
      return XrdSysThread::SetCancelOff();
   };
   int     SetCancelAsynchronous() {
      return XrdSysThread::SetCancelAsynchronous();
   };
   int     SetCancelDeferred() {
      return XrdSysThread::SetCancelDeferred();
   };
   void     CancelPoint() {
      XrdSysThread::CancelPoint();
   };

   int MaskSignal(int snum = 0, bool block = 1);
};
#endif
