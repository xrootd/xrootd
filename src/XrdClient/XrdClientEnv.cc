//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientEnv                                                         // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Singleton used to handle the default parameter values                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClientConst.hh"
#include "XrdClientEnv.hh"

XrdClientEnv *XrdClientEnv::fgInstance = 0;

XrdClientEnv *XrdClientEnv::Instance() {
   // Create unique instance

   if (!fgInstance) {
      fgInstance = new XrdClientEnv;
      if (!fgInstance) {
         abort();
      }
   }
   return fgInstance;
}

//_____________________________________________________________________________
XrdClientEnv::XrdClientEnv() {
   // Constructor
   fOucEnv = new XrdOucEnv();

   int rc;
   pthread_mutexattr_t attr;

   // Initialization of lock mutex
   rc = pthread_mutexattr_init(&attr);
   if (rc == 0) {
      rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (rc == 0)
	 rc = pthread_mutex_init(&fMutex, &attr);
   }

   if (rc) {
      abort();
   }

   PutInt(NAME_CONNECTTIMEOUT, DFLT_CONNECTTIMEOUT);
   PutInt(NAME_CONNECTTIMEOUTWAN, DFLT_CONNECTTIMEOUTWAN);
   PutInt(NAME_REQUESTTIMEOUT, DFLT_REQUESTTIMEOUT);
   PutInt(NAME_MAXREDIRECTCOUNT, DFLT_MAXREDIRECTCOUNT);
   PutInt(NAME_DEBUG, DFLT_DEBUG);
   PutInt(NAME_RECONNECTTIMEOUT, DFLT_RECONNECTTIMEOUT);
   PutInt(NAME_REDIRCNTTIMEOUT, DFLT_REDIRCNTTIMEOUT);
   PutInt(NAME_FIRSTCONNECTMAXCNT, DFLT_FIRSTCONNECTMAXCNT);
   PutInt(NAME_STARTGARBAGECOLLECTORTHREAD, DFLT_STARTGARBAGECOLLECTORTHREAD);
   PutInt(NAME_GOASYNC, DFLT_GOASYNC);
   PutInt(NAME_READCACHESIZE, DFLT_READCACHESIZE);
   PutInt(NAME_READAHEADSIZE, DFLT_READAHEADSIZE);

}

//_____________________________________________________________________________
XrdClientEnv::~XrdClientEnv() {
   // Destructor
   delete fOucEnv;
   delete fgInstance;
   pthread_mutex_destroy(&fMutex);
   fgInstance = 0;
}
