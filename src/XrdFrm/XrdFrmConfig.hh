#ifndef _XRD_FRMCONFIG_H
#define _XRD_FRMCONFIG_H
/******************************************************************************/
/*                                                                            */
/*                       X r d F r m C o n f i g . h h                        */
/*                                                                            */
/* (C) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>
#include <unistd.h>

#include "XrdOss/XrdOssSpace.hh"

class XrdCks;
class XrdNetCmsNotify;
class XrdOfsConfigPI;
class XrdOss;
class XrdOucMsubs;
class XrdOucName2Name;
class XrdOucProg;
class XrdOucStream;
class XrdOucTList;
class XrdSysLogger;

class XrdFrmConfigSE;

struct XrdVersionInfo;
struct stat;

class XrdFrmConfig
{
public:

const char         *myProg;
const char         *myName;
const char         *myInst;
const char         *mySite;
const char         *myFrmid;
const char         *myFrmID;
const char         *lockFN;
char               *AdminPath;
char               *QPath;
char               *PidPath;
char               *myInstance;
char               *StopPurge;
char               *MSSCmd;
XrdOucProg         *MSSProg;

struct Cmd
      {const char  *Desc;
       char        *theCmd;
       XrdOucMsubs *theVec;
       int          TLimit;
       int          Opts;
      }             xfrCmd[4];
static const int    cmdAlloc = 0x0001;
static const int    cmdMDP   = 0x0002;
static const int    cmdStats = 0x0004;
static const int    cmdXPD   = 0x0008;
static const int    cmdRME   = 0x0010;

int                 xfrIN;
int                 xfrOUT;

XrdOfsConfigPI     *OfsCfg;    // -> Plugin Configurator
XrdCks             *CksMan;    // -> Checksum Manager
XrdOucName2Name    *the_N2N;   // -> File mapper object
XrdOss             *ossFS;
XrdNetCmsNotify    *cmsPath;
uid_t               myUid;
gid_t               myGid;
long long           cmdFree;
int                 cmdHold;
int                 AdminMode;
int                 isAgent;
int                 xfrMax;
int                 FailHold;
int                 IdleHold;
int                 WaitQChk;
int                 WaitPurge;
int                 WaitMigr;
int                 haveCMS;
int                 isOTO;
int                 Fix;
int                 Test;
int                 TrackDC;
int                 Verbose;
int                 runOld;    // Backward compatability
int                 runNew;    // Forward  compatability
int                 nonXA;     // Backward compatability for noXA spaces
int                 hasCache;  // Backward compatability for noXA spaces
char              **vectArg;
int                 nextArg;
int                 numcArg;

struct VPInfo
      {VPInfo      *Next;
       char        *Name;
       XrdOucTList *Dir;
       int          Val;
                    VPInfo(char *n, int m=0, struct VPInfo *p=0)
                          : Next(p), Name(strdup(n)), Dir(0), Val(m) {}
                   ~VPInfo() {} // Deletes are not important
      }            *VPList;
VPInfo             *pathList;   // Migr/Purg list of paths
XrdOucTList        *spacList;   // Migr/Purg list of spaces

struct Policy
      {long long minFree;
       long long maxFree;
       int       Hold;
       int       Ext;
       Policy   *Next;
       char      Sname[XrdOssSpace::minSNbsz];
                 Policy(const char *snv, long long minV, long long maxV,
                        int hV, int xV) : minFree(minV), maxFree(maxV),
                        Hold(hV), Ext(xV), Next(0) {strcpy(Sname, snv);}
                ~Policy() {}
      };
Policy           dfltPolicy;

int              dirHold;
int              pVecNum;     // Number of policy variables
static const int pVecMax=8;
char             pVec[pVecMax];
char            *pProg;
char            *xfrFdir;
int              xfrFdln;

enum  PPVar {PP_atime=0, PP_ctime, PP_fname, PP_fsize, PP_fspace,
             PP_mtime,   PP_pfn,   PP_sname, PP_tspace, PP_usage};

int          Configure(int argc, char **argv, int (*ppf)());

int          LocalPath  (const char *oldp, char *newp, int newpsz);

int          LogicalPath(const char *oldp, char *newp, int newpsz);

int          NeedsCTA(const char *Lfn);

unsigned
long long    PathOpts(const char *Lfn);

int          RemotePath (const char *oldp, char *newp, int newpsz);

XrdOucTList *Space(const char *Name, const char *Path=0);

int          Stat(const char *xLfn, const char *xPfn, struct stat *buff);

enum  SubSys {ssAdmin, ssMigr, ssPstg, ssPurg, ssXfr};

      XrdFrmConfig(SubSys ss, const char *vopts, const char *uinfo);
     ~XrdFrmConfig() {}

private:
XrdOucMsubs *ConfigCmd(const char *cname, char *cdata);
int          ConfigMum(XrdFrmConfigSE &theSE);
int          ConfigN2N();
int          ConfigMon(int isxfr);
int          ConfigMP(const char *);
int          ConfigMss();
int          ConfigOTO(char *Parms);
int          ConfigPaths();
void         ConfigPF(const char *pFN);
int          ConfigProc();
int          ConfigXeq(char *var, int mbok);
int          ConfigXfr();
int          getTime(const char *, const char *, int *, int mnv=-1, int mxv=-1);
int          Grab(const char *var, char **Dest, int nosubs);
XrdOucTList *InsertPL(XrdOucTList *pP, const char *Path, int Plen, int isRW);
void         InsertXD(const char *Path);
void         Usage(int rc);
int          xapath();
int          xcks();
int          xcnsd();
int          xcopy();
int          xcopy(int &TLim);
int          xcmax();
int          xdpol();
int          xitm(const char *What, int &tDest);
int          xnml();
int          xmon();
int          xpol();
int          xpolprog();
int          xqchk();
int          xsit();
int          xspace(int isPrg=0, int isXA=1);
void         xspaceBuild(char *grp, char *fn, int isxa);
int          xxfr();

char               *ConfigFN;
char               *LocalRoot;
char               *RemoteRoot;
XrdOucStream       *cFile;
XrdVersionInfo     *myVersion;

bool                doStatPF;
int                 plnDTS;
const char         *pfxDTS;
const char         *vOpts;
const char         *uInfo;
char               *N2N_Lib;   // -> Name2Name Library Path
char               *N2N_Parms; // -> Name2Name Object Parameters
XrdOucName2Name    *lcl_N2N;   // -> File mapper for local  files
XrdOucName2Name    *rmt_N2N;   // -> File mapper for remote files
SubSys              ssID;
};
namespace XrdFrm
{
extern XrdFrmConfig Config;
}
#endif
