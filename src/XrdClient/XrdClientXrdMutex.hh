//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdSock                                                     //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Client Socket with timeout features                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef XRC_XRDMUTEX_H
#define XRC_XRDMUTEX_H

#include "XrdClient/XrdClientMutexImp.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientDebug.hh"
#include <pthread.h>

class XrdClientXrdMutex : public XrdClientMutexImp {

private:
   pthread_mutex_t fMutex;

public:
   XrdClientXrdMutex() {
      int rc;
      pthread_mutexattr_t attr;

      // Initialization of lock mutex
      rc = pthread_mutexattr_init(&attr);

      if (!rc) {
	 rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	 if (!rc)
	    rc = pthread_mutex_init(&fMutex, &attr);
      }

      pthread_mutexattr_destroy(&attr);

      if (rc) {
	 Error("InputBuffer", 
	       "Can't create mutex: out of system resources.");
	 abort();
      }

   };


   ~XrdClientXrdMutex() {
      pthread_mutex_destroy(&fMutex);
   };

   int  Lock() {
      return pthread_mutex_lock(&fMutex);
   };

   int  TryLock() {
      return pthread_mutex_trylock(&fMutex);
   };

   int  UnLock() {
      return pthread_mutex_unlock(&fMutex);
   };


   friend class XrdClientXrdCond;
};

#endif
