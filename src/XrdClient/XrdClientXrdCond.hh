//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdCond                                                     //
//                                                                      //
// Author: Gerardo Ganis & Fabrizio Furano (Cern, INFN Padova, 2005)    //
//                                                                      //
// A concrete implementation for a condition variable                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef XRC_XRDCOND_H
#define XRC_XRDCOND_H

#include "XrdClient/XrdClientMutex.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientCondImp.hh"

#include <pthread.h>
#include <sys/time.h>

class XrdClientXrdCond : public XrdClientCondImp {

private:
   pthread_cond_t fCnd;

public:
   XrdClientXrdCond() {
      int rc;
      
      rc = pthread_cond_init(&fCnd, 0);

      if (rc) {
	 Error("XrdClientXrdCond", 
	       "Can't create condvar: out of system resources.");
	 abort();
      }

   };


   ~XrdClientXrdCond() {
      pthread_cond_destroy(&fCnd);
   };

   int Wait(XrdClientMutexImp *m) {

      return pthread_cond_wait(&fCnd, &( ((XrdClientXrdMutex *)m)->fMutex ) );

   };

   int TimedWait(int secs, XrdClientMutexImp *m) {

      struct timespec t;
      struct timeval now;

      gettimeofday(&now, 0);

      t.tv_sec = now.tv_sec + secs;
      t.tv_nsec = now.tv_usec * 1000;

      return pthread_cond_timedwait(&fCnd, &( ((XrdClientXrdMutex *)m)->fMutex ), &t);

   };

   int  Signal() {

      return pthread_cond_signal(&fCnd);

   };

   int  Broadcast() {

      return pthread_cond_broadcast(&fCnd);

   };


};

#endif
