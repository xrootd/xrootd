/******************************************************************************/
/*                                                                            */
/*                        X r d P f c F S c t l . c c                         */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>
#include <errno.h>
#include <string.h>

#include "XrdOfs/XrdOfsHandle.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdPfc/XrdPfc.hh"
#include "XrdPfc/XrdPfcFSctl.hh"
#include "XrdPfc/XrdPfcTrace.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysTrace.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdPfcFSctl::XrdPfcFSctl(XrdPfc::Cache &cInst, XrdSysLogger *logP)
                        : myCache(cInst), hProc(0), Log(logP, "PfcFsctl"),
                          sysTrace(cInst.GetTrace()), m_traceID("PfcFSctl") {}
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

bool XrdPfcFSctl::Configure(const char    *CfgFN,
                            const char    *Parms,
                            XrdOucEnv     *envP,
                            const Plugins &plugs)
{
// All we are interested in is getting the file handle handler pointer
//
   hProc = (XrdOfsHandle*)envP->GetPtr("XrdOfsHandle*");
   return hProc != 0;
}
    
/******************************************************************************/
/*                          F S c t l   [ F i l e ]                           */
/******************************************************************************/
  
int XrdPfcFSctl::FSctl(const int               cmd,
                             int               alen,
                       const char             *args,
                             XrdSfsFile       &file,
                             XrdOucErrInfo    &eInfo,
                       const XrdSecEntity     *client)
{
   eInfo.setErrInfo(ENOTSUP, "File based fstcl not supported for a cache."); 
   return SFS_ERROR;
}

/******************************************************************************/
/*                          F S c t l   [ B a s e ]                           */
/******************************************************************************/

int XrdPfcFSctl::FSctl(const int               cmd,
                             XrdSfsFSctl      &args,
                             XrdOucErrInfo    &eInfo,
                       const XrdSecEntity     *client)
{
   const char *msg = "", *xeq = args.Arg1;
   int ec, rc;

// Verify command
//
   if (cmd != SFS_FSCTL_PLUGXC)
      {eInfo.setErrInfo(EIDRM, "None-cache command issued to a cache."); 
       return SFS_ERROR;
      }

// Very that we have a command
//
   if (!xeq || args.Arg1Len < 1) 
      {eInfo.setErrInfo(EINVAL, "Missing cache command or argument."); 
       return SFS_ERROR;
      }

// Process command
//
   if ((!strcmp(xeq, "evict") || !strcmp(xeq, "fevict")) && args.Arg2Len == -2)
      {std::string path = args.ArgP[0];
       ec = myCache.UnlinkFile(path, *xeq != 'f');
       switch(ec)
             {case       0: if (hProc) hProc->Hide(path.c_str());
                            [[fallthrough]];
              case -ENOENT: rc = SFS_OK;
                            break;
              case  -EBUSY: ec = ENOTTY;
                            rc = SFS_ERROR;
                            msg = "file is in use";
                            break;
              case -EAGAIN: rc = 5;
                            break;
              default:      rc = SFS_ERROR;
                            msg = "unlink failed";
                            break;
             }
       TRACE(Info,"Cache "<<xeq<<' '<<path<<" rc="<<ec<<" ec="<<ec<<" msg="<<msg);
      } else {
   ec = EINVAL;
   rc = SFS_ERROR;
  }

// Return result
//
   eInfo.setErrInfo(ec, msg);
   return rc;
}
