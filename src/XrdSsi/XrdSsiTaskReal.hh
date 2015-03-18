#ifndef __XRDSSITASKREAL_HH__
#define __XRDSSITASKREAL_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i T a s k R e a l . h h                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdSsi/XrdSsiStream.hh"
#include "XrdSsi/XrdSsiResponder.hh"

class XrdSsiRequest;
class XrdSsiSessReal;

class XrdSsiTaskReal : public XrdCl::ResponseHandler, public XrdSsiResponder,
                       public XrdSsiStream
{
public:

enum TaskStat {isWrite=0, isSync, isReady, isDone, isDead};

void   Detach(bool force=false);

void   HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response);

void  *Implementation() {return (void *)this;}

bool   Kill();

inline
int    ID() {return tskID;}

inline
void   Init(XrdSsiRequest *rP, unsigned short tmo=0)
           {rqstP = rP, tStat = isWrite; tmOut = tmo; mhPend = true;
            attList.next = attList.prev = this;
            BindRequest(rP, (XrdSsiSession *)sessP, (XrdSsiResponder *)this);
           }

int    SetBuff(XrdSsiErrInfo &eInfo, char *buff, int blen, bool &last);

bool   SetBuff(XrdSsiRequest *reqP, char *buff, int blen);

void   SetTaskID(short tid) {tskID = tid;}

       XrdSsiTaskReal(XrdSsiSessReal *sP, short tid)
                 : XrdSsiResponder(this, (void *)0),
                   XrdSsiStream(XrdSsiStream::isPassive), sessP(sP), tskID(tid)
                    {}

      ~XrdSsiTaskReal() {}

void   RespErr(XrdCl::XRootDStatus *status);

struct dlQ {XrdSsiTaskReal *next; XrdSsiTaskReal *prev;};
dlQ             attList;

private:

XrdSsiSessReal *sessP;
XrdSsiRequest  *rqstP;
char           *dataBuff;
int             dataRlen;
TaskStat        tStat;
unsigned short  tmOut;
short           tskID;
bool            mhPend;
};
#endif
