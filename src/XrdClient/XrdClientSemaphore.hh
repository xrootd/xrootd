//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSemaphore                                                   //
//                                                                      //
// Author: F.Furano (INFN, 2005)                                        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_SEM_H
#define XRC_SEM_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSemaphore                                                   //
//                                                                      //
// This class implements a semaphore, with timed primitives             //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientSemaphoreImp.hh"


class XrdClientSemaphore {

 private:
   XrdClientSemaphoreImp
                     *fSemImp;        // pointer to sem implementation
 public:
   XrdClientSemaphore(int InitialCount = 0) {
      fSemImp = XrdClientGetFactory()->CreateSemaphoreImp(InitialCount);
   };

   ~XrdClientSemaphore() {
      delete fSemImp;
      fSemImp = 0;
   };

   int   Wait() {

      if (fSemImp) {

	 return fSemImp->Wait();

      }

      return -1;
   };

   // Return nonzero if timeout
   int   TimedWait(int SecsTimeout) {

      if (fSemImp) {

	 return fSemImp->TimedWait(SecsTimeout);

      }

      return -1;
   };


   int   Signal() {
      if (fSemImp) {

	 return fSemImp->Signal(); 
      }

      return -1;
   };

};

#endif
