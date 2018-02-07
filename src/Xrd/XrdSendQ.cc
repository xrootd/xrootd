/******************************************************************************/
/*                                                                            */
/*                           X r d S e n d Q . c c                            */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
  
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdSendQ.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class LinkShutdown : public XrdJob
{
public:

virtual void DoIt() {myLink->Shutdown(true);
                     myLink->setRef(-1);
                     delete this;
                    }

             LinkShutdown(XrdLink *link)
                         : XrdJob("SendQ Shutdown"), myLink(link) {}

virtual     ~LinkShutdown() {}

private:

XrdLink *myLink;
};

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

XrdScheduler *XrdSendQ::Sched = 0;
XrdSysError  *XrdSendQ::Say   = 0;
unsigned int  XrdSendQ::qWarn = 3;
unsigned int  XrdSendQ::qMax  = 0xffffffff;
bool          XrdSendQ::qPerm = false;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSendQ::XrdSendQ(XrdLink &lP, XrdSysMutex &mP)
                  : XrdJob("sendQ runner"),
                    mLink(lP), wMutex(mP),
                    fMsg(0), lMsg(0), delQ(0), theFD(lP.FDnum()),
                    inQ(0), qWmsg(qWarn), discards(0),
                    active(false), terminate(false) {}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdSendQ::DoIt()
{
   mBuff   *theMsg;
   int      myFD, rc;
   bool     theEnd;

// Obtain the lock
//
   wMutex.Lock();

// Before we start check if we should delete any messages
//
   if (delQ) {RelMsgs(delQ); delQ = 0;}

// Send all queued messages (we can use a blocking send here)
//
   while(!terminate && (theMsg = fMsg))
        {if (!(fMsg = fMsg->next)) lMsg = 0;
         inQ--; myFD = theFD;
         wMutex.UnLock();
         rc = send(myFD, theMsg->mData, theMsg->mLen, 0);
         free(theMsg);
         wMutex.Lock();
         if (rc < 0) {Scuttle(); break;}
        }

// Before we exit check if we should delete any messages
//
   if (delQ) {RelMsgs(delQ); delQ = 0;}
   if ((theEnd = terminate) && fMsg) RelMsgs(fMsg);
   active = false;
   qWmsg  = qWarn;

// Release any messages that need to be released. Note that we may have been
// deleted at this point so we cannot reference anything via "this" once we
// unlock the mutex. We may also need to delete ourselves.
//
   wMutex.UnLock();
   if (theEnd) delete this;
}
  
/******************************************************************************/
/* Private:                         Q M s g                                   */
/******************************************************************************/
  
bool XrdSendQ::QMsg(XrdSendQ::mBuff *theMsg)
{
// Check if we reached the max number of messages
//
   if (inQ >= qMax)
      {discards++;
       if ((discards & 0xff) == 0x01)
          {char qBuff[80];
           snprintf(qBuff, sizeof(qBuff),
                    "%u) reached; %hu message(s) discarded!", qMax, discards);
           Say->Emsg("SendQ", mLink.Host(),
                     "appears to be slow; queue limit (", qBuff);
          }
       return false;
      }

// Add the message at the end of the queue
//
   theMsg->next = 0;
   if (lMsg) lMsg->next = theMsg;
      else   fMsg       = theMsg;
   lMsg = theMsg;
   inQ++;

// If there is no active thread handling this queue, schedule one
//
   if (!active)
      {Sched->Schedule((XrdJob *)this);
       active = true;
      }

// Check if we should issue a warning.
//
   if (inQ >= qWmsg)
      {char qBuff[32];
       qWmsg += qWarn;
       snprintf(qBuff, sizeof(qBuff), "%ud messages queued!", inQ);
       Say->Emsg("SendQ", mLink.Host(), "appears to be slow;", qBuff);
      } else {
       if (inQ < qWarn && qWmsg != qWarn) qWmsg = qWarn;
      }

// All done
//
   return true;
}

/******************************************************************************/
/* Private:                      R e l M s g s                                */
/******************************************************************************/

void XrdSendQ::RelMsgs(XrdSendQ::mBuff *mP)
{
   mBuff *freeMP;

   while((freeMP = mP))
        {mP = mP->next;
         free(freeMP);
        }
}

/******************************************************************************/
/* Private:                      S c u t t l e                                */
/******************************************************************************/
  
void XrdSendQ::Scuttle() // qMutex must be locked!
{
// Simply move any outsanding messages to the deletion queue
//
   if (fMsg)
      {lMsg->next = delQ;
       delQ = fMsg;
       fMsg = lMsg = 0;
       inQ  = 0;
      }
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/

// Called with wMutex locked.
  
int XrdSendQ::Send(const char *buff, int blen)
{
   mBuff *theMsg;
   int bleft, bsent;

// If there is an active thread handling messages then we have to queue it.
// Otherwise try to send it. We need to hold the lock here to prevent messing
// up the message is only part of it could be sent. This is a non-blocking call.
//
   if (active) bleft = blen;
      else if ((bleft = SendNB(buff, blen)) <= 0) return (bleft ? -1 : blen);

// Allocate buffer for the message
//
   if (!(theMsg = (mBuff *)malloc(sizeof(mBuff) + bleft)))
      {errno = ENOMEM;  return -1;}

// Copy the unsent message fragment
//
   bsent = blen - bleft;
   memcpy(theMsg->mData, buff+bsent, bleft);
   theMsg->mLen  = bleft;

// Queue the message.
//
   return (QMsg(theMsg) ? blen : -1);
}

/******************************************************************************/

// Called with wMutex locked.

int XrdSendQ::Send(const struct iovec *iov, int iovcnt, int iotot)
{
   mBuff *theMsg;
   char  *body;
   int bleft, bmore, iovX;

// If there is an active thread handling messages then we have to queue it.
// Otherwise try to send it. We need to hold the lock here to prevent messing
// up the message is only part of it could be sent. This is a non-blocking call.
//
   if (active)
      {bleft = 0;
       for (iovX = 0; iovX < iovcnt; iovX++)
           if ((bleft = iov[iovX].iov_len)) break;
       if (!bleft) return iotot;
      } else {
       if ((bleft = SendNB(iov, iovcnt, iotot, iovX)) <= 0)
          return (bleft ? -1 : 0);
      }

// Readjust the total amount not sent based on where we stopped in the iovec.
//
   bmore = bleft;
   for (int i = iovX+1; i < iovcnt; i++) bmore += iov[i].iov_len;

// Copy the unsent message (for simplicity we will copy the whole iovec stop).
//
   if (!(theMsg = (mBuff *)malloc(bmore+sizeof(mBuff))))
      {errno = ENOMEM;  return -1;}

// Setup the message length
//
   theMsg->mLen = bmore;

// Copy the first fragment (it cannot be zero length)
//
   body = theMsg->mData;
   memcpy(body, ((char *)iov[iovX].iov_base)+(iov[iovX].iov_len-bleft), bleft);
   body += bleft;

// All remaining items
//
   for (int i = iovX+1; i < iovcnt; i++)
       {if (iov[i].iov_len)
           {memcpy(body, iov[i].iov_base, iov[i].iov_len);
            body += iov[i].iov_len;
           }
       }

// Queue the message.
//
   return (QMsg(theMsg) ? iotot : 0);
}

/******************************************************************************/
/*                                S e n d N B                                 */
/******************************************************************************/
  
// Called with wMutex locked.

int XrdSendQ::SendNB(const char *Buff, int Blen)
{
#if !defined(__linux__)
   return -1;
#else
   ssize_t retc = 0, bytesleft = Blen;

// Write the data out
//
   while(bytesleft)
        {do {retc = send(theFD, Buff, bytesleft, MSG_DONTWAIT);}
            while(retc < 0 && errno == EINTR);
         if (retc <= 0) break;
         bytesleft -= retc; Buff += retc;
        }

// All done
//
   if (retc <= 0)
      {if (!retc || errno == EAGAIN || retc == EWOULDBLOCK) return bytesleft;
       Say->Emsg("SendQ", errno, "send to", mLink.ID);
       return -1;
      }
   return bytesleft;
#endif
}

/******************************************************************************/
  
// Called with wMutex locked.
  
int XrdSendQ::SendNB(const struct iovec *iov, int iocnt, int bytes, int &iovX)
{

#if !defined(__linux__)
   return -1;
#else
   char   *msgP;
   ssize_t retc;
   int     msgL, msgF = MSG_DONTWAIT|MSG_MORE, ioLast = iocnt-1;

// Write the data out. The following code only works in Linux as we use the
// new POSIX flags deined for send() which currently is only implemented in
// Linux. This allows us to selectively use non-blocking I/O.
//
   for (iovX = 0; iovX < iocnt; iovX++)
       {msgP = (char *)iov[iovX].iov_base;
        msgL =         iov[iovX].iov_len;
        if (iovX == ioLast) msgF &= ~MSG_MORE;
        while(msgL)
             {do {retc = send(theFD, msgP, msgL, msgF);}
                 while(retc < 0 && errno == EINTR);
              if (retc <= 0)
                 {if (!retc || errno == EAGAIN || retc == EWOULDBLOCK)
                     return msgL;
                  Say->Emsg("SendQ", errno, "send to", mLink.ID);
                  return -1;
                 }
              msgL -= retc;
             }
       }

// All done
//
   return 0;
#endif
}
  
/******************************************************************************/
/*                             T e r m i n a t e                              */
/******************************************************************************/

// This must be called with wMutex locked!
  
void XrdSendQ::Terminate(XrdLink *lP)
{
// First step is to see if we need to schedule a shutdown prior to quiting
//
   if (lP) Sched->Schedule((XrdJob *)new LinkShutdown(lP));

// If there is an active thread then we need to let the thread handle the
// termination of this object. Otherwise, we can do it now.
//
   if (active)
      {Scuttle();
       terminate = true;
       theFD     =-1;
      } else {
       if (fMsg) {RelMsgs(fMsg); fMsg = lMsg = 0;}
       if (delQ) {RelMsgs(delQ); delQ = 0;}
       delete this;
      }
}
