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
  
#include "Experiment/Experiment.hh"
#include "XrdOdc/XrdOdcMsg.hh"
#include "XrdOdc/XrdOdcTrace.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
int         XrdOdcMsg::nextid   =  0;
XrdOdcMsg  *XrdOdcMsg::nextfree =  0;
XrdOdcMsg  *XrdOdcMsg::nextwait =  0;
XrdOdcMsg  *XrdOdcMsg::lastwait =  0;
int         XrdOdcMsg::FreeMax  = 32;
int         XrdOdcMsg::FreeNum  =  0;

XrdOucMutex XrdOdcMsg::MsgWaitQ;
XrdOucMutex XrdOdcMsg::FreeMsgQ;

extern XrdOucTrace OdcTrace;
 
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
   if (!nextfree) mp = new XrdOdcMsg();
      else {mp = nextfree; nextfree = mp->next; FreeNum--;}
   lclid = nextid++;
   FreeMsgQ.UnLock();

// Initialize it
//
   mp->id   = lclid;
   mp->Resp = erp;
   mp->next = 0;

// Place the message on the waiting queue
//
   MsgWaitQ.Lock();
   if (lastwait) lastwait->next = mp;
      else       nextwait       = mp;
   lastwait = mp;
   mp->inwaitq  = 1;
   MsgWaitQ.UnLock();

// Return the message object
//
   return mp;
}
 
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdOdcMsg::Recycle()
{
// Most of the time we are not in the wait queue, do a fast check
//
   myData.Lock();
   if (inwaitq) {int msgid = id; 
                 myData.UnLock(); 
                 XrdOdcMsg::RemFromWaitQ(msgid);
                }
   myData.UnLock();

// Either delete the element or return it to the free queue
//
   FreeMsgQ.Lock();
      if (FreeNum >= FreeMax) delete this;
         else {next = nextfree; nextfree = this; FreeNum++; Resp = 0;}
   FreeMsgQ.UnLock();
}

/******************************************************************************/
/*                                 R e p l y                                  */
/******************************************************************************/
  
int XrdOdcMsg::Reply(int msgid, char *msg)
{
   const char *epname = "Reply";
   XrdOdcMsg *mp;
   char *rbuff;
   int retc;

// Find the appropriate message
//
   if (!(mp = XrdOdcMsg::RemFromWaitQ(msgid)))
      {DEBUG("Msg: Reply to non-existent message; id=" <<msgid);
       return 0;
      }

// Determine the error code
//
   if (strncmp(msg, "?err", 4)) retc = 0;
      else {msg += 4;
            retc = 2;
            while(*msg && (' ' == *msg)) msg++;
           }

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
