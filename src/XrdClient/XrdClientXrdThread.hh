//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdThread                                                   //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// Xrd concrete implementation of a thread class                        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef XRC_XRDTHREAD_H
#define XRC_XRDTHREAD_H

#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientThreadImp.hh"
#include <pthread.h>

class XrdClientXrdThread : public XrdClientThreadImp {

private:
   pthread_t fThr;
   
public:
   VoidRtnFunc_t ThreadFunc;

   XrdClientXrdThread() {
      fThr = 0;
   };

   ~XrdClientXrdThread() {
   };

   int  Join(void **ret) {
      return pthread_join(fThr, ret);
   };

   int  Run(void *arg, XrdClientThread *obj) {
      XrdClientThreadArgs fArg;

      fArg.arg = arg;
      fArg.threadobj = obj;

      if (!fThr)
	 return pthread_create(&fThr, NULL, XrdClientThreadDispatcher, &fArg);

      return -1;
   };

   int  Cancel() {
      return pthread_cancel(fThr);
   };

   int  SetCancelOff() {
      return pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
   };

   int  SetCancelOn() {
      return pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
   };

   int  SetCancelAsynchronous() {
      return pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
   };

   int  SetCancelDeferred() {
      return pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
   };

   void  CancelPoint() {
      pthread_testcancel();
   };

   int Detach() {
      return pthread_detach(fThr);
   };

   int  Exit(void *ret) {
      pthread_exit(ret);
      return 0;
   };

};

#endif
