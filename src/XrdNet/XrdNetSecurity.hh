#ifndef __NETSECURITY__
#define __NETSECURITY__
/******************************************************************************/
/*                                                                            */
/*                     X r d N e t S e c u r i t y . h h                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cctype>
#include <cstdlib>
  
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucNList.hh"
#include "XrdSys/XrdSysPthread.hh"

class  XrdNetAddr;
class  XrdNetTextList;
class  XrdSysTrace;

class XrdNetSecurity
{
public:
  
void  AddHost(char *hname);

void  AddNetGroup(char *hname);

bool  Authorize(const char *hSpec);

bool  Authorize(XrdNetAddr &addr);

void  Merge(XrdNetSecurity *srcp);  // Deletes srcp

void  Trace(XrdSysTrace *et=0) {eTrace = et;}

     XrdNetSecurity() : NetGroups(0), eTrace(0),
                        chkNetLst(false), chkNetGrp(false) {}
    ~XrdNetSecurity() {}

private:

bool hostOK(const char *hname, const char *ipname, const char *why);
bool addHIP(const char *hname);

XrdOucNList_Anchor        HostList;

XrdNetTextList           *NetGroups;

XrdOucHash<char>          OKHosts;
XrdSysMutex               okHMutex;
XrdSysTrace              *eTrace;
bool                      chkNetLst;
bool                      chkNetGrp;

static const char        *TraceID;
};
#endif
