/******************************************************************************/
/*                                                                            */
/*                             X r d N e t . c c                              */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif

#include "XrdNet/XrdNet.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetBuffer.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetPeer.hh"
#include "XrdNet/XrdNetSecurity.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdNet/XrdNetUtils.hh"

#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
#define XRDNET_UDPBUFFSZ 32768
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNet::XrdNet(XrdSysError *erp, XrdNetSecurity *secp)
{
   iofd   = PortType = -1;
   eDest  = erp;
   Police = secp;
   Domlen = Portnum = Windowsz = netOpts = 0;
   Domain = 0;
   BuffQ  = 0;
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdNet::~XrdNet()
{
   unBind();
   if (Domain) free(Domain);
}

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/

int XrdNet::Accept(XrdNetAddr &myAddr, int opts, int timeout)
{
   int retc;

// Make sure we are bound to a port
//
   opts |= netOpts;
   if (iofd < 0) 
      {if (!(opts & XRDNET_NOEMSG))
          eDest->Emsg("Accept", "Network not bound to a port.");
       return 0;
      }

// This interface only accepts TCP connections
//
   if (PortType != SOCK_STREAM)
      {if (!(opts & XRDNET_NOEMSG))
          eDest->Emsg("Accept", "UDP network not supported for NetAddr call.");
       return 0;
      }

// Setup up the poll structure to wait for new connections
//
  do {if (timeout >= 0)
         {struct pollfd sfd = {iofd,
                               POLLIN|POLLRDNORM|POLLRDBAND|POLLPRI|POLLHUP,0};
          do {retc = poll(&sfd, 1, timeout*1000);}
             while(retc < 0 && (errno == EAGAIN || errno == EINTR));
          if (!retc)
             {if (!(opts & XRDNET_NOEMSG))
                 eDest->Emsg("Accept", "Accept timed out.");
              return 0;
             }
         }
     } while(!do_Accept_TCP(myAddr, opts));

   return 1;
}

/******************************************************************************/

int XrdNet::Accept(XrdNetPeer &myPeer, int opts, int timeout)
{
   int retc;

// Make sure we are bound to a port
//
   opts |= netOpts;
   if (iofd < 0) 
      {if (!(opts & XRDNET_NOEMSG))
          eDest->Emsg("Accept", "Network not bound to a port.");
       return 0;
      }

// Setup up the poll structure to wait for new connections
//
  do {if (timeout >= 0)
         {struct pollfd sfd = {iofd,
                               POLLIN|POLLRDNORM|POLLRDBAND|POLLPRI|POLLHUP,0};
          do {retc = poll(&sfd, 1, timeout*1000);}
             while(retc < 0 && (errno == EAGAIN || errno == EINTR));
          if (!retc)
             {if (!(opts & XRDNET_NOEMSG))
                 eDest->Emsg("Accept", "Accept timed out.");
              return 0;
             }
         }
     } while(!(PortType == SOCK_STREAM ? do_Accept_TCP(myPeer, opts)
                                       : do_Accept_UDP(myPeer, opts)));

// Accept completed, trim the host name if a domain has been specified,
//
   if (Domain && !(opts & XRDNET_NODNTRIM)) Trim(myPeer.InetName);
   return 1;
}
  
/******************************************************************************/
/*                                  B i n d                                   */
/******************************************************************************/
  
int XrdNet::Bind(int bindport, const char *contype)
{
    XrdNetSocket mySocket(eDest);
    int opts = XRDNET_SERVER | netOpts;
    int buffsz = Windowsz;

// Close any open socket here
//
   unBind();

// Get correct option settings
//
   if (*contype != 'u') PortType = SOCK_STREAM;
      else {PortType = SOCK_DGRAM;
            opts |= XRDNET_UDPSOCKET;
            if (!buffsz) buffsz = XRDNET_UDPBUFFSZ;
           }

// Try to open and bind to this port
//
   if (mySocket.Open(0, bindport, opts, buffsz) < 0)
      return -mySocket.LastError();

// Success, get the socket number and return
//
   iofd = mySocket.Detach();

// Obtain port number of generic port being used
//
   Portnum = (bindport ? bindport : XrdNetUtils::Port(iofd));

// For udp sockets, we must allocate a buffer queue object
//
   if (PortType == SOCK_DGRAM)
      {BuffSize = buffsz;
       BuffQ = new XrdNetBufferQ(buffsz);
      }
   return 0;
}
  
/******************************************************************************/

int XrdNet::Bind(char *path, const char *contype)
{
    XrdNetSocket mySocket(eDest);
    int opts = XRDNET_SERVER | netOpts;
    int buffsz = Windowsz;

// Make sure this is a path and not a host name
//
   if (*path != '/')
      {eDest->Emsg("Bind", "Invalid bind path -", path);
       return -EINVAL;
      }

// Close any open socket here
//
   unBind();

// Get correct option settings
//
   if (*contype != 'd') PortType = SOCK_STREAM;
      else {PortType = SOCK_DGRAM;
            opts |= XRDNET_UDPSOCKET;
            if (!buffsz) buffsz = XRDNET_UDPBUFFSZ;
           }

// Try to open and bind to this path
//
   if (mySocket.Open(path, -1, opts, buffsz) < 0) return -mySocket.LastError();

// Success, get the socket number and return
//
   iofd = mySocket.Detach();

// For udp sockets, we must allocate a buffer queue object
//
   if (PortType == SOCK_DGRAM)
      {BuffSize = buffsz;
       BuffQ = new XrdNetBufferQ(buffsz);
      }
   return 0;
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/

int XrdNet::Connect(XrdNetAddr &myAddr, const char *host,
                    int port, int opts, int tmo)
{
   XrdNetSocket mySocket(opts & XRDNET_NOEMSG ? 0 : eDest);

// Determine appropriate options but turn off UDP sockets
//
   opts = (opts | netOpts) & ~XRDNET_UDPSOCKET;
   if (tmo > 0) opts = (opts & ~XRDNET_TOUT) | (tmo > 255 ? 255 : tmo);

// Now perform the connect and return the results if successful
//
   if (mySocket.Open(host, port, opts, Windowsz) < 0) return 0;
   myAddr.Set(mySocket.Detach());
   if (!(opts & XRDNET_NORLKUP)) myAddr.Name();
   return 1;
}

/******************************************************************************/

int XrdNet::Connect(XrdNetPeer &myPeer,
                    const char *host, int port, int opts, int tmo)
{
   XrdNetSocket mySocket(opts & XRDNET_NOEMSG ? 0 : eDest);
   const struct sockaddr *sap;
   int buffsz = Windowsz;

// Determine appropriate options
//
   opts |= netOpts;
   if ((opts & XRDNET_UDPSOCKET) && !buffsz) buffsz = XRDNET_UDPBUFFSZ;
   if (tmo > 0) opts = (opts & ~XRDNET_TOUT) | (tmo > 255 ? 255 : tmo);

// Now perform the connect and return the peer structure if successful
//
   if (mySocket.Open(host, port, opts, buffsz) < 0) return 0;
   if (myPeer.InetName) free(myPeer.InetName);
   if ((opts & XRDNET_UDPSOCKET) || !host) 
      {myPeer.InetName = strdup("n/a");
       memset((void *)&myPeer.Inet, 0, sizeof(myPeer.Inet));
      } else {
       const char *pn = mySocket.Peername(&sap);
       if (pn) {memcpy((void *)&myPeer.Inet, sap, sizeof(myPeer.Inet));
                myPeer.InetName = strdup(pn);
                if (Domain && !(opts & XRDNET_NODNTRIM)) Trim(myPeer.InetName);
               } else {
                memset((void *)&myPeer.Inet, 0, sizeof(myPeer.Inet));
                myPeer.InetName = strdup("unknown");
               }
      }
   myPeer.fd = mySocket.Detach();
   return 1;
}

/******************************************************************************/
/*                                 R e l a y                                  */
/******************************************************************************/
  
int XrdNet::Relay(XrdNetPeer &Peer, const char *dest, int opts)
{
   return Connect(Peer, dest, -1, opts | XRDNET_UDPSOCKET);
}

/******************************************************************************/
  
int XrdNet::Relay(const char *dest)
{
   XrdNetPeer myPeer;

   return (Connect(myPeer, dest, -1, XRDNET_UDPSOCKET | XRDNET_SENDONLY)
          ? myPeer.fd : -1);
}
  
/******************************************************************************/
/*                                S e c u r e                                 */
/******************************************************************************/
  
void XrdNet::Secure(XrdNetSecurity *secp)
{

// If we don't have a Police object then use the one supplied. Otherwise
// merge the supplied object into the existing object.
//
   if (Police) Police->Merge(secp);
      else     Police = secp;
}

/******************************************************************************/
/*                                  T r i m                                   */
/******************************************************************************/
  
void XrdNet::Trim(char *hname)
{
  int k = strlen(hname);
  char *hnp;

  if (Domlen && k > Domlen)
     {hnp = hname + (k - Domlen);
      if (!strcmp(Domain, hnp)) *hnp = '\0';
     }
}

/******************************************************************************/
/*                                u n B i n d                                 */
/******************************************************************************/
  
void XrdNet::unBind()
{
   if (iofd >= 0) {close(iofd); iofd=-1; Portnum=0;}
   if (BuffQ) {delete BuffQ; BuffQ = 0;}
}

/******************************************************************************/
/*                                 W S i z e                                  */
/******************************************************************************/
  
int XrdNet::WSize()
{
  int wsz;

  if (iofd >= 0 && !XrdNetSocket::getWindow(iofd, wsz, eDest)) return wsz;
  return 0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                         d o _ A c c e p t _ T C P                          */
/******************************************************************************/
  
int XrdNet::do_Accept_TCP(XrdNetAddr &hAddr, int opts)
{
  static int noAcpt = 0;
  XrdNetSockAddr IP;
  SOCKLEN_t addrlen = sizeof(IP);
  int newfd;

// Remove UDP option if present
//
   opts &= ~XRDNET_UDPSOCKET;

// Accept a connection
//
   do {newfd = XrdSysFD_Accept(iofd, &IP.Addr, &addrlen);}
      while(newfd < 0 && errno == EINTR);

   if (newfd < 0)
      {if (!(opts & XRDNET_NOEMSG) && (errno != EMFILE || !(0x1ff & noAcpt++)))
          eDest->Emsg("Accept", errno, "perform accept");
       return 0;
      }

// Initialize the address of the new connection
//
   hAddr.Set(&IP.Addr, newfd);

// Remove TCP_NODELAY option for unix domain sockets to avoid error message
//
   if (hAddr.isIPType(XrdNetAddrInfo::IPuX)) opts |= XRDNET_DELAY;

// Set all required fd options are set
//
   XrdNetSocket::setOpts(newfd, opts, (opts & XRDNET_NOEMSG ? 0 : eDest));

// Authorize by ip address or full (slow) hostname format
//
   if (Police)
      {if (!Police->Authorize(hAddr))
          {char ipbuff[512];
           hAddr.Format(ipbuff, sizeof(ipbuff),
                        (opts & XRDNET_NORLKUP ? XrdNetAddr::fmtAuto
                                               : XrdNetAddr::fmtName),
                                                 XrdNetAddrInfo::noPort);
           eDest->Emsg("Accept",EACCES,"accept TCP connection from",ipbuff);
           close(newfd);
           return 0;
          }
      }

// Force resolution of the addres unless reverse lookup not desired
//
   if (!(opts & XRDNET_NORLKUP)) hAddr.Name();

// All done
//
   return 1;
}

/******************************************************************************/
  
int XrdNet::do_Accept_TCP(XrdNetPeer &myPeer, int opts)
{
  XrdNetAddr tAddr;
  char hBuff[512];

// Use the new interface to actually do the accept
//
   if (!do_Accept_TCP(tAddr, opts)) return 0;

// Now transfor it back to the old-style interface
//
   memcpy(&myPeer.Inet, tAddr.SockAddr(), tAddr.SockSize());
   myPeer.fd = tAddr.SockFD();
   tAddr.Format(hBuff, sizeof(hBuff), XrdNetAddr::fmtAuto, false);
   if (myPeer.InetName) free(myPeer.InetName);
   myPeer.InetName = strdup(hBuff);
   return 1;
}

/******************************************************************************/
/*                         d o _ A c c e p t _ U D P                          */
/******************************************************************************/
  
int XrdNet::do_Accept_UDP(XrdNetPeer &myPeer, int opts)
{
  char            hBuff[512];
  int             dlen;
  XrdNetSockAddr  IP;
  SOCKLEN_t       addrlen = sizeof(IP);
  XrdNetBuffer   *bp;
  XrdNetAddr      uAddr;

// For UDP connections, get a buffer for the message. To be thread-safe, we
// must actually receive the message to maintain the host-datagram pairing.
//
   if (!(bp = BuffQ->Alloc()))
      {eDest->Emsg("Accept", ENOMEM, "accept UDP message");
       return 0;
      }

// Read the message and get the host address
//
   do {dlen = recvfrom(iofd,(Sokdata_t)bp->data,BuffSize-1,0,&IP.Addr,&addrlen);
      } while(dlen < 0 && errno == EINTR);

   if (dlen < 0)
      {eDest->Emsg("Receive", errno, "perform UDP recvfrom()");
       BuffQ->Recycle(bp);
       return 0;
      } else bp->data[dlen] = '\0';

// Use the new-style address handling for address functions
//
   uAddr.Set(&IP.Addr);

// Authorize this connection. We don't accept messages that set the
// loopback address since this can be trivially spoofed in UDP packets.
//
   if (uAddr.isLoopback() || (Police && !Police->Authorize(uAddr)))
      {eDest->Emsg("Accept", -EACCES, "accept connection from",
                             uAddr.Name("*unknown*"));
       BuffQ->Recycle(bp);
       return 0;
      } else uAddr.Format(hBuff, sizeof(hBuff),
                         (opts & XRDNET_NORLKUP ? XrdNetAddr::fmtAuto
                                                : XrdNetAddr::fmtName),
                                                  XrdNetAddrInfo::noPort);
// Get a new FD if so requested. We use our base FD for outgoing messages.
//
   if (opts & XRDNET_NEWFD)
      {myPeer.fd = XrdSysFD_Dup(iofd);
       if (opts & XRDNET_NOCLOSEX) XrdSysFD_Yield(myPeer.fd);
      } else myPeer.fd = iofd;

// Fill in the peer structure.
//
   memcpy(&myPeer.Inet, &IP, sizeof(myPeer.Inet));
   if (myPeer.InetName) free(myPeer.InetName);
   myPeer.InetName = strdup(hBuff);
   if (myPeer.InetBuff) myPeer.InetBuff->Recycle();
   myPeer.InetBuff = bp;
   return 1;
}
