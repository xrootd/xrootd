//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdMutexLocker                                                       // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A simple class useful to associate a mutex lock/unlock               //
//  to a syntactical block enclosed in {}                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_MUTEXLOCKER_H
#define XRC_MUTEXLOCKER_H

#include <pthread.h>

class XrdMutexLocker {

private:
   pthread_mutex_t fMtx;

public:
 
   inline XrdMutexLocker(pthread_mutex_t mutex) { 
      fMtx = mutex;
      pthread_mutex_lock(&fMtx);
   }

   inline ~XrdMutexLocker() { pthread_mutex_unlock(&fMtx); }
};




#endif
