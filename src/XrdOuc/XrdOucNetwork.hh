#ifndef __OUC_Network_H__
#define __OUC_Network_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c N e t w o r k . h h                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "XrdOuc/XrdOucPthread.hh"

// Options for accept/connect
//
#define OUC_KEEPALIVE 0x0001
#define OUC_NODELAY   0x0002
#define OUC_NOBLOCK   0x0004
#define OUC_NOEMSG    0x0010
#define OUC_DEBUG     0x0020


// The XrdOucNetwork class defines a generic network where we can define common
// initial tcp/ip and udp operations.
//
class XrdOucError;
class XrdOucLink;
class XrdOucSecurity;
class XrdOucTrace;

class XrdOucNetwork
{
public:

XrdOucLink  *Accept(int opts=0, int timeout=-1);

int          Bind(int port, const char *contype="tcp");

XrdOucLink  *Connect(char *host, int port, int opts=0);

static int   findPort(const char *servname, const char *servtype="tcp");

static char *FullHostName(char *host=0);

static int   getHostAddr(char *hostname, struct sockaddr_in InetAddr[],
                         int maxipa=1);

static char *getHostName(struct sockaddr_in &addr);

static int   Host2Dest(char *hostname, struct sockaddr_in &DestAddr);

static char *Host2IP(char *hname, unsigned long *ipaddr=0);

static unsigned long IPAddr(struct sockaddr_in *InetAddr)
                {return (unsigned long)(InetAddr->sin_addr.s_addr);}

static XrdOucLink *Relay(XrdOucError *errp, int opts=0);

void         unBind() {if (iofd >= 0) {close(iofd); iofd = Portnum = -1;}}

             XrdOucNetwork(XrdOucError *erp, XrdOucSecurity *secp=0,
                           XrdOucTrace *trp=0, int tflg=0)
                  {iofd = Portnum = PortType = -1; eDest = erp; 
                   eTrace = trp; tFlag = tflg;
                   Police = secp;
                  }
            ~XrdOucNetwork() {unBind();}

private:

XrdOucSecurity *Police;
XrdOucError    *eDest;
XrdOucTrace    *eTrace;
int             tFlag;
int             iofd;
int             Portnum;
int             PortType;

XrdOucLink     *do_Accept(int);
XrdOucLink     *do_Receive(int);
static char    *LowCase(char *str);
int             setOpts(const char *who, int iofd, int opts=0);
static int      getProtoID(const char *pname);
static char    *Peername(int snum, struct sockaddr_in *sap=0);
};
#endif
