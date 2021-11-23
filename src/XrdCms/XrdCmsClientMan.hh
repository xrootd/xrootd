#ifndef __CMS_CLIENTMAN__
#define __CMS_CLIENTMAN__
/******************************************************************************/
/*                                                                            */
/*                    X r d C m s C l i e n t M a n . h h                     */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <sys/uio.h>

#include "XProtocol/YProtocol.hh"

#include "XrdCms/XrdCmsResp.hh"
#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdInet;
class XrdLink;

class XrdCmsClientMan
{
public:

static char          doDebug;

int                  delayResp(XrdOucErrInfo &Resp);

inline int           isActive() {AtomicRet(myData, Active);}

XrdCmsClientMan     *nextManager() {return Next;}

char                *Name() {return Host;}
char                *NPfx() {return HPfx;}

int                  manPort() {return Port;}

int                  Send(unsigned int &iMan, char *msg, int mlen=0);
int                  Send(unsigned int &iMan,  const struct iovec *iov,
                                   int iovcnt, int iotot=0);

void                *Start();

inline int           Suspended() {AtomicBeg(myData);
                                  int sVal = AtomicGet(Suspend);
                                  AtomicEnd(myData);
                                  if (!sVal) return sVal;
                                  return chkStatus();
                                 }

void                 setNext(XrdCmsClientMan *np) {Next = np;}

static void          setNetwork(XrdInet *nP) {Network = nP;}

static void          setConfig(const char *cfn) {ConfigFN = cfn;}

int                  whatsUp(const char *user, const char *path,
                             unsigned int iMan);

inline int           waitTime() {AtomicRet(myData, repWait);}

                  XrdCmsClientMan(char *host,int port,int cw,int nr,int rw,int rd);
                 ~XrdCmsClientMan();

private:
int   Hookup();
int   Receive();
void  relayResp();
int   chkStatus();
void  setStatus();

static XrdSysMutex   manMutex;
static XrdOucBuffPool BuffPool;
static XrdInet      *Network;
static const char   *ConfigFN;
static const int     chkVal = 256;

XrdSysSemaphore   syncResp;
XrdCmsRespQ       RespQ;

XrdCmsClientMan  *Next;
XrdSysMutex       myData;
XrdLink          *Link;
char             *Host;
char             *HPfx;
int               Port;
unsigned int      manInst;
int               manMask;
int               dally;
int               Active;
int               Silent;
int               Suspend;
int               RecvCnt;
int               SendCnt;
int               nrMax;
int               maxMsgID;
int               repWait;
int               repWMax;
int               minDelay;
int               maxDelay;
int               qTime;
int               chkCount;
time_t            lastUpdt;
time_t            lastTOut;
XrdCms::CmsRRHdr  Response;
XrdOucBuffer     *NetBuff;
};
#endif
