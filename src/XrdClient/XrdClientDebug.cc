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

#include "XrdDebug.hh"

XrdDebug *XrdDebug::fgInstance = 0;

//_____________________________________________________________________________
XrdDebug* XrdDebug::Instance() {
   // Create unique instance

   if (!fgInstance) {
      fgInstance = new XrdDebug;
      if (!fgInstance) {
         abort();
      }
   }
   return fgInstance;
}

//_____________________________________________________________________________
XrdDebug::XrdDebug() {
   // Constructor
   fOucLog = new XrdOucLogger();
   fOucErr = new XrdOucError(fOucLog, "Xrd");

   fDbgLevel = DFLT_DEBUG;
}

//_____________________________________________________________________________
XrdDebug::~XrdDebug() {
   // Destructor
   SafeDelete(fOucErr);
   SafeDelete(fOucLog);

   fOucErr = 0;
   fOucLog = 0;

   SafeDelete(fgInstance);
}
