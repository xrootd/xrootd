/******************************************************************************/
/*                                                                            */
/*                         X r d N e t w o r k . c c                          */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
 
//         $Id$ 

const char *XrdNetworkCVSID = "$Id$";

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "XrdOuc/XrdOucSecurity.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucError.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdNetwork.hh"
#include "Xrd/XrdTrace.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOucTrace  XrdTrace;

       const char *XrdNetwork::TraceID = "Net";

#define UDPMAXMSG 65536

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/

XrdLink *XrdNetwork::Accept(int opts, int timeout)
{
   int retc;
   struct pollfd sfd[1];

// Make sure we are bound to a port
//
   if (iofd < 0) {eDest->Emsg("Accept", "Network not bound to a port.");
                  return (XrdLink *)0;
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
           return (XrdLink *)0;
          }
      }

// Return link based on the type of socket we have
//
   return (PortType == SOCK_STREAM ? do_Accept(opts) : do_Receive(opts));
}
  
/******************************************************************************/
/*                                  B i n d                                   */
/******************************************************************************/
  
int XrdNetwork::Bind(int bindport, const char *contype)
{
    struct sockaddr_in InetAddr;
    struct sockaddr *SockAddr = (struct sockaddr *)&InetAddr;
    int SockSize = sizeof(InetAddr), retc;
    char *action, buff[32];
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
   setOpts("bind", iofd, (PortType == SOCK_STREAM ? XRDNET_NODELAY : 0));

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
      {sprintf(buff, "port %d", bindport);
       return eDest->Emsg("Bind", -errno, (const char *)action, buff);
      }
   TRACE(NET, "Bound to " <<contype <<" port " <<bindport);
   return 0;
}
  
int XrdNetwork::Bind(const char *path, const char *contype)
{
    struct sockaddr_un UnixAddr;
    struct sockaddr *SockAddr = (struct sockaddr *)&UnixAddr;
    int SockSize = sizeof(UnixAddr), retc;
    char *action;

// Make sure that the name of the socket is not too long
//
   if (strlen(path) > sizeof(UnixAddr.sun_path))
      return eDest->Emsg("Bind",-ENAMETOOLONG,"processing", (char *)path);

// Close any open socket here
//
   unBind();
   unlink(path);

// Allocate a socket descriptor and set the options
//
   PortType = ('d'==*contype ? SOCK_DGRAM : SOCK_STREAM);
   if ((iofd = socket(PF_UNIX, PortType, 0)) < 0)
      return eDest->Emsg("Bind",-errno,(char *)"creating named socket",(char *)path);
   fcntl(iofd, F_SETFD, FD_CLOEXEC);

// Attempt to do a bind
//
   action = (char *)"binding socket";
   UnixAddr.sun_family = AF_UNIX;
   strcpy(UnixAddr.sun_path, path);
   if (!(retc = bind(iofd, SockAddr, SockSize)) && PortType == SOCK_STREAM)
      {action = (char *)"listening on socket";
       retc = listen(iofd, 8);
      }

// Check for any errors and return.
//
   if (retc) 
      return eDest->Emsg("Bind",-errno,(const char *)action,(char *)path);
   TRACE(NET, "Bound " <<contype <<" socket " <<path);
   return 0;
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/

XrdLink *XrdNetwork::Connect(char *host, int port, int opts)
{
    struct sockaddr_in InetAddr;
    struct sockaddr *SockAddr = (struct sockaddr *)&InetAddr;
    int SockSize = sizeof(InetAddr);
    int newfd, retc;

// Determine the host address
//
   if (getHostAddr(host, InetAddr))
      {if (!(opts & XRDNET_NOEMSG))
          eDest->Emsg("Connect", EHOSTUNREACH, "unable to find", host);
       return (XrdLink *)0;
      }
   InetAddr.sin_port = htons(port);

// Allocate a socket descriptor
//
   if ((newfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
      {if (!(opts & XRDNET_NOEMSG))
          eDest->Emsg("Connect", errno, "creating inet socket to", host);
       return (XrdLink *)0;
      }

// Allocate a new lnk for this socket and set the options
//
   setOpts("connect", newfd, opts);

// Connect to the host
//
   do {retc = connect(newfd, SockAddr, SockSize);}
      while(retc < 0 && errno == EINTR);
   if (retc)
      {if (!(opts & XRDNET_NOEMSG))
          eDest->Emsg("Connect", errno, "unable to connect to", host);
       close(newfd);
       return (XrdLink *)0;
      }

// Return the link
//
   return XrdLink::Alloc(newfd, &InetAddr, host);
}

/******************************************************************************/
/*                              f i n d P o r t                               */
/******************************************************************************/

int XrdNetwork::findPort(const char *servname, const char *servtype)
{
   struct servent sent, *sp = 0;
   char sbuff[1024];
   int portnum;

// Try to find minimum port number
//
   if (GETSERVBYNAME(servname, servtype, &sent, sbuff, sizeof(sbuff), sp)
   && sp) portnum = sp->s_port;
      else portnum = 0;

// Do final port check and return
//
   return portnum;
}

/******************************************************************************/
/*                          F u l l H o s t N a m e                           */
/******************************************************************************/

char *XrdNetwork::FullHostName(char *host)
{
   char myname[260], *hp;
   struct sockaddr_in InetAddr;
  
// Identify ourselves if we don't have a passed hostname
//
   if (host) hp = host;
      else if (gethostname(myname, sizeof(myname))) hp = (char *)"";
              else hp = myname;

// Get our address
//
   if (getHostAddr(hp, InetAddr)) return strdup(hp);

// Convert it to a fully qualified host name
//
   return getHostName(InetAddr);
}

/******************************************************************************/
/*                           g e t H o s t A d d r                            */
/******************************************************************************/
  
int XrdNetwork::getHostAddr(char *hostname, struct sockaddr_in &InetAddr)
{
    u_long addr;
    struct hostent hent, *hp = 0;
    char hbuff[1024];
    int rc, byaddr = 0;

// Make sure we have something to lookup here
//
    InetAddr.sin_family = AF_INET;
    if (!hostname || !hostname[0]) 
       {InetAddr.sin_addr.s_addr = INADDR_ANY; return 0;}

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
   if (hp) memcpy((void *)&InetAddr.sin_addr, (const void *)hp->h_addr,
                 hp->h_length);
      else if (byaddr) memcpy((void *)&InetAddr.sin_addr, (const void *)&addr,
                 sizeof(addr));
              else {cerr <<"getHostAddr: " <<strerror(errno)
                         <<" obtaining address for " <<hostname <<endl;
                    return 1;
                   }
   return 0;
}

/******************************************************************************/
/*                           g e t H o s t N a m e                            */
/******************************************************************************/

char *XrdNetwork::getHostName(struct sockaddr_in &addr, char *ubuff, int ulen)
{
   struct hostent hent, *hp;
   char *hname, hbuff[1024];
   int rc;

// Convert it to a host name
//
   if (GETHOSTBYADDR((const char *)&addr.sin_addr, sizeof(addr.sin_addr),
                     AF_INET, &hent, hbuff, sizeof(hbuff), hp, &rc))
      if (!ubuff) hname = LowCase(strdup(hp->h_name));
         else {strlcpy(ubuff, hp->h_name, ulen);
               hname = LowCase(ubuff);
              }
      else if (!ubuff) hname = strdup(inet_ntoa(addr.sin_addr));
              else {strlcpy(ubuff, inet_ntoa(addr.sin_addr), ulen);
                    hname = ubuff;
                   }

// Return the name
//
   return hname;
}
 
/******************************************************************************/
/*                               H o s t 2 I P                                */
/******************************************************************************/
  
char *XrdNetwork::Host2IP(char *hname)
{
   struct sockaddr_in InetAddr;

// Convert hostname to an ascii ip address
//
   if (getHostAddr(hname, InetAddr))
      {cerr <<"Network: Unable to find IP address for " <<hname <<endl;
       return 0;
      }
   return inet_ntoa(InetAddr.sin_addr);
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                             d o _ A c c e p t                              */
/******************************************************************************/
  
XrdLink *XrdNetwork::do_Accept(int opts)
{
  int        newfd;
  char      *hname;
  XrdLink *newconn;
  struct sockaddr_in addr;
  socklen_t  addrlen = sizeof(addr);

// Accept a connection
//
   do {newfd = accept(iofd, (struct sockaddr *)&addr, (socklen_t *)&addrlen);}
      while(newfd < 0 && errno == EINTR);

   if (newfd < 0)
      {eDest->Emsg("Accept", errno, "performing accept."); return 0;}

// Authorize by ip address or full (slow) hostname format
//
   if (Police && !(hname = Police->Authorize(&addr)))
      {eDest->Emsg("Accept", -EACCES, "accepting connection from",
                     (hname = getHostName(addr)));
       free(hname);
       close(newfd);
       return 0;
      } else hname = getHostName(addr);

// Trim the host name if we have a domain name
//
   if (Domain) Trim(hname);

// Set socket options
//
   setOpts("accept", newfd, opts);
   TRACE(NET, "Accepted tcp connection from " <<hname);

// Allocate a new network object
//
   if (!(newconn = XrdLink::Alloc(newfd, &addr, hname)))
      {eDest->Emsg("Accept", "Unable to allocate new link for", hname);
       close(newfd);
      }

// Return result
//
   free(hname);
   return newconn;
}

/******************************************************************************/
/*                            d o _ R e c e i v e                             */
/******************************************************************************/

XrdLink *XrdNetwork::do_Receive(int opts)
{
  extern XrdBuffManager XrdBuffPool;
  int        dlen, newfd;
  char      *hname;
  XrdLink *newconn;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  XrdBuffer *bp = XrdBuffPool.Obtain(UDPMAXMSG);

// Receive the next message
//
   do {dlen = recvfrom(iofd, (void *)bp->buff, UDPMAXMSG-1, 0,
                      (struct sockaddr *)&addr, (socklen_t *)&addrlen);}
      while(dlen < 0 && errno == EINTR);

   if (dlen < 0)
      {eDest->Emsg("Receive", errno, "performing recvmsg."); 
      XrdBuffPool.Release(bp);
       return 0;
      }

// Authorize this connection
//
   if (addr.sin_addr.s_addr == 0x7f000001
   || (Police && !(hname = Police->Authorize(&addr))))
      {eDest->Emsg("Accept", -EACCES, "accepting connection from",
                     (hname = getHostName(addr)));
       free(hname);
       close(newfd);
       return 0;
      } else hname = getHostName(addr);

// Trim the host name if we have a domain name
//
   if (Domain) Trim(hname);

// Duplicate this file descriptor
//
   TRACE(NET, hname <<" Rcv " <<dlen <<" udp bytes");
   newfd = dup(iofd);
   fcntl(newfd, F_SETFD, FD_CLOEXEC);

// Allocate a new network object
//
   if (!(newconn = XrdLink::Alloc(newfd, &addr, hname, bp)))
      {eDest->Emsg("Accept", "Unable to allocate new link for", hname);
       XrdBuffPool.Release(bp);
       close(newfd);
      }

// Return result
//
   bp->buff[dlen] = '\0';
   free(hname);
   return newconn;
}

/******************************************************************************/
/*                               L o w C a s e                                */
/******************************************************************************/
  
char *XrdNetwork::LowCase(char *str)
{
   char *sp = str;

   while(*sp) {if (isupper((int)*sp)) *sp = (char)tolower((int)*sp); sp++;}

   return str;
}

/******************************************************************************/
/*                               s e t O p t s                                */
/******************************************************************************/

int XrdNetwork::setOpts(const char *who, int xfd, int opts)
{
   static int tcpprotid = getProtoID("tcp");
   int rc = 0;
   struct linger liopts;
   const int one = 1;
   const socklen_t szone = (socklen_t)sizeof(one);
   const socklen_t szlio = (socklen_t)sizeof(liopts);
   const socklen_t szwb  = (socklen_t)sizeof(Windowsz);

   if (opts == XRDNET_DEFAULTS) opts = netOpts;

   if (fcntl(xfd, F_SETFD, FD_CLOEXEC))
      eDest->Emsg(who, errno, "setting CLOEXEC");

   if (rc |= setsockopt(xfd,SOL_SOCKET,SO_REUSEADDR,(const void *)&one,szone))
      eDest->Emsg(who, errno, "setting REUSEADDR");

   liopts.l_onoff = 1; liopts.l_linger = 1;
   if (rc |= setsockopt(xfd,SOL_SOCKET,SO_LINGER,(const void *)&liopts,szlio))
      eDest->Emsg(who, errno, "setting LINGER");

   if ((opts & XRDNET_KEEPALIVE)
   && (rc |= setsockopt(xfd,SOL_SOCKET,SO_KEEPALIVE,(const void *)&one,szone)))
      eDest->Emsg(who, errno, "setting KEEPALIVE");

   if ((opts & XRDNET_NODELAY)
   && setsockopt(xfd, tcpprotid, TCP_NODELAY, (const void *)&one,szone))
      eDest->Emsg(who, errno, "setting NODELAY");

   if (Windowsz)
      {if (setsockopt(xfd,SOL_SOCKET,SO_SNDBUF,(const void *)&Windowsz,szwb))
          eDest->Emsg(who, errno, "setting SNDBUF");
       if (setsockopt(xfd,SOL_SOCKET,SO_RCVBUF,(const void *)&Windowsz,szwb))
          eDest->Emsg(who, errno, "setting RCVBUF");
      }
   return rc;
}
  
/******************************************************************************/
/*                            g e t P r o t o I D                             */
/******************************************************************************/

#define XRDNET_IPPROTO_TCP 6

int XrdNetwork::getProtoID(const char *pname)
{
    struct protoent *pp;

    if (!(pp = getprotobyname(pname))) return XRDNET_IPPROTO_TCP;
       else return pp->p_proto;
}

/******************************************************************************/
/*                              P e e r n a m e                               */
/******************************************************************************/
  
char *XrdNetwork::Peername(int snum, struct sockaddr_in *sap)
{
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);
      char *hname;

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
 
/******************************************************************************/
/*                                  T r i m                                   */
/******************************************************************************/
  
void XrdNetwork::Trim(char *hname)
{
  int k = strlen(hname);
  char *hnp;

  if (k > Domlen)
     {hnp = hname + (k - Domlen);
      if (!strcmp(Domain, hnp)) *hnp = '\0';
     }
}
