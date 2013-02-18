/******************************************************************************/
/*                                                                            */
/*                       X r d S e c L o a d e r . c c                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSec/XrdSecLoader.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdVersion.hh"

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSecLoader::~XrdSecLoader()
{
   if (secLib) delete secLib;
}

/******************************************************************************/
/*                           G e t P r o t o c o l                            */
/******************************************************************************/

XrdSecProtocol *XrdSecLoader::GetProtocol(const char       *hostname,
                                          XrdNetAddrInfo   &endPoint,
                                          XrdSecParameters &sectoken,
                                          XrdOucErrInfo    *einfo)
{
   static XrdSysMutex ldrMutex;

// Check if we should initialize.
//
   ldrMutex.Lock();
   if (!secLib && !Init(einfo)) {ldrMutex.UnLock(); return 0;}
   ldrMutex.UnLock();

// Return the protocol or nothing
//
   return secGet(hostname, endPoint, sectoken, einfo);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

bool XrdSecLoader::Init(XrdOucErrInfo *einfo)
{
   static XrdVERSIONINFODEF(myVersion, XrdSecLoader, XrdVNUMBER, XrdVERSION);
   char mBuff[1024], path[80];

// Verify that versions are compatible.
//
   if (urVersion->vNum != myVersion.vNum
   &&  !XrdSysPlugin::VerCmp(*urVersion, myVersion, true))
      {snprintf(mBuff,sizeof(mBuff),"Client version %s is incompatible with %s.",
                                     urVersion->vStr, myVersion.vStr);
       if (einfo) einfo->setErrInfo(ENOPROTOOPT, mBuff);
          else cerr <<"SecLoader: " <<mBuff;
       return false;
      }

// Obtain an instance of the security library
//
   strcpy(path, "libXrdSec" LT_MODULE_EXT);
   secLib = new XrdSysPlugin(mBuff,sizeof(mBuff),path,"seclib",urVersion,0);

// Get the client object creator
//
   if ((secGet = (XrdSecGetProt_t)secLib->getPlugin("XrdSecGetProtocol")))
      return true;

// We failed
//
   if (einfo) einfo->setErrInfo(ENOPROTOOPT, mBuff);
      else cerr <<"SecLoader: Unable to initialize; " <<mBuff;

// Cleanup
//
   delete secLib;
   secLib = 0;
   return false;
}
