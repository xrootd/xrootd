//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMutex                                                       //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
// Adapted from TMutex (root.cern.ch) by R. brun, F. Rademakers         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_MUTEX_H
#define XRC_MUTEX_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMutex                                                       //
//                                                                      //
// This class implements mutex locks. A mutex is a mutual exclusive     //
// lock. The actual work is done via the XrdClientMutexImp class        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientMutexImp.hh"
#include "XrdClient/XrdClientFactory.hh"

class XrdClientMutex {

   

private:
   XrdClientMutexImp  *fMutexImp;   // pointer to mutex implementation
   long               fId;          // id of thread which locked mutex
   long               fRef;         // reference count in case of recursive
                                    // locking by same thread
public:
   XrdClientMutex(bool recursive = true) {
      fMutexImp = XrdClientGetFactory()->CreateMutexImp();
   };

   ~XrdClientMutex() { delete fMutexImp; };

   int  Lock() {
      return fMutexImp->Lock();
   };

   int  TryLock() {
      return fMutexImp->TryLock();
   };

   int  UnLock() {
      return fMutexImp->UnLock();
   };

   int  CleanUp() {
      return 0;
   };


   friend class XrdClientCond;


};

#endif
