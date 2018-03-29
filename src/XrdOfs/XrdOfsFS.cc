/******************************************************************************/
/*                                                                            */
/*            X r d S f s G e t D e f a u l t F i l e S y s t e m             */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOfs/XrdOfs.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

// If you are replacing the standard definition of the file system interface,
// with a derived class to perform additional or enhanced functions, you MUST
// define XrdOfsFS to be an instance of your derived class definition. You
// would then create a shared library linking against libXrdOfs.a and manually
// include your definition of XrdOfsFS (obviously upcast to XrdOfs). This
// is how the standard libXrdOfs.so is built.

// If additional configuration is needed, over-ride the Config() method. At the
// the end of your config, return the result of the XrdOfs::Config().


XrdOfs *XrdOfsFS = NULL;

XrdSfsFileSystem *XrdSfsGetDefaultFileSystem(XrdSfsFileSystem *native_fs,
                                             XrdSysLogger     *lp,
                                             const char       *configfn,
                                             XrdOucEnv        *EnvInfo)
{
   extern XrdSysError OfsEroute;
   static XrdSysMutex XrdDefaultOfsMutex;
   static XrdOfs XrdDefaultOfsFS;

// No need to herald this as it's now the default filesystem
//
   OfsEroute.SetPrefix("ofs_");
   OfsEroute.logger(lp);

// Initialize the subsystems
//
   {
      XrdSysMutexHelper sentry(XrdDefaultOfsMutex);
      if (XrdOfsFS == NULL) {
         XrdOfsFS = &XrdDefaultOfsFS;
         XrdOfsFS->ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);
         if ( XrdOfsFS->Configure(OfsEroute, EnvInfo) ) return 0;
      }
   }

// All done, we can return the callout vector to these routines.
//
   return XrdOfsFS;
}
