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

#include <netinet/in.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#include "XrdOuc/XrdOucPthread.hh"

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdObject.hh"
#include "Xrd/XrdProtocol.hh"
  
class XrdBuffer;
class XrdPoll;

/******************************************************************************/
/*                        C l a s s   x r d _ L i n k                         */
/******************************************************************************/

class XrdLink : XrdJob
{
public:
friend class XrdLinkScan;
friend class XrdPoll;
friend class XrdPollPoll;
friend class XrdPollDev;

static XrdObjectQ<XrdLink> LinkStack;

static XrdLink *Alloc(int fd, sockaddr_in *ip, char *host=0,
                       XrdBuffer *bp=0);

int           Close();

void          DoIt();

int           Enable();

int           FDnum() {return FD;}

static XrdLink *fd2link(int fd) {return LinkTab[(fd < 0 ? -fd : fd)];}

char         *ID;      // This is referenced a lot

int           isConnected() {return FD >= 0;}

const char   *Name(sockaddr_in *ipaddr=0)
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

XrdProtocol *setProtocol(XrdProtocol *pp);

int           UseCnt() {return InUse;}

              XrdLink(const char *ltype="connection");
             ~XrdLink() {}

private:

void   Reset();

XrdObject<XrdLink>   LinkLink;
static XrdOucMutex   LTMutex;    // For the LinkTab only LTMutex->IOMutex allowed
static XrdLink     **LinkTab;
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
static XrdOucMutex   statsMutex;

// Identification section
//
struct sockaddr_in  InetAddr;
char                Uname[24];  // Uname and Lname must be adjacent!
char                Lname[232];

XrdOucMutex         IOMutex;
XrdOucSemaphore     IOSemaphore;
XrdLink            *Next;
XrdBuffer          *udpbuff;
XrdProtocol        *Protocol;
XrdProtocol        *ProtoAlt;
XrdPoll            *Poller;
struct pollfd      *PollEnt;
char               *Etext;
int                 FD;
time_t              conTime;
int                 InUse;
int                 doPost;
char                isEnabled;
char                isIdle;
char                isFree;
char                inQ;
};
#endif
