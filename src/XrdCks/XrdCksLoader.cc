/******************************************************************************/
/*                                                                            */
/*                       X r d C k s L o a d e r . c c                        */
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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
  
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksCalcadler32.hh"
#include "XrdCks/XrdCksCalccrc32.hh"
#include "XrdCks/XrdCksCalcmd5.hh"
#include "XrdCks/XrdCksLoader.hh"

#include "XrdOuc/XrdOucPinLoader.hh"

#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdVersion.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCksLoader::XrdCksLoader(XrdVersionInfo &vInfo, const char *libPath)
{
   static XrdVERSIONINFODEF(myVersion, XrdCks, XrdVNUMBER, XrdVERSION);
   static const char libSfx[] = "/libXrdCksCalc%s.so";
   int k, n;

// Verify that versions are compatible.
//
   if (vInfo.vNum != myVersion.vNum
   &&  !XrdSysPlugin::VerCmp(vInfo, myVersion, true))
      {char buff[1024];
       snprintf(buff, sizeof(buff), "Version %s is incompatible with %s.",
                                     vInfo.vStr, myVersion.vStr);
       verMsg = strdup(buff); urVersion = 0;
       return;
      }
   urVersion = &vInfo;
   verMsg = 0;

// Prefill the native digests we support
//
   csTab[0].Name = strdup("adler32");
   csTab[1].Name = strdup("crc32");
   csTab[2].Name = strdup("md5");
   csLast = 2;

// Record the over-ride loader path
//
   if (libPath)
      {n = strlen(libPath);
       ldPath = (char *)malloc(n+sizeof(libSfx)+1);
       k = (libPath[n-1] == '/');
       strcpy(ldPath, libPath);
       strcpy(ldPath+n, libSfx+k);
      } else ldPath = strdup(libSfx+1);
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdCksLoader::~XrdCksLoader()
{
   int i;
   for (i = 0; i <= csLast; i++)
       {if (csTab[i].Name)   free(  csTab[i].Name);
        if (csTab[i].Obj)    csTab[i].Obj->Recycle();
        if (csTab[i].Plugin) delete csTab[i].Plugin;
       }
   if (ldPath) free(ldPath);
   if (verMsg) free(verMsg);
}

/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/
  
#define XRDOSSCKSLIBARGS XrdSysError *, const char *, const char *, const char *

XrdCksCalc *XrdCksLoader::Load(const char *csName, const char *csParms,
                                     char *eBuff,  int eBlen, bool orig)
{
   static XrdSysMutex myMutex;
   XrdSysMutexHelper  ldMutex(myMutex);
   XrdCksCalc      *(*ep)(XRDOSSCKSLIBARGS);
   XrdCksCalc      *csObj;
   XrdOucPinLoader *myPin;
   csInfo          *csIP;
   char ldBuff[2048];
   int n;

// Verify that version checking succeeded
//
   if (verMsg) {if (eBuff) strncpy(eBuff, verMsg, eBlen); return 0;}

// First check if we have loaded this before
//
   if ((csIP = Find(csName)))
      {if (!(csIP->Obj))
          {     if (!strcmp("adler32", csIP->Name))
                   csIP->Obj = new XrdCksCalcadler32;
           else if (!strcmp("crc32",   csIP->Name))
                   csIP->Obj = new XrdCksCalccrc32;
           else if (!strcmp("md5",     csIP->Name))
                   csIP->Obj = new XrdCksCalcmd5;
           else {if (eBuff) snprintf(eBuff, eBlen, "Logic error configuring %s "
                                                   "checksum.", csName);
                 return 0;
                }
          }
       return (orig ? csIP->Obj : csIP->Obj->New());
      }

// Check if we can add a new entry
//
   if (csLast+1 >= csMax)
      {if (eBuff) strncpy(eBuff, "Maximum number of checksums loaded.", eBlen);
       return 0;
      }

// Get the path where this object lives
//
   snprintf(ldBuff, sizeof(ldBuff), ldPath, csName);

// Get the plugin loader
//
   if (!(myPin = new XrdOucPinLoader(eBuff,eBlen,urVersion,"ckslib",ldBuff)))
      return 0;

// Find the entry point
//
   if (!(ep = (XrdCksCalc *(*)(XRDOSSCKSLIBARGS))
              (myPin->Resolve("XrdCksCalcInit"))))
      {myPin->Unload(true); return 0;}

// Get the initial object
//
   if (!(csObj = ep(0, 0, csName, csParms)))
      {if (eBuff)
          snprintf(eBuff, eBlen, "%s checksum initialization failed.", csName);
       myPin->Unload(true);
       return 0;
      }

// Verify the object
//
   if (strcmp(csName, csObj->Type(n)))
      {if (eBuff)
          snprintf(eBuff, eBlen, "%s cksum plugin returned wrong name - %s",
                                 csName, csObj->Type(n));
       delete csObj;
       myPin->Unload(true);
       return 0;
      }

// Allocate a new entry in the table and initialize it
//
   csLast++;
   csTab[csLast].Name   = strdup(csName);
   csTab[csLast].Obj    = csObj;
   csTab[csLast].Plugin = myPin->Export();

// Return new instance of this object
//
   return (orig ? csObj : csObj->New());
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
XrdCksLoader::csInfo *XrdCksLoader::Find(const char *Name)
{
   int i;
   for (i = 0; i <= csLast; i++)
       if (!strcmp(Name, csTab[i].Name)) return &csTab[i];
   return 0;
}
