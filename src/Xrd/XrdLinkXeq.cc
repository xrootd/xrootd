/******************************************************************************/
/*                                                                            */
/*                         X r d L i n k X e q . c c                          */
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

#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>

#if defined(__linux__) || defined(__GNU__)
#include <netinet/tcp.h>
#if !defined(TCP_CORK)
#undef HAVE_SENDFILE
#endif
#endif

#ifdef HAVE_SENDFILE

#if defined(__solaris__) || defined(__linux__) || defined(__GNU__)
#include <sys/sendfile.h>
#endif

#endif

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdLinkCtl.hh"
#include "Xrd/XrdLinkXeq.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdSendQ.hh"
#include "Xrd/XrdTcpMonPin.hh"

#define  TRACE_IDENT ID
#include "Xrd/XrdTrace.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace
{
int getIovMax()
{
int maxiov;
#ifdef _SC_IOV_MAX
    if ((maxiov = sysconf(_SC_IOV_MAX)) > 0) return maxiov;
#endif
#ifdef IOV_MAX
    return IOV_MAX;
#else
    return 1024;
#endif
}
};

namespace XrdGlobal
{
extern XrdSysError    Log;
extern XrdScheduler   Sched;
extern XrdTlsContext *tlsCtx;
       XrdTcpMonPin  *TcpMonPin = 0;
extern int            devNull;
       int            maxIOV = getIovMax();
};

using namespace XrdGlobal;
  
/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/

       const char     *XrdLinkXeq::TraceID = "LinkXeq";

       long long       XrdLinkXeq::LinkBytesIn   = 0;
       long long       XrdLinkXeq::LinkBytesOut  = 0;
       long long       XrdLinkXeq::LinkConTime   = 0;
       long long       XrdLinkXeq::LinkCountTot  = 0;
       int             XrdLinkXeq::LinkCount     = 0;
       int             XrdLinkXeq::LinkCountMax  = 0;
       int             XrdLinkXeq::LinkTimeOuts  = 0;
       int             XrdLinkXeq::LinkStalls    = 0;
       int             XrdLinkXeq::LinkSfIntr    = 0;
       XrdSysMutex     XrdLinkXeq::statsMutex;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdLinkXeq::XrdLinkXeq() : XrdLink(*this), PollInfo((XrdLink &)*this)
{
   XrdLinkXeq::Reset();
}

void XrdLinkXeq::Reset()
{
   memcpy(Uname+sizeof(Uname)-7, "anon.0@", 7);
   strcpy(Lname, "somewhere");
   ID       = &Uname[sizeof(Uname)-5];
   Comment  = ID;
   sendQ    = 0;
   stallCnt = stallCntTot = 0;
   tardyCnt = tardyCntTot = 0;
   SfIntr   = 0;
   isIdle   = 0;
   BytesOut = BytesIn = BytesOutTot = BytesInTot = 0;
   LockReads= false;
   KeepFD   = false;
   Protocol = 0;
   ProtoAlt = 0;

   LinkInfo.Reset();
   PollInfo.Zorch();
   ResetLink();
}

/******************************************************************************/
/*                               B a c k l o g                                */
/******************************************************************************/
  
int XrdLinkXeq::Backlog()
{
   XrdSysMutexHelper lck(wrMutex);

// Return backlog information
//
   return (sendQ ? sendQ->Backlog() : 0);
}

/******************************************************************************/
/*                                C l i e n t                                 */
/******************************************************************************/
  
int XrdLinkXeq::Client(char *nbuf, int nbsz)
{
   int ulen;

// Generate full client name
//
   if (nbsz <= 0) return 0;
   ulen = (Lname - ID);
   if ((ulen + HNlen) >= nbsz) ulen = 0;
      else {strncpy(nbuf, ID, ulen);
            strcpy(nbuf+ulen, HostName);
            ulen += HNlen;
           }
   return ulen;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
int XrdLinkXeq::Close(bool defer)
{  XrdSysMutexHelper opHelper(LinkInfo.opMutex);
   int csec, fd, rc = 0;

// If a defer close is requested, we can close the descriptor but we must
// keep the slot number to prevent a new client getting the same fd number.
// Linux is peculiar in that any in-progress operations will remain in that
// state even after the FD is closed unless there is some activity either on
// the connection or an event occurs that causes an operation restart. We
// portably solve this problem by issuing a shutdown() on the socket prior
// closing it. On most platforms, this informs readers that the connection is
// gone (though not on old (i.e. <= 2.3) versions of Linux, sigh). Also, if
// nonblocking mode is enabled, we need to do this in a separate thread as
// a shutdown may block for a pretty long time if lots\ of messages are queued.
// We will ask the SendQ object to schedule the shutdown for us before it
// commits suicide.
// Note that we can hold the opMutex while we also get the wrMutex.
//
   if (defer)
      {if (!sendQ) Shutdown(false);
          else {TRACEI(DEBUG, "Shutdown FD " <<LinkInfo.FD<<" only via SendQ");
                LinkInfo.InUse++;
                LinkInfo.FD = -LinkInfo.FD; // Leave poll version untouched!
                wrMutex.Lock();
                sendQ->Terminate(this);
                sendQ = 0;
                wrMutex.UnLock();
               }
       return 0;
      }

// If we got here then this is not a deferred close so we just need to check
// if there is a sendq appendage we need to get rid of.
//
   if (sendQ)
      {wrMutex.Lock();
       sendQ->Terminate();
       sendQ = 0;
       wrMutex.UnLock();
      }

// Multiple protocols may be bound to this link. If it is in use, defer the
// actual close until the use count drops to one.
//
   while(LinkInfo.InUse > 1)
      {opHelper.UnLock();
       TRACEI(DEBUG, "Close FD "<<LinkInfo.FD <<" deferred, use count="
                     <<LinkInfo.InUse);
       Serialize();
       opHelper.Lock(&LinkInfo.opMutex);
      }
   LinkInfo.InUse--;
   Instance = 0;

// Add up the statistic for this link
//
   syncStats(&csec);

// Cleanup TLS if it is active
//
   if (isTLS) tlsIO.Shutdown();

// Clean this link up
//
   if (Protocol) {Protocol->Recycle(this, csec, LinkInfo.Etext); Protocol = 0;}
   if (ProtoAlt) {ProtoAlt->Recycle(this, csec, LinkInfo.Etext); ProtoAlt = 0;}
   if (LinkInfo.Etext) {free(LinkInfo.Etext); LinkInfo.Etext = 0;}
   LinkInfo.InUse    = 0;

// At this point we can have no lock conflicts, so if someone is waiting for
// us to terminate let them know about it. Note that we will get the condvar
// mutex while we hold the opMutex. This is the required order! We will also
// zero out the pointer to the condvar while holding the opmutex.
//
   if (LinkInfo.KillcvP)
      {LinkInfo.KillcvP->Lock();
       LinkInfo.KillcvP->Signal();
       LinkInfo.KillcvP->UnLock();
       LinkInfo.KillcvP = 0;
      }

// Remove ourselves from the poll table and then from the Link table. We may
// not hold on to the opMutex when we acquire the LTMutex. However, the link
// table needs to be cleaned up prior to actually closing the socket. So, we
// do some fancy footwork to prevent multiple closes of this link.
//
   fd = abs(LinkInfo.FD);
   if (PollInfo.FD > 0)
      {if (PollInfo.Poller) {XrdPoll::Detach(PollInfo); PollInfo.Poller = 0;}
       PollInfo.FD = -1;
       opHelper.UnLock();
       XrdLinkCtl::Unhook(fd);
      } else opHelper.UnLock();

// Invoke the TCP monitor if it was loaded.
//
   if (TcpMonPin && fd > 2)
      {XrdTcpMonPin::LinkInfo lnkInfo;
       lnkInfo.tident   = ID;
       lnkInfo.fd       = fd;
       lnkInfo.consec   = csec;
       lnkInfo.bytesIn  = BytesInTot;
       lnkInfo.bytesOut = BytesOutTot;
       TcpMonPin->Monitor(Addr, lnkInfo, sizeof(lnkInfo));
      }

// Close the file descriptor if it isn't being shared. Do it as the last
// thing because closes and accepts and not interlocked.
//
   if (fd >= 2) {if (KeepFD) rc = 0;
                    else rc = (close(fd) < 0 ? errno : 0);
                }
   if (rc) Log.Emsg("Link", rc, "close", ID);
   return rc;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
 
void XrdLinkXeq::DoIt()
{
   int rc;

// The Process() return code tells us what to do:
// < 0 -> Stop getting requests, 
//        -EINPROGRESS leave link disabled but otherwise all is well
//        -n           Error, disable and close the link
// = 0 -> OK, get next request, if allowed, o/w enable the link
// > 0 -> Slow link, stop getting requests  and enable the link
//
   if (Protocol)
      do {rc = Protocol->Process(this);} while (!rc && Sched.canStick());
      else {Log.Emsg("Link", "Dispatch on closed link", ID);
            return;
           }

// Either re-enable the link and cycle back waiting for a new request, leave
// disabled, or terminate the connection.
//
   if (rc >= 0)
      {if (PollInfo.Poller && !PollInfo.Poller->Enable(PollInfo)) Close();}
      else if (rc != -EINPROGRESS) Close();
}

/******************************************************************************/
/*                          g e t P e e r C e r t s                           */
/******************************************************************************/

XrdTlsPeerCerts *XrdLinkXeq::getPeerCerts()
{
   return (isTLS ? tlsIO.getCerts(true) : 0);
}
  
/******************************************************************************/
/*                                  P e e k                                   */
/******************************************************************************/
  
int XrdLinkXeq::Peek(char *Buff, int Blen, int timeout)
{
   XrdSysMutexHelper theMutex;
   struct pollfd polltab = {PollInfo.FD, POLLIN|POLLRDNORM, 0};
   ssize_t mlen;
   int retc;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Wait until we can actually read something
//
   isIdle = 0;
   do {retc = poll(&polltab, 1, timeout);} while(retc < 0 && errno == EINTR);
   if (retc != 1)
      {if (retc == 0) return 0;
       return Log.Emsg("Link", -errno, "poll", ID);
      }

// Verify it is safe to read now
//
   if (!(polltab.revents & (POLLIN|POLLRDNORM)))
      {Log.Emsg("Link", XrdPoll::Poll2Text(polltab.revents), "polling", ID);
       return -1;
      }

// Do the peek.
//
   do {mlen = recv(LinkInfo.FD, Buff, Blen, MSG_PEEK);}
      while(mlen < 0 && errno == EINTR);

// Return the result
//
   if (mlen >= 0) return int(mlen);
   Log.Emsg("Link", errno, "peek on", ID);
   return -1;
}
  
/******************************************************************************/
/*                                  R e c v                                   */
/******************************************************************************/
  
int XrdLinkXeq::Recv(char *Buff, int Blen)
{
   ssize_t rlen;

// Note that we will read only as much as is queued. Use Recv() with a
// timeout to receive as much data as possible.
//
   if (LockReads) rdMutex.Lock();
   isIdle = 0;
   do {rlen = read(LinkInfo.FD, Buff, Blen);} while(rlen < 0 && errno == EINTR);
   if (rlen > 0) AtomicAdd(BytesIn, rlen);
   if (LockReads) rdMutex.UnLock();

   if (rlen >= 0) return int(rlen);
   if (LinkInfo.FD >= 0) Log.Emsg("Link", errno, "receive from", ID);
   return -1;
}

/******************************************************************************/

int XrdLinkXeq::Recv(char *Buff, int Blen, int timeout)
{
   XrdSysMutexHelper theMutex;
   struct pollfd polltab = {PollInfo.FD, POLLIN|POLLRDNORM, 0};
   ssize_t rlen, totlen = 0;
   int retc;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Wait up to timeout milliseconds for data to arrive
//
   isIdle = 0;
   while(Blen > 0)
        {do {retc = poll(&polltab,1,timeout);} while(retc < 0 && errno == EINTR);
         if (retc != 1)
            {if (retc == 0)
                {tardyCnt++;
                 if (totlen)
                    {if ((++stallCnt & 0xff) == 1) TRACEI(DEBUG,"read timed out");
                     AtomicAdd(BytesIn, totlen);
                    }
                 return int(totlen);
                }
             return (LinkInfo.FD >= 0 ? Log.Emsg("Link",-errno,"poll",ID) : -1);
            }

         // Verify it is safe to read now
         //
         if (!(polltab.revents & (POLLIN|POLLRDNORM)))
            {Log.Emsg("Link", XrdPoll::Poll2Text(polltab.revents),
                              "polling", ID);
             return -1;
            }

         // Read as much data as you can. Note that we will force an error
         // if we get a zero-length read after poll said it was OK.
         //
         do {rlen = recv(LinkInfo.FD, Buff, Blen, 0);}
            while(rlen < 0 && errno == EINTR);
         if (rlen <= 0)
            {if (!rlen) return -ENOMSG;
             if (LinkInfo.FD > 0) Log.Emsg("Link", -errno, "receive from", ID);
             return -1;
            }
         totlen += rlen; Blen -= rlen; Buff += rlen;
        }

   AtomicAdd(BytesIn, totlen);
   return int(totlen);
}

/******************************************************************************/
  
int XrdLinkXeq::Recv(const struct iovec *iov, int iocnt, int timeout)
{
   XrdSysMutexHelper theMutex;
   struct pollfd polltab = {PollInfo.FD, POLLIN|POLLRDNORM, 0};
   int retc, rlen;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Wait up to timeout milliseconds for data to arrive
//
   isIdle = 0;
   do {retc = poll(&polltab,1,timeout);} while(retc < 0 && errno == EINTR);
   if (retc != 1)
      {if (retc == 0)
          {tardyCnt++;
           return 0;
          }
       return (LinkInfo.FD >= 0 ? Log.Emsg("Link",-errno,"poll",ID) : -1);
      }

// Verify it is safe to read now
//
   if (!(polltab.revents & (POLLIN|POLLRDNORM)))
      {Log.Emsg("Link", XrdPoll::Poll2Text(polltab.revents), "polling", ID);
       return -1;
      }

// If the iocnt is within limits then just go ahead and read once.
//
   if (iocnt <= maxIOV)
      {rlen = RecvIOV(iov, iocnt);
       if (rlen > 0) {AtomicAdd(BytesIn, rlen);}
       return rlen;
      }

// We will have to break this up into allowable segments and we need to add up
// the bytes in each segment so that we know when to stop reading.
//
   int seglen, segcnt = maxIOV, totlen = 0;
   do {seglen = 0;
       for (int i = 0; i < segcnt; i++) seglen += iov[i].iov_len;
       if ((rlen = RecvIOV(iov, segcnt)) < 0) return rlen;
       totlen += rlen;
       if (rlen < seglen) break;
       iov   += segcnt;
       iocnt -= segcnt;
       if (iocnt <= maxIOV) segcnt = iocnt;
      } while(iocnt > 0);

// All done
//
   AtomicAdd(BytesIn, totlen);
   return totlen;
}

/******************************************************************************/
/*                               R e c v A l l                                */
/******************************************************************************/
  
int XrdLinkXeq::RecvAll(char *Buff, int Blen, int timeout)
{
   struct pollfd polltab = {PollInfo.FD, POLLIN|POLLRDNORM, 0};
   ssize_t rlen;
   int     retc;

// Check if timeout specified. Notice that the timeout is the max we will
// for some data. We will wait forever for all the data. Yeah, it's weird.
//
   if (timeout >= 0)
      {do {retc = poll(&polltab,1,timeout);} while(retc < 0 && errno == EINTR);
       if (retc != 1)
          {if (!retc) return -ETIMEDOUT;
           Log.Emsg("Link",errno,"poll",ID);
           return -1;
          }
       if (!(polltab.revents & (POLLIN|POLLRDNORM)))
          {Log.Emsg("Link",XrdPoll::Poll2Text(polltab.revents),"polling",ID);
           return -1;
          }
      }

// Note that we will block until we receive all he bytes.
//
   if (LockReads) rdMutex.Lock();
   isIdle = 0;
   do {rlen = recv(LinkInfo.FD, Buff, Blen, MSG_WAITALL);}
      while(rlen < 0 && errno == EINTR);
   if (rlen > 0) AtomicAdd(BytesIn, rlen);
   if (LockReads) rdMutex.UnLock();

   if (int(rlen) == Blen) return Blen;
        if (!rlen) {TRACEI(DEBUG, "No RecvAll() data; errno=" <<errno);}
   else if (rlen > 0) Log.Emsg("RecvAll", "Premature end from", ID);
   else if (LinkInfo.FD >= 0) Log.Emsg("Link", errno, "receive from", ID);
   return -1;
}
  
/******************************************************************************/
/* Protected:                    R e c v I O V                                */
/******************************************************************************/
  
int XrdLinkXeq::RecvIOV(const struct iovec *iov, int iocnt)
{
   ssize_t retc = 0;

// Read the data in. On some version of Unix (e.g., Linux) a readv() may
// end at any time without reading all the bytes when directed to a socket.
// We always return the number bytes read (or an error). The caller needs to
// restart the read at the appropriate place in the iovec when more data arrives.
//
   do {retc = readv(LinkInfo.FD, iov, iocnt);}
      while(retc < 0 && errno == EINTR);

// Check how we completed
//
   if (retc < 0) Log.Emsg("Link", errno, "receive from", ID);
   return retc;
}

/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/

bool XrdLinkXeq::Register(const char *hName)
{

// First see if we can register this name with the address object
//
   if (!Addr.Register(hName)) return false;

// Make appropriate changes here
//
   if (HostName) free(HostName);
   HostName = strdup(hName);
   strlcpy(Lname, hName, sizeof(Lname));
   return true;
}
  
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdLinkXeq::Send(const char *Buff, int Blen)
{
   ssize_t retc = 0, bytesleft = Blen;

// Get a lock
//
   wrMutex.Lock();
   isIdle = 0;
   AtomicAdd(BytesOut, Blen);

// Do non-blocking writes if we are setup to do so.
//
   if (sendQ)
      {retc = sendQ->Send(Buff, Blen);
       wrMutex.UnLock();
       return retc;
      }

// Write the data out
//
   while(bytesleft)
        {if ((retc = write(LinkInfo.FD, Buff, bytesleft)) < 0)
            {if (errno == EINTR) continue;
                else break;
            }
         bytesleft -= retc; Buff += retc;
        }

// All done
//
   wrMutex.UnLock();
   if (retc >= 0) return Blen;
   Log.Emsg("Link", errno, "send to", ID);
   return -1;
}

/******************************************************************************/
  
int XrdLinkXeq::Send(const struct iovec *iov, int iocnt, int bytes)
{
   int retc;
   static int maxIOV = -1;
   if (maxIOV == -1) {
#ifdef _SC_IOV_MAX
      maxIOV = sysconf(_SC_IOV_MAX);
      if (maxIOV == -1)
#endif
#ifdef IOV_MAX
         maxIOV = IOV_MAX;
#else
         maxIOV = 1024;
#endif
   }

// Get a lock and assume we will be successful (statistically we are)
//
   wrMutex.Lock();
   isIdle = 0;
   AtomicAdd(BytesOut, bytes);

// Do non-blocking writes if we are setup to do so.
//
   if (sendQ)
      {retc = sendQ->Send(iov, iocnt, bytes);
       wrMutex.UnLock();
       return retc;
      }

// If the iocnt is within limits then just go ahead and write this out
//
   if (iocnt <= maxIOV)
      {retc = SendIOV(iov, iocnt, bytes);
       wrMutex.UnLock();
       return retc;
      }

// We will have to break this up into allowable segments
//
   int seglen, segcnt = maxIOV, iolen = 0;
   do {seglen = 0;
       for (int i = 0; i < segcnt; i++) seglen += iov[i].iov_len;
       if ((retc = SendIOV(iov, segcnt, seglen)) < 0)
          {wrMutex.UnLock();
           return retc;
          }
       iolen += retc;
       iov   += segcnt;
       iocnt -= segcnt;
       if (iocnt <= maxIOV) segcnt = iocnt;
      } while(iocnt > 0);

// All done
//
   wrMutex.UnLock();
   return iolen;
}
 
/******************************************************************************/

int XrdLinkXeq::Send(const sfVec *sfP, int sfN)
{
#if !defined(HAVE_SENDFILE)

   return -1;

#elif defined(__solaris__)

    sendfilevec_t vecSF[XrdOucSFVec::sfMax], *vecSFP = vecSF;
    size_t xframt, totamt, bytes = 0;
    ssize_t retc;
    int i = 0;

// Construct the sendfilev() vector
//
   for (i = 0; i < sfN; sfP++, i++)
       {if (sfP->fdnum < 0)
           {vecSF[i].sfv_fd  = SFV_FD_SELF;
            vecSF[i].sfv_off = (off_t)sfP->buffer;
           } else {
            vecSF[i].sfv_fd  = sfP->fdnum;
            vecSF[i].sfv_off = sfP->offset;
           }
        vecSF[i].sfv_flag = 0;
        vecSF[i].sfv_len  = sfP->sendsz;
        bytes += sfP->sendsz;
       }
   totamt = bytes;

// Lock the link, issue sendfilev(), and unlock the link. The documentation
// is very spotty and inconsistent. We can only retry this operation under
// very limited conditions.
//
   wrMutex.Lock();
   isIdle = 0;
do{retc = sendfilev(LinkInfo.FD, vecSFP, sfN, &xframt);

// Check if all went well and return if so (usual case)
//
   if (xframt == bytes)
      {AtomicAdd(BytesOut, bytes);
       wrMutex.UnLock();
       return totamt;
      }

// The only one we will recover from is EINTR. We cannot legally get EAGAIN.
//
   if (retc < 0 && errno != EINTR) break;

// Try to resume the transfer
//
   if (xframt > 0)
      {AtomicAdd(BytesOut, xframt); bytes -= xframt; SfIntr++;
       while(xframt > 0 && sfN)
            {if ((ssize_t)xframt < (ssize_t)vecSFP->sfv_len)
                {vecSFP->sfv_off += xframt; vecSFP->sfv_len -= xframt; break;}
             xframt -= vecSFP->sfv_len; vecSFP++; sfN--;
            }
      }
  } while(sfN > 0);

// See if we can recover without destroying the connection
//
   retc = (retc < 0 ? errno : ECANCELED);
   wrMutex.UnLock();
   Log.Emsg("Link", retc, "send file to", ID);
   return -1;

#elif defined(__linux__) || defined(__GNU__)

   static const int setON = 1, setOFF = 0;
   ssize_t retc = 0, bytesleft;
   off_t myOffset;
   int i, xfrbytes = 0, uncork = 1, xIntr = 0;

// lock the link
//
   wrMutex.Lock();
   isIdle = 0;

// In linux we need to cork the socket. On permanent errors we do not uncork
// the socket because it will be closed in short order.
//
   if (setsockopt(PollInfo.FD, SOL_TCP, TCP_CORK, &setON, sizeof(setON)) < 0)
      {Log.Emsg("Link", errno, "cork socket for", ID);
       uncork = 0; sfOK = 0;
      }

// Send the header first
//
   for (i = 0; i < sfN; sfP++, i++)
       {if (sfP->fdnum < 0) retc = sendData(sfP->buffer, sfP->sendsz);
           else {myOffset = sfP->offset; bytesleft = sfP->sendsz;
                 while(bytesleft
                 && (retc=sendfile(LinkInfo.FD,sfP->fdnum,&myOffset,bytesleft)) > 0)
                      {bytesleft -= retc; xIntr++;}
                }
        if (retc <  0 && errno == EINTR) continue;
        if (retc <= 0) break;
        xfrbytes += sfP->sendsz;
       }

// Diagnose any sendfile errors
//
   if (retc <= 0)
      {if (retc == 0) errno = ECANCELED;
       wrMutex.UnLock();
       Log.Emsg("Link", errno, "send file to", ID);
       return -1;
      }

// Now uncork the socket
//
   if (uncork
   &&  setsockopt(PollInfo.FD, SOL_TCP, TCP_CORK, &setOFF, sizeof(setOFF)) < 0)
      Log.Emsg("Link", errno, "uncork socket for", ID);

// All done
//
   if (xIntr > sfN) SfIntr += (xIntr - sfN);
   AtomicAdd(BytesOut, xfrbytes);
   wrMutex.UnLock();
   return xfrbytes;

#else

   return -1;

#endif
}

/******************************************************************************/
/* Protected:                   s e n d D a t a                               */
/******************************************************************************/
  
int XrdLinkXeq::sendData(const char *Buff, int Blen)
{
   ssize_t retc = 0, bytesleft = Blen;

// Write the data out
//
   while(bytesleft)
        {if ((retc = write(LinkInfo.FD, Buff, bytesleft)) < 0)
            {if (errno == EINTR) continue;
                else break;
            }
         bytesleft -= retc; Buff += retc;
        }

// All done
//
   return retc;
}
  
/******************************************************************************/
/* Protected:                    S e n d I O V                                */
/******************************************************************************/
  
int XrdLinkXeq::SendIOV(const struct iovec *iov, int iocnt, int bytes)
{
   ssize_t bytesleft, n, retc = 0;
   const char *Buff;

// Write the data out. On some version of Unix (e.g., Linux) a writev() may
// end at any time without writing all the bytes when directed to a socket.
// So, we attempt to resume the writev() using a combination of write() and
// a writev() continuation. This approach slowly converts a writev() to a
// series of writes if need be. We must do this inline because we must hold
// the lock until all the bytes are written or an error occurs.
//
   bytesleft = static_cast<ssize_t>(bytes);
   while(bytesleft)
        {do {retc = writev(LinkInfo.FD, iov, iocnt);}
            while(retc < 0 && errno == EINTR);
         if (retc >= bytesleft || retc < 0) break;
         bytesleft -= retc;
         while(retc >= (n = static_cast<ssize_t>(iov->iov_len)))
              {retc -= n; iov++; iocnt--;}
         Buff = (const char *)iov->iov_base + retc; n -= retc; iov++; iocnt--;
         while(n) {if ((retc = write(LinkInfo.FD, Buff, n)) < 0)
                      {if (errno == EINTR) continue;
                          else break;
                      }
                   n -= retc; Buff += retc;
                  }
         if (retc < 0 || iocnt < 1) break;
        }

// All done
//
   if (retc >= 0) return bytes;
   Log.Emsg("Link", errno, "send to", ID);
   return -1;
}
  
/******************************************************************************/
/*                                 s e t I D                                  */
/******************************************************************************/
  
void XrdLinkXeq::setID(const char *userid, int procid)
{
   char buff[sizeof(Uname)], *bp, *sp;
   int ulen;

   snprintf(buff, sizeof(buff), "%s.%d:%d", userid, procid, PollInfo.FD);
   ulen = strlen(buff);
   sp = buff + ulen - 1;
   bp = &Uname[sizeof(Uname)-1];
   if (ulen > (int)sizeof(Uname)) ulen = sizeof(Uname);
   *bp = '@'; bp--;
   while(ulen--) {*bp = *sp; bp--; sp--;}
   ID = bp+1;
   Comment = (const char *)ID;

// Update the ID in the TLS socket if enabled
//
   if (isTLS) tlsIO.SetTraceID(ID);
}
 
/******************************************************************************/
/*                                 s e t N B                                  */
/******************************************************************************/
  
bool XrdLinkXeq::setNB()
{
// We don't support non-blocking output except for Linux at the moment
//
#if !defined(__linux__)
   return false;
#else
// Trace this request
//
   TRACEI(DEBUG,"enabling non-blocking output");

// If we don't already have a sendQ object get one. This is a one-time call
// so to optimize checking if this object exists we also get the opMutex.'
//
   LinkInfo.opMutex.Lock();
   if (!sendQ)
      {wrMutex.Lock();
       sendQ = new XrdSendQ(*this, wrMutex);
       wrMutex.UnLock();
      }
   LinkInfo.opMutex.UnLock();
   return true;
#endif
}

/******************************************************************************/
/*                           s e t P r o t o c o l                            */
/******************************************************************************/
  
XrdProtocol *XrdLinkXeq::setProtocol(XrdProtocol *pp, bool push)
{

// Set new protocol.
//
   LinkInfo.opMutex.Lock();
   XrdProtocol *op = Protocol;
   if (push) ProtoAlt = Protocol;
   Protocol = pp; 
   LinkInfo.opMutex.UnLock();
   return op;
}

/******************************************************************************/
/*                           s e t P r o t N a m e                            */
/******************************************************************************/
  
void XrdLinkXeq::setProtName(const char *name)
{

// Set the protocol name.
//
   LinkInfo.opMutex.Lock();
   Addr.SetDialect(name);
   LinkInfo.opMutex.UnLock();
}
 
/******************************************************************************/
/*                                s e t T L S                                 */
/******************************************************************************/

bool XrdLinkXeq::setTLS(bool enable, XrdTlsContext *ctx)
{ //???
// static const XrdTlsConnection::RW_Mode rwMode=XrdTlsConnection::TLS_RNB_WBL;
   static const XrdTlsSocket::RW_Mode rwMode=XrdTlsSocket::TLS_RBL_WBL;
   static const XrdTlsSocket::HS_Mode hsMode=XrdTlsSocket::TLS_HS_BLOCK;
   const char *eNote;
   XrdTls::RC rc;

// If we are already in a compatible mode, we are done
//

   if (isTLS == enable) return true;

// If this is a shutdown, then do it now.
//
   if (!enable)
      {tlsIO.Shutdown();
       isTLS = enable;
       Addr.SetTLS(enable);
       return true;
      }
// We want to initialize TLS, do so now.
//
   if (!ctx) ctx = tlsCtx;
   eNote = tlsIO.Init(*ctx, PollInfo.FD, rwMode, hsMode, false, false, ID);

// Check for errors
//
   if (eNote)
      {char buff[1024];
       snprintf(buff, sizeof(buff), "Unable to enable tls for %s;", ID);
       Log.Emsg("LinkXeq", buff, eNote);
       return false;
      }

// Now we need to accept this TLS connection
//
   std::string eMsg;
   rc = tlsIO.Accept(&eMsg);

// Diagnose return state
//
   if (rc != XrdTls::TLS_AOK) Log.Emsg("LinkXeq", eMsg.c_str());
      else {isTLS = enable;
            Addr.SetTLS(enable);
            Log.Emsg("LinkXeq", ID, "connection upgraded to", verTLS());
           }
   return rc == XrdTls::TLS_AOK;
}

/******************************************************************************/
/*                               S F E r r o r                                */
/******************************************************************************/
  
int XrdLinkXeq::SFError(int rc)
{
   Log.Emsg("TLS", rc, "send file to", ID);
   return -1;
}

/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/

void XrdLinkXeq::Shutdown(bool getLock)
{
   int temp;

// Trace the entry
//
   TRACEI(DEBUG, (getLock ? "Async" : "Sync") <<" link shutdown in progress");

// Get the lock if we need too (external entry via another thread)
//
   if (getLock) LinkInfo.opMutex.Lock();

// If there is something to do, do it now
//
   temp = Instance; Instance = 0;
   if (!KeepFD)
      {shutdown(PollInfo.FD, SHUT_RDWR);
       if (dup2(devNull, PollInfo.FD) < 0)
          {Instance = temp;
           Log.Emsg("Link", errno, "shutdown FD for", ID);
          }
      }

// All done
//
   if (getLock) LinkInfo.opMutex.UnLock();
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

int XrdLinkXeq::Stats(char *buff, int blen, bool do_sync)
{
   static const char statfmt[] = "<stats id=\"link\"><num>%d</num>"
          "<maxn>%d</maxn><tot>%lld</tot><in>%lld</in><out>%lld</out>"
          "<ctime>%lld</ctime><tmo>%d</tmo><stall>%d</stall>"
          "<sfps>%d</sfps></stats>";
   int i;

// Check if actual length wanted
//
   if (!buff) return sizeof(statfmt)+17*6;

// We must synchronize the statistical counters
//
   if (do_sync) XrdLinkCtl::SyncAll();

// Obtain lock on the stats area and format it
//
   AtomicBeg(statsMutex);
   i = snprintf(buff, blen, statfmt, AtomicGet(LinkCount),
                                     AtomicGet(LinkCountMax),
                                     AtomicGet(LinkCountTot),
                                     AtomicGet(LinkBytesIn),
                                     AtomicGet(LinkBytesOut),
                                     AtomicGet(LinkConTime),
                                     AtomicGet(LinkTimeOuts),
                                     AtomicGet(LinkStalls),
                                     AtomicGet(LinkSfIntr));
   AtomicEnd(statsMutex);
   return i;
}
  
/******************************************************************************/
/*                             s y n c S t a t s                              */
/******************************************************************************/
  
void XrdLinkXeq::syncStats(int *ctime)
{
   long long tmpLL;
   int       tmpI4;

// If this is dynamic, get the opMutex lock
//
   if (!ctime) LinkInfo.opMutex.Lock();

// Either the caller has the opMutex or this is called out of close. In either
// case, we need to get the read and write mutexes; each followed by the stats
// mutex. This order is important because we should not hold the stats mutex
// for very long and the r/w mutexes may take a long time to acquire. If we
// must maintain the link count we need to actually acquire the stats mutex as
// we will be doing compound operations. Atomics are still used to keep other
// threads from seeing partial results.
//
   AtomicBeg(rdMutex);

   if (ctime)
      {*ctime = time(0) - LinkInfo.conTime;
       AtomicAdd(LinkConTime, *ctime);
       statsMutex.Lock();
       if (LinkCount > 0) AtomicDec(LinkCount);
       statsMutex.UnLock();
      }

   AtomicBeg(statsMutex);

   tmpLL = AtomicFAZ(BytesIn);
   AtomicAdd(LinkBytesIn, tmpLL);  AtomicAdd(BytesInTot, tmpLL);
   tmpI4 = AtomicFAZ(tardyCnt);
   AtomicAdd(LinkTimeOuts, tmpI4); AtomicAdd(tardyCntTot, tmpI4);
   tmpI4 = AtomicFAZ(stallCnt);
   AtomicAdd(LinkStalls, tmpI4);   AtomicAdd(stallCntTot, tmpI4);
   AtomicEnd(statsMutex); AtomicEnd(rdMutex);

   AtomicBeg(wrMutex);    AtomicBeg(statsMutex);
   tmpLL = AtomicFAZ(BytesOut);
   AtomicAdd(LinkBytesOut, tmpLL); AtomicAdd(BytesOutTot, tmpLL);
   tmpI4 = AtomicFAZ(SfIntr);
   AtomicAdd(LinkSfIntr, tmpI4);
   AtomicEnd(statsMutex); AtomicEnd(wrMutex);

// Make sure the protocol updates it's statistics as well
//
   if (Protocol) Protocol->Stats(0, 0, 1);

// All done
//
   if (!ctime) LinkInfo.opMutex.UnLock();
}

/******************************************************************************/
/* Protected:                  T L S _ E r r o r                              */
/******************************************************************************/

int XrdLinkXeq::TLS_Error(const char *act, XrdTls::RC rc)
{
   std::string reason = XrdTls::RC2Text(rc);
   char msg[512];

   snprintf(msg, sizeof(msg), "Unable to %s %s;", act, ID);
   Log.Emsg("TLS", msg, reason.c_str());
   return -1;
}
  
/******************************************************************************/
/*                              T L S _ P e e k                               */
/******************************************************************************/
  
int XrdLinkXeq::TLS_Peek(char *Buff, int Blen, int timeout)
{
   XrdSysMutexHelper theMutex;
   XrdTls::RC retc;
   int rc, rlen;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Wait until we can actually read something
//
   isIdle = 0;
   if (timeout)
      {rc = Wait4Data(timeout);
       if (rc < 1) return rc;
      }

// Do the peek and if sucessful, the number of bytes available.
//
   retc = tlsIO.Peek(Buff, Blen, rlen);
   if (retc == XrdTls::TLS_AOK) return rlen;

// Dianose the TLS error and return failure
//
   return TLS_Error("peek on", retc);
}
  
/******************************************************************************/
/*                              T L S _ R e c v                               */
/******************************************************************************/
  
int XrdLinkXeq::TLS_Recv(char *Buff, int Blen)
{
   XrdSysMutexHelper theMutex;
   XrdTls::RC retc;
   int rlen;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Note that we will read only as much as is queued. Use Recv() with a
// timeout to receive as much data as possible.
//
   isIdle = 0;
   retc = tlsIO.Read(Buff, Blen, rlen);
   if (retc != XrdTls::TLS_AOK) return TLS_Error("receive from", retc);
   if (rlen > 0) AtomicAdd(BytesIn, rlen);
   return rlen;
}

/******************************************************************************/

int XrdLinkXeq::TLS_Recv(char *Buff, int Blen, int timeout, bool havelock)
{
   XrdSysMutexHelper theMutex;
   XrdTls::RC retc;
   int pend, rlen, totlen = 0;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads && !havelock) theMutex.Lock(&rdMutex);

// Wait up to timeout milliseconds for data to arrive
//
   isIdle = 0;
   while(Blen > 0)
        {pend = tlsIO.Pending(true);
         if (!pend) pend = Wait4Data(timeout);
         if (pend < 1)
            {if (pend < 0) return -1;
             tardyCnt++;
             if (totlen)
                {if ((++stallCnt & 0xff) == 1) TRACEI(DEBUG,"read timed out");
                 AtomicAdd(BytesIn, totlen);
                }
             return totlen;
            }

         // Read as much data as you can. Note that we will force an error
         // if we get a zero-length read after poll said it was OK. However,
         // if we never read anything, then we simply return -ENOMSG to avoid
         // generating a "read link error" as clearly there was a hangup.
         //
         retc = tlsIO.Read(Buff, Blen, rlen);
         if (retc != XrdTls::TLS_AOK)
            {if (!totlen) return -ENOMSG;
             AtomicAdd(BytesIn, totlen);
             return TLS_Error("receive from", retc);
            }
         if (rlen <= 0) break;
         totlen += rlen; Blen -= rlen; Buff += rlen;
        }

   AtomicAdd(BytesIn, totlen);
   return totlen;
}

/******************************************************************************/

int XrdLinkXeq::TLS_Recv(const struct iovec *iov, int iocnt, int timeout)
{
   XrdSysMutexHelper theMutex;
   char *Buff;
   int Blen, rlen, totlen = 0;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Individually process each element until we can't read any more
//
   isIdle = 0;
   for (int i = 0; i < iocnt; i++)
       {Buff = (char *)iov[i].iov_base;
        Blen =         iov[i].iov_len;
        rlen = TLS_Recv(Buff, Blen, timeout, true);
        if (rlen <= 0) break;
        totlen += rlen;
        if (rlen < Blen) break;
       }

   if (totlen) {AtomicAdd(BytesIn, totlen);}
   return totlen;
}

/******************************************************************************/
/*                           T L S _ R e c v A l l                            */
/******************************************************************************/
  
int XrdLinkXeq::TLS_RecvAll(char *Buff, int Blen, int timeout)
{
   int     retc;

// Check if timeout specified. Notice that the timeout is the max we will
// wait for some data. We will wait forever for all the data. Yeah, it's weird.
//
   if (timeout >= 0)
      {retc = tlsIO.Pending(true);
       if (!retc) retc = Wait4Data(timeout);
       if (retc < 1) return (retc ? -1 : -ETIMEDOUT);
      }

// Note that we will block until we receive all the bytes.
//
   return TLS_Recv(Buff, Blen, -1);
}

/******************************************************************************/
/*                              T L S _ S e n d                               */
/******************************************************************************/
  
int XrdLinkXeq::TLS_Send(const char *Buff, int Blen)
{
   XrdSysMutexHelper lck(wrMutex);
   ssize_t bytesleft = Blen;
   XrdTls::RC retc;
   int byteswritten;

// Prepare to send
//
   isIdle = 0;
   AtomicAdd(BytesOut, Blen);

// Do non-blocking writes if we are setup to do so.
//
   if (sendQ) return sendQ->Send(Buff, Blen);

// Write the data out
//
   while(bytesleft)
        {retc = tlsIO.Write(Buff, bytesleft, byteswritten);
         if (retc != XrdTls::TLS_AOK) return TLS_Error("send to", retc);
         bytesleft -= byteswritten; Buff += byteswritten;
        }

// All done
//
   return Blen;
}

/******************************************************************************/
  
int XrdLinkXeq::TLS_Send(const struct iovec *iov, int iocnt, int bytes)
{
   XrdSysMutexHelper lck(wrMutex);
   XrdTls::RC retc;
   int byteswritten;

// Get a lock and assume we will be successful (statistically we are). Note
// that the calling interface gauranteed bytes are not zero.
//
   isIdle = 0;
   AtomicAdd(BytesOut, bytes);

// Do non-blocking writes if we are setup to do so.
//
   if (sendQ) return sendQ->Send(iov, iocnt, bytes);

// Write the data out.
//
   for (int i = 0; i < iocnt; i++)
       {ssize_t bytesleft = iov[i].iov_len;
        char *Buff = (char *)iov[i].iov_base;
        while(bytesleft)
             {retc = tlsIO.Write(Buff, bytesleft, byteswritten);
              if (retc != XrdTls::TLS_AOK) return TLS_Error("send to", retc);
              bytesleft -= byteswritten; Buff += byteswritten;
             }
       }

// All done
//
   return bytes;
}
 
/******************************************************************************/

int XrdLinkXeq::TLS_Send(const sfVec *sfP, int sfN)
{
   XrdSysMutexHelper lck(wrMutex);
   int bytes, buffsz, fileFD, retc;
   off_t offset;
   ssize_t totamt = 0;
   char myBuff[65536];

// Convert the sendfile to a regular send. The conversion is not particularly
// fast and caller are advised to avoid using sendfile on TLS connections.
//
   isIdle = 0;
   for (int i = 0; i < sfN; sfP++, i++)
       {if (!(bytes = sfP->sendsz)) continue;
        totamt += bytes;
        if (sfP->fdnum < 0)
           {if (!TLS_Write(sfP->buffer, bytes)) return -1;
            continue;
           }
        offset = sfP->offset;
        fileFD = sfP->fdnum;
        buffsz = (bytes < (int)sizeof(myBuff) ? bytes : sizeof(myBuff));
        do {do {retc = pread(fileFD, myBuff, buffsz, offset);}
                       while(retc < 0 && errno == EINTR);
            if (retc < 0) return SFError(errno);
            if (!retc) break;
            if (!TLS_Write(myBuff, buffsz)) return -1;
            offset += buffsz; bytes -= buffsz; totamt += retc;
           } while(bytes > 0);
       }

// We are done
//
   AtomicAdd(BytesOut, totamt);
   return totamt;
}

/******************************************************************************/
/* Protected:                  T L S _ W r i t e                              */
/******************************************************************************/

bool XrdLinkXeq::TLS_Write(const char *Buff, int Blen)
{
   XrdTls::RC retc;
   int byteswritten;

// Write the data out
//
   while(Blen)
        {retc = tlsIO.Write(Buff, Blen, byteswritten);
         if (retc != XrdTls::TLS_AOK)
            {TLS_Error("write to", retc);
             return false;
            }
         Blen -= byteswritten; Buff += byteswritten;
        }

// All done
//
   return true;
}

/******************************************************************************/
/*                                v e r T L S                                 */
/******************************************************************************/
  
const char *XrdLinkXeq::verTLS()
{
   return tlsIO.Version();
}
