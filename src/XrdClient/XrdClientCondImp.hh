//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientCondImp                                                     //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
// Adapted from TConditionImp (root.cern.ch) by R. brun, F. Rademakers  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_CONDIMP_H
#define XRC_CONDIMP_H

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientCondImp                                                     //
//                                                                      //
// This class provides an abstract interface to the package dependent   //
// condition classes.                                                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XProtocol/XPtypes.hh"

class XrdClientCondImp {

public:
   XrdClientCondImp() { }
   virtual ~XrdClientCondImp() { }

   virtual int  Wait(XrdClientMutexImp *m) = 0;
   virtual int  TimedWait(int secs, XrdClientMutexImp *m) = 0;
   virtual int  Signal() = 0;
   virtual int  Broadcast() = 0;
};

#endif
