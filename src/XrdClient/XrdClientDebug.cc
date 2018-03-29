/******************************************************************************/
/*                                                                            */
/*                     X r d C l i e n t D e b u g . c c                      */
/*                                                                            */
/* 2003 Produced by Alvise Dorigo & Fabrizio Furano for INFN padova           */
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

#include "XrdClient/XrdClientDebug.hh"

#include "XrdSys/XrdSysPthread.hh"
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

   fOucLog = new XrdSysLogger();
   fOucErr = new XrdSysError(fOucLog, "Xrd");

   fDbgLevel = EnvGetLong(NAME_DEBUG);
}

//_____________________________________________________________________________
XrdClientDebug::~XrdClientDebug() {
   // Destructor
   delete fOucErr;
   delete fOucLog;

   fOucErr = 0;
   fOucLog = 0;

   delete fgInstance;
   fgInstance = 0;
}
