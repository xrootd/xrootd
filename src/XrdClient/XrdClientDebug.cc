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

#include "XrdClientDebug.hh"

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
   fOucLog = new XrdOucLogger();
   fOucErr = new XrdOucError(fOucLog, "Xrd");

   fDbgLevel = DFLT_DEBUG;
}

//_____________________________________________________________________________
XrdClientDebug::~XrdClientDebug() {
   // Destructor
   SafeDelete(fOucErr);
   SafeDelete(fOucLog);

   fOucErr = 0;
   fOucLog = 0;

   SafeDelete(fgInstance);
}
