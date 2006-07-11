/******************************************************************************/
/*                                                                            */
/*                        X r d O s s G e t S S . c c                         */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <unistd.h>
#include <sys/types.h>
  
#include "XrdOss/XrdOssApi.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlugin.hh"

extern "C"
{
XrdOss *XrdOssGetStorageSystem(XrdOss       *native_oss,
                               XrdOucLogger *Logger,
                               const char   *config_fn,
                               const char   *OssLib)
{
   extern XrdOssSys   XrdOssSS;
   extern XrdOucError OssEroute;
   XrdOucPlugin    *myLib;
   XrdOss          *(*ep)(XrdOss *, XrdOucLogger *, const char *, const char *);
   char *parms;

// If no library has been specified, return the default object
//
   if (!OssLib)
      return (XrdOssSS.Init(Logger, config_fn) ? 0 : (XrdOss *)&XrdOssSS);

// Find the parms (ignore the constness of the variable)
//
   parms = (char *)OssLib;
   while(*parms && *parms != ' ') parms++;
   if (*parms) *parms++ = '\0';
   while(*parms && *parms == ' ') parms++;
   if (!*parms) parms = 0;

// Create a pluin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   OssEroute.logger(Logger);
   if (!(myLib = new XrdOucPlugin(&OssEroute, OssLib))) return 0;

// Now get the entry point of the object creator
//
   ep = (XrdOss *(*)(XrdOss *, XrdOucLogger *, const char *, const char *))
                    (myLib->getPlugin("XrdOssGetStorageSystem"));
   if (!ep) return 0;

// Get the Object now
//
   return ep((XrdOss *)&XrdOssSS, Logger, config_fn, parms);
}
}
