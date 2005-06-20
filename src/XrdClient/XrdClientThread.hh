//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientThread                                                      //
//                                                                      //
// Author: F.Furano (INFN, 2005)                                        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//           $Id$

#ifndef XRC_THREAD_H
#define XRC_THREAD_H

#include "XrdOuc/XrdOucPthread.hh"

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
      fThr = 0;
      ThreadFunc = fn;
   };

   virtual ~XrdClientThread() {
      void *r;

      Cancel();
      Join(&r);
   };

   int Cancel() {
      return XrdOucThread::Cancel(fThr);
   };

   int Run(void *arg = 0) {
      fArg.arg = arg;
      fArg.threadobj = this;
      return XrdOucThread::Run(&fThr, XrdClientThreadDispatcher, (void *)&fArg,
			       XRDOUCTHREAD_HOLD, "");
   };

   int Detach() {
      return XrdOucThread::Detach(fThr);
   };

   int Join(void **ret = 0) {
      return XrdOucThread::Join(fThr, ret);
   };

   // these funcs are to be called only from INSIDE the thread loop
   int     SetCancelOn() {
      return XrdOucThread::SetCancelOn();
   };
   int     SetCancelOff() {
      return XrdOucThread::SetCancelOff();
   };
   int     SetCancelAsynchronous() {
      return XrdOucThread::SetCancelAsynchronous();
   };
   int     SetCancelDeferred() {
      return XrdOucThread::SetCancelDeferred();
   };
   void     CancelPoint() {
      XrdOucThread::CancelPoint();
   };

};



#endif
