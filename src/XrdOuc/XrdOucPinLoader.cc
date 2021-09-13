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

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>

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
   static const int ebsz = 2048;

// Allocate a message buffer if we need to (failure is OK)
//
   errBP = (char *)malloc(ebsz);
   errBL = ebsz;
   frBuff= true;

// Save some symbols and do common initialization
//
   *errBP= 0;
   eDest = 0;
   viP   = vInfo;
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

// If we have a messaging object, use that
//
   if (eDest) {eDest->Say("Config ", txt1, txt2, txt3, txt4, txt5); return;}

// If there is already a message in the buffer, then make sure it prints
//
   char *bP;
   int   bL, n , i = 0;

   if (*errBP)
      {int n = strlen(errBP);
       if (n+16 > errBL) return;
       errBP[n] = '\n';
       bP = errBP + n + 1;
       bL = errBL - n - 1;
      } else {
       bP = errBP;
       bL = errBL;
      }

// Place the message in the buffer
//
   const char *eTxt[] = {txt1, txt2, txt3, txt4, txt5, 0};
   while(bL > 1 && eTxt[i])
        {n = snprintf(bP, bL, "%s", eTxt[i]);
         bP += n; bL -= n; i++;
        }
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/

void XrdOucPinLoader::Init(const char *drctv, const char *plib)
{
   char *plib2 = 0, libBuf[2048];
   int  n;
   bool noFallBack;

// We have no plugin
//
   piP    = 0;
   dName  = drctv;
   global = false;
   badLib = false;

// Check if the path has a version in it. This is generally a no-no.
// We Issue a warning only on servers as that is where it usually occurs.
//
   if ((n = XrdOucVerName::hasVersion(plib, &plib2)))
      {if (plib2)
          {snprintf(libBuf, sizeof(libBuf), "'%s' wrongly coerces version "
                    "'-%d'; using '%s' instead!", plib, n, plib2);
          } else {
           snprintf(libBuf, sizeof(libBuf), "'%s' should not use '-%d' "
                    "version syntax in its name!", plib, n);
          }
       if (eDest) eDest->Say("Config warning: ", dName, " path ", libBuf);
       if (plib2) plib = plib2;
      }

// Perform versioning
//
   if (XrdOucVerName::Version(XRDPLUGIN_SOVERSION, plib, noFallBack,
                               libBuf, sizeof(libBuf)))
      {theLib = strdup(libBuf);
       altLib = (noFallBack ? 0 : strdup(plib));
      } else {
       theLib = 0;
       altLib = strdup(plib);
      }
// Free up any allocated storage
//
   if (plib2) free(plib2);
}
  
/******************************************************************************/
/* Private:                      L o a d L i b                                */
/******************************************************************************/

bool XrdOucPinLoader::LoadLib(int mcnt)
{
   bool allMsgs = altLib == 0;

// Create a plugin object
//
   if (eDest) piP = new XrdSysPlugin(eDest,        theLib, dName, viP, mcnt);
      else    piP = new XrdSysPlugin(errBP, errBL, theLib, dName, viP, mcnt);

// Attempt to load the library
//
   if (piP->getLibrary(allMsgs, global)) return true;

// We failed, so delete this plugin
//
   delete piP; piP = 0;

// If we have an alternate unversioned name then we can try that but only
// if the versioned wasn't found.
//
   if (!altLib && errno != ENOENT)
      {badLib = true;
       return false;
      }

// Indicate what we are doing but only for server-side plugins
//
   if (eDest) eDest->Say("Plugin ", dName, " ", theLib,
                         " not found; falling back to using ", altLib);

// if we have an alternative, readjust library pointers
//
   if (altLib)
      {free(theLib);
       theLib = altLib;
       altLib = 0;
      } else {
       badLib = true;
       return false;
      }

// Try once more
//
   if (eDest) piP = new XrdSysPlugin(eDest,        theLib, dName, viP, mcnt);
      else    piP = new XrdSysPlugin(errBP, errBL, theLib, dName, viP, mcnt);

// Attempt to load the alternate library
//
   if (piP->getLibrary(true, global)) return true;
   badLib = true;
   return false;
}
  
/******************************************************************************/
/*                               R e s o l v e                                */
/******************************************************************************/

void *XrdOucPinLoader::Resolve(const char *symP, int mcnt)
{
   int isOptional = 0;

// Check if we couldn't create the path
//
   if (!theLib && !badLib)
      {Inform("Unable to load ",dName," plugin ",altLib,"; invalid path.");
       badLib = true;
       return 0;
      }

// If we couldn't load the library return failure. This is likely an alternate
// resolution as the caller is looking for an alternate symbol.
//
   if (badLib) return 0;

// Load the library so we can get errors about the library irrespective of
// the symbol we are trying to resolve.
//
   if (!piP && !LoadLib(mcnt)) return 0;

// Handle optional resolution
//
   if (*symP == '?' || *symP == '!')
      {symP++;
       isOptional = (*symP == '!' ? 1 : 2);
      }

// We already have a plugin object so look up the symbol.
//
   return piP->getPlugin(symP, isOptional, global);
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
