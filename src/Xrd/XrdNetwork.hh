#ifndef __XRD_Network_H__
#define __XRD_Network_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d N e t w o r k . h h                          */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "XrdOuc/XrdOucPthread.hh"

// Options for accept/connect
//
#define XRDNET_KEEPALIVE 0x0001
#define XRDNET_NODELAY   0x0002
#define XRDNET_NOEMSG    0x0010
#define XRDNET_DEFAULTS  0x8000


// The XrdNetwork class defines a generic network where we can define common
// initial tcp/ip and udp operations.
//
class XrdOucError;
class XrdOucSecurity;
class XrdLink;

class XrdNetwork
{
public:

XrdLink    *Accept(int opts=XRDNET_DEFAULTS, int timeout=-1);

int          Bind(int port, const char *contype="tcp");
int          Bind(const char *path, const char *contype="stream");

XrdLink    *Connect(char *host, int port, int opts=0);

static int   findPort(const char *servname, const char *servtype="tcp");

static char *FullHostName(char *host=0);

static int   getHostAddr(char *hostname, struct sockaddr_in &InetAddr);

static char *getHostName(struct sockaddr_in &addr, char *hbuff=0, int hlen=0);

static char *Host2IP(char *hname);

       void  setDomain(const char *dname)
                      {if (Domain) free(Domain);
                       Domain = strdup(dname);
                       Domlen = strlen(dname);
                      }
       void  setOption(int opt) {netOpts  = opt;}
       void  setWindow(int wsz) {Windowsz = wsz;}

static unsigned long IPAddr(struct sockaddr_in *InetAddr)
                {return (unsigned long)(InetAddr->sin_addr.s_addr);}

void         unBind() {if (iofd >= 0) {close(iofd); iofd = -1; Portnum = 0;}}

             XrdNetwork(XrdOucError *erp, XrdOucSecurity *secp=0)
                  {iofd = PortType = -1; eDest = erp; Police = secp;
                   Domlen = Portnum = Windowsz = netOpts = 0;
                   Domain = 0;
                  }
            ~XrdNetwork() {unBind();}

private:

XrdOucError       *eDest;
static const char *TraceID;
XrdOucSecurity     *Police;
char              *Domain;
int                Domlen;
int                iofd;
int                Portnum;
int                PortType;
int                Windowsz;
int                netOpts;

XrdLink    *do_Accept(int);
XrdLink    *do_Receive(int);
static char *LowCase(char *str);
       int   setOpts(const char *who, int iofd, int opts=0);
static int   getProtoID(const char *pname);
static char *Peername(int snum, struct sockaddr_in *sap=0);
       void  Trim(char *hname);
};
#endif
