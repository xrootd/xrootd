#ifndef __XRDPFCFSCTL_H__
#define __XRDPFCFSCTL_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P f c F S c t l . h h                         */
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

#include "XrdOfs/XrdOfsFSctl_PI.hh"
#include "XrdSys/XrdSysError.hh"

class XrdOfsHandle;
class XrdOucErrInfo;
class XrdOucEnv;
class XrdSecEntity;
class XrdSfsFile;
class XrdSysLogger;
class XrdSysTrace;

namespace XrdPfc {class Cache;}

class XrdPfcFSctl : public XrdOfsFSctl_PI
{public:

virtual bool           Configure(const char    *CfgFN,
                                 const char    *Parms,
                                 XrdOucEnv     *envP,
                                 const Plugins &plugs) override;

virtual int            FSctl(const int               cmd,
                                   int               alen,
                             const char             *args,
                                   XrdSfsFile       &file,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client = 0) override;

virtual int            FSctl(const int               cmd,
                                   XrdSfsFSctl      &args,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client = 0) override;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

             XrdPfcFSctl(XrdPfc::Cache &cInst, XrdSysLogger *logP);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdPfcFSctl() {}

private:
XrdSysTrace*   GetTrace() {return sysTrace;}

XrdPfc::Cache& myCache;
XrdOfsHandle*  hProc;
XrdSysError    Log;
XrdSysTrace*   sysTrace;
const char *   m_traceID;
};
#endif
