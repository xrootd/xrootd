/******************************************************************************/
/*                                                                            */
/*                          X r d O d c M s g . c c                           */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

const char *XrdOdcMsgCVSID = "$Id$";

#include <stdlib.h>
  
#include "XrdOdc/XrdOdcMsg.hh"
#include "XrdOdc/XrdOdcTrace.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
int         XrdOdcMsg::nextid   =  0;
int         XrdOdcMsg::msgHWM   =  0;

XrdOdcMsg  *XrdOdcMsg::msgTab   =  0;
XrdOdcMsg  *XrdOdcMsg::nextfree =  0;
XrdOdcMsg  *XrdOdcMsg::nextwait =  0;
XrdOdcMsg  *XrdOdcMsg::lastwait =  0;

XrdOucMutex XrdOdcMsg::MsgWaitQ;
XrdOucMutex XrdOdcMsg::FreeMsgQ;

extern XrdOucTrace OdcTrace;

#define XRDODC_OBMSGID 255
#define XRDODC_MIDMASK 255
#define XRDODC_MAXMSGS 255
#define XRDODC_MIDINCR 256
#define XRDODC_INCMASK 0x0fffff00
 
/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdOdcMsg *XrdOdcMsg::Alloc(XrdOucErrInfo *erp)
{
   XrdOdcMsg *mp;
   int       lclid;

// Allocate a message object
//
   FreeMsgQ.Lock();
   lclid = nextid = (nextid + XRDODC_MIDINCR) & XRDODC_INCMASK;
   if (nextfree) {mp = nextfree; nextfree = mp->next;}
      else if ((mp = new XrdOdcMsg())) mp->id = XRDODC_OBMSGID;
              else {FreeMsgQ.UnLock(); return mp;}
   FreeMsgQ.UnLock();

// Initialize it
//
   mp->id      = (mp->id & XRDODC_MIDMASK) | lclid;
   mp->Resp    = erp;
   mp->next    = 0;
   mp->inwaitq = 1;

// Place the message on the waiting queue if this is an outboard msg
//
   if ((mp->id & XRDODC_MIDMASK) == XRDODC_OBMSGID)
      {MsgWaitQ.Lock();
       if (lastwait) lastwait->next = mp;
          else       nextwait       = mp;
       lastwait = mp;
       MsgWaitQ.UnLock();
      }

// Return the message object
//
   return mp;
}
 
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdOdcMsg::Init(int numalloc)
{
   EPNAME("Init");
   const int numppage = 4096/sizeof(XrdOdcMsg);
   int i;
   XrdOdcMsg *msgp;

// Compute the number of initial msg objects. These can be addressed directly
// by msgid (0 to 254) and will never be freed.
//
   while (numalloc < numppage) numalloc += numppage;
   if (numalloc > XRDODC_MAXMSGS) numalloc = XRDODC_MAXMSGS;
   DEBUG("Msg: Creating " <<numalloc <<" msg objects");

// Allocate the fixed number of msg blocks
//
   if (!(msgp = new XrdOdcMsg[numalloc]())) return 1;
   msgTab = &msgp[0];
   msgHWM = numalloc;
   nextid = numalloc;

// Place all of the msg blocks on the free list
//
  for (i = 0; i < numalloc; i++)
     {msgp->next = nextfree; nextfree = msgp; msgp->id = i; msgp++;}

// All done
//
   return 0;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdOdcMsg::Recycle()
{
// Most of the time we are not in the wait queue, do a fast check
//
   myData.Lock();
   if (inwaitq) 
      if ((id && XRDODC_MIDMASK) != XRDODC_OBMSGID) inwaitq = 0;
         else {int msgid = id;
               myData.UnLock();
               XrdOdcMsg::RemFromWaitQ(msgid);
              }
   myData.UnLock();

// Delete this element if it's an outboard msg object
//
   FreeMsgQ.Lock();
      if ((id & XRDODC_MIDMASK) == XRDODC_OBMSGID) delete this;
         else {next = nextfree; nextfree = this; Resp = 0;}
   FreeMsgQ.UnLock();
}

/******************************************************************************/
/*                                 R e p l y                                  */
/******************************************************************************/
  
int XrdOdcMsg::Reply(int msgid, char *msg)
{
   EPNAME("Reply")
   XrdOdcMsg *mp;
   int retc;

// Find the appropriate message
//
   if (!(mp = XrdOdcMsg::RemFromWaitQ(msgid)))
      {DEBUG("Msg: Reply to non-existent message; id=" <<msgid);
       return 0;
      }

// Determine the error code
//
        if (!strncmp(msg, "!try", 4))
           {msg += 5;
            retc = -EREMOTE;
            while(*msg && (' ' == *msg)) msg++;
           }
   else if (!strncmp(msg, "!wait", 5))
           {msg += 6;
            retc = -EAGAIN;
            while(*msg && (' ' == *msg)) msg++;
           }
   else if (!strncmp(msg, "?err", 4))
           {msg += 5;
            retc = -EINVAL;
            while(*msg && (' ' == *msg)) msg++;
           }
   else retc = -EINVAL;

// Make sure the reply is not too long
//
   if (strlen(msg) >= OUC_MAX_ERROR_LEN)
      {DEBUG("Msg: Truncated: " <<msg);
       msg[OUC_MAX_ERROR_LEN-1] = '\0';
      }

// Reply and return
//
   mp->Resp->setErrInfo(retc, (const char *)msg);
   mp->Hold.Signal();
   mp->myData.UnLock();
   return 1;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                          R e m F r o m W a i t Q                           */
/******************************************************************************/
  
XrdOdcMsg *XrdOdcMsg::RemFromWaitQ(int msgid)
{
   XrdOdcMsg *pp = 0, *mp = 0;
   int msgnum;

// If this is a inboard msg object, we can locate it immediately
//
  if ((msgnum = msgid & 0xff) < msgHWM)
     {msgTab[msgnum].myData.Lock();
      if (msgTab[msgnum].inwaitq && msgTab[msgnum].id == msgid) 
         {msgTab[msgnum].inwaitq = 0; 
          msgTab[msgnum].myData.UnLock();
          return &msgTab[msgnum];
         }
      msgTab[msgnum].myData.UnLock();
      return (XrdOdcMsg *)0;
     }

// Remove the message from the
//
   MsgWaitQ.Lock();
   mp = nextwait;
   while (mp && mp->id != msgid) {pp = mp; mp = mp->next;}
   if (mp) {if (pp)
               {if (mp == lastwait) lastwait = pp;
                pp->next = mp->next;
               } else {
                if (mp == lastwait) nextwait = lastwait = 0;
                   else nextwait = mp->next;
               }
            mp->myData.Lock();
            mp->inwaitq = 0;
           }
   MsgWaitQ.UnLock();
   return mp;
}
