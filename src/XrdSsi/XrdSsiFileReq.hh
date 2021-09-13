#ifndef __SSI_FILEREQ_H__
#define __SSI_FILEREQ_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i F i l e R e q . h h                       */
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

#include <cstring>
#include <sys/types.h>

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSfs/XrdSfsXio.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiResponder.hh"
#include "XrdSsi/XrdSsiStream.hh"
#include "XrdSys/XrdSysPthread.hh"

class  XrdOucErrInfo;
class  XrdSsiAlert;
class  XrdSsiFileResource;
class  XrdSsiFileSess;
class  XrdSsiRespInfoMsg;
class  XrdSsiRRInfo;
class  XrdSsiService;
class  XrdSsiStream;

class XrdSsiFileReq : public XrdSsiRequest, public XrdOucEICB, public XrdJob
{
public:


// SsiRequest methods
//
        void           Activate(XrdOucBuffer *oP, XrdSfsXioHandle bR, int rSz);

        void           Alert(XrdSsiRespInfoMsg &aMsg);

static  XrdSsiFileReq *Alloc(XrdOucErrInfo  *eP, XrdSsiFileResource *rP,
                             XrdSsiFileSess *fP, const char         *sn,
                             const char     *id, unsigned int        rnum);

        void           Finalize();

        using          XrdSsiRequest::Finished;

        void           Finished(      XrdSsiRequest  &rqstR,
                                const XrdSsiRespInfo &rInfo,
                                      bool            cancel=false) {}

        char          *GetRequest(int &rLen);

        bool           ProcessResponse(const XrdSsiErrInfo  &eInfo,
                                       const XrdSsiRespInfo &resp);

        XrdSfsXferSize Read(bool           &done,
                            char           *buffer,
                            XrdSfsXferSize  blen);
                        
        void           RelRequestBuffer();

        int            Send(XrdSfsDio *sfDio, XrdSfsXferSize size);

static  void           SetMax(int mVal) {freeMax = mVal;}

        bool           WantResponse(XrdOucErrInfo &eInfo);

// OucEICB methods
//
        void           Done(int &Result, XrdOucErrInfo *cbInfo,
                            const char *path=0);

        int            Same(unsigned long long arg1, unsigned long long arg2)
                           {return 0;}
// Job methods
//
        void           DoIt();

// Constructor and destructor
//
                       XrdSsiFileReq(const char *cID=0)
                                    : frqMutex(XrdSsiMutex::Recursive)
                                      {Init(cID);}

virtual               ~XrdSsiFileReq() {if (tident) free(tident);}

enum reqState {wtReq=0, xqReq, wtRsp, doRsp, odRsp, erRsp, rsEnd};
enum rspState {isNew=0, isBegun, isBound, isAbort, isDone, isMax};

private:

void                   BindDone(); // Override
void                   Dispose();  // Override
int                    Emsg(const char *pfx, int ecode, const char *op);
int                    Emsg(const char *pfx, XrdSsiErrInfo &eObj,
                            const char *op);
void                   Init(const char *cID=0);
XrdSfsXferSize         readStrmA(XrdSsiStream *strmP, char *buff,
                                 XrdSfsXferSize blen);
XrdSfsXferSize         readStrmP(XrdSsiStream *strmP, char *buff,
                                 XrdSfsXferSize blen);
int                    sendStrmA(XrdSsiStream *strmP, XrdSfsDio *sfDio,
                                 XrdSfsXferSize blen);
void                   Recycle();
void                   WakeUp(XrdSsiAlert *aP=0);

static XrdSysMutex     aqMutex;
static XrdSsiFileReq  *freeReq;
static int             freeCnt;
static int             freeMax;

XrdSsiMutex            frqMutex;
XrdSsiFileReq         *nextReq;
XrdSysSemaphore       *finWait;
XrdOucEICB            *respCB;
unsigned long long     respCBarg;

XrdSsiAlert           *alrtSent;
XrdSsiAlert           *alrtPend;
XrdSsiAlert           *alrtLast;

char                  *tident;
const char            *sessN;
XrdOucErrInfo         *cbInfo;
XrdSsiFileResource    *fileR;
XrdSsiFileSess        *fileP;
char                  *respBuf;
long long              respOff;
union {long long       fileSz;
       int             respLen;
      };
XrdSfsXioHandle        sfsBref;
XrdOucBuffer          *oucBuff;
XrdSsiStream::Buffer  *strBuff;
reqState               myState;
rspState               urState;
int                    reqSize;
unsigned int           reqID;
bool                   haveResp;
bool                   respWait;
bool                   strmEOF;
bool                   schedDone;
bool                   isEnding;
char                   rID[8];
};
#endif
