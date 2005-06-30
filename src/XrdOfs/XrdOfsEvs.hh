#ifndef __XRDOFSEVS_H__
#define __XRDOFSEVS_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d O f s E v s . h h                           */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*             Based on code developed by Derek Feichtinger, CERN.            */
/******************************************************************************/
  
//         $Id$

#include <strings.h>
#include "XrdOuc/XrdOucPthread.hh"

class XrdOucError;
class XrdOucProg;
class XrdOfsEvsMsg;

class XrdOfsEvs
{
public:

static const int   minMsgSize = (16+320+1024);
static const int   maxMsgSize = (16+320+1024+1024);

enum Event {All    = 0xffff, None   = 0x0000, Chmod  = 0x0001,
            Closer = 0x0002, Closew = 0x0004, Close  = 0x0006,
            Mkdir  = 0x0008, Mv     = 0x0010,
            Openr  = 0x0020, Openw  = 0x0040, Open   = 0x0060,
            Rm     = 0x0080, Rmdir  = 0x0100, Fwrite = 0x0200
           };

int         Enabled(Event theEvents) {return theEvents & enEvents;}

int         maxSmsg() {return maxMin;}
int         maxLmsg() {return maxMax;}

void        Notify(Event theEvent, const char *tident,
                                   const char *arg1, const char *arg2=0);

const char *Prog() {return theTarget;}

void        sendEvents(void);

int         Start(XrdOucError *eobj);

      XrdOfsEvs(Event theEvents, const char *Target, int minq=90, int maxq=10)
               {enEvents = theEvents; theTarget = strdup(Target);
                eDest = 0; theProg = 0; maxMin = minq; maxMax = maxq;
                msgFirst = msgLast = msgFreeMax = msgFreeMin = 0;
                numMax = numMin = 0; tid = 0;
               }
     ~XrdOfsEvs();

private:

XrdOfsEvsMsg   *getMsg(int bigmsg);
void            retMsg(XrdOfsEvsMsg *tp);

pthread_t       tid;
char           *theTarget;
Event           enEvents;
XrdOucError    *eDest;
XrdOucProg     *theProg;
XrdOucMutex     qMut;
XrdOucSemaphore qSem;
XrdOfsEvsMsg   *msgFirst;
XrdOfsEvsMsg   *msgLast;
XrdOucMutex     fMut;
XrdOfsEvsMsg   *msgFreeMax;
XrdOfsEvsMsg   *msgFreeMin;
int             numMax;
int             maxMax;
int             numMin;
int             maxMin;
};
#endif
