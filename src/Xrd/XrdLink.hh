#ifndef __XRD_LINK_H__
#define __XRD_LINK_H__
/******************************************************************************/
/*                                                                            */
/*                            X r d L i n k . h h                             */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#include "XrdOuc/XrdOucPthread.hh"

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdProtocol.hh"
  
/******************************************************************************/
/*                       X r d L i n k   O p t i o n s                        */
/******************************************************************************/
  
#define XRDLINK_RDLOCK  0x0001
#define XRDLINK_NOCLOSE 0x0002

/******************************************************************************/
/*                      C l a s s   D e f i n i t i o n                       */
/******************************************************************************/
  
class XrdNetBuffer;
class XrdNetPeer;
class XrdPoll;

class XrdLink : XrdJob
{
public:
friend class XrdLinkScan;
friend class XrdPoll;
friend class XrdPollPoll;
friend class XrdPollDev;

static XrdLink *Alloc(XrdNetPeer &Peer, int opts=0);

int           Close();

void          DoIt();

int           Enable();

int           FDnum() {return FD;}

static XrdLink *fd2link(int fd)
                {if (fd < 0) fd = -fd; return (LinkBat[fd] ? LinkTab[fd] : 0);}

static XrdLink *fd2link(int fd, int inst)
                {if (fd < 0) fd = -fd; 
                 if (LinkBat[fd] && LinkTab[fd] 
                 && LinkTab[fd]->Instance == inst) return LinkTab[fd];
                 return (XrdLink *)0;
                }

char         *ID;      // This is referenced a lot

int           isConnected() {return FD >= 0;}

const char   *Name(sockaddr *ipaddr=0)
                     {if (ipaddr) memcpy(ipaddr, &InetAddr, sizeof(ipaddr));
                      return (const char *)Lname;
                     }

static XrdLink *nextLink(int &nextFD);

int           Peek(char *buff, long blen, int timeout=-1);

int           Recv(char *buff, long blen);
int           Recv(char *buff, long blen, int timeout);

int           Send(char *buff, long blen);
int           Send(const struct iovec *iov, int iocnt, long bytes=0);

void          setEtext(const char *text);

void          setID(const char *userid, int procid);

void          Serialize();                              // ASYNC Mode

void          setRef(int cnt);                          // ASYNC Mode

static int    Setup(int maxfd, int idlewait);

static int    Stats(char *buff, int blen, int do_sync=0);

       void   syncStats(int *ctime=0);

XrdProtocol  *setProtocol(XrdProtocol *pp);

int           UseCnt() {return InUse;}

              XrdLink();
             ~XrdLink() {}  // Is never deleted!

private:

void   Reset();

static XrdOucMutex   LTMutex;    // For the LinkTab only LTMutex->IOMutex allowed
static XrdLink     **LinkTab;
static char         *LinkBat;
static unsigned int  LinkAlloc;
static int           LTLast;
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
       long long        BytesIn;
       long long        BytesOut;
       int              stallCnt;
       int              tardyCnt;
static XrdOucMutex  statsMutex;

// Identification section
//
struct sockaddr     InetAddr;
char                Uname[24];  // Uname and Lname must be adjacent!
char                Lname[232];

XrdOucMutex         opMutex;
XrdOucMutex         rdMutex;
XrdOucMutex         wrMutex;
XrdOucSemaphore     IOSemaphore;
XrdLink            *Next;
XrdNetBuffer       *udpbuff;
XrdProtocol        *Protocol;
XrdProtocol        *ProtoAlt;
XrdPoll            *Poller;
struct pollfd      *PollEnt;
char               *Etext;
int                 FD;
int                 Instance;
time_t              conTime;
int                 InUse;
int                 doPost;
char                LockReads;
char                KeepFD;
char                isEnabled;
char                isIdle;
char                inQ;
};
#endif
