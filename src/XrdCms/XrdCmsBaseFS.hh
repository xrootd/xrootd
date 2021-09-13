#ifndef __CMS_BASEFS_H__
#define __CMS_BASEFS_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s B a s e F S . h h                        */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdCms/XrdCmsPList.hh"
#include "XrdCms/XrdCmsRRData.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                    C l a s s   X r d C m s B a s e F R                     */
/******************************************************************************/

class XrdCmsPInfo;

class XrdCmsBaseFR
{
public:

SMask_t          Route;
SMask_t          RouteW;
XrdCmsBaseFR    *Next;
char            *Buff;
char            *Path;
short            PathLen;
short            PDirLen;
kXR_unt32        Sid;
kXR_char         Mod;

                 XrdCmsBaseFR(XrdCmsRRData &Arg, XrdCmsPInfo &Who, int Dln)
                             : Route(Who.rovec), RouteW(Who.rwvec), Next(0),
                               PathLen(Arg.PathLen), PDirLen(Dln),
                               Sid(Arg.Request.streamid),
                               Mod(Arg.Request.modifier)
                             {if (Arg.Buff)
                                 {Path=Arg.Path; Buff=Arg.Buff; Arg.Buff=0;}
                                 else Buff = Path = strdup(Arg.Path);
                             }

                 XrdCmsBaseFR(XrdCmsRRData *aP,  XrdCmsPInfo &Who, int Dln)
                             : Route(Who.rovec), RouteW(Who.rwvec),
                               Next(0), Buff(0), Path(aP->Path),
                               PathLen(aP->PathLen), PDirLen(Dln),
                               Sid(aP->Request.streamid),
                               Mod(aP->Request.modifier)
                             {}

               ~XrdCmsBaseFR() {if (Buff) free(Buff); Buff = 0;}
};
  
/******************************************************************************/
/*                    C l a s s   X r d C m s B a s e F S                     */
/******************************************************************************/

class XrdCmsBaseFS
{
public:

       int              dfsTries() {return dfsMaxTries;}

// Exists() returns a tri-logic state:
// CmsHaveRequest::Online  -> File is known to exist and is available
// CmsHaveRequest::Pending -> File is known to exist but is not available
//  0                      -> File state unknown, result will be provided later
// -1                      -> File is known not to exist
//
       int              Exists(XrdCmsRRData &Arg,XrdCmsPInfo &Who,int noLim=0);

// The following exists works as above but limits are never enforced and it
// never returns 0. Additionally, the fnpos parameter works as follows:
//
// > 0 -> Offset into path to the last slash before the filename.
// = 0 -> A valid offset has not been calculated nor should it be calculated.
// < 0 -> A valid offset has not been calculated, fnpos = -(length of Path).

       int              Exists(char *Path, int fnPos, int UpAT=0);

// Valid Opts for Init()
//
static const int        Cntrl  = 0x0001; // Centralize stat() o/w distribute it
static const int        DFSys  = 0x0002; // Distributed filesystem o/w jbods
static const int        Immed  = 0x0004; // Redirect immediately o/w preselect
static const int        Servr  = 0x0100; // This is a pure server node

       void             Init(int Opts, int DMlife, int DPLife);

inline int              isDFS() {return dfsSys;}

inline int              Limit() {return theQ.rLimit;}

       void             Limit(int rLim, int qMax);

inline int              Local() {return lclStat;}

       void             Pacer();

       void             Runner();

static const int dfltDfsTries = 2;
static const int dfltStgTries = 3;

       void             SetTries(bool xdfs, int tcnt)
                                {if (xdfs) dfsMaxTries = 
                                           (tcnt < 0 ? dfltDfsTries : tcnt);
                                    else   stgMaxTries =
                                           (tcnt < 1 ? dfltStgTries : tcnt);
                                }

       void             Start();

       int              stgTries() {return stgMaxTries;}

inline int              Trim() {return preSel;}

inline int              Traverse() {return Punt;}

       XrdCmsBaseFS(void (*theCB)(XrdCmsBaseFR *, int))
                   : cBack(theCB), dfsMaxTries(dfltDfsTries),
                                   stgMaxTries(dfltStgTries),
                     dmLife(0), dpLife(0), lclStat(0), preSel(1),
                     dfsSys(0), Server(0), Fixed(0), Punt(0) {}
      ~XrdCmsBaseFS() {}

private:

struct dMoP {int        Present;};

       int              Bypass();
       int              FStat( char *Path, int fnPos, int upat=0);
       int              hasDir(char *Path, int fnPos);
       void             Queue(XrdCmsRRData &Arg, XrdCmsPInfo &Who,
                              int dln, int Frc=0);
       void             Xeq(XrdCmsBaseFR *rP);

       XrdSysMutex      fsMutex;
       XrdOucHash<dMoP> fsDirMP;
       void             (*cBack)(XrdCmsBaseFR *, int);

struct RequestQ
      {XrdSysMutex      Mutex;
       XrdSysSemaphore  pqAvail;
       XrdSysSemaphore  rqAvail;
       XrdCmsBaseFR    *pqFirst;
       XrdCmsBaseFR    *pqLast;
       XrdCmsBaseFR    *rqFirst;
       XrdCmsBaseFR    *rqLast;
       int              rLimit;   // Maximum number of requests per second
       int              qHWM;     // Queue high watermark
       int              qMax;     // Maximum elements to be queued
       int              qNum;     // Total number of queued elements (pq + rq)
       int              rLeft;    // Number of non-queue requests allowed
       int              rAgain;   // Value to reinitialize rLeft
       RequestQ() : pqAvail(0), rqAvail(0),
                    pqFirst(0), pqLast(0), rqFirst(0), rqLast(0),
                    rLimit(0),  qHWM(0),   qMax(1),    qNum(0),
                    rLeft(0),   rAgain(0)  {}
      ~RequestQ() {}
      }                 theQ;

       int              dfsMaxTries;
       int              stgMaxTries;
       int              dmLife;
       int              dpLife;
       char             lclStat;  // 1-> Local stat() calls wanted
       char             preSel;   // 1-> Preselect before redirect
       char             dfsSys;   // 1-> Distributed Filesystem
       char             Server;   // 1-> This is a data server
       char             Fixed;    // 1-> Use fixed rate processing
       char             Punt;     // 1-> Pass through any forwarding
};
namespace XrdCms
{
extern XrdCmsBaseFS baseFS;
}
#endif
