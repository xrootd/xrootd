/***********************************************************/
/*                T X D e b u g . c c                      */
/*                        2003                             */
/*             Produced by Alvise Dorigo                   */
/*         & Fabrizio Furano for INFN padova               */
/***********************************************************/
//
//   $Id$
//
// Author: Alvise Dorigo, Fabrizio Furano

#include "XrdClient/XrdClientDebug.hh"

#include "XrdClient/XrdClientMutexLocker.hh"
XrdClientDebug *XrdClientDebug::fgInstance = 0;

//_____________________________________________________________________________
XrdClientDebug* XrdClientDebug::Instance() {
   // Create unique instance

   if (!fgInstance) {
      fgInstance = new XrdClientDebug;
      if (!fgInstance) {
         abort();
      }
   }
   return fgInstance;
}

//_____________________________________________________________________________
XrdClientDebug::XrdClientDebug() {
   // Constructor
   int rc;
   pthread_mutexattr_t attr;

   fOucLog = new XrdOucLogger();
   fOucErr = new XrdOucError(fOucLog, "Xrd");

   // Initialization of lock mutex
   rc = pthread_mutexattr_init(&attr);
   if (rc == 0) {
      rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (rc == 0)
	 rc = pthread_mutex_init(&fMutex, &attr);
   }

   pthread_mutexattr_destroy(&attr);

   if (rc) {
      Error("InputBuffer", 
            "Can't create mutex: out of system resources.");
      abort();
   }

   fDbgLevel = EnvGetLong(NAME_DEBUG);
}

//_____________________________________________________________________________
XrdClientDebug::~XrdClientDebug() {
   // Destructor
   delete fOucErr;
   delete fOucLog;

   fOucErr = 0;
   fOucLog = 0;
   pthread_mutex_destroy(&fMutex);
   delete fgInstance;
}
