#ifndef __SEC_PMANAGER_HH__
#define __SEC_PMANAGER_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S e c P M a n a g e r . h h                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdNetAddrInfo;
class XrdOucErrInfo;
class XrdSecProtList;
class XrdSecProtocol;
class XrdSysError;

typedef int XrdSecPMask_t;

#define PROTPARMS const char, const char *, XrdNetAddrInfo &, \
                  const char *, XrdOucErrInfo *

class XrdSecPManager
{
public:

XrdSecPMask_t   Find(const         char  *pid,      // In
                                   char **parg=0);  // Out

XrdSecProtocol *Get(const char        *hname,
                    XrdNetAddrInfo    &endPoint,
                    const char        *pname,
                    XrdOucErrInfo     *erp);

XrdSecProtocol *Get(const char       *hname,
                    XrdNetAddrInfo   &netaddr,
                    XrdSecParameters &secparm)
                    {return Get(hname, netaddr, secparm, (XrdOucErrInfo *)0);} 

XrdSecProtocol *Get(const char       *hname,
                    XrdNetAddrInfo   &netaddr,
                    XrdSecParameters &secparm,
                    XrdOucErrInfo     *erp);

int             Load(XrdOucErrInfo *eMsg,    // In
                     const char     pmode,   // In 'c' | 's'
                     const char    *pid,     // In
                     const char    *parg,    // In
                     const char    *path)    // In
                     {return (0 != ldPO(eMsg, pmode, pid, parg, path));}

void            setDebug(int dbg) {DebugON = dbg;}

void            setErrP(XrdSysError *eP) {errP = eP;}

                XrdSecPManager(int dbg=0)
                              : protnum(1), First(0), Last(0), errP(0),
                                DebugON(dbg) {}
               ~XrdSecPManager() {}

private:

XrdSecProtList    *Add(XrdOucErrInfo  *eMsg, const char *pid,
                       XrdSecProtocol *(*ep)(PROTPARMS), const char *parg);
XrdSecProtList    *ldPO(XrdOucErrInfo *eMsg,    // In
                        const char     pmode,   // In 'c' | 's'
                        const char    *pid,     // In
                        const char    *parg=0,  // In
                        const char    *spath=0);// In
XrdSecProtList    *Lookup(const char *pid);

XrdSecPMask_t      protnum;
XrdSysMutex        myMutex;
XrdSecProtList    *First;
XrdSecProtList    *Last;
XrdSysError       *errP;
int                DebugON;
};
#endif
