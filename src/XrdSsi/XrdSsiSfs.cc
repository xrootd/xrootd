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
#include "XrdNet/XrdNetIF.hh"

#include "XrdSsi/XrdSsiDir.hh"
#include "XrdSsi/XrdSsiFile.hh"
#include "XrdSsi/XrdSsiProvider.hh"
#include "XrdSsi/XrdSsiSfs.hh"
#include "XrdSsi/XrdSsiSfsConfig.hh"
#include "XrdSsi/XrdSsiStats.hh"
#include "XrdSsi/XrdSsiTrace.hh"

#include "XrdCms/XrdCmsClient.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucLock.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdOuc/XrdOucTList.hh"
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
extern XrdSsiProvider   *Provider;

extern XrdNetIF         *myIF;

       XrdSfsFileSystem *theFS = 0;

extern XrdOucPListAnchor FSPath;

extern bool              fsChk;

extern XrdSysError       Log;

extern XrdSysLogger     *Logger;

extern XrdSysTrace       Trace;

extern XrdSsiStats       Stats;
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
   static XrdSsiSfs       mySfs;
   static XrdSsiSfsConfig myConfig;

// Set pointer to the config and file system
//
   Config = &myConfig;
   theFS  = nativeFS;
   Stats.setFS(nativeFS);

// No need to herald this as it's now the default filesystem
//
   Log.SetPrefix("ssi_");
   Log.logger(logger);
   Logger = logger;
   Trace.SetLogger(logger);

// Initialize the subsystems
//
   if (!myConfig.Configure(configFn)) return 0;

// All done, we can return the callout vector to these routines.
//
   return &mySfs;
}
}
  
/******************************************************************************/
/*                                c h k s u m                                 */
/******************************************************************************/

int XrdSsiSfs::chksum(      csFunc            Func,   // In
                            const char       *csName, // In
                            const char       *Path,   // In
                            XrdOucErrInfo    &einfo,  // Out
                      const XrdSecEntity     *client, // In
                      const char             *opaque) // In
{
// Reroute this request if we can
//
   if (fsChk) return theFS->chksum(Func, csName, Path, einfo, client, opaque);
   einfo.setErrInfo(ENOTSUP, "Checksums are not supported.");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                                 c h m o d                                  */
/******************************************************************************/

int XrdSsiSfs::chmod(const char             *path,    // In
                           XrdSfsMode        Mode,    // In
                           XrdOucErrInfo    &einfo,   // Out
                     const XrdSecEntity     *client,  // In
                     const char             *info)    // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->chmod(path, Mode, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "chmod is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "chmod is not supported.");
   return SFS_ERROR;
}
  
/******************************************************************************/
/* Private:                         E m s g                                   */
/******************************************************************************/

int XrdSsiSfs::Emsg(const char    *pfx,    // Message prefix value
                    XrdOucErrInfo &einfo,  // Place to put text & error code
                    int            ecode,  // The error code
                    const char    *op,     // Operation being performed
                    const char    *target) // The target (e.g., fname)
{
   char buffer[MAXPATHLEN+80];

// Format the error message
//
   XrdOucERoute::Format(buffer, sizeof(buffer), ecode, op, target);

// Print it out if debugging is enabled
//
#ifndef NODEBUG
    Log.Emsg(pfx, einfo.getErrUser(), buffer);
#endif

// Place the error message in the error object and return
//
    einfo.setErrInfo(ecode, buffer);
    return SFS_ERROR;
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
/*                                e x i s t s                                 */
/******************************************************************************/

int XrdSsiSfs::exists(const char                *path,        // In
                            XrdSfsFileExistence &file_exists, // Out
                            XrdOucErrInfo       &einfo,       // Out
                      const XrdSecEntity        *client,      // In
                      const char                *info)        // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->exists(path, file_exists, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "exists is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "exists is not supported.");
   return SFS_ERROR;
}

/******************************************************************************/
/*                                 f s c t l                                  */
/******************************************************************************/

int XrdSsiSfs::fsctl(const int               cmd,
                     const char             *args,
                     XrdOucErrInfo          &einfo,
                     const XrdSecEntity     *client)
/*
  Function: Perform filesystem operations:

  Input:    cmd       - Operation command (currently supported):
                        SFS_FSCTL_LOCATE - locate resource
            args      - Command dependent argument:
                      - Locate: The path whose location is wanted
            einfo     - Error/Response information structure.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   EPNAME("fsctl");
   const char *tident = einfo.getErrUser();

   char pbuff[1024], rType[3] = {'S', 'w', 0};
   const char *Resp[2] = {rType, pbuff};
   const char *opq, *Path = Split(args,&opq,pbuff,sizeof(pbuff));
   XrdNetIF::ifType ifType;
   int Resp1Len;

// Do some debugging
//
   DEBUG(args);

// We only process the locate request here. Reroute it if we can otherwise.
//
   if ((cmd & SFS_FSCTL_CMD) != SFS_FSCTL_LOCATE)
      {if (fsChk) return theFS->fsctl(cmd, args, einfo, client);
       einfo.setErrInfo(ENOTSUP, "Requested fsctl operation not supported.");
       return SFS_ERROR;
      }

// Preprocess the argument
//
            if (*Path == '*')      Path++;
       else if (cmd & SFS_O_TRUNC) Path = 0;

// Check if we should reoute this request
//
   if (fsChk && Path && FSPath.Find(Path))
      return theFS->fsctl(cmd, args, einfo, client);

// If we have a path then see if we really have it
//
   if (Path)
      {if (!Provider) return Emsg(epname, einfo, EHOSTUNREACH, "locate", Path);
          else {XrdSsiProvider::rStat rStat = Provider->QueryResource(Path);
                     if (rStat == XrdSsiProvider::isPresent) rType[0] = 'S';
                else if (rStat == XrdSsiProvider::isPending) rType[0] = 's';
                else return Emsg(epname, einfo, ENOENT, "locate", Path);
               }
      }

// Compute interface return options
//
   ifType = XrdNetIF::GetIFType((einfo.getUCap() & XrdOucEI::uIPv4)  != 0,
                                (einfo.getUCap() & XrdOucEI::uIPv64) != 0,
                                (einfo.getUCap() & XrdOucEI::uPrip)  != 0);
   bool retHN = (cmd & SFS_O_HNAME) != 0;

// Get our destination
//
   if ((Resp1Len = myIF->GetDest(pbuff, sizeof(pbuff), ifType, retHN)))
      {einfo.setErrInfo(Resp1Len+3, (const char **)Resp, 2);
       return SFS_DATA;
      }

// We failed for some unknown reason
//
   return Emsg(epname, einfo, ENETUNREACH, "locate", Path);
}


/******************************************************************************/
/*                              g e t S t a t s                               */
/******************************************************************************/

int XrdSsiSfs::getStats(char *buff, int blen)
{
// Return statustics
//
   return Stats.Stats(buff, blen);
}

/******************************************************************************/
/*                            g e t V e r s i o n                             */
/******************************************************************************/
  
const char *XrdSsiSfs::getVersion() {return XrdVERSION;}

/******************************************************************************/
/*                                 m k d i r                                  */
/******************************************************************************/

int XrdSsiSfs::mkdir(const char             *path,    // In
                           XrdSfsMode        Mode,    // In
                           XrdOucErrInfo    &einfo,   // Out
                     const XrdSecEntity     *client,  // In
                     const char             *info)    // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->mkdir(path, Mode, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "mkdir is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "mkdir is not supported.");
   return SFS_ERROR;
}

/******************************************************************************/
/*                               p r e p a r e                                */
/******************************************************************************/

int XrdSsiSfs::prepare(      XrdSfsPrep       &pargs,      // In
                             XrdOucErrInfo    &out_error,  // Out
                       const XrdSecEntity     *client)     // In
{
// Reroute this if we can
//
   if (theFS) return theFS->prepare(pargs, out_error, client);
   return SFS_OK;
}

/******************************************************************************/
/*                                   r e m                                    */
/******************************************************************************/

int XrdSsiSfs::rem(const char             *path,    // In
                         XrdOucErrInfo    &einfo,   // Out
                   const XrdSecEntity     *client,  // In
                   const char             *info)    // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->rem(path, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "rem is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "rem is not supported.");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                                r e m d i r                                 */
/******************************************************************************/

int XrdSsiSfs::remdir(const char             *path,    // In
                            XrdOucErrInfo    &einfo,   // Out
                      const XrdSecEntity     *client,  // In
                      const char             *info)    // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->rem(path, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "remdir is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "remdir is not supported.");
   return SFS_ERROR;
}

/******************************************************************************/
/*                                r e n a m e                                 */
/******************************************************************************/

int XrdSsiSfs::rename(const char             *old_name,  // In
                      const char             *new_name,  // In
                            XrdOucErrInfo    &einfo,     //Out
                      const XrdSecEntity     *client,    // In
                      const char             *infoO,     // In
                      const char             *infoN)     // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(old_name))
          return theFS->rename(old_name,new_name,einfo,client,infoO,infoN);
       einfo.setErrInfo(ENOTSUP, "rename is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "rename is not supported.");
   return SFS_ERROR;
}

/******************************************************************************/
/* Private:                        S p l i t                                  */
/******************************************************************************/
  
const char * XrdSsiSfs::Split(const char *Args, const char **Opq,
                              char *Path, int Plen)
{
   int xlen;
   *Opq = index(Args, '?');
   if (!(*Opq)) return Args;
   xlen = (*Opq)-Args;
   if (xlen >= Plen) xlen = Plen-1;
   strncpy(Path, Args, xlen);
   return Path;
}

/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdSsiSfs::stat(const char             *path,        // In
                          struct stat      *buf,         // Out
                          XrdOucErrInfo    &einfo,       // Out
                    const XrdSecEntity     *client,      // In
                    const char             *info)        // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->stat(path, buf, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "stat is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "stat is not supported.");
   return SFS_ERROR;
}

/******************************************************************************/

int XrdSsiSfs::stat(const char             *path,        // In
                          mode_t           &mode,        // Out
                          XrdOucErrInfo    &einfo,       // Out
                    const XrdSecEntity     *client,      // In
                    const char             *info)        // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->stat(path, mode, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "stat is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "stat is not supported.");
   return SFS_ERROR;
}

/******************************************************************************/
/*                              t r u n c a t e                               */
/******************************************************************************/

int XrdSsiSfs::truncate(const char             *path,    // In
                              XrdSfsFileOffset  Size,    // In
                              XrdOucErrInfo    &einfo,   // Out
                        const XrdSecEntity     *client,  // In
                        const char             *info)    // In
{
// Reroute this request if we can
//
   if (fsChk)
      {if (FSPath.Find(path))
          return theFS->truncate(path, Size, einfo, client, info);
       einfo.setErrInfo(ENOTSUP, "truncate is not supported for given path.");
      } else einfo.setErrInfo(ENOTSUP, "truncate is not supported.");
   return SFS_ERROR;
}
