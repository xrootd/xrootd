//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientDebug                                                             // 
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
#include "XrdClientConst.hh"

#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucError.hh"

using namespace std;



#define DebugLevel() XrdClientDebug::Instance()->GetDebugLevel()

#define Info(lvl, where, what) {  XrdClientDebug::Instance()->outs << where << " " << what; \
      XrdClientDebug::Instance()->TraceStream((short)lvl, XrdClientDebug::Instance()->outs); }
                               
#define Error(where, what) {  XrdClientDebug::Instance()->outs << where << " " << what; \
      XrdClientDebug::Instance()->TraceStream((short)XrdClientDebug::kNODEBUG, XrdClientDebug::Instance()->outs); }


class XrdClientDebug {
 private:
   short           fDbgLevel;

   XrdOucLogger   *fOucLog;
   XrdOucError    *fOucErr;

   static XrdClientDebug *fgInstance;

 protected:
   XrdClientDebug();
   ~XrdClientDebug();

 public:

   enum {
      kNODEBUG   = 0,
      kUSERDEBUG = 1,
      kHIDEBUG   = 2,
      kDUMPDEBUG = 3
   };

   short           GetDebugLevel() { return fDbgLevel; }
   static XrdClientDebug *Instance();

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
