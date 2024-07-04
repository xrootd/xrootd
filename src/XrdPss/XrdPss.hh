#ifndef _XRDPSS_API_H
#define _XRDPSS_API_H
/******************************************************************************/
/*                                                                            */
/*                             X r d P s s . h h                              */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdOuc/XrdOucECMsg.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdOuc/XrdOucSid.hh"
#include "XrdOss/XrdOss.hh"

/******************************************************************************/
/*                             X r d P s s D i r                              */
/******************************************************************************/

class XrdPssDir : public XrdOssDF
{
public:
int     Close(long long *retsz=0) override;
bool    getErrMsg(std::string& eText) override;
int     Opendir(const char *, XrdOucEnv &) override;
int     Readdir(char *buff, int blen) override;

// Store the `buf` pointer in the directory object.  Future calls to `Readdir`
// will, as a side-effect, fill in the corresponding `stat` information in
// the memory referred to from the pointer.
//
// Returns -errno on failure; otherwise, returns 0 and stashes away the pointer.
int     StatRet(struct stat *buf);

        // Constructor and destructor
        XrdPssDir(const char *tid)
                 : XrdOssDF(tid, XrdOssDF::DF_isDir|XrdOssDF::DF_isProxy),
                   myDir(0), lastEtrc(0) {}

       ~XrdPssDir() {if (myDir) Close();}
private:
         DIR       *myDir;
    std::string    lastEtext;
         int       lastEtrc;
};
  
/******************************************************************************/
/*                            X r d P s s F i l e                             */
/******************************************************************************/

struct XrdOucIOVec;
class  XrdSecEntity;
class  XrdSfsAio;
  
class XrdPssFile : public XrdOssDF
{
public:

// The following two are virtual functions to allow for upcasting derivations
// of this implementation
//
virtual int     Close(long long *retsz=0);
virtual int     Open(const char *, int, mode_t, XrdOucEnv &);

int     Fchmod(mode_t mode) {return XrdOssOK;}
int     Fstat(struct stat *);
int     Fsync();
int     Fsync(XrdSfsAio *aiop);
int     Ftruncate(unsigned long long);
bool    getErrMsg(std::string& eText) override;
ssize_t pgRead (void* buffer, off_t offset, size_t rdlen,
                uint32_t* csvec, uint64_t opts);
int     pgRead (XrdSfsAio* aioparm, uint64_t opts);
ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen,
                uint32_t* csvec, uint64_t opts);
int     pgWrite(XrdSfsAio* aioparm, uint64_t opts);
ssize_t Read(               off_t, size_t);
ssize_t Read(       void *, off_t, size_t);
int     Read(XrdSfsAio *aiop);
ssize_t ReadV(XrdOucIOVec *readV, int n);
ssize_t ReadRaw(    void *, off_t, size_t);
ssize_t Write(const void *, off_t, size_t);
int     Write(XrdSfsAio *aiop);
 
         // Constructor and destructor
         XrdPssFile(const char *tid)
                   : XrdOssDF(tid, XrdOssDF::DF_isFile|XrdOssDF::DF_isProxy),
                     rpInfo(0), tpcPath(0), entity(0), lastEtrc(0) {}

virtual ~XrdPssFile() {if (fd >= 0) Close();
                       if (rpInfo) delete(rpInfo);
                       if (tpcPath) free(tpcPath);
                      }

private:

struct tprInfo
      {char  *tprPath;
       char  *dstURL;
       size_t fSize;

              tprInfo(const char *fn) : tprPath(strdup(fn)),dstURL(0),fSize(0)
                                        {}
             ~tprInfo() {if (tprPath) free(tprPath);
                         if (dstURL)  free(dstURL);
                        }
      } *rpInfo;

      char         *tpcPath;
const XrdSecEntity *entity;
std::string         lastEtext;
int                 lastEtrc;
};

/******************************************************************************/
/*                             X r d P s s S y s                              */
/******************************************************************************/
  
class XrdNetSecurity;
class XrdOucEnv;
class XrdOucStream;
class XrdOucTList;
class XrdPssUrlInfo;
class XrdSecsssID;
class XrdSysError;

struct XrdVersionInfo;

class XrdPssSys : public XrdOss
{
public:
virtual XrdOssDF *newDir(const char *tident) override
                       {return (XrdOssDF *)new XrdPssDir(tident);}
virtual XrdOssDF *newFile(const char *tident) override
                       {return (XrdOssDF *)new XrdPssFile(tident);}

virtual void      Connect(XrdOucEnv &) override;

virtual void      Disc(XrdOucEnv &) override;

int       Chmod(const char *, mode_t mode, XrdOucEnv *eP=0) override;
bool      ConfigMapID();
virtual
int       Create(const char *, const char *, mode_t, XrdOucEnv &, int opts=0) override;
void      EnvInfo(XrdOucEnv *envP) override;
uint64_t  Features() override {return myFeatures;}
bool      getErrMsg(std::string& eText) override;
int       Init(XrdSysLogger *, const char *) override {return -ENOTSUP;}
int       Init(XrdSysLogger *, const char *, XrdOucEnv *envP) override;
int       Lfn2Pfn(const char *Path, char *buff, int blen) override;
const
char     *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc) override;
int       Mkdir(const char *, mode_t mode, int mkpath=0, XrdOucEnv *eP=0) override;
int       Remdir(const char *, int Opts=0, XrdOucEnv *eP=0) override;
int       Rename(const char *, const char *,
                 XrdOucEnv *eP1=0, XrdOucEnv *eP2=0) override;
int       Stat(const char *, struct stat *, int opts=0, XrdOucEnv *eP=0) override;
int       Stats(char *bp, int bl) override;
int       Truncate(const char *, unsigned long long, XrdOucEnv *eP=0) override;
int       Unlink(const char *, int Opts=0, XrdOucEnv *eP=0) override;

static const int    PolNum = 2;
enum   PolAct {PolPath = 0, PolObj = 1};

static int   Info(int rc);
static int   P2DST(int &retc, char *hBuff, int hBlen, PolAct pType,
                   const char *path);
static int   P2OUT(char *pbuff, int pblen, XrdPssUrlInfo &uInfo);
static int   P2URL(char *pbuff, int pblen, XrdPssUrlInfo &uInfo,
                   bool doN2N=true);

static const char  *ConfigFN;       // -> Pointer to the config file name
static const char  *myHost;
static const char  *myName;
static
XrdOucPListAnchor   XPList;        // Exported path list

static XrdNetSecurity *Police[PolNum];
static XrdOucTList *ManList;
static       char  *fileOrgn;
static const char  *protName;
static const char  *hdrData;
static int          hdrLen;
static int          Streams;
static int          Workers;
static int          Trace;
static int          dcaCTime;

static bool         xLfn2Pfn;
static bool         dcaCheck;
static bool         dcaWorld;
static bool         deferID;  // Defer ID mapping until needed
static bool         reProxy;  // TPC requires reproxing

         XrdPssSys();
virtual ~XrdPssSys() {}

private:

char              *HostArena;// -> path qualification for remote origins
char              *LocalRoot;// -> pss Local n2n root, if any
XrdOucName2Name   *theN2N;   // -> File mapper object
unsigned long long DirFlags; // Defaults for exports
XrdVersionInfo    *myVersion;// -> Compilation version
XrdSecsssID       *idMapper; // -> Auth ID mapper
uint64_t          myFeatures;// Our feature set

int    Configure(const char *, XrdOucEnv *);
int    ConfigProc(const char *ConfigFN);
int    ConfigXeq(char*, XrdOucStream&);
int    xconf(XrdSysError *Eroute, XrdOucStream &Config);
int    xdef( XrdSysError *Eroute, XrdOucStream &Config);
int    xdca( XrdSysError *errp,   XrdOucStream &Config);
int    xexp( XrdSysError *Eroute, XrdOucStream &Config);
int    xperm(XrdSysError *errp,   XrdOucStream &Config);
int    xpers(XrdSysError *errp,   XrdOucStream &Config);
int    xorig(XrdSysError *errp,   XrdOucStream &Config);
};
#endif
