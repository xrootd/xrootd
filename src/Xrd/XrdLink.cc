/******************************************************************************/
/*                                                                            */
/*                            X r d L i n k . c c                             */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//           $Id$

const char *XrdLinkCVSID = "$Id$";

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#ifdef __linux__
#include <sys/poll.h>
#else
#include <poll.h>
#endif


#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucTimer.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdNetwork.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdScheduler.hh"
#define  TRACELINK this
#include "Xrd/XrdTrace.hh"
 
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                    C l a s s   x r d _ L i n k S c a n                     */
/******************************************************************************/
  
class XrdLinkScan : XrdJob
{
public:

void          DoIt() {idleScan();}

              XrdLinkScan(int im, int it, const char *lt="idle link scan") :
                                           XrdJob(lt)
                          {idleCheck = im; idleTicks = it;}
             ~XrdLinkScan() {}

private:

             void   idleScan();

             int    idleCheck;
             int    idleTicks;

static const char *TraceID;
};
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOucError     XrdLog;

extern XrdScheduler    XrdScheduler;

extern XrdOucTrace     XrdTrace;

       XrdLink       **XrdLink::LinkTab;
       int             XrdLink::LTLast = -1;
       XrdOucMutex     XrdLink::LTMutex;

       const char     *XrdLink::TraceID = "Link";

       long long       XrdLink::LinkBytesIn   = 0;
       long long       XrdLink::LinkBytesOut  = 0;
       long long       XrdLink::LinkConTime   = 0;
       long long       XrdLink::LinkCountTot  = 0;
       int             XrdLink::LinkCount     = 0;
       int             XrdLink::LinkCountMax  = 0;
       int             XrdLink::LinkTimeOuts  = 0;
       int             XrdLink::LinkStalls    = 0;
       XrdOucMutex     XrdLink::statsMutex;

XrdObjectQ<XrdLink> XrdLink::LinkStack("LinkOQ", "link anchor");

       const char     *XrdLinkScan::TraceID = "LinkScan";
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdLink::XrdLink(const char *ltype) :
          LinkLink(this), IOSemaphore(0), XrdJob(ltype)
{
  Etext = 0;
  Reset();
}

void XrdLink::Reset()
{
  int i;

  FD    = -1;
  if (Etext) {free(Etext); Etext = 0;}
  Uname[sizeof(Uname)-1] = '@';
  Uname[sizeof(Uname)-2] = '?';
  Lname[0] = '?';
  Lname[1] = '\0';
  ID       = &Uname[sizeof(Uname)-2];
  Comment  = (const char *)ID;
  Next     = 0;
  Protocol = 0; 
  ProtoAlt = 0;
  conTime  = time(0);
  stallCnt = 0;
  tardyCnt = 0;
  InUse    = 1;
  Poller   = 0; 
  PollEnt  = 0;
  isEnabled= 0;
  isIdle   = 0;
  inQ      = 0;
  BytesOut = BytesIn = 0;
  doPost   = 0;
  isFree   = 0;
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdLink *XrdLink::Alloc(int fd, sockaddr_in *ip, char *host, XrdBuffer *bp)
{
   XrdLink *lp;
   char *unp, buff[16];
   int bl;

// Get a link object off the stack (if none, allocate a new one)
//
   if (lp = LinkStack.Pop()) lp->Reset();
      else lp = new XrdLink();

// Establish the address and connection type of this link
//
   memcpy((void *)&(lp->InetAddr),(const void *)ip,sizeof(struct sockaddr_in));
   if (!host) XrdNetwork::getHostName(*ip, lp->Lname, sizeof(lp->Lname));
      else strlcpy(lp->Lname, host, sizeof(lp->Lname));
   bl = sprintf(buff, "?:%d", fd);
   unp = lp->Lname - bl - 1;
   strncpy(unp, buff, bl);
   lp->ID = unp;
   lp->FD = fd;
   lp->udpbuff = bp;

// Insert this link into the link table
//
   LTMutex.Lock();
   LinkTab[fd] = lp;
   if (fd > LTLast) LTLast = fd;
   LTMutex.UnLock();

// Return the link
//
   statsMutex.Lock();
   LinkCountTot++;
   if (LinkCountMax == LinkCount++) LinkCountMax = LinkCount;
   statsMutex.UnLock();
   return lp;
}
  
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
int XrdLink::Close()
{   int fd, csec, rc = 0;
    char buff[256], ctbuff[24], *sfxp = ctbuff;

// Multiple protocols may be bound to this link, figure it out here
//
   IOMutex.Lock();
   InUse--;
   if (InUse > 0) {IOMutex.UnLock(); return 0;}

// Add up the statistic for this link
//
   syncStats(&csec);
   IOMutex.UnLock();

// Remove ourselves from the poll table and then from the Link table
//
   fd = (FD < 0 ? -FD : FD);
   if (FD != -1)
      {if (Poller) {XrdPoll::Detach(this); Poller = 0;}
       LTMutex.Lock();
       LinkTab[fd] = 0;
       if (fd == LTLast) while(LTLast-- && !(LinkTab[LTLast])) {};
       LTMutex.UnLock();
      }

// Document this close
//
   XrdOucTimer::s2hms(csec, ctbuff, sizeof(ctbuff));
   if (Etext) {snprintf(buff, sizeof(buff), "%s (%s)", ctbuff, Etext);
               sfxp = buff;
               free(Etext); Etext = 0;
              }
   XrdLog.Emsg("Link",(const char *)ID, (char *)"disconnected after", sfxp);

// Clean this link up, we don't need a lock now because no one is using it
//
   if (FD >= 0)  {rc = (close(FD) < 0 ? errno : 0); FD = -1;}
   if (Protocol) {Protocol->Recycle(); Protocol = 0;}
   if (ProtoAlt) {ProtoAlt->Recycle(); ProtoAlt = 0;}
   InUse    = 0;
   if (rc) XrdLog.Emsg("Link", rc, "closing", ID);
   if (!isFree) {isFree = 1; LinkStack.Push(&LinkLink);}
      else XrdLog.Emsg("Link",(const char *)ID, (char *)"dup recycle averted");
   return rc;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
 
void XrdLink::DoIt()
{
   int rc;

// The Process() return code tells us what to do:
// < 0 -> Error, stop getting requests,  do not enable the link
// = 0 -> OK, get next request, if allowed, o/w enable the link
// > 0 -> Slow link, stop getting requests  and enable the link
//
   do {rc = Protocol->Process(this);} while (!rc && XrdScheduler.canStick());

// Re-enable the link and cycle back waiting for a new request. Warning, this
// object may have been deleted upon return. Don't use any object data now.
//
   if (rc >= 0) Enable();
}

/******************************************************************************/
/*                                E n a b l e                                 */
/******************************************************************************/

int XrdLink::Enable() 
{
    return (Poller ? Poller->Enable(this) : 0);
}
  
/******************************************************************************/
/*                              n e x t L i n k                               */
/******************************************************************************/
  
XrdLink *XrdLink::nextLink(int &nextFD)
{
   XrdLink *lp = 0;

// Lock the link table
//
   LTMutex.Lock();

// Find the next existing link
//
   while(nextFD <= LTLast && !(lp = LinkTab[nextFD])) nextFD++;

// Increase the reference count so that this link does not escape while
// it is being exported. It's the caller's responsibility to reduce the
// reference count when the caller is through with the link
//
   if (lp) lp->setRef(1);

// Unlock the table and return the link pointer
//
   LTMutex.UnLock();
   return lp;
}

/******************************************************************************/
/*                                  P e e k                                   */
/******************************************************************************/
  
int XrdLink::Peek(char *Buff, long Blen, int timeout)
{
   struct pollfd polltab = {FD, POLLIN|POLLRDNORM, 0};
   ssize_t mlen;
   int retc;

   IOMutex.Lock();
   isIdle = 0;
   do {retc = poll(&polltab, 1, timeout);} while(retc < 0 && errno == EINTR);
   if (retc != 1)
      {IOMutex.UnLock();
       if (retc == 0) return 0;
       return XrdLog.Emsg("Link", -errno, "polling", ID);
      }

// Verify it is safe to read now
//
   if (!(polltab.revents & (POLLIN|POLLRDNORM)))
      {IOMutex.UnLock();
       XrdLog.Emsg("Link", XrdPoll::Poll2Text(polltab.revents),
                           (char *)"polling", ID);
       return -1;
      }

// Do the peek.
//
   do {mlen = recv(FD, Buff, Blen, MSG_PEEK);}
      while(mlen < 0 && errno == EINTR);
   IOMutex.UnLock();

// Return the result
//
   if (mlen >= 0) return (int)mlen;
   XrdLog.Emsg("Link", errno, "peeking on", ID);
   return -1;
}
  
/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/
  
/******************************************************************************/
/*                                  R e c v                                   */
/******************************************************************************/
  
int XrdLink::Recv(char *Buff, long Blen)
{
   ssize_t rlen;

// Note that we will read only as much as is queued. Use Recv() with a
// timeout to receive as much data as possible.
//
   IOMutex.Lock();
   isIdle = 0;
   do {rlen = read(FD, Buff, Blen);} while(rlen < 0 && errno == EINTR);
   IOMutex.UnLock();

   if (rlen >= 0) return (int)rlen;
   XrdLog.Emsg("Link", errno, "receiving from", ID);
   return -1;
}

/******************************************************************************/

int XrdLink::Recv(char *Buff, long Blen, int timeout)
{
   struct pollfd polltab = {FD, POLLIN|POLLRDNORM, 0};
   ssize_t rlen, totlen = 0;
   int retc;

// Wait up to timeout milliseconds for data to arrive
//
   IOMutex.Lock();
   isIdle = 0;
   while(Blen > 0)
        {do {retc = poll(&polltab,1,timeout);} while(retc < 0 && errno == EINTR);
         if (retc != 1)
            {IOMutex.UnLock();
             if (retc == 0)
                {tardyCnt++;
                 if (totlen  && (++stallCnt & 0xff) == 1)
                    XrdLog.Emsg("Link", ID, (char *)"read timed out");
                 return totlen;
                }
             return XrdLog.Emsg("Link", -errno, "polling", ID);
            }

         // Verify it is safe to read now
         //
         if (!(polltab.revents & (POLLIN|POLLRDNORM)))
            {IOMutex.UnLock();
             XrdLog.Emsg("Link", XrdPoll::Poll2Text(polltab.revents),
                                 (char *)"polling", ID);
             return -1;
            }

         // Read as much data as you can. Note that we will force an error
         // if we get a zero-length read after poll said it was OK.
         //
         do {rlen = recv(FD, Buff, Blen, 0);} while(rlen < 0 && errno == EINTR);
         if (rlen <= 0)
            {IOMutex.UnLock();
             if (!rlen) return -ENODATA;
             return XrdLog.Emsg("Link", -errno, "receiving from", ID);
            }
         BytesIn += rlen; totlen += rlen; Blen -= rlen; Buff += rlen;
        }
   IOMutex.UnLock();

   return totlen;
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdLink::Send(char *Buff, long Blen)
{
   long retc, bytesleft = Blen;

// Get a lock
//
   IOMutex.Lock();
   isIdle = 0;

// Write the data out
//
   while(bytesleft)
        {if ((retc = write(FD, Buff, bytesleft)) < 0)
            if (errno == EINTR) continue;
               else break;
         BytesOut += retc; bytesleft -= retc; Buff += retc;
        }

// All done
//
   IOMutex.UnLock();
   if (retc >= 0) return Blen;
   XrdLog.Emsg("Link", errno, "sending to", ID);
   return -1;
}

/******************************************************************************/
  
int XrdLink::Send(const struct iovec *iov, int iocnt, long bytes)
{
   int i, bytesleft, retc;

// Add up bytes if they were not given to us
//
   if (!bytes) for (i = 0; i < iocnt; i++) bytes += iov->iov_len;
   bytesleft = bytes;

// Get a lock
//
   IOMutex.Lock();
   isIdle = 0;
   BytesOut += bytes;

// Write the data out. For now we will assume that writev() fully completes
// an iov element or never starts. That's true on most platforms but not all. 
// So, we will force an error if our assumption does not hold for this platform
// and then go back and convert the writev() to a write()*iocnt for those.
//
   while(bytesleft)
        {if ((retc = writev(FD, iov, iocnt)) < 0)
            if (errno == EINTR) continue;
               else break;
         if (retc >= bytesleft) break;
         if (retc != iov->iov_len)
            {retc = -1; errno = EBADE; break;}
         iov++; iocnt--; bytesleft -= retc; BytesOut += retc;
        }

// All done
//
   IOMutex.UnLock();
   if (retc >= 0) return bytes;
   XrdLog.Emsg("Link", errno, "sending to", ID);
   return -1;
}
 
/******************************************************************************/
/*                              s e t E t e x t                               */
/******************************************************************************/

void XrdLink::setEtext(const char *text)
{
     IOMutex.Lock();
     if (Etext) free(Etext);
     Etext = (Etext ? strdup(text) : 0);
     IOMutex.UnLock();
}
  
/******************************************************************************/
/*                                 s e t I D                                  */
/******************************************************************************/
  
void XrdLink::setID(const char *userid, int procid)
{
   char buff[sizeof(Uname)], *bp, *sp;
   int ulen;

   snprintf(buff, sizeof(buff), "%s.%d:%d", userid, procid, FD);
   ulen = strlen(buff);
   sp = buff + ulen - 1;
   bp = &Uname[sizeof(Uname)-1];
   if (ulen > sizeof(Uname)) ulen = sizeof(Uname);
   *bp = '@'; bp--;
   while(ulen--) {*bp = *sp; bp--; sp--;}
   ID = bp+1;
   Comment = (const char *)ID;
}
 
/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/

int XrdLink::Setup(int maxfds, int idlewait)
{
   int iticks, ichk;

// Create the link table
//
   if (!(LinkTab = (XrdLink **)malloc(maxfds*sizeof(XrdLink *))))
      {XrdLog.Emsg("Link", ENOMEM, "creating LinkTab"); return 0;}
   memset((void *)LinkTab, 0, maxfds*sizeof(XrdLink *));

// Create an idle connection scan job
//
   if (!(ichk = idlewait/3)) {iticks = 1; ichk = idlewait;}
      else iticks = 3;
   XrdLinkScan *ls = new XrdLinkScan(ichk, iticks);
   XrdScheduler.Schedule((XrdJob *)ls, ichk+time(0));

   return 1;
}
  
/******************************************************************************/
/*                             S e r i a l i z e                              */
/******************************************************************************/
  
void XrdLink::Serialize()
{

// This is meant to make sure that no protocol objects are refering to this
// link so that we can safely run in psuedo single thread mode for critical
// functions.
//
   IOMutex.Lock();
   if (InUse <= 1) IOMutex.UnLock();
      else {doPost++;
            IOMutex.UnLock();
            IOSemaphore.Wait();
           }
}

/******************************************************************************/
/*                           s e t P r o t o c o l                            */
/******************************************************************************/
  
XrdProtocol *XrdLink::setProtocol(XrdProtocol *pp)
{

// Set new protocol.
//
   IOMutex.Lock();
   XrdProtocol *op = Protocol;
   Protocol = pp; 
   IOMutex.UnLock();
   return op;
}

/******************************************************************************/
/*                                s e t R e f                                 */
/******************************************************************************/
  
void XrdLink::setRef(int use)
{
   IOMutex.Lock();
   InUse += use;
   if (InUse < 1)
      {char *etp = (InUse < 0 ? (char *)"use count underflow" : 0);
       InUse = 1;
       IOMutex.UnLock();
       setEtext(etp);
       Close();
      }
   if (InUse == 1 && doPost)
      {doPost--;
       IOSemaphore.Post();
       TRACE(CONN, "setRef posted link fd " <<FD);
      }
   IOMutex.UnLock();
}
 
/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

int XrdLink::Stats(char *buff, int blen)
{
   static const char statfmt[] = "<stats id=\"link\"><num>%ld</num>"
          "<maxn>%ld</maxn><tot>%lld</tot><in>%lld</in><out>%lld</out>"
          "<ctime>%lld</ctime><tmo>%ld</tmo><stall>%d</stall></stats>";
   int i;

// Check if actual length wanted
//
   if (!buff) return sizeof(statfmt)+17*6;

// We must synchronize the statistical counters
//
   LTMutex.Lock();
   for (i = 0; i <= LTLast; i++) if (LinkTab[i]) LinkTab[i]->syncStats();
   LTMutex.UnLock();

// Obtain lock on the stats area and format it
//
   statsMutex.Lock();
   i = snprintf(buff, blen, statfmt, LinkCount, LinkCountMax, LinkBytesIn,
                      LinkConTime, LinkBytesOut, LinkTimeOuts, LinkStalls);
   statsMutex.UnLock();
   return i;
}
  
/******************************************************************************/
/*                             s y n c S t a t s                              */
/******************************************************************************/
  
void XrdLink::syncStats(int *ctime)
{

// If this is dynamic, get the IOMutex lock
//
   if (!ctime) IOMutex.Lock();

// Either the caller has the IOMutex or this is called out of close
//
   statsMutex.Lock();
   LinkBytesIn  += BytesIn;
   LinkBytesOut += BytesOut;
   LinkTimeOuts += tardyCnt;
   LinkStalls   += stallCnt;
   if (ctime)
      {*ctime = time(0) - conTime;
       LinkConTime += *ctime;
       if (!(LinkCount--)) LinkCount = 0;
      }
   statsMutex.UnLock();

// Make sure the protocol updates it's statistics as well
//
   if (Protocol) Protocol->syncStats();

// Clear our local counters
//
   BytesIn = BytesOut = 0;
   tardyCnt = 0;
   if (!ctime) IOMutex.UnLock();
}
 
/******************************************************************************/
/*                              i d l e S c a n                               */
/******************************************************************************/
  
#undef   TRACELINK
#define  TRACELINK lp

void XrdLinkScan::idleScan()
{
   XrdLink *lp;
   int i, ltlast, lnum = 0, tmo = 0, tmod = 0;

// Scan across all links looking for idle links
//
   XrdLink::LTMutex.Lock();
   ltlast = XrdLink::LTLast;
   for (i = 0; i <= ltlast; i++)
       {if (!(lp = XrdLink::LinkTab[i])) continue;
        lnum++;
        lp->IOMutex.Lock();
        if (lp->isIdle) tmo++;
        lp->isIdle++;
        if ((int)(lp->isIdle) < idleTicks) {lp->IOMutex.UnLock(); continue;}
        XrdLink::LTMutex.UnLock();
        lp->isIdle = 0;
        if (!(lp->Poller) || !(lp->isEnabled))
           XrdLog.Emsg("LinkScan","Link",lp->ID,(char *)"is disabled and idle.");
           else if (lp->InUse == 1)
                   {lp->Poller->Disable(lp, "idle timeout");
                    tmod++;
                   }
        lp->IOMutex.UnLock();
        XrdLink::LTMutex.Lock();
       }
   XrdLink::LTMutex.UnLock();

// Trace what we did
//
   TRACE(CONN, lnum <<" links; " <<tmo <<" idle; " <<tmod <<" force closed");

// Reschedule ourselves
//
   XrdScheduler.Schedule((XrdJob *)this, idleCheck+time(0));
}
