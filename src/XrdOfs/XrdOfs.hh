#ifndef __OFS_API_H__
#define __OFS_API_H__
/******************************************************************************/
/*                                                                            */
/*                             X r d O f s . h h                              */
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

#include <cstring>
#include <dirent.h>
#include <sys/types.h>
  
#include "XrdOfs/XrdOfsEvr.hh"
#include "XrdOfs/XrdOfsHandle.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdCms/XrdCmsClient.hh"

class XrdNetIF;
class XrdOfsEvs;
class XrdOfsPocq;
class XrdOfsPrepare;
class XrdOss;
class XrdOssDF;
class XrdOssDir;
class XrdOucEnv;
class XrdOucPListAnchor;
class XrdSysError;
class XrdSysLogger;
class XrdOucStream;
class XrdSfsAio;

struct XrdVersionInfo;

/******************************************************************************/
/*                       X r d O f s D i r e c t o r y                        */
/******************************************************************************/
  
class XrdOfsDirectory : public XrdSfsDirectory
{
public:

        int         open(const char              *dirName,
                         const XrdSecEntity      *client,
                         const char              *opaque = 0);

        const char *nextEntry();

        int         close();

inline  void        copyError(XrdOucErrInfo &einfo) {einfo = error;}

const   char       *FName() {return (const char *)fname;}

        int         autoStat(struct stat *buf);

                    XrdOfsDirectory(XrdOucErrInfo &eInfo, const char *user)
                          : XrdSfsDirectory(eInfo), tident(user ? user : ""),
                            fname(0), dp(0), atEOF(0) {}

virtual            ~XrdOfsDirectory() {if (dp) close();}

protected:
const char    *tident;
char          *fname;
XrdOssDF      *dp;
int            atEOF;
char           dname[MAXNAMLEN];
};

class XrdOfsDirFull : public XrdOfsDirectory
{
public:
                    XrdOfsDirFull(const char *user, int MonID)
                          : XrdOfsDirectory(myEInfo, user), myEInfo(user, MonID)
                          {}

virtual            ~XrdOfsDirFull() {}

private:
XrdOucErrInfo  myEInfo; // Accessible only by reference error
};

/******************************************************************************/
/*                            X r d O f s F i l e                             */
/******************************************************************************/

class XrdOfsTPC;
class XrdOucChkPnt;
  
class XrdOfsFile : public XrdSfsFile
{
public:

        int            open(const char                *fileName,
                                  XrdSfsFileOpenMode   openMode,
                                  mode_t               createMode,
                            const XrdSecEntity        *client,
                            const char                *opaque = 0);

        int            checkpoint(XrdSfsFile::cpAct act,
                                  struct iov *range=0, int n=0);

        int            close();

        using          XrdSfsFile::fctl;

        int            fctl(const int               cmd,
                            const char             *args,
                                  XrdOucErrInfo    &out_error);

        int            fctl(const int               cmd,
                                  int               alen,
                                  const char       *args,
                            const XrdSecEntity     *client = 0);

        const char    *FName() {return (oh ? oh->Name() : "?");}

        int            getMmap(void **Addr, off_t &Size);

        XrdSfsXferSize pgRead(XrdSfsFileOffset   offset,
                              char              *buffer,
                              XrdSfsXferSize     rdlen,
                              uint32_t          *csvec,
                              uint64_t           opts=0);

        int            pgRead(XrdSfsAio *aioparm, uint64_t opts=0);


        XrdSfsXferSize pgWrite(XrdSfsFileOffset   offset,
                               char              *buffer,
                               XrdSfsXferSize     wrlen,
                               uint32_t          *csvec,
                               uint64_t           opts=0);

        int            pgWrite(XrdSfsAio *aioparm, uint64_t opts=0);


        int            read(XrdSfsFileOffset   fileOffset,   // Preread only
                            XrdSfsXferSize     amount);

        XrdSfsXferSize read(XrdSfsFileOffset   fileOffset,
                            char              *buffer,
                            XrdSfsXferSize     buffer_size);

        XrdSfsXferSize readv(XrdOucIOVec      *readV,
                             int               readCount);

        int            read(XrdSfsAio *aioparm);

        XrdSfsXferSize write(XrdSfsFileOffset   fileOffset,
                             const char        *buffer,
                             XrdSfsXferSize     buffer_size);

        int            write(XrdSfsAio *aioparm);

        int            sync();

        int            sync(XrdSfsAio *aiop);

        int            stat(struct stat *buf);

        int            truncate(XrdSfsFileOffset   fileOffset);

        int            getCXinfo(char cxtype[4], int &cxrsz);

                       XrdOfsFile(XrdOucErrInfo &eInfo, const char *user);

                      ~XrdOfsFile() {viaDel = 1; if (oh) close();}

protected:

const char    *tident;
XrdOfsHandle  *oh;
XrdOfsTPC     *myTPC;
XrdOucChkPnt  *myCKP;
int            dorawio;
char           viaDel;
bool           ckpBad;

private:

void           GenFWEvent();
int            CreateCKP();
};

class XrdOfsFileFull : public XrdOfsFile
{
public:
                    XrdOfsFileFull(const char *user, int MonID)
                          : XrdOfsFile(myEInfo, user), myEInfo(user, MonID)
                          {}

virtual            ~XrdOfsFileFull() {}

private:
XrdOucErrInfo  myEInfo; // Accessible only by reference error
};

/******************************************************************************/
/*                          C l a s s   X r d O f s                           */
/******************************************************************************/

class XrdAccAuthorize;
class XrdCks;
class XrdCmsClient;
class XrdOfsConfigPI;
class XrdOfsFSctl_PI;
class XrdOfsPoscq;
class XrdSfsFACtl;
  
class XrdOfs : public XrdSfsFileSystem
{
friend class XrdOfsDirectory;
friend class XrdOfsFile;

public:

// Object allocation
//
        XrdSfsDirectory *newDir(char *user=0, int MonID=0)
                        {return new XrdOfsDirFull(user, MonID);}

        XrdSfsDirectory *newDir(XrdOucErrInfo &eInfo)
                        {return new XrdOfsDirectory(eInfo, eInfo.getErrUser());}

        XrdSfsFile      *newFile(char *user=0,int MonID=0)
                        {return new XrdOfsFileFull(user, MonID);}

        XrdSfsFile      *newFile(XrdOucErrInfo &eInfo)
                        {return new XrdOfsFile(eInfo, eInfo.getErrUser());}

// Other functions
//
        int            chksum(      csFunc            Func,
                              const char             *csName,
                              const char             *Path,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecEntity     *client = 0,
                              const char             *opaque = 0);

        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecEntity     *client,
                             const char             *opaque = 0);

        void           Connect(const XrdSecEntity     *client = 0);

        void           Disc(const XrdSecEntity *client = 0);

        int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &out_error,
                              const XrdSecEntity        *client,
                              const char                *opaque = 0);

        int            FAttr(      XrdSfsFACtl      *faReq,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client = 0);

        int            FSctl(const int               cmd,
                                   XrdSfsFSctl      &args,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client = 0);

        int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecEntity     *client = 0);

        int            getStats(char *buff, int blen);

const   char          *getVersion();

        int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecEntity     *client,
                             const char             *opaque = 0);

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecEntity     *client = 0);

        int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecEntity     *client,
                           const char             *info = 0)
                          {return remove('f', path, out_error, client, info);}

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecEntity     *client,
                              const char             *info = 0)
                             {return remove('d',dirName,out_error,client,info);}

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecEntity     *client,
                              const char             *infoO = 0,
                               const char            *infoN = 0);

        int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecEntity     *client,
                            const char             *opaque = 0);

        int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecEntity     *client,
                            const char             *opaque = 0);

        int            truncate(const char             *Name,
                                      XrdSfsFileOffset fileOffset,
                                      XrdOucErrInfo    &out_error,
                                const XrdSecEntity     *client = 0,
                                const char             *opaque = 0);
// Management functions
//
virtual int            Configure(XrdSysError &); // Backward Compatibility

virtual int            Configure(XrdSysError &, XrdOucEnv *);

        void           Config_Cluster(XrdOss *);

        void           Config_Display(XrdSysError &);

                       XrdOfs();
virtual               ~XrdOfs() {}  // Too complicate to delete :-)

/******************************************************************************/
/*                  C o n f i g u r a t i o n   V a l u e s                   */
/******************************************************************************/
  
// Configuration values for this filesystem
//
enum {Authorize = 0x0001,    // Authorization wanted
      XAttrPlug = 0x0002,    // Extended Attribute Plugin
      isPeer    = 0x0050,    // Role peer
      isProxy   = 0x0020,    // Role proxy
      isManager = 0x0040,    // Role manager
      isServer  = 0x0080,    // Role server
      isSuper   = 0x00C0,    // Role supervisor
      isMeta    = 0x0100,    // Role meta + above
      haveRole  = 0x01F0,    // A role is present
      Forwarding= 0x1000,    // Fowarding wanted
      ThirdPC   = 0x2000,    // This party copy wanted
      SubCluster= 0x4000,    // all.subcluster directive encountered
      RdrTPC    = 0x8000
     };                      // These are set in Options below

int   Options;               // Various options
int   myPort;                // Port number being used

// TPC related things
//
char             *tpcRdrHost[2];  // TPC redirect target or null if none
int               tpcRdrPort[2];  // TPC redirect target port number

// Networking
//
XrdNetIF *myIF;

// Forward options
//
struct fwdOpt
      {const char *Cmd;
             char *Host;
             int   Port;
             void  Reset() {Cmd = 0; Port = 0;
                            if (Host) {free(Host); Host = 0;}
                           }
                   fwdOpt() : Cmd(0), Host(0), Port(0) {}
                  ~fwdOpt() {}
      };

struct fwdOpt fwdCHMOD;
struct fwdOpt fwdMKDIR;
struct fwdOpt fwdMKPATH;
struct fwdOpt fwdMV;
struct fwdOpt fwdRM;
struct fwdOpt fwdRMDIR;
struct fwdOpt fwdTRUNC;

static int MaxDelay;  //    Max delay imposed during staging
static int OSSDelay;  //    Delay to impose when oss interface times out

char *ConfigFN;       //    ->Configuration filename

/******************************************************************************/
/*                       P r o t e c t e d   I t e m s                        */
/******************************************************************************/

protected:

XrdOfsEvr     evrObject;      // Event receiver
XrdCmsClient *Finder;         // ->Cluster Management Service

virtual int   ConfigXeq(char *var, XrdOucStream &, XrdSysError &);
static  int   Emsg(const char *, XrdOucErrInfo  &, int, const char *x,
                   XrdOfsHandle *hP);
static  int   Emsg(const char *, XrdOucErrInfo  &, int, const char *x,
                   const char *y="");
static  int   fsError(XrdOucErrInfo &myError, int rc);
const char   *Split(const char *Args, const char **Opq, char *Path, int Plen);
        int   Stall(XrdOucErrInfo  &, int, const char *);
        void  Unpersist(XrdOfsHandle *hP, int xcev=1);
        char *WaitTime(int, char *, int);

/******************************************************************************/
/*                 P r i v a t e   C o n f i g u r a t i o n                  */
/******************************************************************************/

private:
  
char             *myRole;
XrdOfsFSctl_PI   *FSctl_PI;       //    ->FSctl plugin
XrdAccAuthorize  *Authorization;  //    ->Authorization   Service
XrdCmsClient     *Balancer;       //    ->Cluster Local   Interface
XrdOfsEvs        *evsObject;      //    ->Event Notifier
XrdOucPListAnchor*ossRPList;      //    ->Oss exoprt list

XrdOfsPoscq      *poscQ;          //    -> poscQ if  persist on close enabled
char             *poscLog;        //    -> Directory for posc recovery log
int               poscHold;       //       Seconds to hold a forced close
short             poscSync;       //       Number of requests before sync
signed char       poscAuto;       //  1 -> Automatic persist on close

char              ossRW;          // The oss r/w capability

XrdOfsConfigPI   *ofsConfig;      // Plugin   configurator
XrdOfsPrepare    *prepHandler;    // Plugin   prepare
XrdCks           *Cks;            // Checksum manager
bool              CksPfn;         // Checksum needs a pfn
bool              CksRdr;         // Checksum may be redirected (i.e. not local)
bool              prepAuth;       // Prepare requires authorization
char              OssIsProxy;     // !0 if we detect the oss plugin is a proxy
char              myRType[4];     // Role type for consistency with the cms

uint64_t          ossFeatures;    // The oss features

int               usxMaxNsz;      // Maximum length of attribute name
int               usxMaxVsz;      // Maximum length of attribute value

static XrdOfsHandle     *dummyHandle;
XrdSysMutex              ocMutex; // Global mutex for open/close

bool              DirRdr;         // Opendir() can be redirected.
bool              reProxy;        // Reproxying required for TPC
bool              OssHasPGrw;     // True: oss implements full rgRead/Write

/******************************************************************************/
/*                            O t h e r   D a t a                             */
/******************************************************************************/

// Internal file attribute methods
//
int ctlFADel(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo);
int ctlFAGet(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo);
int ctlFALst(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo);
int ctlFASet(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo);

// Common functions
//
int   remove(const char type, const char *path, XrdOucErrInfo &out_error,
             const XrdSecEntity *client, const char *opaque);

// Function used during Configuration
//
int           ConfigDispFwd(char *buff, struct fwdOpt &Fwd);
int           ConfigPosc(XrdSysError &Eroute);
int           ConfigRedir(XrdSysError &Eroute, XrdOucEnv *EnvInfo);
int           ConfigTPC(XrdSysError &Eroute, XrdOucEnv *EnvInfo);
int           ConfigTPC(XrdSysError &Eroute);
char         *ConfigTPCDir(XrdSysError &Eroute, const char *sfx,
                                                const char *xPath=0);
const char   *Fname(const char *);
int           Forward(int &Result, XrdOucErrInfo &Resp, struct fwdOpt &Fwd,
                      const char *arg1=0, const char *arg2=0,
                      XrdOucEnv  *Env1=0, XrdOucEnv  *Env2=0);
int           FSctl(XrdOfsFile &file, int cmd, int alen, const char *args,
                    const XrdSecEntity *client);
int           Reformat(XrdOucErrInfo &);
const char   *theRole(int opts);
int           xcrds(XrdOucStream &, XrdSysError &);
int           xdirl(XrdOucStream &, XrdSysError &);
int           xexp(XrdOucStream &, XrdSysError &, bool);
int           xforward(XrdOucStream &, XrdSysError &);
int           xmaxd(XrdOucStream &, XrdSysError &);
int           xnmsg(XrdOucStream &, XrdSysError &);
int           xnot(XrdOucStream &, XrdSysError &);
int           xpers(XrdOucStream &, XrdSysError &);
int           xrole(XrdOucStream &, XrdSysError &);
int           xtpc(XrdOucStream &, XrdSysError &);
int           xtpcal(XrdOucStream &, XrdSysError &);
int           xtpcr(XrdOucStream &, XrdSysError &);
int           xtrace(XrdOucStream &, XrdSysError &);
int           xatr(XrdOucStream &, XrdSysError &);
};
#endif
