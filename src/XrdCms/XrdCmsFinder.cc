/******************************************************************************/
/*                                                                            */
/*                       X r d C m s F i n d e r . c c                        */
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "XrdVersion.hh"
  
#include "XProtocol/YProtocol.hh"

#include "XrdCms/XrdCmsClientConfig.hh"
#include "XrdCms/XrdCmsClientMan.hh"
#include "XrdCms/XrdCmsClientMsg.hh"

#include "XrdCms/XrdCmsFinder.hh"
#include "XrdCms/XrdCmsParser.hh"
#include "XrdCms/XrdCmsResp.hh"
#include "XrdCms/XrdCmsRRData.hh"
#include "XrdCms/XrdCmsSecurity.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucReqID.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"

using namespace XrdCms;

class XrdInet;

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdCms
{
XrdSysError  Say(0, "cms_");
  
XrdOucTrace  Trace(&Say);

XrdVERSIONINFODEF(myVersion,cmsclient,XrdVNUMBER,XrdVERSION);
};

/******************************************************************************/
/*                         R e m o t e   F i n d e r                          */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCmsFinderRMT::XrdCmsFinderRMT(XrdSysLogger *lp, int whoami, int Port)
               : XrdCmsClient(XrdCmsClient::amRemote)
{
     myManagers  = 0;
     myManCount  = 0;
     myManList   = 0;
     myPort      = Port;
     SMode       = 0;
     sendID      = 0;
     isMeta      = whoami & IsMeta;
     isProxy     = whoami & IsProxy;
     isTarget    = whoami & IsTarget;
     savePath    = 0;
     Say.logger(lp);
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdCmsFinderRMT::~XrdCmsFinderRMT()
{
    XrdCmsClientMan *mp, *nmp = myManagers;
    XrdOucTList *tp, *tpp = myManList;

    while((mp = nmp)) {nmp = mp->nextManager(); delete mp;}

    while((tp = tpp)) {tpp = tp->next; delete tp;}
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdCmsFinderRMT::Configure(const char *cfn, char *Args, XrdOucEnv *envP)
{
   XrdCmsClientConfig             config;
   XrdCmsClientConfig::configHow  How;
   XrdCmsClientConfig::configWhat What;
   XrdInet *netP;
   int Topts = IsRedir;

// Establish what we will be configuring
//
   if (isProxy)
      {How = XrdCmsClientConfig::configProxy; Topts |= IsProxy;}
      else if (isMeta) How = XrdCmsClientConfig::configMeta;
              else     How = XrdCmsClientConfig::configNorm;
   What = (isTarget ? XrdCmsClientConfig::configSuper
                    : XrdCmsClientConfig::configMan);

// Establish the network interface that the caller must provide
//
   if (!envP || !(netP = (XrdInet *)envP->GetPtr("XrdInet*")))
      {Say.Emsg("Finder", "Network not defined; unable to connect to cmsd.");
       return 0;
      }
   XrdCmsClientMan::setNetwork(netP);
   XrdCmsClientMan::setConfig(cfn);
   XrdCmsSecurity::setSecFunc(envP->GetPtr("XrdSecGetProtocol*"));

// Now call the configration object
//
   if (config.Configure(cfn, What, How)) return 0;

// Set configured values and start the managers
//
   CMSPath    = config.CMSPath;
   RepDelay   = config.RepDelay;
   RepNone    = config.RepNone;
   RepWait    = config.RepWait;
   ConWait    = config.ConWait;
   FwdWait    = config.FwdWait;
   PrepWait   = config.PrepWait;
   if (isProxy)
           {SMode = config.SModeP;
            StartManagers(config.PanList);
            config.PanList = 0;
           }
      else {SMode = config.SMode;
            StartManagers(config.ManList);
            config.ManList = 0;
           }

// If we are tracing or if redirect monitoring is enabled, we will need
// to save path information.
//
   if (QTRACE(Redirect) || getenv("XRDMONRDR")) savePath = 1;

// If we are a plain manager but have a meta manager then we must start
// a responder (that we will hide) to pass through the port number.
//
   if (!isMeta && !isTarget && config.haveMeta)
      {XrdCmsFinderTRG *Rsp = new XrdCmsFinderTRG(Say.logger(),Topts,myPort);
       return Rsp->RunAdmin(CMSPath);
      }

// All done
//
   return 1;
}

/******************************************************************************/
/*                               F o r w a r d                                */
/******************************************************************************/

int XrdCmsFinderRMT::Forward(XrdOucErrInfo &Resp, const char *cmd, 
                             const char *arg1,    const char *arg2,
                             XrdOucEnv  *Env1,    XrdOucEnv  *Env2)
{
   static XrdSysMutex fwdMutex;
   static struct timeval fwdClk = {time(0),0};
   static const int xNum   = 12;

   XrdCmsClientMan *Manp;
   XrdCmsRRData     Data;
   int              iovcnt, is2way, doAll = 0, opQ1Len = 0, opQ2Len = 0;
   char             Work[xNum*12];
   struct iovec     xmsg[xNum];

// Encode the request as a redirector command
//
   if ((is2way = (*cmd == '+'))) cmd++;

        if (!strcmp("chmod", cmd)) Data.Request.rrCode = kYR_chmod;
   else if (!strcmp("mkdir", cmd)) Data.Request.rrCode = kYR_mkdir;
   else if (!strcmp("mkpath",cmd)) Data.Request.rrCode = kYR_mkpath;
   else if (!strcmp("mv",    cmd)){Data.Request.rrCode = kYR_mv;    doAll=1;}
   else if (!strcmp("rm",    cmd)){Data.Request.rrCode = kYR_rm;    doAll=1;}
   else if (!strcmp("rmdir", cmd)){Data.Request.rrCode = kYR_rmdir; doAll=1;}
   else if (!strcmp("trunc", cmd)) Data.Request.rrCode = kYR_trunc;
   else {Say.Emsg("Finder", "Unable to forward '", cmd, "'.");
         Resp.setErrInfo(EINVAL, "Internal error processing file.");
         return SFS_ERROR;
        }

// Fill out the RR data structure
//
   Data.Ident   = (char *)(XrdCmsClientMan::doDebug ? Resp.getErrUser() : "");
   Data.Path    = (char *)arg1;
   Data.Mode    = (char *)arg2;
   Data.Path2   = (char *)arg2;
   Data.Opaque  = (Env1 ? Env1->Env(opQ1Len) : 0);
   Data.Opaque2 = (Env2 ? Env2->Env(opQ2Len) : 0);

// Pack the arguments
//
   if (!(iovcnt = Parser.Pack(int(Data.Request.rrCode), &xmsg[1], &xmsg[xNum],
                              (char *)&Data, Work)))
      {Resp.setErrInfo(EINVAL, "Internal error processing file.");
       return SFS_ERROR;
      }

// Insert the header into the stream
//
   Data.Request.streamid = 0;
   Data.Request.modifier = 0;
   xmsg[0].iov_base      = (char *)&Data.Request;
   xmsg[0].iov_len       = sizeof(Data.Request);

// This may be a 2way message. If so, use the longer path.
//
   if (is2way) return send2Man(Resp, (arg1 ? arg1 : "/"), xmsg, iovcnt+1);

// Check if we have exceeded the maximum rate for requests. If we have, we
// will wait to repace the stream so as to not burden the cmsd.
//
   if (doAll && FwdWait)
      {struct timeval nowClk;
       time_t Window;
       fwdMutex.Lock();
       gettimeofday(&nowClk, 0);
       fwdClk.tv_sec  = nowClk.tv_sec  - fwdClk.tv_sec;
       fwdClk.tv_usec = nowClk.tv_usec - fwdClk.tv_usec;
       if (fwdClk.tv_usec < 0) {fwdClk.tv_sec--; fwdClk.tv_usec += 1000000;}
       Window = fwdClk.tv_sec*1000 + fwdClk.tv_usec/1000;
       if (Window < FwdWait) XrdSysTimer::Wait(FwdWait - Window);
       fwdClk = nowClk;
       fwdMutex.UnLock();
      }

// Select the right manager for this request
//
   if (!(Manp = SelectManager(Resp, (arg1 ? arg1 : "/")))) return ConWait;

// Send message and simply wait for the reply
//
   if (Manp->Send(xmsg, iovcnt+1))
      {if (doAll)
          {Data.Request.modifier |= kYR_dnf;
           Inform(Manp, xmsg, iovcnt+1);
          }
       return 0;
      }

// Indicate client should retry later
//
   Resp.setErrInfo(RepDelay, "");
   return RepDelay;
}
  
/******************************************************************************/
/*                                I n f o r m                                 */
/******************************************************************************/
  
void XrdCmsFinderRMT::Inform(XrdCmsClientMan *xman,
                             struct iovec     xmsg[], int xnum)
{
   XrdCmsClientMan *Womp, *Manp;

// Make sure we are configured
//
   if (!myManagers)
      {Say.Emsg("Finder", "SelectManager() called prior to Configure().");
       return;
      }

// Start at the beginning (we will avoid the previously selected one)
//
   Womp = Manp = myManagers;
   do {if (Manp != xman && Manp->isActive()) Manp->Send(xmsg, xnum);
      } while((Manp = Manp->nextManager()) != Womp);
}
  
/******************************************************************************/
/*                                L o c a t e                                 */
/******************************************************************************/
  
int XrdCmsFinderRMT::Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                            XrdOucEnv *Env)
{
   static const int xNum   = 12;

   XrdCmsRRData   Data;
   int            n, iovcnt;
   char           Work[xNum*12];
   struct iovec   xmsg[xNum];

// Fill out the RR data structure
//
   Data.Ident   = (char *)(XrdCmsClientMan::doDebug ? Resp.getErrUser() : "");
   Data.Path    = (char *)path;
   Data.Opaque  = (Env ? Env->Env(n)       : 0);
   Data.Avoid   = (Env ? Env->Get("tried") : 0);

// Set options and command
//
   if (flags & SFS_O_LOCATE)
      {bool doAll = (flags & SFS_O_FORCE) != 0;
       if (flags & SFS_O_LOCAL) return LocLocal(Resp, Env);
       Data.Request.rrCode = kYR_locate;
       Data.Opts = (flags & SFS_O_NOWAIT ? CmsLocateRequest::kYR_asap    : 0)
                 | (flags & SFS_O_RESET  ? CmsSelectRequest::kYR_refresh : 0);
       if (Resp.getUCap() & XrdOucEI::uPrip)
          Data.Opts |= CmsLocateRequest::kYR_prvtnet;
       if (Resp.getUCap() & XrdOucEI::uIPv4)
         {Data.Opts |= (Resp.getUCap() & XrdOucEI::uIPv64 || doAll
                     ?  CmsLocateRequest::kYR_retipv46 : 0);
         } else {
          Data.Opts |= (Resp.getUCap() & XrdOucEI::uIPv64 || doAll
                     ?  CmsLocateRequest::kYR_retipv64 :
                        CmsLocateRequest::kYR_retipv6);
         }
       if (flags & SFS_O_HNAME) Data.Opts |= CmsLocateRequest::kYR_retname;
       if (doAll)               Data.Opts |= CmsLocateRequest::kYR_listall;
      } else
  {     Data.Request.rrCode = kYR_select;
        if (flags & SFS_O_TRUNC) Data.Opts = CmsSelectRequest::kYR_trunc;
   else if (flags & SFS_O_CREAT)
           {   Data.Opts = CmsSelectRequest::kYR_create;
            if (flags & SFS_O_REPLICA)
               Data.Opts|= CmsSelectRequest::kYR_replica;
           }
   else if (flags & SFS_O_STAT)  Data.Opts = CmsSelectRequest::kYR_stat;
   else                          Data.Opts = 0;

   Data.Opts |= (flags & (SFS_O_WRONLY | SFS_O_RDWR)
              ? CmsSelectRequest::kYR_write : CmsSelectRequest::kYR_read);

   if (flags & SFS_O_META)      Data.Opts  |= CmsSelectRequest::kYR_metaop;

   if (flags & SFS_O_NOWAIT)    Data.Opts  |= CmsSelectRequest::kYR_online;

   if (flags & SFS_O_RESET)     Data.Opts  |= CmsSelectRequest::kYR_refresh;

   if (Resp.getUCap() & XrdOucEI::uPrip)
      Data.Opts |= CmsSelectRequest::kYR_prvtnet;
       if (Resp.getUCap() & XrdOucEI::uIPv4)
         {Data.Opts |= (Resp.getUCap() & XrdOucEI::uIPv64
                     ?  CmsSelectRequest::kYR_retipv46 : 0);
         } else {
          Data.Opts |= (Resp.getUCap() & XrdOucEI::uIPv64
                     ?  CmsSelectRequest::kYR_retipv64 :
                        CmsSelectRequest::kYR_retipv6);
         }
  }

// Pack the arguments
//
   if (!(iovcnt = Parser.Pack(int(Data.Request.rrCode), &xmsg[1], &xmsg[xNum],
                              (char *)&Data, Work)))
      {Resp.setErrInfo(EINVAL, "Internal error processing file.");
       return SFS_ERROR;
      }

// Insert the header into the stream
//
   Data.Request.streamid = 0;
   Data.Request.modifier = 0;
   xmsg[0].iov_base      = (char *)&Data.Request;
   xmsg[0].iov_len       = sizeof(Data.Request);

// Send the 2way message
//
   return send2Man(Resp, path, xmsg, iovcnt+1);
}
  
/******************************************************************************/
/*                              L o c L o c a l                               */
/******************************************************************************/

int XrdCmsFinderRMT::LocLocal(XrdOucErrInfo &Resp, XrdOucEnv *Env)
{
   XrdCmsClientMan *Womp, *Manp;
   XrdOucBuffer *xBuff = 0;
   char *mBeg, *mBuff, mStat;
   int   mBlen, n;

// If we have no managers or no role, we are not clustered
//
   if (!myManagers)
      {Resp.setErrInfo(0, "");
       return SFS_DATA;
      }

// Get where to start and where to put the information
//
   Womp = Manp  = myManagers;
   mBeg = mBuff = Resp.getMsgBuff(mBlen);

// Check if we can use the internal buffer or need to get an external buffer
//
   n = 8 + (myManCount * (256+6+2));
   if (n > mBlen)
      {mBeg = mBuff = (char *)malloc(n);
       if (!mBeg)
          {Resp.setErrInfo(ENOMEM, "Insufficient memory.");
           return SFS_ERROR;
          }
       xBuff = new XrdOucBuffer(mBeg, n);
       mBlen = n;
      }

// Make sure we have enough space to continue
//
   if (mBlen < 1024)
      {Resp.setErrInfo(EINVAL, "Invalid role.");
       return SFS_ERROR;
      }

// List the status of each manager
//
   do {if (Manp->isActive()) mStat = (Manp->Suspended() ? 's' : 'c');
          else mStat = 'd';
       n = snprintf(mBuff, mBlen, "%s:%d/%c ",
                           Manp->Name(), Manp->manPort(), mStat);
       mBuff += n; mBlen -= n;
      } while((Manp = Manp->nextManager()) != Womp && mBlen > 0);

// We should not have overrun the buffer; if we did declare failure
//
   if (mBlen < 0)
      {Resp.setErrInfo(EINVAL, "Internal processing error.");
       if (xBuff) xBuff->Recycle();
       return SFS_ERROR;
      }

// Set the final result
//
   n = mBuff - mBeg;
   if (!xBuff) Resp.setErrCode(n);
      else {xBuff->SetLen(n);
            Resp.setErrInfo(n, xBuff);
           }

// All done
//
   return SFS_DATA;
}
  
/******************************************************************************/
/*                               P r e p a r e                                */
/******************************************************************************/
  
int XrdCmsFinderRMT::Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs,
                             XrdOucEnv *envP)
{
   EPNAME("Prepare")
   static const int   xNum   = 16;
   static XrdSysMutex prepMutex;

   XrdCmsRRData       Data;
   XrdOucTList       *tp, *op;
   XrdCmsClientMan   *Manp = 0;

   int                iovcnt = 0, NoteLen, n;
   char               Prty[1032], *NoteNum = 0, *colocp = 0;
   char               Work[xNum*12];
   struct iovec       xmsg[xNum];

// Prefill the RR data structure and iovec
//
   Data.Ident = (char *)(XrdCmsClientMan::doDebug ? Resp.getErrUser() : "");
   Data.Reqid = pargs.reqid;
   Data.Request.streamid = 0;
   Data.Request.modifier = 0;
   xmsg[0].iov_base = (char *)&Data.Request;
   xmsg[0].iov_len  = sizeof(Data.Request);

// Check for a cancel request
//
   if (!(tp = pargs.paths))
      {Data.Request.rrCode = kYR_prepdel;
       if (!(iovcnt = Parser.Pack(kYR_prepdel, &xmsg[1], &xmsg[xNum],
                                 (char *)&Data, Work)))
          {Resp.setErrInfo(EINVAL, "Internal error processing file.");
           return SFS_ERROR;
          }
       if (!(Manp = SelectManager(Resp, 0))) return ConWait;
       if (Manp->Send((const struct iovec *)&xmsg, iovcnt+1)) return 0;
       DEBUG("Finder: Failed to send prepare cancel to " 
             <<Manp->Name() <<" reqid=" <<pargs.reqid);
       Resp.setErrInfo(RepDelay, "");
       return RepDelay;
      }

// Set prepadd options
//
   Data.Request.modifier =
               (pargs.opts & Prep_STAGE ? CmsPrepAddRequest::kYR_stage : 0)
             | (pargs.opts & Prep_WMODE ? CmsPrepAddRequest::kYR_write : 0)
             | (pargs.opts & Prep_FRESH ? CmsPrepAddRequest::kYR_fresh : 0);

// Set coloc information if staging wanted and there are atleast two paths
//

// Set the prepadd mode
//
   if (!pargs.notify || !(pargs.opts & Prep_SENDACK))
      {Data.Mode   = (char *)(pargs.opts & Prep_WMODE ? "wq" : "rq");
       Data.Notify = (char *)"*";
       NoteNum     = 0;
      } else {
       NoteLen      = strlen(pargs.notify);
       Data.Notify  = (char *)malloc(NoteLen+16);
       strcpy(Data.Notify, pargs.notify);
       NoteNum = Data.Notify+NoteLen; *NoteNum++ = '-';
       if (pargs.opts & Prep_SENDERR) 
               Data.Mode = (char *)(pargs.opts & Prep_WMODE ? "wn"  : "rn");
          else Data.Mode = (char *)(pargs.opts & Prep_WMODE ? "wnq" : "rnq");
      }

// Set the priority (if co-locate, add in the co-locate path)
//
   n = sprintf(Prty, "%d", (pargs.opts & Prep_PMASK));
   if (pargs.opts & Prep_STAGE && pargs.opts & Prep_COLOC
   &&  pargs.paths && pargs.paths->next) 
      {colocp = Prty + n;
       strlcpy(colocp+1, pargs.paths->text, sizeof(Prty)-n-1);
      }
   Data.Prty = Prty;

// Distribute out paths to the various managers
//
   Data.Request.rrCode = kYR_prepadd;
   op = pargs.oinfo;
   while(tp)
        {if (NoteNum) sprintf(NoteNum, "%d", tp->val);
         Data.Path = tp->text;
         if (op) {Data.Opaque = op->text; op = op->next;}
            else  Data.Opaque = 0;
         if (!(iovcnt = Parser.Pack(kYR_prepadd, &xmsg[1], &xmsg[xNum],
                                   (char *)&Data, Work))) break;
         if (!(Manp = SelectManager(Resp, tp->text))) break;
         DEBUG("Finder: Sending " <<Manp->Name() <<' ' <<Data.Reqid
                      <<' ' <<Data.Path);
         if (!Manp->Send((const struct iovec *)&xmsg, iovcnt+1)) break;
         if ((tp = tp->next))
            {prepMutex.Lock(); XrdSysTimer::Wait(PrepWait); prepMutex.UnLock();}
         if (colocp) {Data.Request.modifier |= CmsPrepAddRequest::kYR_coloc;
                      *colocp = ' '; colocp = 0;
                     }
        }

// Check if all went well
//
   if (NoteNum) free(Data.Notify);
   if (!tp) return 0;

// Decode the error condition
//
   if (!Manp) return ConWait;

   if (!iovcnt)
      {Say.Emsg("Finder", "Unable to send prepadd; too much data.");
       Resp.setErrInfo(EINVAL, "Internal error processing file.");
       return SFS_ERROR;
      }

   Resp.setErrInfo(RepDelay, "");
   DEBUG("Finder: Failed to send prepare to " <<(Manp ? Manp->Name() : "?")
                  <<" reqid=" <<pargs.reqid);
   return RepDelay;
}

/******************************************************************************/
/*                         S e l e c t M a n a g e r                          */
/******************************************************************************/
  
XrdCmsClientMan *XrdCmsFinderRMT::SelectManager(XrdOucErrInfo &Resp, 
                                                const char    *path)
{
   XrdCmsClientMan *Womp, *Manp;

// Make sure we are configured
//
   if (!myManagers)
      {Say.Emsg("Finder", "SelectManager() called prior to Configure().");
       Resp.setErrInfo(ConWait, "");
       return (XrdCmsClientMan *)0;
      }

// Get where to start
//
   if (SMode != XrdCmsClientConfig::RoundRob || !path) Womp = Manp = myManagers;
      else Womp = Manp = myManTable[XrdOucReqID::Index(myManCount, path)];

// Find the next active server
//
   do {if (Manp->isActive()) return (Manp->Suspended() ? 0 : Manp);
      } while((Manp = Manp->nextManager()) != Womp);

// All managers are dead
//
   SelectManFail(Resp);
   return (XrdCmsClientMan *)0;
}
  
/******************************************************************************/
/*                         S e l e c t M a n F a i l                          */
/******************************************************************************/
  
void XrdCmsFinderRMT::SelectManFail(XrdOucErrInfo &Resp)
{
   EPNAME("SelectManFail")
   static time_t nextMsg = 0;
   time_t now;

// All servers are dead, indicate so every minute
//
   now = time(0);
   myData.Lock();
   if (nextMsg < now)
      {nextMsg = now + 60;
       myData.UnLock();
       Say.Emsg("Finder", "All managers are dysfunctional.");
      } else myData.UnLock();
   Resp.setErrInfo(ConWait, "");
   TRACE(Redirect, "user=" <<Resp.getErrUser() <<" No managers available; wait " <<ConWait);
}
  
/******************************************************************************/
/*                              s e n d 2 M a n                               */
/******************************************************************************/
  
int XrdCmsFinderRMT::send2Man(XrdOucErrInfo &Resp, const char *path,
                              struct iovec  *xmsg, int         xnum)
{
   EPNAME("send2Man")
   int              retc;
   XrdCmsClientMsg *mp;
   XrdCmsClientMan *Manp;

// Select the right manager for this request
//
   if (!(Manp = SelectManager(Resp, path)) || Manp->Suspended()) 
      return ConWait;

// Allocate a message object. There is only a fixed number of these and if
// all of them are in use, th client has to wait to prevent over-runs.
//
   if (!(mp = XrdCmsClientMsg::Alloc(&Resp)))
      {Resp.setErrInfo(RepDelay, "");
       TRACE(Redirect, Resp.getErrUser() <<" no more msg objects; path=" <<path);
       return RepDelay;
      }

// Insert the message number into the header
//
   ((CmsRRHdr *)(xmsg[0].iov_base))->streamid = mp->ID();
   if (savePath) Resp.setErrData(path);
      else Resp.setErrData(0);

// Send message and simply wait for the reply (msg object is locked via Alloc)
//
   if (!Manp->Send(xmsg, xnum) || (mp->Wait4Reply(Manp->waitTime())))
      {mp->Recycle();
       retc = Manp->whatsUp(Resp.getErrUser(), path);
       Resp.setErrInfo(retc, "");
       return retc;
      }

// A reply was received; process as appropriate
//
   retc = mp->getResult();
   if (retc == SFS_STARTED) retc = Manp->delayResp(Resp);
      else if (retc == SFS_STALL) retc = Resp.getErrInfo();

// All done
//
   mp->Recycle();
   return retc;
}

/******************************************************************************/
/*                         S t a r t M a n a g e r s                          */
/******************************************************************************/
  
void *XrdCmsStartManager(void *carg)
      {XrdCmsClientMan *mp = (XrdCmsClientMan *)carg;
       return mp->Start();
      }

void *XrdCmsStartResp(void *carg)
      {XrdCmsResp::Reply();
       return (void *)0;
      }

int XrdCmsFinderRMT::StartManagers(XrdOucTList *theManList)
{
   XrdOucTList *tp;
   XrdCmsClientMan *mp, *firstone = 0;
   int i = 0;
   pthread_t tid;
   char buff[128];

// Save the proper manager list for later reporting
//
   myManList = theManList;

// Clear manager table
//
   memset((void *)myManTable, 0, sizeof(myManTable));

// For each manager, start a thread to handle it
//
   tp = theManList;
   while(tp && i < MaxMan)
        {mp = new XrdCmsClientMan(tp->text,tp->val,ConWait,RepNone,RepWait,RepDelay);
         myManTable[i] = mp;
         if (myManagers) mp->setNext(myManagers);
            else firstone = mp;
         myManagers = mp;
         if (XrdSysThread::Run(&tid,XrdCmsStartManager,(void *)mp,0,mp->Name()))
            Say.Emsg("Finder", errno, "start manager");
         tp = tp->next; i++;
        }

// Check if we exceeded maximum manager count
//
   if (tp) 
      while(tp)
           {Say.Emsg("Config warning: too many managers;",tp->text,"ignored.");
            tp = tp->next;
           }

// Make this a circular chain
//
   if (firstone) firstone->setNext(myManagers);

// Indicate how many managers have been started
//
   sprintf(buff, "%d manager(s) started.", i);
   Say.Say("Config ", buff);
   myManCount = i;

// Now Start that many callback threads
//
   while(i--)
        if (XrdSysThread::Run(&tid,XrdCmsStartResp,(void *)0,0,"async callback"))
            Say.Emsg("Finder", errno, "start callback manager");

// All done
//
   return 0;
}
 
/******************************************************************************/
/*                                 S p a c e                                  */
/******************************************************************************/
  
int XrdCmsFinderRMT::Space(XrdOucErrInfo &Resp, const char *path, XrdOucEnv *eP)
{
   static const int xNum   = 4;

   XrdCmsRRData   Data;
   int            iovcnt;
   char           Work[xNum*12];
   struct iovec   xmsg[xNum];

// Fill out the RR data structure
//
   Data.Ident   = (char *)(XrdCmsClientMan::doDebug ? Resp.getErrUser() : "");
   Data.Path    = (char *)path;

// Pack the arguments
//
   if (!(iovcnt = Parser.Pack(kYR_statfs, &xmsg[1], &xmsg[xNum],
                                  (char *)&Data, Work)))
      {Resp.setErrInfo(EINVAL, "Internal error processing file.");
       return SFS_ERROR;
      }

// Insert the header into the stream
//
   Data.Request.rrCode   = kYR_statfs;
   Data.Request.streamid = 0;
   Data.Request.modifier = 0;
   xmsg[0].iov_base      = (char *)&Data.Request;
   xmsg[0].iov_len       = sizeof(Data.Request);

// Send the 2way message
//
   return send2Man(Resp, path, xmsg, iovcnt+1);
}
  
/******************************************************************************/
/*                                V C h e c k                                 */
/******************************************************************************/
  
bool XrdCmsFinderRMT::VCheck(XrdVersionInfo &urVersion)
{
   return XrdSysPlugin::VerCmp(urVersion, myVersion);
}

/******************************************************************************/
/*                         T a r g e t   F i n d e r                          */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCmsFinderTRG::XrdCmsFinderTRG(XrdSysLogger *lp, int whoami, int port,
                                 XrdOss *theSS)
               : XrdCmsClient(XrdCmsClient::amTarget)
{
   char buff [256];
   isRedir = whoami & IsRedir;
   isProxy = whoami & IsProxy;
   SS      = theSS;
   CMSPath = 0;
   myManList = 0;
   CMSp    = new XrdOucStream(&Say);
   Active  = 0;
   myPort  = port;
   resMax  = -1;
   resCur  = 0;
   sprintf(buff, "login %c %d port %d\n",(isProxy ? 'P' : 'p'),
                 static_cast<int>(getpid()), port);
   Login = strdup(buff);
   Say.logger(lp);
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdCmsFinderTRG::~XrdCmsFinderTRG()
{
  XrdOucTList *tp, *tpp = myManList;

  if (CMSp)  delete CMSp;
  if (Login) free(Login);

  while((tp = tpp)) {tpp = tp->next; delete tp;}
}
  
/******************************************************************************/
/*                                 A d d e d                                  */
/******************************************************************************/
  
void XrdCmsFinderTRG::Added(const char *path, int Pend)
{
   char *data[4];
   int   dlen[4];

// Set up to notify the cluster that a file has been added
//
   data[0] = (char *)"newfn ";   dlen[0] = 6;
   data[1] = (char *)path;       dlen[1] = strlen(path);
   if (Pend)
  {data[2] = (char *)" p\n";     dlen[2] = 3;}
      else
  {data[2] = (char *)"\n";       dlen[2] = 1;}
   data[3] = 0;                  dlen[3] = 0;

// Now send the notification
//
   myData.Lock();
   if (Active && CMSp->Put((const char **)data, (const int *)dlen))
      {CMSp->Close(); Active = 0;}
   myData.UnLock();
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

void *XrdCmsStartRsp(void *carg)
      {XrdCmsFinderTRG *mp = (XrdCmsFinderTRG *)carg;
       return mp->Start();
      }
  
int XrdCmsFinderTRG::Configure(const char *cfn, char *Ags, XrdOucEnv *envP)
{
   XrdCmsClientConfig             config;
   XrdCmsClientConfig::configWhat What;

// Establish what we will be configuring
//
   What = (isRedir ? XrdCmsClientConfig::configSuper 
                   : XrdCmsClientConfig::configServer);

// Steal the manlist as we might have to report it
//
   if (isProxy) {myManList = config.PanList; config.PanList = 0;}
      else      {myManList = config.ManList; config.ManList = 0;}

// Set the error dest and simply call the configration object and if
// successful, run the Admin thread. Note that unlike FinderRMT, we do not
// extract the security function pointer or the network object pointer from
// the environment as we don't need these at all.
//
   if (config.Configure(cfn, What, XrdCmsClientConfig::configNorm)) return 0;
   return RunAdmin(config.CMSPath);
}
  
/******************************************************************************/
/*                                L o c a t e                                 */
/******************************************************************************/
  
int XrdCmsFinderTRG::Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                            XrdOucEnv *Env)
{
   char *mBuff;
   int mBlen, n;

// We only support locate on the local configuration
//
   if (!(flags & SFS_O_LOCATE) || !(flags & SFS_O_LOCAL))
      {Resp.setErrInfo(EINVAL, "Invalid locate option for target config.");
       return SFS_ERROR;
      }

// Get the buffer for the result
//
   mBuff = Resp.getMsgBuff(mBlen);

// Return information
//
   n = snprintf(mBuff, mBlen, "localhost:0/%c", (Active ? 'a' : 'd'));
   Resp.setErrCode(n);
   return SFS_DATA;
}
  
/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
int XrdCmsFinderTRG::Release(int rNum)
{
   int resOld;

// Lock the variables of interest
//
   rrMutex.Lock();
   resOld = resCur;

// If reserve/release not enabled or we have a non-positive value, return
//
   if (resMax < 0 || rNum <= 0) {rrMutex.UnLock(); return resOld;}

// Adjust resource and check if we can resume
//
   resCur += rNum;
   if (resCur > resMax) resCur = resMax;
   if (resOld < 1 && resCur > 0) Resume(0);

// All done
//
   resOld = resCur;
   rrMutex.UnLock();
   return resOld;
}
  
/******************************************************************************/
/*                               R e m o v e d                                */
/******************************************************************************/
  
void XrdCmsFinderTRG::Removed(const char *path)
{
   char *data[4];
   int   dlen[4];

// Set up to notify the cluster that a file has been removed
//
   data[0] = (char *)"rmdid ";   dlen[0] = 6;
   data[1] = (char *)path;       dlen[1] = strlen(path);
   data[2] = (char *)"\n";       dlen[2] = 1;
   data[3] = 0;                  dlen[3] = 0;

// Now send the notification
//
   myData.Lock();
   if (Active && CMSp->Put((const char **)data, (const int *)dlen))
      {CMSp->Close(); Active = 0;}
   myData.UnLock();
}
  
/******************************************************************************/
/*                               R e s e r v e                                */
/******************************************************************************/
  
int XrdCmsFinderTRG::Reserve(int rNum)
{
   int resOld;

// Lock the variables of interest
//
   rrMutex.Lock();
   resOld = resCur;

// If reserve/release not enabled or we have a non-positive value, return
//
   if (resMax < 0 || rNum <= 0) {rrMutex.UnLock(); return resOld;}

// Adjust resource and check if we can suspend
//
   resCur -= rNum;
   if (resOld > 0 && resCur < 1) Suspend(0);

// All done
//
   resOld = resCur;
   rrMutex.UnLock();
   return resOld;
}
  
/******************************************************************************/
/*                              R e s o u r c e                               */
/******************************************************************************/
  
int XrdCmsFinderTRG::Resource(int rNum)
{
   int resOld;

// Lock the variables of interest
//
   rrMutex.Lock();
   resOld = (resMax < 0 ? 0 : resMax);

// If we have a non-positive value, return
//
   if (rNum <= 0) {rrMutex.UnLock(); return resOld;}

// Set the resource and adjust the current value as needed
//
   resMax = rNum;
   if (resCur > resMax) resCur = resMax;

// All done
//
   rrMutex.UnLock();
   return resOld;
}
  
/******************************************************************************/
/*                                R e s u m e                                 */
/******************************************************************************/
  
void XrdCmsFinderTRG::Resume(int Perm)
{                               // 1234567890
   static const char *rPerm[2] = {"resume\n", 0};
   static const char *rTemp[2] = {"resume t\n", 0};
   static       int   lPerm[2] = { 7, 0};
   static       int   lTemp[2] = { 9, 0};

// Now send the notification
//
   myData.Lock();
   if (Active && CMSp->Put((const char **)(Perm ? rPerm : rTemp),
                           (const int *)  (Perm ? lPerm : lTemp)))
      {CMSp->Close(); Active = 0;}
   myData.UnLock();
}

/******************************************************************************/
/*                               S u s p e n d                                */
/******************************************************************************/
  
void XrdCmsFinderTRG::Suspend(int Perm)
{                               // 1234567890
   static const char *sPerm[2] = {"suspend\n", 0};
   static const char *sTemp[2] = {"suspend t\n", 0};
   static       int   lPerm[2] = { 8, 0};
   static       int   lTemp[2] = {10, 0};

// Now send the notification
//
   if (Active && CMSp->Put((const char **)(Perm ? sPerm : sTemp),
                           (const int *)  (Perm ? lPerm : lTemp)))
      {CMSp->Close(); Active = 0;}
   myData.UnLock();
}

/******************************************************************************/
/*                              R u n A d m i n                               */
/******************************************************************************/
  
int XrdCmsFinderTRG::RunAdmin(char *Path)
{
   pthread_t tid;

// Make sure we have a path to the cmsd
//
   if (!(CMSPath = Path))
      {Say.Emsg("Config", "Unable to determine cms admin path"); return 0;}

// Start a thread to connect with the local cmsd
//
   if (XrdSysThread::Run(&tid, XrdCmsStartRsp, (void *)this, 0, "cms i/f"))
      {Say.Emsg("Config", errno, "start cmsd interface"); return 0;}

   return 1;
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
void *XrdCmsFinderTRG::Start()
{
   XrdCmsRRData Data;

// First step is to connect to the local cmsd. We also establish a binary
// read stream (old olbd's never used it) to get requests that can only be
// executed by the xrootd (e.g., rm and mv).
//
   while(1)
        {do {Hookup();

             // Login to cmsd
             //
             myData.Lock();
             CMSp->Put(Login);
             myData.UnLock();

             // Get the FD for this connection
             //
             Data.Routing = CMSp->FDNum();

             // Put up a read to process local requests. Sould the cmsd die,
             // we will notice and try to reconnect.
             //
             while(recv(Data.Routing, &Data.Request, sizeof(Data.Request),
                        MSG_WAITALL) > 0 && Process(Data)) {}
             break;
            } while(1);

         // The cmsd went away
         //
         myData.Lock();
         CMSp->Close();
         Active = 0;
         myData.UnLock();
         Say.Emsg("Finder", "Lost contact with cmsd via", CMSPath);
         XrdSysTimer::Wait(10*1000);
        }

// We should never get here
//
   return (void *)0;
}
  
/******************************************************************************/
/*                                V C h e c k                                 */
/******************************************************************************/
  
bool XrdCmsFinderTRG::VCheck(XrdVersionInfo &urVersion)
{
   return XrdSysPlugin::VerCmp(urVersion, myVersion);
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                H o o k u p                                 */
/******************************************************************************/
  
void XrdCmsFinderTRG::Hookup()
{
   struct stat buf;
   XrdNetSocket Sock(&Say);
   int opts = 0, tries = 6;

// Wait for the cmsd path to be created
//
   while(stat(CMSPath, &buf))
        {if (!tries--)
            {Say.Emsg("Finder", "Waiting for cms path", CMSPath); tries=6;}
         XrdSysTimer::Wait(10*1000);
        }

// We can now try to connect
//
   tries = 0;
   while(Sock.Open(CMSPath, -1, opts) < 0)
        {if (!tries--)
            {opts = XRDNET_NOEMSG;
             tries = 6;
            } else if (!tries) opts = 0;
         XrdSysTimer::Wait(10*1000);
        };

// Transfer the socket FD to a stream
//
   myData.Lock();
   Active = 1;
   CMSp->Attach(Sock.Detach());
   myData.UnLock();

// Tell the world
//
   Say.Emsg("Finder", "Connected to cmsd via", CMSPath);
}

/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/
  
int XrdCmsFinderTRG::Process(XrdCmsRRData &Data)
{
   EPNAME("Process")
   static const int maxReqSize = 16384;
   static       int Wmsg = 255;
   const char *myArgs, *myArgt, *Act;
   char buff[16];
   int rc;

// Decode the length and get the rest of the data
//
   Data.Dlen = static_cast<int>(ntohs(Data.Request.datalen));
   if (!(Data.Dlen)) {myArgs = myArgt = 0;}
      else {if (Data.Dlen > maxReqSize)
               {Say.Emsg("Finder","Request args too long from local cmsd");
                return 0;
               }
            if ((!Data.Buff || Data.Blen < Data.Dlen)
            &&  !Data.getBuff(Data.Dlen))
               {Say.Emsg("Finder", "No buffers to serve local cmsd");
                return 0;
               }
            if (recv(Data.Routing,Data.Buff,Data.Dlen,MSG_WAITALL) != Data.Dlen)
                return 0;
            myArgs = Data.Buff; myArgt = Data.Buff + Data.Dlen;
           }

// Process the request as needed. We ignore opaque information for now.
// If the request is not valid is could be that we lost sync on the connection.
// The only way to recover is to tear it down and start over.
//
   switch(Data.Request.rrCode)
         {case kYR_mv:    Act = "mv";                             break;
          case kYR_rm:    Act = "rm";    Data.Path2 = (char *)""; break;
          case kYR_rmdir: Act = "rmdir"; Data.Path2 = (char *)""; break;
          default: sprintf(buff, "%d", Data.Request.rrCode);
                   Say.Emsg("Finder","Local cmsd sent an invalid request -",buff);
                   return 0;
         }

// Parse the arguments
//
   if (!myArgs || !Parser.Parse(int(Data.Request.rrCode),myArgs,myArgt,&Data))
      {Say.Emsg("Finder", "Local cmsd sent a badly formed",Act,"request");
       return 1;
      }
   DEBUG("cmsd requested " <<Act <<" " <<Data.Path <<' ' <<Data.Path2);

// If we have no storage system then issue a warning but otherwise
// ignore this operation (this may happen in proxy mode).
//
   if (SS == 0)
      {Wmsg++;
       if (!(Wmsg & 255)) Say.Emsg("Finder", "Local cmsd request",Act,
                                   "ignored; no storage system provided.");
       return 1;
      }

// Perform the request
//
   switch(Data.Request.rrCode)
         {case kYR_mv:    rc = SS->Rename(Data.Path, Data.Path2);   break;
          case kYR_rm:    rc = SS->Unlink(Data.Path);               break;
          case kYR_rmdir: rc = SS->Remdir(Data.Path);               break;
          default:        rc = 0;                                   break;
         }
   if (rc) Say.Emsg("Finder", rc, Act, Data.Path);

// All Done
//
   return 1;
}
