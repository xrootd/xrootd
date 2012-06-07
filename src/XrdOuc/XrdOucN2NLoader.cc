/******************************************************************************/
/*                                                                            */
/*                    X r d O u c N 2 N L o a d e r . c c                     */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include "XrdVersion.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/
  
XrdOucName2Name *XrdOucN2NLoader::Load(const char     *libName,
                                       XrdVersionInfo &urVer)
{
   XrdSysPlugin     myLib(eRoute, libName, "namelib", &urVer);
   XrdOucName2Name *(*ep)(XrdOucgetName2NameArgs);
   static XrdVERSIONINFODEF (myVer, XrdN2N, XrdVNUMBER, XrdVERSION);

// Use the default mapping if there is no library. Verify version numbers
// as we are normally in a different shared library.
//
   if (!libName)
      {if (!XrdSysPlugin::VerCmp(urVer, myVer)) return 0;
       if (lclRoot) XrdOucEnv::Export("XRDLCLROOT", lclRoot);
       if (rmtRoot) XrdOucEnv::Export("XRDRMTROOT", rmtRoot);
       return XrdOucgetName2Name(eRoute, cFN, libParms, lclRoot, rmtRoot);
      } else {
       XrdOucEnv::Export("XRDN2NLIB", libName);
       if (libParms) XrdOucEnv::Export("XRDN2NPARMS", libParms);
      }

// Get the entry point of the object creator
// 
   ep = (XrdOucName2Name *(*)(XrdOucgetName2NameArgs))(myLib.getPlugin("XrdOucgetName2Name"));
   if (!ep) return 0;
   myLib.Persist();

// Get the Object now
// 
   return ep(eRoute, cFN, libParms, lclRoot, rmtRoot);
}
