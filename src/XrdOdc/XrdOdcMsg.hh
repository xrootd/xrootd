#ifndef __ODC_MSG__
#define __ODC_MSG__
/******************************************************************************/
/*                                                                            */
/*                          X r d O d c M s g . h h                           */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//          $Id$

#include "XrdOuc/XrdOucPthread.hh"

class XrdOucErrInfo;

class XrdOdcMsg
{
public:

static XrdOdcMsg *Alloc(XrdOucErrInfo *erp);

inline int       ID() {return id;}

       void      Recycle();

static int       Reply(int msgid, char *reply);

static void      setKeep(int mk) {FreeMax = mk;}

       int       Wait4Reply(int wtime) {return Hold.Wait(wtime);}

      XrdOdcMsg() {inwaitq = 0; Resp = 0; next = 0;}
     ~XrdOdcMsg() {}

private:
static XrdOdcMsg  *RemFromWaitQ(int msgid);

static int         nextid;

static XrdOdcMsg   *nextfree;
static XrdOucMutex  FreeMsgQ;
static int          FreeMax;
static int          FreeNum;

static XrdOdcMsg   *nextwait;
static XrdOdcMsg   *lastwait;
static XrdOucMutex  MsgWaitQ;

XrdOdcMsg          *next;
XrdOucMutex         myData;
XrdOucCondVar       Hold;
int                 inwaitq;
int                 id;
XrdOucErrInfo      *Resp;
};
#endif
