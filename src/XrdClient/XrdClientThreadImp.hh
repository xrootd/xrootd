//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientThreadImp                                                   //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
// Adapted from TThread (root.cern.ch) by R. brun, F. Rademakers        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_THREADIMP_H
#define XRC_THREADIMP_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientThreadImp                                                   //
//                                                                      //
// This class implements threads. A thread is an execution environment  //
// much lighter than a process. A single process can have multiple      //
// threads.                                                             //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class XrdClientThread;

typedef void *(*VoidRtnFunc_t)(void *, XrdClientThread *);

extern "C" void * XrdClientThreadDispatcher(void * arg);

class XrdClientThreadImp {

public:
   XrdClientThreadImp() { }
   virtual ~XrdClientThreadImp() { }

   VoidRtnFunc_t ThreadFunc;

   virtual int  Join(void **ret) = 0;

   virtual int  Cancel() = 0;
   virtual int  Detach() = 0;
   virtual int  Run(void *arg, XrdClientThread *obj) = 0;
   virtual int  SetCancelOff() = 0;
   virtual int  SetCancelOn() = 0;
   virtual int  SetCancelAsynchronous() = 0;
   virtual int  SetCancelDeferred() = 0;
   virtual void CancelPoint() = 0;

   virtual int  Exit(void *ret) = 0;
};


struct XrdClientThreadArgs {
   void *arg;
   XrdClientThread *threadobj;
};



#endif
