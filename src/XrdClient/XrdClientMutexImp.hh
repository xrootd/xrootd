//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMutexImp                                                    //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
// Adapted from TMutex (root.cern.ch) by R. brun, F. Rademakers         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_MUTEXIMP_H
#define XRC_MUTEXIMP_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMutexImp                                                    //
//                                                                      //
// This class provides an abstract interface to package dependent mutex //
// classes.                                                             //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class XrdClientMutexImp {

public:
   XrdClientMutexImp() { }
   virtual ~XrdClientMutexImp() { }

   virtual int  Lock() = 0;
   virtual int  TryLock() = 0;
   virtual int  UnLock() = 0;
};

#endif
