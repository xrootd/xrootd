//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdSemaphore                                                //
//                                                                      //
// Author: Gerardo Ganis & Fabrizio Furano (Cern, INFN Padova, 2005)    //
//                                                                      //
// A concrete implementation for a semaphore                            //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef XRC_XRDSEM_H
#define XRC_XRDSEM_H

#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientSemaphoreImp.hh"

#include <pthread.h>
#include <sys/time.h>

class XrdClientXrdSemaphore : public XrdClientSemaphoreImp {

private:
   pthread_cond_t fCnd;
   pthread_mutex_t fMtx;
   int fCnt;

public:
   XrdClientXrdSemaphore(int value) {
      int rc;
      
      rc = pthread_cond_init(&fCnd, 0);

      if (rc) {
	 Error("XrdClientXrdSemaphore", 
	       "Can't create condvar: out of system resources.");
	 abort();
      }

      rc = pthread_mutex_init(&fMtx, 0);

      if (rc) {
	 Error("XrdClientXrdSemaphore", 
	       "Can't create mutex: out of system resources.");
	 abort();
      }
   };


   ~XrdClientXrdSemaphore() {
      pthread_cond_destroy(&fCnd);
      pthread_mutex_destroy(&fMtx);
   };

   int Wait() {

      pthread_mutex_lock(&fMtx);

      if (fCnt > 0) fCnt--;
      else
	 pthread_cond_wait(&fCnd, &fMtx);

      pthread_mutex_unlock(&fMtx);

      return 0;
   };

   // Return nonzero if timeout
   int TimedWait(int secs) {

      int rc = 0;

      pthread_mutex_lock(&fMtx);

      if (fCnt > 0) fCnt--;
      else {
	 struct timespec t;
	 struct timeval now;
	 
	 gettimeofday(&now, 0);

	 t.tv_sec = now.tv_sec + secs;
	 t.tv_nsec = now.tv_usec * 1000;

	 rc = pthread_cond_timedwait(&fCnd, &fMtx, &t);

      }

      pthread_mutex_unlock(&fMtx);

      return rc;


   };

   int  Signal() {

      pthread_mutex_lock(&fMtx);

      fCnt++;
      pthread_cond_signal(&fCnd);

      pthread_mutex_unlock(&fMtx);

      return 0;

   };


};

#endif
