/******************************************************************************/
/*                                                                            */
/*                          X r d S s i S f s . c c                           */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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

#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddr.hh"

#include "XrdSsi/XrdSsiSfs.hh"
#include "XrdSsi/XrdSsiSfsConfig.hh"

#include "XrdCms/XrdCmsClient.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucLock.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdVersion.hh"

#ifdef AIX
#include <sys/mode.h>
#endif

/******************************************************************************/
/*                V e r s i o n   I d e n t i f i c a t i o n                 */
/******************************************************************************/
  
XrdVERSIONINFO(XrdSfsGetFileSystem,ssi);

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

namespace
{
XrdSsiSfsConfig *Config;
};

namespace XrdSsi
{
XrdSysError      Log(0);

XrdSysLogger    *Logger;

XrdOucTrace      Trace(&Log);
};

using namespace XrdSsi;

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
int               XrdSsiSfs::freeMax = 256;

/******************************************************************************/
/*                   X r d S f s G e t F i l e S y s t e m                    */
/******************************************************************************/
  
extern "C"
{
XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *nativeFS,
                                      XrdSysLogger     *logger,
                                      const char       *configFn)
{
   static XrdSsiSfs       Sfs;
   static XrdSsiSfsConfig myConfig;

// Set pointer to the config
//
   Config = &myConfig;

// No need to herald this as it's now the default filesystem
//
   Log.SetPrefix("ssi_");
   Log.logger(logger);
   Logger = logger;

// Initialize the subsystems
//
   if (!myConfig.Configure(configFn)) return 0;

// All done, we can return the callout vector to these routines.
//
   return &Sfs;
}
}

/******************************************************************************/
/*                               E n v I n f o                                */
/******************************************************************************/
  
void XrdSsiSfs::EnvInfo(XrdOucEnv *envP)
{
    if (!envP) Log.Emsg("EnvInfo", "No environmental information passed!");
    if (!envP || !Config->Configure(envP)) abort();
}

/******************************************************************************/
/*                            g e t V e r s i o n                             */
/******************************************************************************/
  
const char *XrdSsiSfs::getVersion() {return XrdVERSION;}
