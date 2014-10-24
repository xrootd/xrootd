/******************************************************************************/
/*                                                                            */
/*                    X r d O u c P i n L o a d e r . c c                     */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucVerName.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOucPinLoader::XrdOucPinLoader(XrdSysError    *errP,
                                 XrdVersionInfo *vInfo,
                                 const char     *drctv,
                                 const char     *plib)
{

// Save some symbols and do common initialization
//
   eDest = errP;
   viP   = vInfo;
   errBP = 0;
   errBL = 0;
   Init(drctv, plib);
}

/******************************************************************************/

XrdOucPinLoader::XrdOucPinLoader(char           *eBuff,
                                 int             eBlen,
                                 XrdVersionInfo *vInfo,
                                 const char     *drctv,
                                 const char     *plib)
{

// Save some symbols and do common initialization
//
   eDest = 0;
   viP   = vInfo;
   errBP = (eBlen > 0 ? eBuff : 0);
   errBL = (eBlen > 0 ? eBlen : 0);
   frBuff= false;
   if (errBP) *errBP = 0;
   Init(drctv, plib);
}

/******************************************************************************/

XrdOucPinLoader::XrdOucPinLoader(XrdVersionInfo *vInfo,
                                 const char     *drctv,
                                 const char     *plib)
{

// Save some symbols and do common initialization
//
   eDest = 0;
   viP   = vInfo;
   errBP = 0;
   errBL = 0;
   frBuff= true;
   Init(drctv, plib);
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOucPinLoader::~XrdOucPinLoader()
{

// Releae storage
//
   if (theLib) free(theLib);
   if (altLib) free(altLib);

// Persist the image if we have one
//
   if (piP) {piP->Persist(); delete piP;}
   if (errBP && frBuff) free(errBP);
}

/******************************************************************************/
/* Private:                       I n f o r m                                 */
/******************************************************************************/

void XrdOucPinLoader::Inform(const char *txt1, const char *txt2,
                             const char *txt3, const char *txt4,
                             const char *txt5)
{
   static const int ebsz = 1024;

// Allocate a message buffer if we need to (failure is OK)
//
   if (!errBP && frBuff)
      {errBP = (char *)malloc(ebsz);
       errBL = ebsz;
      }

// If we have a messaging object, use that
//
   if (eDest) eDest->Say("Config ", txt1, txt2, txt3, txt4, txt5);
      else   {const char *eTxt[] = {txt1, txt2, txt3, txt4, txt5, 0};
              char *bP;
              int n, i, bL;
              if ((bP = errBP))
                 {i = 0; bL = errBL;
                  while(bL > 1 && eTxt[i])
                       {n = snprintf(bP, bL, "%s", eTxt[i]);
                        bP += n; bL -= n; i++;
                       }
                 }
             }
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/

void XrdOucPinLoader::Init(const char *drctv, const char *plib)
{
   char libBuf[2048];
   bool noFallBack;

// We have no plugin
//
   piP    = 0;
   dName  = drctv;
   global = false;

// Perform versioning
//
   if (!XrdOucVerName::Version(XRDPLUGIN_SOVERSION, plib, noFallBack,
                               libBuf, sizeof(libBuf)))
      {theLib = 0;
       altLib = strdup(plib);
       tryLib = "?";
      } else {
       tryLib = theLib = strdup(libBuf);
       altLib = (noFallBack ? 0 : strdup(plib));
      }
}
  
/******************************************************************************/
/*                               R e s o l v e                                */
/******************************************************************************/

void *XrdOucPinLoader::Resolve(const char *symP, int mcnt)
{
   void *symAddr;

// Check if we couldn't create the path
//
   if (!theLib)
      {Inform("Unable to load ",dName," plugin ",altLib,"; invalid path.");
       return 0;
      }

// If we already have a plugin object, then use it
//
   if (piP) return piP->getPlugin(symP);

// Now that we have the name of the library we can declare the plugin object
// inline so that it gets deleted when we return (icky but simple)
//
   if (eDest) piP = new XrdSysPlugin(eDest,        theLib, dName, viP, mcnt);
      else    piP = new XrdSysPlugin(errBP, errBL, theLib, dName, viP, mcnt);

// Resolve the plugin symbol. This may fail for a number of reasons.
//
   if ((symAddr = piP->getPlugin(symP, 0, global))) return symAddr;

// We failed, so delete this plugin and see if we can revert to the unversioned
// name or declare a failure.
//
   delete piP; piP = 0;
   if (!altLib) return 0;
   tryLib = altLib;
   if (eDest) eDest->Say("Config ", "Falling back to using ", altLib);
   if (eDest) piP = new XrdSysPlugin(eDest,        altLib, dName, viP, mcnt);
      else    piP = new XrdSysPlugin(errBP, errBL, altLib, dName, viP, mcnt);
   if ((symAddr = piP->getPlugin(symP, 0, global))) return symAddr;

// We failed
//
   delete piP; piP = 0;
   Inform("Unable to load ", dName, " plugin ", altLib);
   return 0;
}

/******************************************************************************/
/*                                U n L o a d                                 */
/******************************************************************************/
  
void XrdOucPinLoader::Unload(bool dodel)
{

// Simply delete any plugin object
//
   if (piP) {delete piP; piP = 0;}
   if (dodel) delete this;
}
