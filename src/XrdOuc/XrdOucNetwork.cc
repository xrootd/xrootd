/******************************************************************************/
/*                                                                            */
/*                      X r d O u c N e t w o r k . c c                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

const char *XrdOucNetworkCVSID = "$Id$";
  
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLink.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucSecurity.hh"
#include "XrdOuc/XrdOucTrace.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#ifndef NODEBUG
#define NETDBG(x) {if (eTrace && eTrace->What & tFlag) \
                      {eTrace->Beg(epname); cerr <<x; eTrace->End();}}
#else
#define NETDBG(x)
#endif

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/

XrdOucLink *XrdOucNetwork::Accept(int opts, int timeout)
{
   int retc;
   struct pollfd sfd[1];

// Make sure we are bound to a port
//
   if (iofd < 0) {eDest->Emsg("Accept", "Network not bound to a port.");
                  return (XrdOucLink *)0;
                 }

// Setup up the poll structure to wait for new connections
//
   if (timeout >= 0)
      {sfd[0].fd     = iofd;
       sfd[0].events = POLLIN|POLLRDNORM|POLLRDBAND|POLLPRI|POLLHUP;
       sfd[0].revents = 0;
       do {retc = poll(sfd, 1, timeout);}
           while(retc < 0 && (errno == EAGAIN || errno == EINTR));
       if (!sfd[0].revents)
          {eDest->Emsg("Accept", "Accept timed out.");
           return (XrdOucLink *)0;
          }
      }

// Return link based on the type of socket we have
//
   return (PortType == SOCK_STREAM ? do_Accept(opts) : do_Receive(opts));
}
  
/******************************************************************************/
/*                                  B i n d                                   */
/******************************************************************************/
  
int XrdOucNetwork::Bind(int bindport, const char *contype)
{
    const char *epname = "Bind";
    struct sockaddr_in InetAddr;
    struct sockaddr *SockAddr = (struct sockaddr *)&InetAddr;
    int SockSize = sizeof(InetAddr), retc;
    char *action;
    unsigned short port = bindport;

// Close any open socket here
//
   unBind();

// Establish port type
//
   PortType = ('u'==*contype ? SOCK_DGRAM : SOCK_STREAM);
   Portnum  = bindport;

// Allocate a socket descriptor and set the options
//
   if ((iofd = socket(PF_INET, PortType, 0)) < 0)
      return eDest->Emsg("Bind",-errno,contype,(char *)"creating inet socket");
   setOpts("bind", iofd, (PortType == SOCK_STREAM ? OUC_NODELAY : 0));

// Attempt to do a bind
//
   action = (char *)"binding";
   InetAddr.sin_family = AF_INET;
   InetAddr.sin_addr.s_addr = INADDR_ANY;
   InetAddr.sin_port = htons(port);
   if (!(retc = bind(iofd, SockAddr, SockSize)) && PortType == SOCK_STREAM)
      {action = (char *)"listening on";
       retc = listen(iofd, 8);
      }

// Check for any errors and return.
//
   if (retc) 
      return eDest->Emsg("Bind",-errno,(const char *)action,(char *)"socket");
   NETDBG("Bound to " <<contype <<" port " <<bindport);
   return 0;
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/

XrdOucLink *XrdOucNetwork::Connect(char *host, int port, int opts)
{
    struct sockaddr_in InetAddr[1];
    struct sockaddr *SockAddr = (struct sockaddr *)&InetAddr[0];
    int SockSize = sizeof(InetAddr[0]);
    int newfd, retc;

// Determine the host address
//
   if (!getHostAddr(host, InetAddr))
      {if (!(opts & OUC_NOEMSG))
          eDest->Emsg("Connect", EHOSTUNREACH, "unable to find", host);
       return (XrdOucLink *)0;
      }
   InetAddr[0].sin_port = htons(port);

// Allocate a socket descriptor
//
   if ((newfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
      {if (!(opts & OUC_NOEMSG))
          eDest->Emsg("Connect", errno, "creating inet socket to", host);
       return (XrdOucLink *)0;
      }

// Allocate a new lnk for this socket and set the options
//
   setOpts("connect", newfd, opts);

// Connect to the host
//
   do {retc = connect(newfd, SockAddr, SockSize);}
      while(retc < 0 && errno == EINTR);
   if (retc)
      {if (!(opts & OUC_NOEMSG))
          eDest->Emsg("Connect", errno, "unable to connect to", host);
       close(newfd);
       return (XrdOucLink *)0;
      }

// Return the link
//
   return XrdOucLink::Alloc(eDest, newfd, &InetAddr[0], host);
}
  
/******************************************************************************/
/*                              f i n d P o r t                               */
/******************************************************************************/

int XrdOucNetwork::findPort(const char *servname, const char *servtype)
{
   struct servent sent, *sp;
   char sbuff[1024];
   int portnum;

// Try to find minimum port number
//
   if (GETSERVBYNAME(servname, servtype, &sent, sbuff, sizeof(sbuff), sp))
      portnum = sp->s_port;
      else portnum = 0;

// Do final port check and return
//
   return portnum;
}

/******************************************************************************/
/*                          F u l l H o s t N a m e                           */
/******************************************************************************/

char *XrdOucNetwork::FullHostName(char *host)
{
   char myname[260], *hp;
   struct sockaddr_in InetAddr[1];
  
// Identify ourselves if we don't have a passed hostname
//
   if (host) hp = host;
      else if (gethostname(myname, sizeof(myname))) hp = (char *)"";
              else hp = myname;

// Get our address
//
   if (!getHostAddr(hp, InetAddr)) return strdup(hp);

// Convert it to a fully qualified host name
//
   return getHostName(InetAddr[0]);
}

/******************************************************************************/
/*                           g e t H o s t A d d r                            */
/******************************************************************************/
  
int XrdOucNetwork::getHostAddr(char               *hostname,
                               struct sockaddr_in  InetAddr[],
                               int                 maxipa)
{
    u_long addr;
    struct hostent hent, *hp = 0;
    char **p, hbuff[1024];
    int rc = 0, i = 0, byaddr = 0;

// Make sure we have something to lookup here
//
    if (!hostname || !hostname[0]) return 0;

// Try to resulve the name
//
    if (hostname[0] > '9' || hostname[0] < '0')
       GETHOSTBYNAME((const char *)hostname,&hent,hbuff,sizeof(hbuff),hp,&rc);
       else if ((int)(addr = inet_addr(hostname)) == -1) errno = EINVAL;
               else {GETHOSTBYADDR((const char *)&addr,sizeof(addr),AF_INET,
                                   &hent, hbuff, sizeof(hbuff), hp, &rc);
                     byaddr = 1;
                    }

// Check if we resolved the name
//
   if (hp) 
      {for (p = hp->h_addr_list; *p != 0 && i < maxipa; p++, i++)
           {memcpy((void *)&InetAddr[i].sin_addr, (const void *)*p,
                   sizeof(InetAddr[0].sin_addr));
            InetAddr[i].sin_family = AF_INET;
           }
       return i;
      }

// We failed to match
//
   return 0;
}

/******************************************************************************/
/*                           g e t H o s t N a m e                            */
/******************************************************************************/

char *XrdOucNetwork::getHostName(struct sockaddr_in &addr)
{
   struct hostent hent, *hp;
   char *hname, hbuff[1024];
   int rc;

// Convert it to a host name
//
   if (GETHOSTBYADDR((const char *)&addr.sin_addr, sizeof(addr.sin_addr),
                     AF_INET, &hent, hbuff, sizeof(hbuff), hp, &rc))
             hname = LowCase(strdup(hp->h_name));
        else hname = strdup(inet_ntoa(addr.sin_addr));

// Return the name
//
   return hname;
}
 
/******************************************************************************/
/*                             H o s t 2 D e s t                              */
/******************************************************************************/
  
int XrdOucNetwork::Host2Dest(char               *hostname,
                             struct sockaddr_in &DestAddr)
{ char *cp;
  int port;
  struct sockaddr_in InetAddr[1];

// Find the colon in the host name
//
   if (!(cp = index((const char *)hostname, (int)':'))) return 0;
   *cp = '\0';

// Convert hostname to an ascii ip address
//
   if (!getHostAddr(hostname, InetAddr)) {*cp = ':'; return 0;}

// Insert port number in address
//
   if (!(port = atoi((const char *)cp+1)) || port > 0xffff)
      {*cp = ':'; return 0;}

// Compose the destination address
//
   *cp = ':';
   InetAddr[0].sin_family = AF_INET;
   InetAddr[0].sin_port = htons(port);
   memcpy((void *)&DestAddr, (const void *)&InetAddr[0], sizeof(sockaddr_in));
   return 1;
}

/******************************************************************************/
/*                               H o s t 2 I P                                */
/******************************************************************************/
  
char *XrdOucNetwork::Host2IP(char *hname, unsigned long *ipaddr)
{
   struct sockaddr_in InetAddr[1];

// Convert hostname to an ascii ip address
//
   if (!getHostAddr(hname, InetAddr)) return 0;
   if (ipaddr) memcpy(ipaddr, &InetAddr[0].sin_addr, sizeof(unsigned long));
   return inet_ntoa(InetAddr[0].sin_addr);
}

/******************************************************************************/
/*                                 R e l a y                                  */
/******************************************************************************/
  
XrdOucLink *XrdOucNetwork::Relay(XrdOucError *errp, int opts)
{
   struct sockaddr_in InetAddr;
   int myiofd;

// Allocate a socket descriptor and set the options
//
   if ((myiofd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
      {if (errp) errp->Emsg("Relay",-errno,(char *)"creating udp socket");
       return (XrdOucLink *)0;
      }

// Return a link owning this socket
//
   fcntl(myiofd, F_SETFD, FD_CLOEXEC);
   memset((void *)&InetAddr, 0, sizeof(InetAddr));
   return XrdOucLink::Alloc(errp, myiofd, &InetAddr, strdup("relay"));
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                             d o _ A c c e p t                              */
/******************************************************************************/
  
XrdOucLink *XrdOucNetwork::do_Accept(int opts)
{
  const char *epname = "Accept";
  int        newfd;
  char      *hname;
  XrdOucLink *newconn;
  struct sockaddr_in addr;
  socklen_t  addrlen = sizeof(addr);

// Accept a connection
//
   do {newfd = accept(iofd, (struct sockaddr *)&addr, (socklen_t *)&addrlen);}
      while(newfd < 0 && errno == EINTR);

   if (newfd < 0)
      {eDest->Emsg("Accept", errno, "performing accept."); return 0;}

// Authorize by ip address of full (slow) hostname format
//
   if (Police && !(hname = Police->Authorize(&addr)))
      {eDest->Emsg("Accept", -EACCES, "accepting connection from",
                     (hname = getHostName(addr)));
       free(hname);
       close(newfd);
       return 0;
      } else hname = getHostName(addr);

// Set socket options
//
   setOpts("accept", newfd, opts);
   NETDBG("Accepted tcp connection from " <<hname);

// Allocate a new network object
//
   if (!(newconn = XrdOucLink::Alloc(eDest, newfd, &addr, hname)))
      {eDest->Emsg("Accept", "Unable to allocate new link for", hname);
       close(newfd);
      }

// Return result
//
   return newconn;
}

/******************************************************************************/
/*                            d o _ R e c e i v e                             */
/******************************************************************************/

XrdOucLink *XrdOucNetwork::do_Receive(int opts)
{
  const char *epname = "Receive";
  int        maxlen, dlen, newfd;
  char      *hname;
  XrdOucLink *newconn;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  XrdOucBuffer *bp;

// Get a buffer
//
   bp = XrdOucBuffer::Alloc();
   maxlen = bp->BuffSize()-1;

// Receive the next message
//
   do {dlen = recvfrom(iofd, (void *)bp->data, maxlen, 0,
                      (struct sockaddr *)&addr, (socklen_t *)&addrlen);}
      while(dlen < 0 && errno == EINTR);

   if (dlen < 0)
      {eDest->Emsg("Receive", errno, "performing recvmsg."); 
       bp->Recycle();
       return 0;
      }

// Authorize this connection
//
   if (addr.sin_addr.s_addr == 0x7f000001
   || (Police && !(hname = Police->Authorize(&addr))))
      {eDest->Emsg("Accept", -EACCES, "accepting connection from",
                    (hname = getHostName(addr)));
       bp->Recycle();
       free(hname);
       return 0;
      } else hname = getHostName(addr);

// Duplicate this file descriptor
//
   NETDBG("Rcv " <<hname <<" udp " <<dlen <<" bytes");
   newfd = dup(iofd);
   fcntl(newfd, F_SETFD, FD_CLOEXEC);

// Allocate a new network object
//
   if (!(newconn = XrdOucLink::Alloc(eDest, newfd, &addr, hname, bp)))
      {eDest->Emsg("Accept", "Unable to allocate new link for", hname);
       bp->Recycle(); close(newfd);
      }

// Return result
//
   bp->dlen = dlen;
   bp->data[dlen] = '\0';
   return newconn;
}

/******************************************************************************/
/*                               L o w C a s e                                */
/******************************************************************************/
  
char *XrdOucNetwork::LowCase(char *str)
{
   char *sp = str;

   while(*sp) {if (isupper((int)*sp)) *sp = (char)tolower((int)*sp); sp++;}

   return str;
}

/******************************************************************************/
/*                               s e t O p t s                                */
/******************************************************************************/

int XrdOucNetwork::setOpts(const char *who, int xfd, int opts)
{
   static int tcpprotid = getProtoID("tcp");
   int rc = 0;
   struct linger liopts;
   const int one = 1;
   const socklen_t szone = (socklen_t)sizeof(one);
   const socklen_t szlio = (socklen_t)sizeof(liopts);

   fcntl(xfd, F_SETFD, FD_CLOEXEC);
   if (opts & OUC_NOBLOCK) fcntl(xfd, F_SETFD, O_NONBLOCK);

   if (rc |= setsockopt(xfd,SOL_SOCKET,SO_REUSEADDR,(const void *)&one,szone))
      eDest->Emsg(who, errno, "setting REUSEADDR");

   liopts.l_onoff = 1; liopts.l_linger = 1;
   if (rc |= setsockopt(xfd,SOL_SOCKET,SO_LINGER,(const void *)&liopts,szlio))
      eDest->Emsg(who, errno, "setting LINGER");

   if ((opts & OUC_KEEPALIVE)
   && (rc |= setsockopt(xfd,SOL_SOCKET,SO_KEEPALIVE,(const void *)&one,szone)))
      eDest->Emsg(who, errno, "setting KEEPALIVE");

   if ((opts & OUC_NODELAY)
   && setsockopt(xfd, tcpprotid, TCP_NODELAY, (const void *)&one,szone))
      eDest->Emsg(who, errno, "setting NODELAY");

   return rc;
}
  
/******************************************************************************/
/*                            g e t P r o t o I D                             */
/******************************************************************************/

#define OUC_IPPROTO_TCP 6

int XrdOucNetwork::getProtoID(const char *pname)
{
    struct protoent *pp;

    if (!(pp = getprotobyname(pname))) return OUC_IPPROTO_TCP;
       else return pp->p_proto;
}

/******************************************************************************/
/*                              P e e r n a m e                               */
/******************************************************************************/
  
char *XrdOucNetwork::Peername(int snum, struct sockaddr_in *sap)
{
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);

// Get the address on the other side of this socket
//
   if (!sap) sap = &addr;
   if (getpeername(snum, (struct sockaddr *)sap, &addrlen) < 0)
      {cerr <<"Peername: " <<strerror(errno) <<"obtaining peer name." <<endl;
       return (char *)0;
      }

// Convert it to a host name
//
   return getHostName(*sap);
}
