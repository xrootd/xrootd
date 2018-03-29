#ifndef __XRDSECSERVER_H__
#define __XRDSECSERVER_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S e c S e r v e r . h h                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecPManager.hh"

class XrdSecProtBind;
class XrdOucTrace;
class XrdNetAddrInfo;
  
class XrdSecServer : XrdSecService
{
public:

const char             *getParms(int &size, XrdNetAddrInfo *endPoint=0);

// = 0 -> No protocol can be returned (einfo has the reason)
// ! 0 -> Address of protocol object is bing returned.
//
XrdSecProtocol         *getProtocol(const char              *host,    // In
                                    XrdNetAddrInfo          &endPoint,// In
                                    const XrdSecCredentials *cred,    // In
                                    XrdOucErrInfo           *einfo=0);// Out

int                     Configure(const char *cfn);

                        XrdSecServer(XrdSysLogger *lp);
                       ~XrdSecServer() {}      // Server is never deleted

private:

static XrdSecPManager  PManager;

XrdSysError     eDest;
XrdOucTrace    *SecTrace;
XrdSecProtBind *bpFirst;
XrdSecProtBind *bpLast;
XrdSecProtBind *bpDefault;
char           *SToken;
char           *STBuff;
int             STBlen;
int             Enforce;
int             implauth;

int             add2token(XrdSysError &erp,char *,char **,int &,XrdSecPMask_t &);
int             ConfigFile(const char *cfn);
int             ConfigXeq(char *var, XrdOucStream &Config, XrdSysError &Eroute);
int             ProtBind_Complete(XrdSysError &Eroute);
int             xlevel(XrdOucStream &Config, XrdSysError &Eroute);
int             xpbind(XrdOucStream &Config, XrdSysError &Eroute);
int             xpparm(XrdOucStream &Config, XrdSysError &Eroute);
int             xprot(XrdOucStream &Config, XrdSysError &Eroute);
int             xtrace(XrdOucStream &Config, XrdSysError &Eroute);
};
#endif
