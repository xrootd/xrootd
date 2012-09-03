#ifndef _XRDCMSXMIREQ_H_
#define _XRDCMSXMIREQ_H_
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s X m i R e q . h h                        */
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

#include "XrdCms/XrdCmsReq.hh"
#include "XrdCms/XrdCmsXmi.hh"
#include "XrdSys/XrdSysPthread.hh"
  
class XrdCmsXmiReq : XrdCmsXmi
{
public:

       enum ReqType {do_chmod, do_mkdir, do_mkpath,do_mv,
                     do_prep,  do_rm,    do_rmdir, do_stage, do_stat};

       int  Chmod (      XrdCmsReq      *Request,
                         mode_t          mode,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_chmod, (int)mode, path, opaque);}

       int  Mkdir (      XrdCmsReq      *Request,
                         mode_t          mode,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_mkdir, (int)mode, path, opaque);}

       int  Mkpath(      XrdCmsReq      *Request,
                         mode_t          mode,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_mkpath, (int)mode, path, opaque);}

       int  Prep  (const char           *ReqID,
                         int             opts,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(0, do_prep, 0, path, opaque, ReqID);}

       int  Rename(      XrdCmsReq      *Request,
                   const char           *oldpath,
                   const char           *oldopaque,
                   const char           *newpath,
                   const char           *newopaque)
                  {return Qit(Request, do_mv, 0, oldpath, oldopaque,
                                                 newpath, newopaque);}

       int  Remdir(      XrdCmsReq      *Request,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_rmdir, 0, path, opaque);}

       int  Remove(      XrdCmsReq      *Request,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_rm, 0, path, opaque);}

       int  Select(      XrdCmsReq      *Request,
                         int             opts,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_stage, opts, path, opaque);}

       int  Stat  (      XrdCmsReq      *Request,
                   const char           *path,
                   const char           *opaque)
                  {return Qit(Request, do_stat, 0, path, opaque);}

static void processPrpQ();

static void processReqQ();

static void processStgQ();

            XrdCmsXmiReq(XrdCmsXmi *xp);

            XrdCmsXmiReq(XrdCmsReq *reqp, ReqType rqtype, int parms,
                      const char *path,    const char *opaque,
                      const char *path2=0, const char *opaque2=0);

virtual    ~XrdCmsXmiReq();

private:
void Start();
int  Qit(XrdCmsReq *rp, ReqType, int parms, 
         const char *path,    const char *opaque,
         const char *path2=0, const char *opaque2=0);

static XrdCmsXmi      *XmiP;
static XrdSysMutex     prpMutex;
static XrdSysSemaphore prpReady;
static XrdCmsXmiReq   *prpFirst;
static XrdCmsXmiReq   *prpLast;
static XrdSysMutex     reqMutex;
static XrdSysSemaphore reqReady;
static XrdCmsXmiReq   *reqFirst;
static XrdCmsXmiReq   *reqLast;
static XrdSysMutex     stgMutex;
static XrdSysSemaphore stgReady;
static XrdCmsXmiReq   *stgFirst;
static XrdCmsXmiReq   *stgLast;
       XrdCmsXmiReq   *Next;
       XrdCmsReq      *ReqP;
       int             Parms;
       ReqType         Rtype;
       char           *Path;
       char           *Opaque;
       char           *Path2;
       char           *Opaque2;
};
#endif
