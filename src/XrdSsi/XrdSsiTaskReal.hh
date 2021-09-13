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

#include <cstdio>

#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiEvent.hh"
#include "XrdSsi/XrdSsiStream.hh"
#include "XrdSsi/XrdSsiResponder.hh"

class XrdSsiRequest;
class XrdSsiSessReal;
class XrdSysSemaphore;

class XrdSsiTaskReal : public XrdSsiEvent, public XrdSsiResponder,
                       public XrdSsiStream
{
public:

enum TaskStat {isPend=0, isWrite, isSync, isReady, isDone, isDead};

void   Detach(bool force=false);

void   Finished(      XrdSsiRequest  &rqstR,
                const XrdSsiRespInfo &rInfo,
                      bool            cancel=false);

void  *Implementation() {return (void *)this;}

bool   Kill();

inline
int    ID() {return tskID;}

inline
void   Init(XrdSsiRequest *rP, unsigned short tmo=0)
           {rqstP = rP, tStat = isPend; tmOut = tmo; wPost = 0;
            mhPend = false; defer = 0;
            attList.next = attList.prev = this;
            if (mdResp) {delete mdResp; mdResp = 0;}
           }

void   PostError();

const 
char  *RequestID() {return rqstP->GetRequestID();}

void   SchedError(XrdSsiErrInfo *eInfo=0);

void   SendError();

bool   SendRequest(const char *node);

int    SetBuff(XrdSsiErrInfo &eRef, char *buff, int blen, bool &last);

bool   SetBuff(XrdSsiErrInfo &eRef, char *buff, int blen);

void   SetTaskID(uint32_t tid, uint32_t sid)
                {tskID = tid;
                 snprintf(tident, sizeof(tident), "T %u#%u", sid, tid);
                }

int    XeqEvent(XrdCl::XRootDStatus *status, XrdCl::AnyObject **respP);

void   XeqEvFin();

       XrdSsiTaskReal(XrdSsiSessReal *sP)
                     : XrdSsiStream(XrdSsiStream::isPassive),
                       sessP(sP), mdResp(0), wPost(0), tskID(0),
                       defer(0), mhPend(false)
                    {}

      ~XrdSsiTaskReal() {if (mdResp) delete mdResp;}

struct dlQ {XrdSsiTaskReal *next; XrdSsiTaskReal *prev;};
dlQ             attList;

enum respType     {isBad=0, isAlert, isData, isStream};

private:

bool              Ask4Resp();
respType          GetResp(XrdCl::AnyObject **respP, char *&dbuf, int &dlen);
bool              RespErr(XrdCl::XRootDStatus *status);

XrdSsiErrInfo     errInfo;
XrdSsiSessReal   *sessP;
XrdSsiRequest    *rqstP;
XrdCl::AnyObject *mdResp;
XrdSysSemaphore  *wPost;
char             *dataBuff;
int               dataRlen;
TaskStat          tStat;
uint32_t          tskID;
int               defer;  // Number of oustanding defer requests
unsigned short    tmOut;
bool              mhPend;
};
#endif
