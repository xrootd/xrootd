#ifndef XRC_DEBUG_H
#define XRC_DEBUG_H
/******************************************************************************/
/*                                                                            */
/*                   X r d C l i e n t D e b u g . h h                        */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/* Adapted from TXNetFile (root.cern.ch) originally done by                   */
/*  Alvise Dorigo, Fabrizio Furano                                            */
/*          INFN Padova, 2003                                                 */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Singleton used to handle the debug level and the log output          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <sstream>
#include "XrdClient/XrdClientConst.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysError.hh"

using namespace std;

#define DebugLevel() XrdClientDebug::Instance()->GetDebugLevel()
#define DebugSetLevel(l) XrdClientDebug::Instance()->SetLevel(l)

#define Info(lvl, where, what) { \
XrdClientDebug::Instance()->Lock();\
if (XrdClientDebug::Instance()->GetDebugLevel() >= lvl) {\
ostringstream outs;\
outs << where << ": " << what; \
XrdClientDebug::Instance()->TraceStream((short)lvl, outs);\
}\
XrdClientDebug::Instance()->Unlock();\
}
                               
#define Error(where, what) { \
ostringstream outs;\
outs << where << ": " << what; \
XrdClientDebug::Instance()->TraceStream((short)XrdClientDebug::kNODEBUG, outs);\
}


class XrdClientDebug {
 private:
   short                       fDbgLevel;

   XrdSysLogger                *fOucLog;
   XrdSysError                 *fOucErr;

   static XrdClientDebug       *fgInstance;

   XrdSysRecMutex                 fMutex;

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

   short           GetDebugLevel() {
       XrdSysMutexHelper m(fMutex);
       return fDbgLevel;
       }

   static XrdClientDebug *Instance();

   inline void SetLevel(int l) {
      XrdSysMutexHelper m(fMutex);
      fDbgLevel = l;
   }

   inline void TraceStream(short DbgLvl, ostringstream &s) {
      XrdSysMutexHelper m(fMutex);

      if (DbgLvl <= GetDebugLevel())
	 fOucErr->Emsg("", s.str().c_str() );

      s.str("");
   }

   //   ostringstream outs;  // Declare an output string stream.

   inline void TraceString(short DbgLvl, char * s) {
      XrdSysMutexHelper m(fMutex);
      if (DbgLvl <= GetDebugLevel())
	 fOucErr->Emsg("", s);
   }

   inline void Lock() { fMutex.Lock(); }
   inline void Unlock() { fMutex.UnLock(); }

};
#endif
