//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientThread                                                      //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
// Adapted from TThread (root.cern.ch) by R. brun, F. Rademakers        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_THREAD_H
#define XRC_THREAD_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientThread                                                      //
//                                                                      //
// This class implements threads. A thread is an execution environment  //
// much lighter than a process. A single process can have multiple      //
// threads. The actual work is done via the XrdClientThreadImp class.   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientThreadImp.hh"
#include "XrdClient/XrdClientFactory.hh"


class XrdClientThread {

public:

   

private:
   XrdClientThreadImp   *fThreadImp;      // pointer to thread implementation
   friend void *XrdClientThreadDispatcher(void *);
public:


   

   XrdClientThread(VoidRtnFunc_t fn) {
      fThreadImp = XrdClientGetFactory()->CreateThreadImp();
      fThreadImp->ThreadFunc = fn;
   };

   virtual ~XrdClientThread() {
      delete fThreadImp;
   };

   // these funcs are to be called only from OUTSIDE the thread loop
   int              Cancel() {
      return fThreadImp->Cancel();
   };

   int              Run(void *arg = 0) {
      return fThreadImp->Run(arg, this);
   };

   int              Detach() {
      return fThreadImp->Detach();
   };

   int              Join(void **ret = 0) {
      return fThreadImp->Join(ret);
   };

   // these funcs are to be called only from INSIDE the thread loop
   int     SetCancelOn() {
      return fThreadImp->SetCancelOn();
   };
   int     SetCancelOff() {
      return fThreadImp->SetCancelOff();
   };
   int     SetCancelAsynchronous() {
      return fThreadImp->SetCancelAsynchronous();
   };
   int     SetCancelDeferred() {
      return fThreadImp->SetCancelDeferred();
   };
   void     CancelPoint() {
      fThreadImp->CancelPoint();
   };

};



#endif
