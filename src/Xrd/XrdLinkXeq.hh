#ifndef __XRD_LINKXEQ_H__
#define __XRD_LINKXEQ_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d L i n k X e q . h h                          */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>
#include <fcntl.h>
#include <ctime>

#include "Xrd/XrdLink.hh"
#include "Xrd/XrdLinkInfo.hh"
#include "Xrd/XrdPollInfo.hh"
#include "Xrd/XrdProtocol.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdTls/XrdTls.hh"
#include "XrdTls/XrdTlsSocket.hh"
  
/******************************************************************************/
/*                      C l a s s   D e f i n i t i o n                       */
/******************************************************************************/
  
class XrdSendQ;

class XrdLinkXeq : protected XrdLink
{
public:

inline
XrdNetAddrInfo *AddrInfo() {return (XrdNetAddrInfo *)&Addr;}

int           Backlog();

int           Client(char *buff, int blen);

int           Close(bool defer=false);

void          DoIt(); // Override


       int    getIOStats(long long &inbytes, long long &outbytes,
                              int  &numstall,     int  &numtardy)
                        { inbytes = BytesIn + BytesInTot;
                         outbytes = BytesOut+BytesOutTot;
                         numstall = stallCnt + stallCntTot;
                         numtardy = tardyCnt + tardyCntTot;
                         return LinkInfo.InUse;
                        }

XrdTlsPeerCerts *getPeerCerts();

static int    getName(int &curr, char *bname, int blen, XrdLinkMatch *who=0);

inline
XrdProtocol  *getProtocol() {return Protocol;}

inline
const char   *Name() const {return (const char *)Lname;}

inline const
XrdNetAddr   *NetAddr() const {return &Addr;}

int           Peek(char *buff, int blen, int timeout=-1);

int           Recv(char *buff, int blen);
int           Recv(char *buff, int blen, int timeout);
int           Recv(const struct iovec *iov, int iocnt, int timeout);

int           RecvAll(char *buff, int blen, int timeout=-1);

bool          Register(const char *hName);

int           Send(const char *buff, int blen);
int           Send(const struct iovec *iov, int iocnt, int bytes=0);

int           Send(const sfVec *sdP, int sdn); // Iff sfOK > 0

void          setID(const char *userid, int procid);

void          setLocation(XrdNetAddrInfo::LocInfo &loc) {Addr.SetLocation(loc);}

bool          setNB();

XrdProtocol  *setProtocol(XrdProtocol *pp, bool push);

void          setProtName(const char *name);

bool          setTLS(bool enable, XrdTlsContext *ctx=0);

       void   Shutdown(bool getLock);

static int    Stats(char *buff, int blen, bool do_sync=false);

       void   syncStats(int *ctime=0);

int           TLS_Peek(char *Buff, int Blen, int timeout);

int           TLS_Recv(char *Buff, int Blen);

int           TLS_Recv(char *Buff, int Blen, int timeout, bool havelock=false);

int           TLS_Recv(const struct iovec *iov, int iocnt, int timeout);

int           TLS_RecvAll(char *Buff, int Blen, int timeout);

int           TLS_Send(const char *Buff, int Blen);

int           TLS_Send(const struct iovec *iov, int iocnt, int bytes);

int           TLS_Send(const sfVec *sfP, int sfN);

const char   *verTLS();

              XrdLinkXeq();
             ~XrdLinkXeq() {}  // Is never deleted!

XrdLinkInfo   LinkInfo;
XrdPollInfo   PollInfo;

protected:

int    RecvIOV(const struct iovec *iov, int iocnt);
void   Reset();
int    sendData(const char *Buff, int Blen);
int    SendIOV(const struct iovec *iov, int iocnt, int bytes);
int    SFError(int rc);
int    TLS_Error(const char *act, XrdTls::RC rc);
bool   TLS_Write(const char *Buff, int Blen);

static const char   *TraceID;

// Statistical area (global and local)
//
static long long    LinkBytesIn;
static long long    LinkBytesOut;
static long long    LinkConTime;
static long long    LinkCountTot;
static int          LinkCount;
static int          LinkCountMax;
static int          LinkTimeOuts;
static int          LinkStalls;
static int          LinkSfIntr;
       long long    BytesIn;
       long long    BytesInTot;
       long long    BytesOut;
       long long    BytesOutTot;
       int          stallCnt;
       int          stallCntTot;
       int          tardyCnt;
       int          tardyCntTot;
       int          SfIntr;
static XrdSysMutex  statsMutex;

// Protocol section
//
XrdProtocol   *Protocol;             // -> Protocol tied to the link
XrdProtocol   *ProtoAlt;             // -> Alternate/stacked protocol

// TLS section
//
XrdTlsSocket   tlsIO;

// Identification section
//
XrdNetAddr          Addr;
XrdSysMutex         rdMutex;
XrdSysMutex         wrMutex;
XrdSendQ           *sendQ;          // Protected by wrMutex && opMutex
int                 HNlen;
bool                LockReads;
bool                KeepFD;
char                isIdle;
char                Uname[24];       // Uname and Lname must be adjacent!
char                Lname[256];
};
#endif
