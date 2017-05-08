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

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "XrdSys/XrdSysHeaders.hh"
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
int     Close(long long *retsz=0);
int     Opendir(const char *, XrdOucEnv &);
int     Readdir(char *buff, int blen);

        // Constructor and destructor
        XrdPssDir(const char *tid) : tident(tid), myDir(0) {}
       ~XrdPssDir() {if (myDir) Close();}
private:
const    char      *tident;
         DIR       *myDir;
};
  
/******************************************************************************/
/*                            X r d P s s F i l e                             */
/******************************************************************************/

struct XrdOucIOVec;
class  XrdSfsAio;
  
class XrdPssFile : public XrdOssDF
{
public:

// The following two are virtual functions to allow for upcasting derivations
// of this implementation
//
virtual int     Close(long long *retsz=0);
virtual int     Open(const char *, int, mode_t, XrdOucEnv &);

int     Fstat(struct stat *);
int     Fsync();
int     Fsync(XrdSfsAio *aiop);
int     Ftruncate(unsigned long long);
off_t   getMmap(void **addr);
int     isCompressed(char *cxidp=0);
ssize_t Read(               off_t, size_t);
ssize_t Read(       void *, off_t, size_t);
int     Read(XrdSfsAio *aiop);
ssize_t ReadV(XrdOucIOVec *readV, int n);
ssize_t ReadRaw(    void *, off_t, size_t);
ssize_t Write(const void *, off_t, size_t);
int     Write(XrdSfsAio *aiop);
 
         // Constructor and destructor
         XrdPssFile(const char *tid) : tident(tid) {fd = -1;}

virtual ~XrdPssFile() {if (fd >= 0) Close();}

private:

const char *tident;
      int   crOpts;
};

/******************************************************************************/
/*                             X r d P s s S y s                              */
/******************************************************************************/
  
class XrdNetSecurity;
class XrdOucEnv;
class XrdSysError;
class XrdOucStream;
class XrdOucTList;

struct XrdVersionInfo;

class XrdPssSys : public XrdOss
{
public:
virtual XrdOssDF *newDir(const char *tident)
                       {return (XrdOssDF *)new XrdPssDir(tident);}
virtual XrdOssDF *newFile(const char *tident)
                       {return (XrdOssDF *)new XrdPssFile(tident);}

int       Chmod(const char *, mode_t mode, XrdOucEnv *eP=0);
virtual
int       Create(const char *, const char *, mode_t, XrdOucEnv &, int opts=0);
void      EnvInfo(XrdOucEnv *envP);
int       Init(XrdSysLogger *, const char *);
int       Lfn2Pfn(const char *Path, char *buff, int blen);
const
char     *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc);
int       Mkdir(const char *, mode_t mode, int mkpath=0, XrdOucEnv *eP=0);
int       Remdir(const char *, int Opts=0, XrdOucEnv *eP=0);
int       Rename(const char *, const char *,
                 XrdOucEnv *eP1=0, XrdOucEnv *eP2=0);
int       Stat(const char *, struct stat *, int opts=0, XrdOucEnv *eP=0);
int       Truncate(const char *, unsigned long long, XrdOucEnv *eP=0);
int       Unlink(const char *, int Opts=0, XrdOucEnv *eP=0);

static const int    PolNum = 2;
enum   PolAct {PolPath = 0, PolObj = 1};

static
const  char *P2CGI(int &cgilen, char *cbuff, int cblen,
                   const char *Cgi1, const char *Cgi2);
static int   P2DST(int &retc, char *hBuff, int hBlen, PolAct pType,
                   const char *path);
static char *P2ID (XrdOucSid::theSid *idVal, char *idBuff, int idBsz);
static char *P2OUT(int &retc,  char *pbuff, int pblen,
                   const char *path, const char *Cgi, const char *Ident);
static char *P2URL(int &retc, char *pbuff, int pblen,
                   const char *path,       int Split=0,
                   const char *Cgi=0,      int CgiLn=0,
                   const char *tIdent=0,   int doN2N=1);
static int   T2UID(const char *Ident);

static const char  *ConfigFN;       // -> Pointer to the config file name
static const char  *myHost;
static const char  *myName;
static uid_t        myUid;
static gid_t        myGid;
static
XrdOucPListAnchor   XPList;        // Exported path list

static XrdNetSecurity *Police[PolNum];
static XrdOucTList *ManList;
static const char  *urlPlain;
static int          urlPlen;
static int          hdrLen;
static const char  *hdrData;
static const char  *urlRdr;
static int          Streams;
static int          Workers;
static int          Trace;

static bool         outProxy; // True means outgoing proxy
static bool         pfxProxy; // True means outgoing proxy is prefixed

static char         allChmod;
static char         allMkdir;
static char         allMv;
static char         allRmdir;
static char         allRm;
static char         allTrunc;

static char         cfgDone;   // Configuration completed
static char        *sUrlPrefix; // URL prefix

         XrdPssSys();
virtual ~XrdPssSys() {}

private:

char              *LocalRoot;// -> Local n2n root, if any
char              *N2NLib;   // -> Name2Name Library Path
char              *N2NParms; // -> Name2Name Object Parameters
XrdOucName2Name   *theN2N;   // -> File mapper object
unsigned long long DirFlags; // Defaults for exports
char              *cPath;    // -> Cache path
char              *cParm;    // -> Cache parameters
XrdVersionInfo    *myVersion;// -> Compilation version
int                TraceLvl; // Tracing options


int    buildHdr();
int    Configure(const char *);
int    ConfigProc(const char *ConfigFN);
int    ConfigXeq(char*, XrdOucStream&);
int    ConfigN2N();
int    getCache();
const
char  *getDomain(const char *hName);
int    xcach(XrdSysError *Eroute, XrdOucStream &Config);
int    xcacl(XrdSysError *Eroute, XrdOucStream &Config);
char  *xcapr(XrdSysError *Eroute, XrdOucStream &Config, char *pBuff);
int    xconf(XrdSysError *Eroute, XrdOucStream &Config);
int    xdef( XrdSysError *Eroute, XrdOucStream &Config);
int    xexp( XrdSysError *Eroute, XrdOucStream &Config);
int    xinet(XrdSysError *errp,   XrdOucStream &Config);
int    xperm(XrdSysError *errp,   XrdOucStream &Config);
int    xorig(XrdSysError *errp,   XrdOucStream &Config);
int    xsopt(XrdSysError *Eroute, XrdOucStream &Config);
int    xtrac(XrdSysError *Eroute, XrdOucStream &Config);
int    xnml (XrdSysError *Eroute, XrdOucStream &Config);
int    xurlp(XrdSysError *Eroute, XrdOucStream& Config);
};
#endif
