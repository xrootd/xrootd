#ifndef _XRDOUCPSX_H
#define _XRDOUCPSX_H
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c P s x . h h                           */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>
#include <cstring>
#include <sys/types.h>

#include "XrdOuc/XrdOucCacheCM.hh"
  
class XrdOucEnv;
class XrdOucName2Name;
class XrdSysError;
class XrdOucStream;
class XrdOucTList;

struct XrdVersionInfo;

class XrdOucPsx
{
public:

const
char     *CCMInfo(const char *&path) {path = mPath; return mParm;}

bool      ClientConfig(const char *pfx, bool hush=false);

bool      ConfigSetup(XrdSysError &eDest, bool hush=false);

bool      hasCache() {return mCache != 0 || cPath != 0;}

bool      ParseCache(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseCio(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseCLib(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseMLib(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseINet(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseNLib(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseSet(XrdSysError *Eroute, XrdOucStream &Config);

bool      ParseTrace(XrdSysError *Eroute, XrdOucStream &Config);

void      SetRoot(const char *lroot, const char *oroot=0);

char                *configFN;    // -> Pointer to the config file name
XrdSysLogger        *theLogger;
XrdOucEnv           *theEnv;
XrdOucName2Name     *theN2N;      // -> File mapper object
XrdOucCache         *theCache;    // -> Cache object
XrdOucCacheCMInit_t  initCCM;     // -> Cache context manager initializer
char                *mCache;      // -> memory cache parameters
XrdOucTList         *setFirst;
XrdOucTList         *setLast;
int                  maxRHCB;
int                  traceLvl;
int                  debugLvl;
int                  cioWait;
int                  cioTries;
bool                 useV4;
bool                 xLfn2Pfn;
char                 xPfn2Lfn; // See xP2Lxxx definitions below
bool                 xNameLib;

static const int     xP2Loff = 0;
static const int     xP2Lon  = 1;
static const int     xP2Lsrc = 2;
static const int     xP2Lsgi = 3;

          XrdOucPsx(XrdVersionInfo *vInfo, const char *cfn,
                    XrdSysLogger *lp=0,    XrdOucEnv  *vp=0)
                   : configFN(strdup(cfn)), theLogger(lp), theEnv(vp),
                     theN2N(0), theCache(0), initCCM(0),
                     mCache(0), setFirst(0), setLast(0), maxRHCB(0),
                     traceLvl(0), debugLvl(0), cioWait(0), cioTries(0),
                     useV4(false), xLfn2Pfn(false), xPfn2Lfn(xP2Loff),
                     xNameLib(false),
                     LocalRoot(0), RemotRoot(0), N2NLib(0), N2NParms(0),
                     cPath(0), cParm(0), mPath(0), mParm(0),
                     myVersion(vInfo) {}
         ~XrdOucPsx();

private:

char              *LocalRoot;// -> Local  n2n root, if any
char              *RemotRoot;// -> Remote n2n root, if any
char              *N2NLib;   // -> Name2Name Library Path
char              *N2NParms; // -> Name2Name Object Parameters
char              *cPath;    // -> Cache path
char              *cParm;    // -> Cache parameters
char              *mPath;    // -> CCM   path
char              *mParm;    // -> CCM   parameters
XrdVersionInfo    *myVersion;// -> Compilation version

bool   ConfigCache(XrdSysError &eDest);
bool   ConfigN2N(XrdSysError &eDest);
bool   LoadCCM(XrdSysError &eDest);
bool   Parse(char*, XrdOucStream&, XrdSysError&);
char  *ParseCache(XrdSysError *Eroute, XrdOucStream &Config, char *pBuff);
void   ParseSet(const char *kword, int kval);
void   WarnConfig(XrdSysError &eDest, XrdOucTList *tList, bool fatal);
void   WarnPlugin(XrdSysError &eDest, XrdOucTList *tList,
                  const char  *txt1,   const char  *txt2);
};
#endif
