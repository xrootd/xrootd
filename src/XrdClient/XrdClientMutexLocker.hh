//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMutexLocker                                                 //
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

//       $Id$

#ifndef XRC_MUTEXLOCKER_H
#define XRC_MUTEXLOCKER_H

#include "XrdClientMutex.hh"

class XrdClientMutexLocker {

private:
   XrdClientMutex *fMtx;

public:
 
   inline XrdClientMutexLocker(XrdClientMutex &mutex) { 
      fMtx = &mutex;
      fMtx->Lock();
   };

   inline ~XrdClientMutexLocker() { 
      fMtx->UnLock();
      fMtx = 0;
   };

};




#endif
