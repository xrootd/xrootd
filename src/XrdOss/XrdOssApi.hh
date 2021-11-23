#ifndef _XRDOSS_API_H
#define _XRDOSS_API_H
/******************************************************************************/
/*                                                                            */
/*                          X r d O s s A p i . h h                           */
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

#include <sys/types.h>
#include <cerrno>
#include "XrdSys/XrdSysHeaders.hh"

#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssStatInfo.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                              o o s s _ D i r                               */
/******************************************************************************/

class XrdOssDir : public XrdOssDF
{
public:
int     Close(long long *retsz=0);
int     Opendir(const char *, XrdOucEnv &);
int     Readdir(char *buff, int blen);
int     StatRet(struct stat *buff);
int     getFD() {return fd;}

        // Constructor and destructor
        XrdOssDir(const char *tid, DIR *dP=0)
                 : XrdOssDF(tid, DF_isDir),
                   lclfd(dP), mssfd(0), Stat(0), ateof(false),
                   isopen(dP != 0), dOpts(0) {if (dP) fd = dirfd(dP);}

       ~XrdOssDir() {if (isopen) Close();}
private:
         DIR       *lclfd;
         void      *mssfd;
struct   stat      *Stat;
         bool       ateof;
         bool       isopen;
unsigned char       dOpts;
static const int    isStage  = 0x01;
static const int    noCheck  = 0x02;
static const int    noDread  = 0x04;
};
  
/******************************************************************************/
/*                             o o s s _ F i l e                              */
/******************************************************************************/

class oocx_CXFile;
class XrdSfsAio;
class XrdOssCache_FS;
class XrdOssMioFile;
  
class XrdOssFile : public XrdOssDF
{
public:

// The following two are virtual functions to allow for upcasting derivations
// of this implementation
//
virtual int     Close(long long *retsz=0);
virtual int     Open(const char *, int, mode_t, XrdOucEnv &);

int     Fchmod(mode_t mode);
int     Fctl(int cmd, int alen, const char *args, char **resp=0);
void    Flush();
int     Fstat(struct stat *);
int     Fsync();
int     Fsync(XrdSfsAio *aiop);
int     Ftruncate(unsigned long long);
int     getFD() {return fd;}
off_t   getMmap(void **addr);
int     isCompressed(char *cxidp=0);
ssize_t Read(               off_t, size_t);
ssize_t Read(       void *, off_t, size_t);
int     Read(XrdSfsAio *aiop);
ssize_t ReadV(XrdOucIOVec *readV, int);
ssize_t ReadRaw(    void *, off_t, size_t);
ssize_t Write(const void *, off_t, size_t);
int     Write(XrdSfsAio *aiop);
 
        // Constructor and destructor
        XrdOssFile(const char *tid, int fdnum=-1)
                  : XrdOssDF(tid, DF_isFile, fdnum),
                    cxobj(0), cacheP(0), mmFile(0),
                    rawio(0), cxpgsz(0) {cxid[0] = '\0';}

virtual ~XrdOssFile() {if (fd >= 0) Close();}

private:
int     Open_ufs(const char *, int, int, unsigned long long);

static int      AioFailure;
oocx_CXFile    *cxobj;
XrdOssCache_FS *cacheP;
XrdOssMioFile  *mmFile;
long long       FSize;
int             rawio;
int             cxpgsz;
char            cxid[4];
};

/******************************************************************************/
/*                              o o s s _ S y s                               */
/******************************************************************************/
  
class XrdFrcProxy;
class XrdOssCache_Group;
class XrdOssCache_Space;
class XrdOssCreateInfo;
class XrdOucMsubs;
class XrdOucName2Name;
class XrdOucProg;
class XrdOssSpace;
class XrdOssStage_Req;

struct XrdVersionInfo;

class XrdOssSys : public XrdOss
{
public:
virtual XrdOssDF *newDir(const char *tident)
                       {return (XrdOssDF *)new XrdOssDir(tident);}
virtual XrdOssDF *newFile(const char *tident)
                       {return (XrdOssDF *)new XrdOssFile(tident);}

int       Chmod(const char *, mode_t mode, XrdOucEnv *eP=0);
int       Configure(const char *, XrdSysError &, XrdOucEnv *envP);
void      Config_Display(XrdSysError &);
virtual
int       Create(const char *, const char *, mode_t, XrdOucEnv &, int opts=0);
uint64_t  Features() {return XRDOSS_HASNAIO;} // Turn async I/O off for disk
int       GenLocalPath(const char *, char *);
int       GenRemotePath(const char *, char *);
int       Init(XrdSysLogger *, const char *, XrdOucEnv *envP);
int       Init(XrdSysLogger *lP, const char *cP) {return Init(lP, cP, 0);}
int       IsRemote(const char *path) 
                  {return (RPList.Find(path) & XRDEXP_REMOTE) != 0;}
int       Lfn2Pfn(const char *Path, char *buff, int blen);
const char *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc);
int       Mkdir(const char *, mode_t mode, int mkpath=0, XrdOucEnv *eP=0);
int       Mkpath(const char *, mode_t mode);
unsigned long long PathOpts(const char *path) {return RPList.Find(path);}
int       Reloc(const char *tident, const char *path,
                const char *cgName, const char *anchor=0);
int       Remdir(const char *, int Opts=0, XrdOucEnv *eP=0);  // In Unlink()
int       Rename(const char *, const char *,
                 XrdOucEnv *eP1=0, XrdOucEnv *eP2=0);
virtual 
int       Stage(const char *, const char *, XrdOucEnv &, int, mode_t, unsigned long long );
void     *Stage_In(void *carg);
int       Stat(const char *, struct stat *, int opts=0, XrdOucEnv *Env=0);
int       StatFS(const char *path, char *buff, int &blen, XrdOucEnv *Env=0);
int       StatFS(const char *path, unsigned long long &Opt,
                 long long &fSize, long long &fSpace);
int       StatLS(XrdOucEnv &env, const char *path, char *buff, int &blen);
int       StatPF(const char *, struct stat *, int);
int       StatVS(XrdOssVSInfo *sP, const char *sname=0, int updt=0);
int       StatXA(const char *path, char *buff, int &blen, XrdOucEnv *Env=0);
int       StatXP(const char *path, unsigned long long &attr, XrdOucEnv *Env=0);
int       Truncate(const char *, unsigned long long Size, XrdOucEnv *eP=0);
int       Unlink(const char *, int Opts=0, XrdOucEnv *eP=0);

int       Stats(char *bp, int bl);

static int   AioInit();
static int   AioAllOk;

static char  tryMmap;           // Memory mapped files enabled
static char  chkMmap;           // Memory mapped files are selective
   
int       MSS_Closedir(void *);
int       MSS_Create(const char *path, mode_t, XrdOucEnv &);
void     *MSS_Opendir(const char *, int &rc);
int       MSS_Readdir(void *fd, char *buff, int blen);
int       MSS_Remdir(const char *, const char *) {return -ENOTSUP;}
int       MSS_Rename(const char *, const char *);
int       MSS_Stat(const char *, struct stat *buff=0);
int       MSS_Unlink(const char *);

static const int MaxArgs = 15;

char     *ConfigFN;       // -> Pointer to the config file name
char     *LocalRoot;      // -> Path prefix for local  filename
char     *RemoteRoot;     // -> Path prefix for remote filename
int       MaxTwiddle;     //    Maximum seconds of internal wait
int       StageRealTime;  //    If 1, Invoke stage command on demand
int       StageAsync;     //    If 1, return EINPROGRESS to the caller
int       StageCreate;    //    If 1, use open path to create files
int       StageFormat;    //    Format for default stagecmd
char     *StageCmd;       // -> Staging command to use
char     *StageMsg;       // -> Staging message to be passed
XrdOucMsubs *StageSnd;    // -> Parsed Message
XrdFrcProxy *StageFrm;    // -> Built-in stagecmd or zero

char     *StageEvents;    // -> file:////<adminpath> if async staging
int       StageEvSize;    //    Length of above
int       StageActLen;    //    Length of below
char     *StageAction;    // -> "wq " if sync | "wfn " if async

char     *StageArg[MaxArgs];
int       StageAln[MaxArgs];
int       StageAnum;      //    Count of valid Arg/Aln array elements
char     *RSSCmd;         // -> Remote Storage Service Command
int       isMSSC;         //    RSSCmd is old-style msscmd
int       RSSTout;        //    RSSCmd response timeout
long long MaxSize;        //    Maximum file size (*obsolete*)
int       FDFence;        //    Smallest file FD number allowed
int       FDLimit;        //    Largest  file FD number allowed
unsigned long long DirFlags;//  Default directory settings
int       Trace;          //    Trace flags
int       Solitary;       //    True if running in stand-alone mode
int       OptFlags;       //    General option flags

XrdOucPListAnchor SPList;    // The path to space list
#define           spAssign 1

char             *N2N_Lib;   // -> Name2Name Library Path
char             *N2N_Parms; // -> Name2Name Object Parameters
XrdOucName2Name  *lcl_N2N;   // -> File mapper for local  files
XrdOucName2Name  *rmt_N2N;   // -> File mapper for remote files
XrdOucName2Name  *the_N2N;   // -> File mapper object
XrdOucPListAnchor RPList;    //    The real path list
OssDPath         *DPList;    //    The stat path list
int               lenDP;
short             numDP;
short             numCG;

char             *STT_Lib;   // -> StatInfo  Library Path
char             *STT_Parms; // -> StatInfo  Library Paramaters
union {
XrdOssStatInfo_t  STT_Func;
XrdOssStatInfo2_t STT_Fund;
      };
int               STT_PreOp;
char              STT_DoN2N;
char              STT_V2;
char              STT_DoARE;

long long         prPBits;   //    Page lo order bit mask
long long         prPMask;   //    Page hi order bit mask
int               prPSize;   //    preread page size
int               prBytes;   //    preread byte limit
int               prActive;  //    preread activity count
short             prDepth;   //    preread depth
short             prQSize;   //    preread maximum allowed

XrdVersionInfo   *myVersion; //    Compilation version set by constructor
   
         XrdOssSys();
virtual ~XrdOssSys() {}

protected:
// Cache management related data and methods
//
long long minalloc;          //    Minimum allocation
int       ovhalloc;          //    Allocation overage
int       fuzalloc;          //    Allocation fuzz
int       cscanint;          //    Seconds between cache scans
int       xfrspeed;          //    Average transfer speed (bytes/second)
int       xfrovhd;           //    Minimum seconds to get a file
int       xfrhold;           //    Second hold limit on failing requests
int       xfrkeep;           //    Second keep queued requests
int       xfrthreads;        //    Number of threads for staging
int       xfrtcount;         //    Actual count of threads (used for dtr)
long long pndbytes;          //    Total bytes to be staged (pending)
long long stgbytes;          //    Total bytes being staged (active)
long long totbytes;          //    Total bytes were  staged (active+pending)
int       totreqs;           //    Total   successful requests
int       badreqs;           //    Total unsuccessful requests

XrdOucProg     *StageProg;    //    Command or manager than handles staging
XrdOucProg     *RSSProg;      //    Command for Remote Storage Services

char           *UDir;         // -> Usage logdir
char           *QFile;        // -> Quota file
char           *xfrFdir;      // -> Fail file base dir
int             xfrFdln;      //    strlen(xfrFDir)
short           USync;        // Usage sync interval
bool            pfcMode;      // Setup for Proxy File Cache

int                Alloc_Cache(XrdOssCreateInfo &, XrdOucEnv &);
int                Alloc_Local(XrdOssCreateInfo &, XrdOucEnv &);
int                BreakLink(const char *local_path, struct stat &statbuff);
int                CalcTime();
int                CalcTime(XrdOssStage_Req *req);
int                SetFattr(XrdOssCreateInfo &crInfo, int datfd, time_t mtime);
void               doScrub();
int                Find(XrdOssStage_Req *req, void *carg);
int                getCname(const char *path, struct stat *sbuff, char *cgbuff);
int                getStats(char *buff, int blen);
int                GetFile(XrdOssStage_Req *req);
int                getID(const char *, XrdOucEnv &, char *, int);
time_t             HasFile(const char *fn, const char *sfx, time_t *mTime=0);
int                Stage_QT(const char *, const char *, XrdOucEnv &, int, mode_t);
int                Stage_RT(const char *, const char *, XrdOucEnv &, unsigned long long);

// Configuration related methods
//
void   ConfigCache(XrdSysError &Eroute, bool pass2=false);
void   ConfigMio(XrdSysError &Eroute);
int    ConfigN2N(XrdSysError &Eroute, XrdOucEnv *envP);
int    ConfigProc(XrdSysError &Eroute);
void   ConfigSpace(XrdSysError &Eroute);
void   ConfigSpace(const char *Lfn);
void   ConfigSpath(XrdSysError &Eroute, const char *Pn,
                   unsigned long long &Fv, int noMSS);
int    ConfigStage(XrdSysError &Eroute);
int    ConfigStageC(XrdSysError &Eroute);
int    ConfigStatLib(XrdSysError &Eroute, XrdOucEnv *envP);
void   ConfigStats(XrdSysError &Eroute);
void   ConfigStats(dev_t Devnum, char *lP);
int    ConfigXeq(char *, XrdOucStream &, XrdSysError &);
void   List_Path(const char *, const char *, unsigned long long, XrdSysError &);
int    xalloc(XrdOucStream &Config, XrdSysError &Eroute);
int    xcache(XrdOucStream &Config, XrdSysError &Eroute);
int    xcachescan(XrdOucStream &Config, XrdSysError &Eroute);
int    xdefault(XrdOucStream &Config, XrdSysError &Eroute);
int    xfdlimit(XrdOucStream &Config, XrdSysError &Eroute);
int    xmaxsz(XrdOucStream &Config, XrdSysError &Eroute);
int    xmemf(XrdOucStream &Config, XrdSysError &Eroute);
int    xnml(XrdOucStream &Config, XrdSysError &Eroute);
int    xpath(XrdOucStream &Config, XrdSysError &Eroute);
int    xprerd(XrdOucStream &Config, XrdSysError &Eroute);
int    xspace(XrdOucStream &Config, XrdSysError &Eroute, int *isCD=0);
int    xspace(XrdOucStream &Config, XrdSysError &Eroute,
              const char *grp, bool isAsgn);
int    xspaceBuild(OssSpaceConfig &sInfo, XrdSysError &Eroute);
int    xstg(XrdOucStream &Config, XrdSysError &Eroute);
int    xstl(XrdOucStream &Config, XrdSysError &Eroute);
int    xusage(XrdOucStream &Config, XrdSysError &Eroute);
int    xtrace(XrdOucStream &Config, XrdSysError &Eroute);
int    xxfr(XrdOucStream &Config, XrdSysError &Eroute);

// Mass storage related methods
//
int    tranmode(char *);
int    MSS_Xeq(XrdOucStream **xfd, int okerr,
               const char *cmd, const char *arg1=0, const char *arg2=0);

// Other methods
//
int    RenameLink(char *old_path, char *new_path);
int    RenameLink3(char *cPath, char *old_path, char *new_path);
};

/******************************************************************************/
/*                  A P I   S p e c i f i c   D e f i n e s                   */
/******************************************************************************/

// The Check_RO macro is valid only for XrdOssSys objects.
//
#define Check_RO(act, flags, path, opname) \
   XRDEXP_REMOTE & (flags = PathOpts(path)); \
   if (flags & XRDEXP_NOTRW) \
      return OssEroute.Emsg(#act, -XRDOSS_E8005, opname, path)

#define Check_RW(act, path, opname) \
   if (PathOpts(path) & XRDEXP_NOTRW) \
      return OssEroute.Emsg(#act, -XRDOSS_E8005, opname, path)
#endif
