//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientCond                                                        //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
// Adapted from TCondition (root.cern.ch) by R. brun, F. Rademakers     //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_COND_H
#define XRC_COND_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientCond                                                        //
//                                                                      //
// This class implements a condition variable. Use a condition variable //
// to signal threads. The actual work is done via the XrdClientCondImp  //
// class                                                                //
//                                                                      //
// NOTE: this class associates a cond with its mutex, but, unlike       //
//    some similar implementations, it does not lock/unlock the mutex   //
//    in the wait primitives. This allows better flexibility and        //
//    simpler prevention of race conditions.                            //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientCondImp.hh"
#include "XrdClient/XrdClientMutex.hh"
#include "XrdClient/XrdClientMutexLocker.hh"

class XrdClientCond {

 private:
   XrdClientCondImp  *fCondImp;       // pointer to condition variable implementation
   XrdClientMutex    *fMutex;         // mutex used around Wait() and TimedWait()
   bool              fPrivateMutex;  // is fMutex our private mutex
   
 public:
   XrdClientCond(XrdClientMutex *m = 0) {
      fPrivateMutex = false;
      fMutex = m;

      if (!fMutex) {
	 fMutex = new XrdClientMutex();
	 fPrivateMutex = true;
      }

      fCondImp = XrdClientGetFactory()->CreateCondImp();
   };

   ~XrdClientCond() {
      delete fCondImp;
      fCondImp = 0;

      if (fPrivateMutex)
	 delete fMutex;

      fMutex = 0;
   };

   int   Wait() {

      if (fCondImp) {
	 XrdClientMutexLocker m(*fMutex);

	 return fCondImp->Wait(fMutex->fMutexImp);

      }

      return -1;
   };

   int   TimedWait(int secs) {

      if (fCondImp) {
	 XrdClientMutexLocker m(*fMutex);

	 return fCondImp->TimedWait(secs, fMutex->fMutexImp);

      }


      return -1;
   };

   int   Signal() {
      if (fCondImp) {
	 XrdClientMutexLocker m(*fMutex);
	 return fCondImp->Signal(); 
      }

      return -1;
   };
   int   Broadcast() {
      if (fCondImp) {
	 XrdClientMutexLocker m(*fMutex);
	 return fCondImp->Broadcast();
      }

      return -1;
   };

};

#endif
