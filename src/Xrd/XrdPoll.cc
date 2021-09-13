/******************************************************************************/
/*                                                                            */
/*                            X r d P o l l . c c                             */
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

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
  
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdProtocol.hh"

#define  TRACE_IDENT pInfo.Link.ID
#include "Xrd/XrdTrace.hh"

#if defined( __linux__ )
#include "Xrd/XrdPollE.hh"
//#include "Xrd/XrdPollPoll.hh"
#else
#include "Xrd/XrdPollPoll.hh"
#endif

#include "Xrd/XrdPollInfo.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdPoll_End : public XrdProtocol
{
public:

void          DoIt() {}

XrdProtocol  *Match(XrdLink *lp) {return (XrdProtocol *)0;}

int           Process(XrdLink *lp) {return -1;}

void          Recycle(XrdLink *lp, int x, const char *y) {}

int           Stats(char *buff, int blen, int do_sync=0) {return 0;}

      XrdPoll_End() : XrdProtocol("link termination") {}
     ~XrdPoll_End() {}
};

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/
  
       XrdPoll   *XrdPoll::Pollers[XRD_NUMPOLLERS] = {0, 0, 0};

       XrdSysMutex  XrdPoll::doingAttach;

       const char *XrdPoll::TraceID = "Poll";

namespace XrdGlobal
{
extern XrdSysError  Log;
extern XrdScheduler Sched;
}

using namespace XrdGlobal;

/******************************************************************************/
/*              T h r e a d   S t a r t u p   I n t e r f a c e               */
/******************************************************************************/

struct XrdPollArg
       {XrdPoll      *Poller;
        int            retcode;
        XrdSysSemaphore PollSync;

        XrdPollArg() : PollSync(0, "poll sync") {}
       ~XrdPollArg()               {}
       };

  
void *XrdStartPolling(void *parg)
{
     struct XrdPollArg *PArg = (struct XrdPollArg *)parg;
     PArg->Poller->Start(&(PArg->PollSync), PArg->retcode);
     return (void *)0;
}
 
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdPoll::XrdPoll()
{
   int fildes[2];

   TID=0;
   numAttached=numEnabled=numEvents=numInterrupts=0;

   if (XrdSysFD_Pipe(fildes) == 0)
      {CmdFD = fildes[1];
       ReqFD = fildes[0];
      } else {
       CmdFD = ReqFD = -1;
       Log.Emsg("Poll", errno, "create poll pipe");
      }
   PipeBuff        = 0;
   PipeBlen        = 0;
   PipePoll.fd     = ReqFD;
   PipePoll.events = POLLIN | POLLRDNORM;
}

/******************************************************************************/
/*                                A t t a c h                                 */
/******************************************************************************/
  
int XrdPoll__Attach(XrdLink *lp) {return lp->Activate();}

int XrdPoll::Attach(XrdPollInfo &pInfo)
{
   int i;
   XrdPoll *pp;

// We allow only one attach at a time to simplify the processing
//
   doingAttach.Lock();

// Find a poller with the smallest number of entries
//
   pp = Pollers[0];
   for (i = 1; i < XRD_NUMPOLLERS; i++)
       if (pp->numAttached > Pollers[i]->numAttached) pp = Pollers[i];

// Include this FD into the poll set of the poller
//
   if (!pp->Include(pInfo)) {doingAttach.UnLock(); return 0;}

// Complete the link setup
//
   pInfo.Poller = pp;
   pp->numAttached++;
   doingAttach.UnLock();
   TRACEI(POLL, "FD " <<pInfo.FD <<" attached to poller " <<pp->PID
                <<"; num=" <<pp->numAttached);
   return 1;                                                           
}

/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/
  
void XrdPoll::Detach(XrdPollInfo &pInfo)
{
   XrdPoll *pp;

// If link is not attached, simply return
//
   if (!(pp = pInfo.Poller)) return;

// Exclude this link from the associated poll set
//
   pp->Exclude(pInfo);

// Make sure we are consistent
//
   doingAttach.Lock();
   if (!pp->numAttached)
      {Log.Emsg("Poll","Underflow detaching", pInfo.Link.ID); abort();}
   pp->numAttached--;
   doingAttach.UnLock();
   TRACEI(POLL, "FD " <<pInfo.FD <<" detached from poller " <<pp->PID
                <<"; num=" <<pp->numAttached);
}

/******************************************************************************/
/*                                F i n i s h                                 */
/******************************************************************************/
  
int XrdPoll::Finish(XrdPollInfo &pInfo, const char *etxt)
{
   static XrdPoll_End LinkEnd;

// If this link is already scheduled for termination, ignore this call.
//
   if (pInfo.Link.getProtocol() == &LinkEnd)
      {TRACEI(POLL, "Link " <<pInfo.FD <<" already terminating; "
                    <<(etxt ? etxt : "") <<" request ignored.");
       return 0;
      }

// Set the protocol pointer to be link termination
//
   pInfo.Link.setProtocol(&LinkEnd, false, true);
   if (!etxt) etxt = "reason unknown";
   pInfo.Link.setEtext(etxt);
   TRACEI(POLL, "Link " <<pInfo.FD <<" terminating: " <<etxt);
   return 1;
}
  
/******************************************************************************/
/*                            g e t R e q u e s t                             */
/******************************************************************************/

// Warning: This method runs unlocked. The caller must have exclusive use of
//          the ReqBuff otherwise unpredictable results will occur.

int XrdPoll::getRequest()
{
   ssize_t rlen;
   int rc;

// See if we are to resume a read or start a fresh one
//
   if (!PipeBlen) 
      {PipeBuff = (char *)&ReqBuff; PipeBlen = sizeof(ReqBuff);}

// Wait for the next request. Some OS's (like Linux) don't support non-blocking
// pipes. So, we must front the read with a poll.
//
   do {rc = poll(&PipePoll, 1, 0);}
      while(rc < 0 && (errno == EAGAIN || errno == EINTR));
   if (rc < 1) return 0;

// Now we can put up a read without a delay. Normally a full command will be
// present. Under some heavy conditions, this may not be the case.
//
   do {rlen = read(ReqFD, PipeBuff, PipeBlen);} 
      while(rlen < 0 && errno == EINTR);
   if (rlen <= 0)
      {if (rlen) Log.Emsg("Poll", errno, "read from request pipe");
       return 0;
      }

// Check if all the data has arrived. If not all the data is present, defer
// this request until more data arrives.
//
   if (!(PipeBlen -= rlen)) return 1;
   PipeBuff += rlen;
   TRACE(POLL, "Poller " <<PID <<" still needs " <<PipeBlen <<" req pipe bytes");
   return 0;
}

/******************************************************************************/
/*                             P o l l 2 T e x t                              */
/******************************************************************************/
  
char *XrdPoll::Poll2Text(short events)
{
   if (events & POLLERR) return strdup("socket error");

   if (events & POLLHUP) return strdup("hangup");

   if (events & POLLNVAL) return strdup("socket closed");

  {char buff[64];
   sprintf(buff, "unusual event (%.4x)", events);
   return strdup(buff);
  }
  return (char *)0;
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
int XrdPoll::Setup(int numfd)
{
   pthread_t tid;
   int maxfd, retc, i;
   struct XrdPollArg PArg;

// Calculate the number of table entries per poller
//
   maxfd  = (numfd / XRD_NUMPOLLERS) + 16;

// Verify that we initialized the poller table
//
   for (i = 0; i < XRD_NUMPOLLERS; i++)
       {if (!(Pollers[i] = newPoller(i, maxfd))) return 0;
        Pollers[i]->PID = i;

   // Now start a thread to handle this poller object
   //
        PArg.Poller = Pollers[i];
        PArg.retcode= 0;
        TRACE(POLL, "Starting poller " <<i);
        if ((retc = XrdSysThread::Run(&tid,XrdStartPolling,(void *)&PArg,
                                      XRDSYSTHREAD_BIND, "Poller")))
           {Log.Emsg("Poll", retc, "create poller thread"); return 0;}
        Pollers[i]->TID = tid;
        PArg.PollSync.Wait();
        if (PArg.retcode)
           {Log.Emsg("Poll", PArg.retcode, "start poller");
            return 0;
           }
       }

// All done
//
   return 1;
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdPoll::Stats(char *buff, int blen, int do_sync)
{
   static const char statfmt[] = "<stats id=\"poll\"><att>%d</att>"
   "<en>%d</en><ev>%d</ev><int>%d</int></stats>";
   int i, numatt = 0, numen = 0, numev = 0, numint = 0;
   XrdPoll *pp;

// Return number of bytes if so wanted
//
   if (!buff) return (sizeof(statfmt)+(4*16))*XRD_NUMPOLLERS;

// Get statistics. While we wish we could honor do_sync, doing so would be
// costly and hardly worth it. So, we do not include code such as:
//    x = pp->y; if (do_sync) while(x != pp->y) x = pp->y; tot += x;
//
   for (i = 0; i < XRD_NUMPOLLERS; i++)
       {pp = Pollers[i];
        numatt += pp->numAttached; 
        numen  += pp->numEnabled;
        numev  += pp->numEvents;
        numint += pp->numInterrupts;
       }

// Format and return
//
   return snprintf(buff, blen, statfmt, numatt, numen, numev, numint);
}
  
/******************************************************************************/
/*              I m p l e m e n t a t i o n   S p e c i f i c s               */
/******************************************************************************/

#if defined( __linux__ )
#include "Xrd/XrdPollE.icc"
//#include "Xrd/XrdPollPoll.icc"
#else
#include "Xrd/XrdPollPoll.icc"
#endif
