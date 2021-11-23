/******************************************************************************/
/*                                                                            */
/*                    X r d O u c N 2 N L o a d e r . c c                     */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
  
#include "XrdVersion.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/
  
XrdOucName2Name *XrdOucN2NLoader::Load(const char     *libName,
                                       XrdVersionInfo &urVer,
                                       XrdOucEnv      *envP)
{
   extern XrdOucName2NameVec *XrdOucN2NVec_P;
   XrdOucName2Name *(*ep)(XrdOucgetName2NameArgs);
   static XrdVERSIONINFODEF (myVer, XrdN2N, XrdVNUMBER, XrdVERSION);
   XrdOucName2Name    *n2nP;
   XrdOucName2NameVec *n2nV;

// Use the default mapping if there is no library. Verify version numbers
// as we are normally in a different shared library.
//
   if (!libName)
      {if (!XrdSysPlugin::VerCmp(urVer, myVer)) return 0;
       if (lclRoot)
          {struct stat Stat;
           if (stat(lclRoot, &Stat))
              {eRoute->Emsg("N2N", errno, "use localroot", lclRoot);
               return 0;
              }
           if (!S_ISDIR(Stat.st_mode))
              {eRoute->Emsg("N2N", ENOTDIR, "use localroot", lclRoot);
               return 0;
              }
           XrdOucEnv::Export("XRDLCLROOT", lclRoot);
          }
       if (rmtRoot) XrdOucEnv::Export("XRDRMTROOT", rmtRoot);
       n2nP = XrdOucgetName2Name(eRoute, cFN, libParms, lclRoot, rmtRoot);
       if (XrdOucN2NVec_P && envP)
          envP->PutPtr("XrdOucName2NameVec*", XrdOucN2NVec_P);
       return n2nP;
      } else {
       XrdOucEnv::Export("XRDN2NLIB", libName);
       if (libParms) XrdOucEnv::Export("XRDN2NPARMS", libParms);
      }

// Get the entry point of the object creator
// 
   XrdOucPinLoader  myLib(eRoute, &urVer, "namelib", libName);
   ep = (XrdOucName2Name *(*)(XrdOucgetName2NameArgs))(myLib.Resolve("XrdOucgetName2Name"));
   if (!ep) return 0;

// Get the Object now
// 
   if ((n2nP = ep(eRoute, cFN, libParms, lclRoot, rmtRoot)) && envP)
      {n2nV = (XrdOucName2NameVec *)myLib.Resolve("?Name2NameVec");
       if (n2nV) envP->PutPtr("XrdOucName2NameVec*", n2nV);
      }
   return n2nP;
}
