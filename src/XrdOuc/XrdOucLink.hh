#ifndef __OUC_LINK_H__
#define __OUC_LINK_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d O u c L i n k . h h                          */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucChain.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucPthread.hh"

// Options for SetOpts
//
#define OUC_LINK_NOBLOCK 0x0001

// The XrdOucLink class defines the i/o operations on a network link.
//
class XrdOucError;
class XrdOucStream;

class XrdOucLink
{
public:

XrdOucQSItem<XrdOucLink> LinkLink;

static XrdOucLink *Alloc(XrdOucError *erp, int fd, sockaddr_in *ip,
                         char *host=0,  XrdOucBuffer *bp=0);

int           Close(int defer=0);

int           FDnum() {return FD;}

char         *GetLine();

char         *GetToken(char **rest);
char         *GetToken(void);

int           isConnected(void) {return (Stream != 0) && (FD >= 0);}

int           LastError();

unsigned long Addr() {return XrdOucNetwork::IPAddr(&InetAddr);}

char         *Name() {return Lname;}

void          Recycle();

int           Send(char *buff, int blen=0, int tmo=-1);
int           Send(void *buff, int blen=0, int tmo=-1);
int           Send(char *dest, char *buff, int blen=0, int tmo=-1);
int           Send(const struct iovec iov[], int iovcnt, int tmo = -1);
int           Send(char *dest, const struct iovec iov[], int iovcnt, int tmo=-1);

int           Recv(char *buff, long blen);

void          Set(int maxl);

void          SetOpts(int opts);

              XrdOucLink(XrdOucError *erp) : LinkLink(this)
                          {FD = -1; Lname = 0; recvbuff = sendbuff = 0;
                           Stream = 0; eDest = erp;
                          }
             ~XrdOucLink() {Close();}

private:

int OK2Send(int timeout=0, char *dest=0);
int retErr(int ecode, char *dest=0);

XrdOucMutex         IOMutex;
int                 FD;
struct sockaddr_in  InetAddr;
char               *Lname;
XrdOucBuffer       *recvbuff;  // udp receive buffer
XrdOucBuffer       *sendbuff;  // udp send    buffer
XrdOucStream       *Stream;
XrdOucError        *eDest;

static XrdOucMutex             LinkList;
static XrdOucStack<XrdOucLink> LinkStack;
static int                     size;
static int                     maxlink;
static int                     numlink;
};
#endif
