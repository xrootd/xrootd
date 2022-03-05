/******************************************************************************/
/*                                                                            */
/*                      X r d S y s L o g g i n g . c c                       */
/*                                                                            */
/*(c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

#include <stddef.h>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>

#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysLogging.hh"
#include "XrdSys/XrdSysPlatform.hh"
  
/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

namespace
{
static const int buffOvhd  = 8;

XrdSysMutex      msgMutex;
XrdSysSemaphore  msgAlert(0);
XrdSysLogPI_t    piLogger = 0;
char            *pendMsg  = 0;  // msg to be processed, if nil means none
char            *lastMsg  = 0;  // last msg in the processing queue
char            *buffOrg = 0;  // Base address of global message buffer
char            *buffBeg = 0;  // buffOrg + overhead
char            *buffEnd = 0;  // buffOrg + size of buffer
struct timeval   todLost;      // time last message was lost
int              numLost = 0;  // Number of messages lost
bool             logDone = false;
bool             doSync  = false;

static const int syncBSZ = 8192;
};

pthread_t XrdSysLogging::lpiTID;
bool      XrdSysLogging::lclOut = false;
bool      XrdSysLogging::rmtOut = false;
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
bool XrdSysLogging::Configure(XrdSysLogger &logr, Parms &parms)
{
   char eBuff[256];
   int  rc;

// Set logger parameters
//
   if (parms.hiRes) logr.setHiRes();

// If we are going to send output to a local destination, configure it.
//
   if (parms.logfn)
      {if (strcmp(parms.logfn, "-") && (rc=logr.Bind(parms.logfn,parms.keepV)))
          {sprintf(eBuff, "Error %d (%s) binding to log file %s.\n",
                   -rc, XrdSysE2T(-rc), parms.logfn);
           return EMsg(logr, eBuff);
          }
       lclOut = true;
      }

// If we are not sending output to a remote destination, we are done
//
   if (!parms.logpi) {lclOut = true; return true;}
   piLogger= parms.logpi;
   logDone = !lclOut;
   rmtOut  = true;

// We have a plugin, setup the synchronous case if so desired
//
   if (!parms.bufsz)
      {logr.setForwarding(true);
       doSync = true;
       return true;
      }

// Allocate a log buffer
//
   int bsz = (parms.bufsz < 0 ? 65536 : parms.bufsz);
   rc = posix_memalign((void **)&buffOrg, getpagesize(), bsz);
   if (rc != 0 || !buffOrg) return EMsg(logr, "Unable to allocate log buffer!\n");

   buffBeg = buffOrg + buffOvhd;
   buffEnd = buffOrg + bsz;

// Start the forwarding thread
//
   if (XrdSysThread::Run(&lpiTID, Send2PI, (void *)0, 0, "LogPI handler"))
      {sprintf(eBuff, "Error %d (%s) starting LogPI handler.\n",
                       errno, XrdSysE2T(errno));
       return EMsg(logr, eBuff);
      }

// We are all done
//
   logr.setForwarding(true);
   return true;
}

/******************************************************************************/
/* Private:                    C o p y T r u n c                              */
/******************************************************************************/

int XrdSysLogging::CopyTrunc(char *mbuff, struct iovec *iov, int iovcnt)
{
   char *mbP = mbuff;
   int segLen, bLeft = syncBSZ - 1;

// Copy message with truncation
//
   for (int i = 0; i < iovcnt; i++)
       {segLen = iov[i].iov_len;
        if (segLen >= bLeft) segLen = bLeft;
        memcpy(mbP, iov[i].iov_base, segLen);
        mbP += segLen; bLeft -= segLen;
        if (bLeft <= 0) break;
       }
   *mbP = 0;

// Return actual length
//
   return mbP - mbuff;
}

/******************************************************************************/
/* Private:                         E M s g                                   */
/******************************************************************************/

bool XrdSysLogging::EMsg(XrdSysLogger &logr, const char *msg)
{
   struct iovec iov[] = {{0,0}, {(char *)msg,0}};

   iov[1].iov_len = strlen((const char *)iov[1].iov_base);
   logr.Put(2, iov);
   return false;
}
  
/******************************************************************************/
/*                               F o r w a r d                                */
/******************************************************************************/

bool XrdSysLogging::Forward(struct timeval mtime, unsigned long tID,
                            struct iovec  *iov,   int iovcnt)
{
   MsgBuff *theMsg;
   char    *fence, *freeMsg, *msgText;
   int      dwords, msgLen = 0;
   bool     doPost = false;

// Calculate the message length
//
   for (int i = 0; i < iovcnt; i++) msgLen += iov[i].iov_len;

// If we are doing synchronous forwarding, do so now (we do not get a lock)
//
   if (doSync)
      {char *mbP, mbuff[syncBSZ];
       if (msgLen >= syncBSZ) msgLen = CopyTrunc(mbuff, iov, iovcnt);
          else {mbP = mbuff;
                for (int i = 0; i < iovcnt; i++)
                    {memcpy(mbP, iov[i].iov_base, iov[i].iov_len);
                     mbP += iov[i].iov_len;
                    }
                *mbP = 0;
               }
        (*piLogger)(mtime, tID, mbuff, msgLen);
        return logDone;
       }

// Serialize remainder of code
//
   msgMutex.Lock();

// If the message is excessively long, treat it as a lost message
//
   if (msgLen > maxMsgLen)
      {todLost = mtime;
       numLost++;
       msgMutex.UnLock();
       return logDone;
      }

// Get the actual doublewords bytes we need (account for null byte in the msg).
// We need to increase the size by the header size if there are outsanding
// lost messages.
//
   dwords = msgLen+8 + sizeof(MsgBuff);
   if (numLost) dwords += sizeof(MsgBuff);
   dwords = dwords/8;

// Compute the allocation fence. The choices are as follows:
// a) When pendMsg is present then the fence is the end of the buffer if
//    lastMsg >= pendMsg and pendMsg otherwise.
// b) When pendMsg is nil then we can reset the buffer pointers so that the
//    fence is the end of the buffer.
//
   if (pendMsg)
      {freeMsg  = lastMsg + ((MsgBuff *)lastMsg)->buffsz*8;
       fence    = (lastMsg >= pendMsg ? buffEnd : pendMsg);
      } else {
       freeMsg   = buffBeg;
       fence     = buffEnd;
       lastMsg   = 0;
       doPost = true;
      }

// Check if there is room for this message. If not, count this as a lost
// message and tell the caller full forwarding did not happen.
//
   if ((freeMsg + (dwords*8)) > fence)
      {todLost = mtime;
       numLost++;
       msgMutex.UnLock();
       return logDone;
      }

// We can allocate everything. So, check if we will be inserting a lost
// message entry here. We preallocated this above when numLost != 0;
//
   if (numLost)
      {theMsg = (MsgBuff *)freeMsg;
       theMsg->msgtod = mtime;
       theMsg->tID    = tID;
       theMsg->buffsz = mbDwords;
       theMsg->msglen = -numLost;
       if (lastMsg) ((MsgBuff *)lastMsg)->next = freeMsg - buffOrg;
       lastMsg = freeMsg;
       freeMsg += msgOff;
      }

// Insert the message
//
   theMsg = (MsgBuff *)freeMsg;
   theMsg->msgtod = mtime;
   theMsg->tID    = tID;
   theMsg->next   = 0;
   theMsg->buffsz = dwords;
   theMsg->msglen = msgLen;
   if (lastMsg) ((MsgBuff *)lastMsg)->next = freeMsg - buffOrg;
   lastMsg = freeMsg;

// Copy the message text into the buffer
//
   msgText = freeMsg + msgOff;
   for (int i = 0; i < iovcnt; i++)
       {memcpy(msgText, iov[i].iov_base, iov[i].iov_len);
        msgText += iov[i].iov_len;
       }
   *msgText = 0;

// If we need to write this to another log file do so here.
//

// Do final post processing (release the lock prior to posting)
//
   if (doPost) pendMsg = freeMsg;
   msgMutex.UnLock();
   if (doPost) msgAlert.Post();
   return logDone;
}
  
/******************************************************************************/
/* Private:                       g e t M s g                                 */
/******************************************************************************/
  
XrdSysLogging::MsgBuff *XrdSysLogging::getMsg(char **msgTxt, bool cont)
{
   XrdSysMutexHelper msgHelp(msgMutex);
   MsgBuff *theMsg;

// If we got incorrectly posted, ignore this call
//
   if (!pendMsg) return 0;

// Check if this is a continuation. If so, skip to next message. If there is no
// next message, clear the pendMsg pointer to indicate we stopped pulling any
// messages (we will get posted when another message arrives).
//
   if (cont)
      {if (((MsgBuff *)pendMsg)->next)
          pendMsg = buffOrg + ((MsgBuff *)pendMsg)->next;
          else pendMsg = 0;
      }

// Return the message
//
   theMsg = (MsgBuff *)pendMsg;
  *msgTxt = pendMsg + msgOff;
   return theMsg;
}

/******************************************************************************/
/* Private:                      S e n d 2 P I                                */
/******************************************************************************/

void *XrdSysLogging::Send2PI(void *arg)
{
   (void)arg;
   MsgBuff *theMsg;
   char    *msgTxt, lstBuff[80];
   int      msgLen;
   bool     cont;

// Infinit loop feeding the logger plugin
//
do{msgAlert.Wait();
   cont = false;
   while((theMsg = getMsg(&msgTxt, cont)))
        {if ((msgLen = theMsg->msglen) < 0)
            {int n = -msgLen; // Note we will never overflow lstBuff!
             msgLen = snprintf(lstBuff, sizeof(lstBuff), "%d message%s lost!",
                               n, (n == 1 ? "" : "s"));
             msgTxt = lstBuff;
            }
         (*piLogger)(theMsg->msgtod, theMsg->tID, msgTxt, msgLen);
         cont = true;
        }
  } while(true);

// Here to keep the compiler happy
//
   return (void *)0;
}
