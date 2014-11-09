#ifndef __CMS_FINDER__
#define __CMS_FINDER__
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s F i n d e r . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdCms/XrdCmsClient.hh"

#include "XrdSys/XrdSysPthread.hh"

class  XrdCmsClientMan;
class  XrdOss;
class  XrdOucEnv;
class  XrdOucErrInfo;
class  XrdOucTList;
struct XrdCmsData;
class  XrdCmsRRData;
struct XrdSfsPrep;
class  XrdSysLogger;
struct XrdVersionInfo;

/******************************************************************************/
/*                         R e m o t e   F i n d e r                          */
/******************************************************************************/

class XrdCmsFinderRMT : public XrdCmsClient
{
public:
        void   Added(const char *path, int Pend=0) {}

        int    Configure(const char *cfn, char *Args, XrdOucEnv *EnvInfo);

        int    Forward(XrdOucErrInfo &Resp, const char *cmd,
                       const char *arg1=0,  const char *arg2=0,
                       XrdOucEnv  *Env1=0,  XrdOucEnv  *Env2=0);

        int    Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                      XrdOucEnv *Info=0);

XrdOucTList   *Managers() {return myManList;}

        int    Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs,
                       XrdOucEnv *Info=0);

        void   Removed(const char *path) {}

        void   setSS(XrdOss *thess) {}

        int    Space(XrdOucErrInfo &Resp, const char *path, XrdOucEnv *Info=0);

static  bool   VCheck(XrdVersionInfo &urVersion);

               XrdCmsFinderRMT(XrdSysLogger *lp, int whoami=0, int Port=0);
              ~XrdCmsFinderRMT();

static const int MaxMan = 15;

private:
int              Decode(char **resp);
void             Inform(XrdCmsClientMan *xman, struct iovec xmsg[], int xnum);
int              LocLocal(XrdOucErrInfo &Resp, XrdOucEnv *Env);
XrdCmsClientMan *SelectManager(XrdOucErrInfo &Resp, const char *path);
void             SelectManFail(XrdOucErrInfo &Resp);
int              send2Man(XrdOucErrInfo &, const char *, struct iovec *, int);
int              StartManagers(XrdOucTList *);

XrdCmsClientMan *myManTable[MaxMan];
XrdCmsClientMan *myManagers;
XrdOucTList     *myManList;
int              myManCount;
XrdSysMutex      myData;
char            *CMSPath;
int              ConWait;
int              RepDelay;
int              RepNone;
int              RepWait;
int              FwdWait;
int              PrepWait;
int              isMeta;
int              isProxy;
int              isTarget;
int              myPort;
unsigned char    SMode;
unsigned char    sendID;
unsigned char    savePath;
};

/******************************************************************************/
/*                         T a r g e t   F i n d e r                          */
/******************************************************************************/

class XrdOucStream;
class XrdOucTList;
  
class XrdCmsFinderTRG : public XrdCmsClient
{
public:
        void   Added(const char *path, int Pend=0);

        int    Configure(const char *cfn, char *Args, XrdOucEnv *EnvInfo);

        int    Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                      XrdOucEnv *Info=0);

        int    Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs,
                       XrdOucEnv *Info=0) {return 0;}

XrdOucTList   *Managers() {return myManList;}

        void   Removed(const char *path);

        void   Resume (int Perm=1);
        void   Suspend(int Perm=1);

        int    Resource(int n);
        int    Reserve (int n);
        int    Release (int n);

        int    RunAdmin(char *Path);

        int    Space(XrdOucErrInfo &Resp, const char *path, XrdOucEnv *envP=0)
                    {return 0;}

        void  *Start();

static  bool   VCheck(XrdVersionInfo &urVersion);

               XrdCmsFinderTRG(XrdSysLogger *, int, int, XrdOss *theSS=0);
              ~XrdCmsFinderTRG();

private:

void  Hookup();
int   Process(XrdCmsRRData &Data);

XrdOss        *SS;
char          *CMSPath;
char          *Login;
XrdOucTList   *myManList;
XrdOucStream  *CMSp;
XrdSysMutex    myData;
XrdSysMutex    rrMutex;
int            resMax;
int            resCur;
int            myPort;
int            isRedir;
int            isProxy;
int            Active;
};
#endif
