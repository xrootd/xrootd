/******************************************************************************/
/*                                                                            */
/*                         X r d S s i S t a t . c c                          */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "XrdVersion.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssStatInfo.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdSsi/XrdSsiProvider.hh"
#include "XrdSsi/XrdSsiSfsConfig.hh"
#include "XrdSsi/XrdSsiService.hh"
#include "XrdSys/XrdSysError.hh"

//------------------------------------------------------------------------------
//! This file defines a default plug-in that can be used to handle stat()
//! calls for the Scalable Service Interface.
//------------------------------------------------------------------------------


/******************************************************************************/
/*                               E x t e r n s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSsiProvider   *Provider;

extern XrdOucPListAnchor FSPath;

extern bool              fsChk;

extern XrdSysError       Log;
};

using namespace XrdSsi;
  
/******************************************************************************/
/*                        X r d S s i S t a t I n f o                         */
/******************************************************************************/

extern "C"
{
int XrdSsiStatInfo(const char *path, struct stat *buff,
                   int         opts, XrdOucEnv   *envP, const char *lfn)
{
   static const int regFile = S_IFREG | S_IRUSR | S_IWUSR;
   XrdSsiProvider::rStat rStat;

// Check for stat changes
//
   if (!buff)
      {if (!Provider || (fsChk && FSPath.Find(lfn))) return 0;
         if (opts == XrdOssStatEvent::FileRemoved)
            Provider->ResourceRemoved(lfn);
       else Provider->ResourceAdded(lfn);
       return 0;
      }

// Check if this should be issued to the file system
//
   if (fsChk && FSPath.Find(lfn)) return stat(path, buff);

// Check resource availability
//
   if (Provider && (rStat = Provider->QueryResource(path)))
      {memset(buff, 0, sizeof(struct stat));
       buff->st_mode = regFile;
       if (rStat == XrdSsiProvider::isPresent) return 0;
       if (!(opts & XRDOSS_resonly)) {buff->st_mode |= S_IFBLK; return 0;}
      }

// Resource is not available
//
   errno = ENOENT;
   return -1;
}

/******************************************************************************/
/*                    X r d O s s S t a t I n f o I n i t                     */
/******************************************************************************/

//------------------------------------------------------------------------------
//! The following function is invoked by the plugin manager to obtain the
//! function that is to be used for stat() calls.
//------------------------------------------------------------------------------
  
XrdOssStatInfo2_t XrdOssStatInfoInit2(XrdOss        *native_oss,
                                      XrdSysLogger  *Logger,
                                      const char    *config_fn,
                                      const char    *parms,
                                      XrdOucEnv     *envP)
{
   XrdSsiSfsConfig Config(true);

// Setup the logger
//
   Log.logger(Logger);

// Process the configuration file so that we get the service provider object
//
   if (!Config.Configure(config_fn) || !Config.Configure(envP))
      return 0;

// Return the stat function
//
    return (XrdOssStatInfo2_t)XrdSsiStatInfo;
}
};

/******************************************************************************/
/*                   V e r s i o n   I n f o r m a t i o n                    */
/******************************************************************************/

XrdVERSIONINFO(XrdOssStatInfoInit,XrdSsiStat);
