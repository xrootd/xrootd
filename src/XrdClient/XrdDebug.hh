//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdDebug                                                             // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Singleton used to handle the debug level and the log output          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_DEBUG_H
#define XRC_DEBUG_H

#include <iostream>
#include <string>
#include <sstream>
#include "XrdConst.hh"

#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucError.hh"

using namespace std;



#define DebugLevel() XrdDebug::Instance()->GetDebugLevel()

#define Info(lvl, where, what) {  XrdDebug::Instance()->outs << where << " " << what; \
      XrdDebug::Instance()->TraceStream((short)lvl, XrdDebug::Instance()->outs); }
                               
#define Error(where, what) {  XrdDebug::Instance()->outs << where << " " << what; \
      XrdDebug::Instance()->TraceStream((short)XrdDebug::kNODEBUG, XrdDebug::Instance()->outs); }


class XrdDebug {
 private:
   short           fDbgLevel;

   XrdOucLogger   *fOucLog;
   XrdOucError    *fOucErr;

   static XrdDebug *fgInstance;

 protected:
   XrdDebug();
   ~XrdDebug();

 public:

   enum {
      kNODEBUG   = 0,
      kUSERDEBUG = 1,
      kHIDEBUG   = 2,
      kDUMPDEBUG = 3
   };

   short           GetDebugLevel() { return fDbgLevel; }
   static XrdDebug *Instance();

   inline void TraceStream(short DbgLvl, ostringstream &s) {
      if (DbgLvl <= GetDebugLevel())
	 fOucErr->Emsg("", s.str().c_str() );

      s.str("");
   }

   ostringstream outs;  // Declare an output string stream.

   inline void TraceString(short DbgLvl, char * s) {
      if (DbgLvl <= GetDebugLevel())
	 fOucErr->Emsg("", s);
   }

};

#endif
