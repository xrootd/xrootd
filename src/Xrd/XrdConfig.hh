#ifndef _XRD_CONFIG_H
#define _XRD_CONFIG_H
/******************************************************************************/
/*                                                                            */
/*                          X r d C o n f i g . h h                           */
/*                                                                            */
/* (C) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdInet.hh"
#include "Xrd/XrdProtLoad.hh"
#include "Xrd/XrdProtocol.hh"
#include "Xrd/XrdScheduler.hh"
#define   XRD_TRACE Trace.
#include "Xrd/XrdTrace.hh"

#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

class XrdNetSecurity;
class XrdOucStream;
class XrdConfigProt;

class XrdConfig
{
public:

int         Configure(int argc, char **argv);

int         ConfigXeq(char *var, XrdOucStream &Config, XrdSysError *eDest=0);

         XrdConfig();
        ~XrdConfig() {}

XrdProtocol_Config  ProtInfo;
XrdInet            *NetADM;
XrdInet            *NetTCP[XrdProtLoad::ProtoMax+1];

private:

int   ASocket(const char *path, const char *fname, mode_t mode);
int   ConfigProc(void);
int   getUG(char *parm, uid_t &theUid, gid_t &theGid);
void  Manifest(const char *pidfn);
void  setCFG();
int   setFDL();
int   Setup(char *dfltp);
void  Usage(int rc);
int   xallow(XrdSysError *edest, XrdOucStream &Config);
int   xapath(XrdSysError *edest, XrdOucStream &Config);
int   xbuf(XrdSysError *edest, XrdOucStream &Config);
int   xnet(XrdSysError *edest, XrdOucStream &Config);
int   xnkap(XrdSysError *edest, char *val);
int   xlog(XrdSysError *edest, XrdOucStream &Config);
int   xport(XrdSysError *edest, XrdOucStream &Config);
int   xprot(XrdSysError *edest, XrdOucStream &Config);
int   xrep(XrdSysError *edest, XrdOucStream &Config);
int   xsched(XrdSysError *edest, XrdOucStream &Config);
int   xsit(XrdSysError *edest, XrdOucStream &Config);
int   xtrace(XrdSysError *edest, XrdOucStream &Config);
int   xtmo(XrdSysError *edest, XrdOucStream &Config);
int   yport(XrdSysError *edest, const char *ptyp, const char *pval);

static const char  *TraceID;

XrdSysLogger        Logger;
XrdSysError         Log;
XrdOucTrace         Trace;
XrdScheduler        Sched;
XrdBuffManager      BuffPool;
XrdNetSecurity     *Police;
const char         *myProg;
const char         *myName;
const char         *myDomain;
const char         *mySitName;
const char         *myInsName;
char               *myInstance;
char               *AdminPath;
char               *ConfigFN;
char               *repDest[2];
XrdConfigProt      *Firstcp;
XrdConfigProt      *Lastcp;
int                 Net_Blen;
int                 Net_Opts;
int                 Wan_Blen;
int                 Wan_Opts;

int                 PortTCP;      // TCP Port to listen on
int                 PortUDP;      // UDP Port to listen on (currently unsupported)
int                 PortWAN;      // TCP port to listen on for WAN connections
int                 NetTCPlep;
int                 AdminMode;
int                 repInt;
char                repOpts;
char                ppNet;
signed char         coreV;
};
#endif
