/******************************************************************************/
/*                                                                            */
/*                            X r d L i n k . c c                             */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <poll.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/uio.h>

#if defined(__linux__) || defined(__GNU__)
#include <netinet/tcp.h>
#if !defined(TCP_CORK)
#undef HAVE_SENDFILE
#endif
#endif

#ifdef HAVE_SENDFILE

#ifndef __APPLE__
#if !defined(__FreeBSD__)
#include <sys/sendfile.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#endif
#endif

#endif

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "Xrd/XrdBuffer.hh"

#include "Xrd/XrdLinkCtl.hh"
#include "Xrd/XrdPoll.hh"

#define  TRACE_IDENT ID
#include "Xrd/XrdTrace.hh"

#include "XrdSys/XrdSysError.hh"

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdGlobal
{
extern XrdSysError  Log;
};

using namespace XrdGlobal;
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

#if defined(HAVE_SENDFILE)
       bool        XrdLink::sfOK = true;
#else
       bool        XrdLink::sfOK = false;
#endif

namespace
{
const char   KillMax =   60;
const char   KillMsk = 0x7f;
const char   KillXwt = 0x80;

const char  *TraceID = "Link";
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdLink::XrdLink(XrdLinkXeq &lxq) : XrdJob("connection"), linkXQ(lxq),
                                    HostName(0)
{
   memset(rsvd1, 0, sizeof(rsvd1));
   memset(rsvd2, 0, sizeof(rsvd2));
   ResetLink();
}

void XrdLink::ResetLink()
{
   if (HostName) {free(HostName); HostName = 0;}
   Instance =  0;
   isBridged= false;
   isTLS    = false;
}

/******************************************************************************/
/*                              A c t i v a t e                               */
/******************************************************************************/
  
bool XrdLink::Activate()
{

// Attach this link to a poller
//
   return XrdPoll::Attach(linkXQ.PollInfo);
}

/******************************************************************************/
/*                              A d d r I n f o                               */
/******************************************************************************/

XrdNetAddrInfo *XrdLink::AddrInfo() {return linkXQ.AddrInfo();}
  
/******************************************************************************/
/*                             a r m B r i d g e                              */
/******************************************************************************/

void XrdLink::armBridge() {isBridged = 1;}
  
/******************************************************************************/
/*                               B a c k l o g                                */
/******************************************************************************/
  
int XrdLink::Backlog() {return linkXQ.Backlog();}

/******************************************************************************/
/*                                C l i e n t                                 */
/******************************************************************************/
  
int XrdLink::Client(char *nbuf, int nbsz) {return linkXQ.Client(nbuf, nbsz);}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
int XrdLink::Close(bool defer) {return linkXQ.Close(defer);}

/******************************************************************************/
/* Protected:                       D o I t                                   */
/******************************************************************************/
 
void XrdLink::DoIt() {} // This is overridden by the implementation
  
/******************************************************************************/
/*                                E n a b l e                                 */
/******************************************************************************/
  
void XrdLink::Enable()
{
   if (linkXQ.PollInfo.Poller) linkXQ.PollInfo.Poller->Enable(linkXQ.PollInfo);
}

/******************************************************************************/
/*                                 F D n u m                                  */
/******************************************************************************/

int XrdLink::FDnum()
{
  return linkXQ.PollInfo.FD;
}
               
/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/

XrdLink *XrdLink::Find(int &curr, XrdLinkMatch *who)
                      {return XrdLinkCtl::Find(curr, who);}
  
/******************************************************************************/
/*                            g e t I O S t a t s                             */
/******************************************************************************/

int XrdLink::getIOStats(long long &inbytes, long long &outbytes,
                             int  &numstall,     int  &numtardy)
                       {return linkXQ.getIOStats(inbytes,  outbytes,
                                                 numstall, numtardy);
                       }
  
/******************************************************************************/
/*                               g e t N a m e                                */
/******************************************************************************/

// Warning: curr must be set to a value of 0 or less on the initial call and
//          not touched therafter unless null is returned. Returns the length
//          the name in nbuf.
//
int XrdLink::getName(int &curr, char *nbuf, int nbsz, XrdLinkMatch *who)
                    {return XrdLinkCtl::getName(curr, nbuf, nbsz, who);}

/******************************************************************************/
/*                          g e t P e e r C e r t s                           */
/******************************************************************************/

XrdTlsPeerCerts *XrdLink::getPeerCerts()
{
   return linkXQ.getPeerCerts();
}
  
/******************************************************************************/
/*                           g e t P r o t o c o l                            */
/******************************************************************************/

XrdProtocol *XrdLink::getProtocol() {return linkXQ.getProtocol();}
  
/******************************************************************************/
/*                                  H o l d                                   */
/******************************************************************************/

void XrdLink::Hold(bool lk) 
{
   (lk ? linkXQ.LinkInfo.opMutex.Lock() : linkXQ.LinkInfo.opMutex.UnLock());
}
  
/******************************************************************************/
/*                              i s F l a w e d                               */
/******************************************************************************/

bool XrdLink::isFlawed() const {return linkXQ.LinkInfo.Etext != 0;}
  
/******************************************************************************/
/*                            i s I n s t a n c e                             */
/******************************************************************************/
  
bool XrdLink::isInstance(unsigned int inst) const
                        {return Instance == inst && linkXQ.PollInfo.FD >= 0;}

/******************************************************************************/
/*                                  N a m e                                   */
/******************************************************************************/

const char *XrdLink::Name() const {return linkXQ.Name();}
  
/******************************************************************************/
/*                               N e t A d d r                                */
/******************************************************************************/
const
XrdNetAddr *XrdLink::NetAddr() const {return linkXQ.NetAddr();}
  
/******************************************************************************/
/*                                  P e e k                                   */
/******************************************************************************/
  
int XrdLink::Peek(char *Buff, int Blen, int timeout)
{
   if (isTLS) return linkXQ.TLS_Peek(Buff, Blen, timeout);
   else       return linkXQ.Peek    (Buff, Blen, timeout);
}
  
/******************************************************************************/
/*                                  R e c v                                   */
/******************************************************************************/
  
int XrdLink::Recv(char *Buff, int Blen)
{
   if (isTLS) return linkXQ.TLS_Recv(Buff, Blen);
   else       return linkXQ.Recv    (Buff, Blen);
}

/******************************************************************************/

int XrdLink::Recv(char *Buff, int Blen, int timeout)
{
   if (isTLS) return linkXQ.TLS_Recv(Buff, Blen, timeout);
   else       return linkXQ.Recv    (Buff, Blen, timeout);
}

/******************************************************************************/

int XrdLink::Recv(const struct iovec *iov, int iocnt, int timeout)
{
// Execute the send
//
   if (isTLS) return linkXQ.TLS_Recv(iov, iocnt, timeout);
   else       return linkXQ.Recv    (iov, iocnt, timeout);
}

/******************************************************************************/
/*                               R e c v A l l                                */
/******************************************************************************/
  
int XrdLink::RecvAll(char *Buff, int Blen, int timeout)
{
   if (isTLS) return linkXQ.TLS_RecvAll(Buff, Blen, timeout);
   else       return linkXQ.RecvAll    (Buff, Blen, timeout);
}

/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

bool XrdLink::Register(const char *hName)
{
   return linkXQ.Register(hName);
}
  
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdLink::Send(const char *Buff, int Blen)
{
   if (isTLS) return linkXQ.TLS_Send(Buff, Blen);
   else       return linkXQ.Send    (Buff, Blen);
}

/******************************************************************************/
  
int XrdLink::Send(const struct iovec *iov, int iocnt, int bytes)
{
// Allways make sure we have a total byte count
//
   if (!bytes) for (int i = 0; i < iocnt; i++) bytes += iov[i].iov_len;

// Execute the send
//
   if (isTLS) return linkXQ.TLS_Send(iov, iocnt, bytes);
   else       return linkXQ.Send    (iov, iocnt, bytes);
}
 
/******************************************************************************/

int XrdLink::Send(const sfVec *sfP, int sfN)
{
// Make sure we have valid vector count
//
   if (sfN < 1 || sfN > XrdOucSFVec::sfMax)
      {Log.Emsg("Link", E2BIG, "send file to", ID);
       return -1;
      }

// Do the send
//
   if (isTLS) return linkXQ.TLS_Send(sfP, sfN);
   else       return linkXQ.Send    (sfP, sfN);
}

/******************************************************************************/
/*                             S e r i a l i z e                              */
/******************************************************************************/
  
void XrdLink::Serialize()
{

// This is meant to make sure that no protocol objects are refering to this
// link so that we can safely run in pseudo single thread mode for critical
// functions.
//
   linkXQ.LinkInfo.opMutex.Lock();
   if (linkXQ.LinkInfo.InUse <= 1) linkXQ.LinkInfo.opMutex.UnLock();
      else {linkXQ.LinkInfo.doPost++;
            linkXQ.LinkInfo.opMutex.UnLock();
            TRACEI(DEBUG, "Waiting for link serialization; use=" 
                          <<linkXQ.LinkInfo.InUse);
            linkXQ.LinkInfo.IOSemaphore.Wait();
           }
}

/******************************************************************************/
/*                              s e t E t e x t                               */
/******************************************************************************/

int XrdLink::setEtext(const char *text)
{
     linkXQ.LinkInfo.opMutex.Lock();
     if (linkXQ.LinkInfo.Etext) free(linkXQ.LinkInfo.Etext);
     linkXQ.LinkInfo.Etext = (text ? strdup(text) : 0);
     linkXQ.LinkInfo.opMutex.UnLock();
     return -1;
}
  
/******************************************************************************/
/*                                 s e t I D                                  */
/******************************************************************************/
  
void XrdLink::setID(const char *userid, int procid)
                   {linkXQ.setID(userid, procid);}
 
/******************************************************************************/
/*                                 s e t N B                                  */
/******************************************************************************/
  
bool XrdLink::setNB() {return linkXQ.setNB();}

/******************************************************************************/
/*                           s e t L o c a t i o n                            */
/******************************************************************************/
  
void XrdLink::setLocation(XrdNetAddrInfo::LocInfo &loc)
                         {linkXQ.setLocation(loc);}

/******************************************************************************/
/*                           s e t P r o t o c o l                            */
/******************************************************************************/
  
XrdProtocol *XrdLink::setProtocol(XrdProtocol *pp, bool runit, bool push)
{

// Ask the mplementation to set or push the protocol
//
   XrdProtocol *op = linkXQ.setProtocol(pp, push);

// Run the protocol if so requested
//
   if (runit) DoIt();
   return op;
}

/******************************************************************************/
/*                           s e t P r o t N a m e                            */
/******************************************************************************/
  
void XrdLink::setProtName(const char *name)
{

// Ask the mplementation to set the name.
//
   linkXQ.setProtName(name);
}
  
/******************************************************************************/
/*                                s e t R e f                                 */
/******************************************************************************/
  
void XrdLink::setRef(int use)
{
   linkXQ.LinkInfo.opMutex.Lock();
   TRACEI(DEBUG,"Setting FD "<<linkXQ.LinkInfo.FD <<" ref to " 
                <<linkXQ.LinkInfo.InUse <<'+'
                 <<use <<" post=" <<linkXQ.LinkInfo.doPost);
   linkXQ.LinkInfo.InUse += use;

         if (!linkXQ.LinkInfo.InUse)
            {linkXQ.LinkInfo.InUse = 1; linkXQ.LinkInfo.opMutex.UnLock();
             Log.Emsg("Link", "Zero use count for", ID);
            }
    else if (linkXQ.LinkInfo.InUse == 1 && linkXQ.LinkInfo.doPost)
            {while(linkXQ.LinkInfo.doPost)
                {linkXQ.LinkInfo.IOSemaphore.Post();
                 TRACEI(CONN, "setRef posted link");
                 linkXQ.LinkInfo.doPost--;
                }
             linkXQ.LinkInfo.opMutex.UnLock();
            }
    else if (linkXQ.LinkInfo.InUse < 0)
            {linkXQ.LinkInfo.InUse = 1;
             linkXQ.LinkInfo.opMutex.UnLock();
             Log.Emsg("Link", "Negative use count for", ID);
            }
    else linkXQ.LinkInfo.opMutex.UnLock();
}
 
/******************************************************************************/
/*                                s e t T L S                                 */
/******************************************************************************/

bool XrdLink::setTLS(bool enable, XrdTlsContext *ctx)
{
// If we are already in a compatible mode, we are done
//
   if (isTLS == enable) return true;

   return linkXQ.setTLS(enable, ctx);
}
  
/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/

void XrdLink::Shutdown(bool getLock) {linkXQ.Shutdown(getLock);}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

int XrdLink::Stats(char *buff, int blen, bool do_sync)
                  {return XrdLinkXeq::Stats(buff, blen, do_sync);}
  
/******************************************************************************/
/*                             s y n c S t a t s                              */
/******************************************************************************/
  
void XrdLink::syncStats(int *ctime) {linkXQ.syncStats(ctime);}
 
/******************************************************************************/
/*                             T e r m i n a t e                              */
/******************************************************************************/
  
int XrdLink::Terminate(const char *owner, int fdnum, unsigned int inst)
{

// Find the correspodning link and check for self-termination. Otherwise, if
// the target link is owned by the owner then ask the link to terminate itself.
//
   if (!owner)
      {XrdLink *lp;
       char *cp;
       if (!(lp = XrdLinkCtl::fd2link(fdnum, inst))) return -ESRCH;
       if (lp == this) return 0;
       lp->Hold(true);
       if (!(cp = index(ID, ':')) || strncmp(lp->ID, ID, cp-ID)
       || strcmp(HostName, lp->Host()))
          {lp->Hold(false);
           return -EACCES;
          }
       int rc = lp->Terminate(ID, fdnum, inst);
       lp->Hold(false);
       return rc;
      }

// At this pint, we are excuting in the context of the target link.
// If this link is now dead, simply ignore the request. Typically, this
// indicates a race condition that the server won.
//
   if ( linkXQ.PollInfo.FD != fdnum || Instance != inst
   || !linkXQ.PollInfo.Poller || !linkXQ.getProtocol()) return -EPIPE;

// Check if we have too many tries here
//
   int wTime, killTries;
   killTries = linkXQ.LinkInfo.KillCnt & KillMsk;
   if (killTries > KillMax) return -ETIME;

// Wait time increases as we have more unsuccessful kills. Update numbers.
//
   wTime = killTries++;
   linkXQ.LinkInfo.KillCnt = killTries | KillXwt;

// Make sure we can disable this link. If not, then force the caller to wait
// a tad more than the read timeout interval.
//
   if (!linkXQ.PollInfo.isEnabled || linkXQ.LinkInfo.InUse > 1
   ||  linkXQ.LinkInfo.KillcvP)
      {wTime = wTime*2+XrdLinkCtl::waitKill;
       return (wTime > 60 ? 60: wTime);
      }

// Set the pointer to our condvar. We are holding the opMutex to prevent a race.
//
   XrdSysCondVar killDone(0);
   linkXQ.LinkInfo.KillcvP = &killDone;
   killDone.Lock();

// We can now disable the link and schedule a close
//
   char buff[1024];
   snprintf(buff, sizeof(buff), "ended by %s", owner);
   buff[sizeof(buff)-1] = '\0';
   linkXQ.PollInfo.Poller->Disable(linkXQ.PollInfo, buff);
   linkXQ.LinkInfo.opMutex.UnLock();

// Now wait for the link to shutdown. This avoids lock problems.
//
   if (killDone.Wait(int(XrdLinkCtl::killWait))) wTime += XrdLinkCtl::killWait;
      else wTime = -EPIPE;
   killDone.UnLock();

// Reobtain the opmutex so that we can zero out the pointer the condvar pntr
// This is really stupid code but because we don't have a way of associating
// an arbitrary mutex with a condvar. But since this code is rarely executed
// the ugliness is sort of tolerable.
//
   linkXQ.LinkInfo.opMutex.Lock();
   linkXQ.LinkInfo.KillcvP = 0;
   linkXQ.LinkInfo.opMutex.UnLock();

// Do some tracing
//
   TRACEI(DEBUG,"Terminate " << (wTime <= 0 ? "complete ":"timeout ") <<wTime);
   return wTime;
}

/******************************************************************************/
/*                               t i m e C o n                                */
/******************************************************************************/

time_t XrdLink::timeCon() const {return linkXQ.LinkInfo.conTime;}
  
/******************************************************************************/
/*                                U s e C n t                                 */
/******************************************************************************/

int XrdLink::UseCnt() const {return linkXQ.LinkInfo.InUse;}
  
/******************************************************************************/
/*                                v e r T L S                                 */
/******************************************************************************/
  
const char *XrdLink::verTLS()
{
   return (isTLS ? linkXQ.verTLS() : "");
}

/******************************************************************************/
/*                             W a i t 4 D a t a                              */
/******************************************************************************/
  
int XrdLink::Wait4Data(int timeout)
{
   struct pollfd polltab = {linkXQ.PollInfo.FD, POLLIN|POLLRDNORM, 0};
   int retc;

// Issue poll and do preliminary check
//
   do {retc = poll(&polltab, 1, timeout);} while(retc < 0 && errno == EINTR);
   if (retc != 1)
      {if (retc == 0) return 0;
       Log.Emsg("Link", -errno, "poll", ID);
       return -1;
      }

// Verify it is safe to read now
//
   if (!(polltab.revents & (POLLIN|POLLRDNORM)))
      {Log.Emsg("Link", XrdPoll::Poll2Text(polltab.revents), "polling", ID);
       return -1;
      }
   return 1;
}
