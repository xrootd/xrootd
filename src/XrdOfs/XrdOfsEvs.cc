/******************************************************************************/
/*                                                                            */
/*                          X r d O f s E v s . c c                           */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*             Based on code developed by Derek Feichtinger, CERN.            */
/******************************************************************************/
  
//         $Id$

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "XrdOfs/XrdOfsEvs.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucProg.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdOfsEvsMsg
{
public:

XrdOfsEvsMsg *next;
char         *text;
int           tlen;
int           isBig;

             XrdOfsEvsMsg(char *tval=0, int big=0)
                        {text = tval; tlen=0; isBig = big; next=0;}

            ~XrdOfsEvsMsg() {if (text) free(text);}
};

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
void *XrdOfsEvsSend(void *pp)
{
     XrdOfsEvs *evs = (XrdOfsEvs *)pp;
     evs->sendEvents();
     return (void *)0;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOfsEvs::~XrdOfsEvs()
{
  XrdOfsEvsMsg *tp;

// Kill the notification thread. This may cause a msg block to be orphaned
// but, in practice, this object does not really get deleted after being 
// started. So, the problem is moot.
//
   if (tid) XrdOucThread::Kill(tid);

// Release all queued message bocks
//
  qMut.Lock();
  while ((tp = msgFirst)) {msgFirst = tp->next; delete tp;}
  if (theTarget) free(theTarget);
  if (theProg) {delete theProg; theProg = 0;}
  qMut.UnLock();

// Release all free message blocks
//
  fMut.Lock();
  while ((tp = msgFreeMax)) {msgFreeMax = tp->next; delete tp;}
  while ((tp = msgFreeMin)) {msgFreeMin = tp->next; delete tp;}
  fMut.UnLock();
}

/******************************************************************************/
/*                                N o t i f y                                 */
/******************************************************************************/
  
void XrdOfsEvs::Notify(Event theEvent, const char *tident,
                                       const char *arg1, const char *arg2)
{
   static int warnings = 0;
   XrdOfsEvsMsg *tp;
   const char *evName;
   int isBig = 0;

// Get the character event name
//
   switch(theEvent)
        {case XrdOfsEvs::Chmod:  evName = "chmod";  break;
         case XrdOfsEvs::Closer: evName = "closer"; break;
         case XrdOfsEvs::Closew: evName = "closew"; break;
         case XrdOfsEvs::Mkdir:  evName = "mkdir";  break;
         case XrdOfsEvs::Mv:     evName = "mv";
                                 isBig  = 1;        break;
         case XrdOfsEvs::Openr:  evName = "openr";  break;
         case XrdOfsEvs::Openw:  evName = "openw";  break;
         case XrdOfsEvs::Rm:     evName = "rm";     break;
         case XrdOfsEvs::Rmdir:  evName = "rmdir";  break;
         case XrdOfsEvs::Fwrite: evName = "fwrite"; break;
         default: return;
        }

// Get a message block
//
   if (!(tp = getMsg(isBig)))
      {if ((++warnings & 0xff) == 1)
          eDest->Emsg("Notify", "Ran out of message objects;", evName,
                                "event notification not sent.");
          return;
      }

// Format the message
//
   if (arg2) tp->tlen = snprintf(tp->text, maxMsgSize, "%s %s %s %s\n",
                                           tident, evName, arg1, arg2);
      else   tp->tlen = snprintf(tp->text, maxMsgSize, "%s %s %s\n",
                                           tident, evName, arg1);

// Put the message on the queue and return
//
   tp->next = 0;
   qMut.Lock();
   if (msgLast) {msgLast->next = tp; msgLast = tp;}
      else msgFirst = msgLast = tp;
   qMut.UnLock();
   qSem.Post();
}

/******************************************************************************/
/*                            s e n d E v e n t s                             */
/******************************************************************************/
  
void XrdOfsEvs::sendEvents(void)
{
   XrdOfsEvsMsg *tp;
   const char *theData[2] = {0,0};
         int   theDlen[2] = {0,0};

// This is an endless loop that just gets things off the event queue and
// send them out. This allows us to only hang a simgle thread should the
// receiver get blocked, instead of the whole process.
//
   while(1)
        {qSem.Wait();
         qMut.Lock();
         if (!theProg) break;
         if ((tp = msgFirst) && !(msgFirst = tp->next)) msgLast = 0;
         qMut.UnLock();
         if (tp) 
            {theData[0] = tp->text; theDlen[0] = tp->tlen;
             theProg->Feed(theData, theDlen);
             retMsg(tp);
            }
         }
   qMut.UnLock();
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
int XrdOfsEvs::Start(XrdOucError *eobj)
{
   int rc;

// Set the error object pointer
//
   eDest = eobj;

// Allocate a new program object if we don't have one
//
   if (theProg) return 0;
   theProg = new XrdOucProg(eobj);

// Setup the program
//
   if (theProg->Setup(theTarget, eobj)) return -1;
   if ((rc = theProg->Start()))
      {eobj->Emsg("Evs", rc, "start event collector"); return -1;}

// Now start a thread to get messages and send them to the collector
//
   if ((rc = XrdOucThread::Run(&tid, XrdOfsEvsSend, static_cast<void *>(this),
                          0, "Event notification sender")))
      {eobj->Emsg("Evs", rc, "create event notification thread");
       return -1;
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                g e t M s g                                 */
/******************************************************************************/

XrdOfsEvsMsg *XrdOfsEvs::getMsg(int bigmsg)
{
   XrdOfsEvsMsg *tp;
   int msz;

// Lock the free queue
//
   fMut.Lock();

// Get a free element from the big or small queue, as needed
//
   if (bigmsg)
        if ((tp = msgFreeMax)) msgFreeMax = tp->next;
           else msz = maxMsgSize;
   else if ((tp = msgFreeMin)) msgFreeMin = tp->next;
           else msz = minMsgSize;

// Check if we have to allocate a new item
//
   if (!tp && (numMax + numMin) < (maxMax + maxMin))
      if ((tp = new XrdOfsEvsMsg((char *)malloc(msz), bigmsg)))
         if (!(tp->text)) {delete tp; tp = 0;}
            else if (bigmsg) numMax++;
                    else     numMin++;

// Unlock and return result
//
   fMut.UnLock();
   return tp;
}

/******************************************************************************/
/*                                r e t M s g                                 */
/******************************************************************************/

void XrdOfsEvs::retMsg(XrdOfsEvsMsg *tp)
{

// Lock the free queue
//
   fMut.Lock();

// Check if we exceeded the hold quotax
//
   if (tp->isBig)
      if (numMax > maxMax) {delete tp; numMax--;}
         else {tp->next = msgFreeMax; msgFreeMax = tp;}
      else
      if (numMin > maxMin) {delete tp; numMin--;}
         else {tp->next = msgFreeMin; msgFreeMin = tp;}

// Unlock and return
//
   fMut.UnLock();
}
