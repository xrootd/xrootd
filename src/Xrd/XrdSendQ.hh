#ifndef __XRDSENDQ__H
#define __XRDSENDQ__H
/******************************************************************************/
/*                                                                            */
/*                           X r d S e n d Q . h h                            */
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

#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
  
#include "Xrd/XrdJob.hh"

class XrdLink;
class XrdSysMutex;

class XrdSendQ : public XrdJob
{
public:

unsigned int  Backlog() {return inQ;}

virtual  void DoIt();

static   void Init(XrdSysError *eP, XrdScheduler *sP) {Say = eP; Sched = sP;}

         int  Send(const char *buff, int blen);

         int  Send(const struct iovec *iov, int iovcnt, int iotot);

static   void SetAQ(bool onoff)         {qPerm = onoff;}

static   void SetQM(unsigned int qmVal) {qMax  = qmVal;}

static   void SetQW(unsigned int qwVal) {qWarn = qwVal;}

         void Terminate(XrdLink *lP=0);

         XrdSendQ(XrdLink &lP, XrdSysMutex &mP);

private:

virtual ~XrdSendQ() {}

int      SendNB(const char *Buff, int Blen);
int      SendNB(const struct iovec *iov, int iocnt, int bytes, int &iovX);

struct mBuff
{
mBuff *next;
int    mLen;
char   mData[4]; // Always made long enough
};

bool     QMsg(mBuff *theMsg);
void     RelMsgs(mBuff *mP);
void     Scuttle();

static XrdScheduler *Sched;
static XrdSysError  *Say;
static unsigned int  qWarn;
static unsigned int  qMax;
static bool          qPerm;
XrdLink             &mLink;
XrdSysMutex         &wMutex;

mBuff               *fMsg;
mBuff               *lMsg;
mBuff               *delQ;
int                  theFD;
unsigned int         inQ;
unsigned int         qWmsg;
unsigned short       discards;
bool                 active;
bool                 terminate;
};
#endif
