//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSemaphoreImp                                                //
//                                                                      //
// Author: F. Furano (INFN, 2005)                                       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_SEMIMP_H
#define XRC_SEMIMP_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSemaphoreImp                                                //
//                                                                      //
// This class provides an abstract interface to the package dependent   //
// semaphore classes.                                                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class XrdClientSemaphoreImp {

public:
   XrdClientSemaphoreImp() { }
   virtual ~XrdClientSemaphoreImp() { }

   virtual int  Wait() = 0;
   virtual int  TimedWait(int secs) = 0;
   virtual int  Signal() = 0;
};

#endif
