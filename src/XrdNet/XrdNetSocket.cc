/******************************************************************************/
/*                                                                            */
/*                       X r d N e t S o c k e t . c c                        */
/*                                                                            */
/* (C) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                          All Rights Reserved                               */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Deprtment of Energy               */
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

#ifndef WIN32
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <Winsock2.h>
#include "XrdSys/XrdWin32.hh"
#endif

#include "XrdNet/XrdNetConnect.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace XrdNetSocketCFG
{
       int ka_Idle = 0;
       int ka_Itvl = 0;
       int ka_Icnt = 0;
};

/******************************************************************************/
/*                         l o c a l   d e f i n e s                          */
/******************************************************************************/
  
#define XRDNETSOCKET_MAXBKLG 255
#define XRDNETSOCKET_LINGER    3

#define Err(p,a,b,c) (ErrCode = (eroute ? eroute->Emsg(#p, a, b, c) : ErrCode),-1)
#define ErrM(p,a,b,c) (ErrCode = (eroute ? eroute->Emsg(#p, a, b, c) : ErrCode),-1)

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdNetSocket::XrdNetSocket(XrdSysError *erobj, int SockFileDesc)
{
   ErrCode  = 0;
   eroute   = erobj;
   SockFD   = SockFileDesc;
}

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/
  
int XrdNetSocket::Accept(int timeout)
{
   int retc, ClientSock;

   ErrCode = 0;

   // Check if a timeout was requested
   //
   if (timeout >= 0)
      {struct pollfd sfd = {SockFD,
                            POLLIN|POLLRDNORM|POLLRDBAND|POLLPRI|POLLHUP, 0};
       do {retc = poll(&sfd, 1, timeout);}
                  while(retc < 0 && (errno == EAGAIN || errno == EINTR));
       if (!sfd.revents) return -1;
      }

   do {ClientSock = XrdSysFD_Accept(SockFD, (struct sockaddr *)0, 0);}
      while(ClientSock < 0 && errno == EINTR);

   if (ClientSock < 0 && eroute) eroute->Emsg("Accept",errno,"accept connection");

   // Return the socket number.
   //
   return ClientSock;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

void XrdNetSocket::Close()
{
     // Close any open file descriptor.
     //
     if (SockFD >= 0) {close(SockFD); SockFD=-1;} 

     // Reset values and return.
     //
     ErrCode=0;
}

/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/

XrdNetSocket *XrdNetSocket::Create(XrdSysError *Say, const char *path, 
                                   const char *fn, mode_t mode, int opts)
{
   XrdNetSocket *ASock;
   int pflags    = (opts & XRDNET_FIFO ? S_IFIFO : S_IFSOCK);
   int sflags    = (opts & XRDNET_UDPSOCKET) | XRDNET_SERVER;
   int rc = 0;
   mode_t myMode = (mode & (S_IRWXU | S_IRWXG));
   const char *eMsg = 0;
   char fnbuff[1024] = {0};

// Setup the path
//
   if (!socketPath(Say, fnbuff, path, fn, mode|pflags))
      return (XrdNetSocket *)0;

// Connect to the path
//
   ASock = new XrdNetSocket(Say);
#ifndef WIN32
   if (opts & XRDNET_FIFO)
      {if ((ASock->SockFD = mkfifo(fnbuff, mode)) < 0 && errno != EEXIST)
         {eMsg = "create fifo"; rc = errno;}
         else if ((ASock->SockFD = XrdSysFD_Open(fnbuff, O_RDWR, myMode)) < 0)
                 {eMsg = "open fifo"; rc = errno;}
                 else if (opts & XRDNET_NOCLOSEX) XrdSysFD_Yield(ASock->SockFD);
      } else if (ASock->Open(fnbuff, -1, sflags) < 0) 
                {eMsg = "create socket"; rc = ASock->LastError();}
#else
   if (ASock->Open(fnbuff, -1, sflags) < 0)
      {eMsg = "create socket"; rc = ASock->LastError();}
#endif

// Return the result
//
   if (eMsg) {Say->Emsg("Create", rc, eMsg, fnbuff);
              delete ASock; ASock = 0;
             }
   return ASock;
}

/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/
  
int XrdNetSocket::Detach()
{  int oldFD = SockFD;
   SockFD = -1;
   return oldFD;
}

/******************************************************************************/
/*                             g e t W i n d o w                              */
/******************************************************************************/
  
int XrdNetSocket::getWindow(int fd, int &Windowsz, XrdSysError *eDest)
{
   socklen_t szb = (socklen_t)sizeof(Windowsz);

   if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (Sokdata_t)&Windowsz, &szb))
      {if (eDest) eDest->Emsg("setWindow", errno, "set socket RCVBUF");
       return -1;
      }
    return 0;
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdNetSocket::Open(const char *inpath, int port, int flags, int windowsz)
{
   const char *epath, *eText, *action = "configure socket";
   char pbuff[128];
   int myEC, backlog, SockProt;
   int SockType = (flags & XRDNET_UDPSOCKET ? SOCK_DGRAM : SOCK_STREAM);
   const int one = 1;
   const SOCKLEN_t szone = (SOCKLEN_t)sizeof(one);

// Supply actual port number in error messages
//
   if (inpath) epath = inpath;
      else {sprintf(pbuff, "port %d", port);
            epath = pbuff;
           }

// Make sure this object is available for a new socket
//
   if (SockFD >= 0) return Err(Open, EBUSY, "create socket for", epath);

// Save the request flags, sometimes we need to check them from the local copy
//
   myEC = ErrCode = 0;

// Preset out address information
//
   if ((eText = SockInfo.Set(inpath,(port < 0 ? XrdNetAddr::PortInSpec:port))))
      {ErrCode = EHOSTUNREACH;
       if (eroute)
          {char buff[256];
           snprintf(buff, sizeof(buff), "'; %s", eText);
           eroute->Emsg("Open", "Unable to create socket for '", epath, buff);
          }
       return -1;
      }

// Allocate a socket descriptor of the right type
//
   SockProt = SockInfo.Protocol();
   if ((SockFD = XrdSysFD_Socket(SockProt, SockType, 0)) < 0)
      return Err(Open, errno, "create socket for", epath);

// Based on the type socket, set appropriate options. For server-side Unix
// sockets we must unlink the corresponding Unix path name or bind will fail.
// In some OS's, this creates a problem (e.g., Solaris) since the file inode is
// used to identify the socket and will likely change. This means that connects
// occuring before the bind will hang up to 3 minutes and client needs to retry.
// For non-Unix socketsr be prepared to timeout connects and try again.
//
   if (SockProt == PF_UNIX)
      {setOpts(SockFD, flags | XRDNET_UDPSOCKET, eroute);
       if (flags & XRDNET_SERVER) unlink((const char *)inpath);
      } else {
       setOpts(SockFD, flags, eroute);
       if (setsockopt(SockFD,SOL_SOCKET,SO_REUSEADDR, (Sokdata_t)&one, szone)
       &&  eroute) eroute->Emsg("Open",errno,"set socket REUSEADDR for",epath);
      }

// Set the window size or udp buffer size, as needed (ignore errors)
//
   if (windowsz) setWindow(SockFD, windowsz, eroute);

// Either do a bind or a connect.
//
   if (flags & XRDNET_SERVER)
      {action = "bind socket to";
       if (bind(SockFD, SockInfo.SockAddr(), SockInfo.SockSize())) myEC = errno;
          else if (SockType == SOCK_STREAM)
                  {action = "listen on stream";
                   if (!(backlog = flags & XRDNET_BKLG))
                      backlog = XRDNETSOCKET_MAXBKLG;
                   if (listen(SockFD, backlog)) myEC = errno;
                  }
       if (SockProt == PF_UNIX) chmod(inpath, S_IRWXU);
      } else {
       if (SockType == SOCK_STREAM)
          {int tmo = flags & XRDNET_TOUT;
           action = "connect socket to";
           if (tmo) myEC = XrdNetConnect::Connect(SockFD, SockInfo.SockAddr(),
                                                  SockInfo.SockSize(),tmo);
              else if (connect(SockFD,SockInfo.SockAddr(),SockInfo.SockSize()))
                      myEC = errno;
          }
      }

// Check for any errors and return (Close() sets SockFD to -1).
//
   if (myEC)
      {Close(); 
       ErrCode = myEC;
       if (!(flags & XRDNET_NOEMSG) && eroute)
          eroute->Emsg("Open", ErrCode, action, epath);
      }
   return SockFD;
}

/******************************************************************************/
/*                              P e e r n a m e                               */
/******************************************************************************/
  
const char *XrdNetSocket::Peername(const struct sockaddr **InetAddr,
                                                     int  *InetSize)
{
   const char *errtxt, *PeerName;

// Make sure  we have something to look at
//
   if (SockFD < 0) 
      {if (eroute) eroute->Emsg("Peername", 
                                "Unable to obtain peer name; socket not open");
       return (char *)0;
      }

// Get the host name on the other side of this socket
//
   if (!(PeerName = SockInfo.Name(0, &errtxt)))
      {if (eroute) 
          eroute->Emsg("Peername", "Unable to obtain peer name; ",errtxt);
       ErrCode = ESRCH;
      }

// Return possible address, length and the name
//
     if (InetAddr) *InetAddr = SockInfo.SockAddr();
     if (InetSize) *InetSize = SockInfo.SockSize();
     return PeerName;
}

/******************************************************************************/
/*                               s e t O p t s                                */
/******************************************************************************/
  
int XrdNetSocket::setOpts(int xfd, int opts, XrdSysError *eDest)
{
   int rc = 0;
   const int one = 1;
   const int szint = sizeof(int);
   const SOCKLEN_t szone = (SOCKLEN_t)sizeof(one);
   static int tcpprotid = XrdNetUtils::ProtoID("tcp");
   static struct linger liopts = {1, XRDNETSOCKET_LINGER};
   const SOCKLEN_t szlio = (SOCKLEN_t)sizeof(liopts);

   if (opts & XRDNET_NOCLOSEX && !XrdSysFD_Yield(xfd))
      {rc = 1;
       if (eDest) eDest->Emsg("setOpts", errno, "set fd close on exec");
      }

   if (opts & XRDNET_UDPSOCKET) return rc;

   if (!(opts & XRDNET_NOLINGER)
   &&  setsockopt(xfd,SOL_SOCKET,SO_LINGER,(Sokdata_t)&liopts,szlio))
      {rc = 1;
       if (eDest) eDest->Emsg("setOpts", errno, "set socket LINGER");
      }

   if (opts & XRDNET_KEEPALIVE)
      {if (setsockopt(xfd,SOL_SOCKET,SO_KEEPALIVE,(Sokdata_t)&one,szone))
          {rc = 1;
           if (eDest) eDest->Emsg("setOpts", errno, "set socket KEEPALIVE");
          }
#ifdef __linux__
           else if (opts & XRDNET_SERVER) // Following are inherited in Linux
      {if (XrdNetSocketCFG::ka_Idle
       &&  setsockopt(xfd,SOL_TCP,TCP_KEEPIDLE,&XrdNetSocketCFG::ka_Idle,szint))
          {rc = 1;
           if (eDest) eDest->Emsg("setOpts", errno, "set socket KEEPIDLE");
          }
       if (XrdNetSocketCFG::ka_Itvl
       &&  setsockopt(xfd,SOL_TCP,TCP_KEEPINTVL,&XrdNetSocketCFG::ka_Itvl,szint))
          {rc = 1;
           if (eDest) eDest->Emsg("setOpts", errno, "set socket KEEPINTVL");
          }
       if (XrdNetSocketCFG::ka_Icnt
       &&  setsockopt(xfd,SOL_TCP,TCP_KEEPCNT,  &XrdNetSocketCFG::ka_Icnt,szint))
          {rc = 1;
           if (eDest) eDest->Emsg("setOpts", errno, "set socket KEEPCNT");
          }
      }
#endif
      }

   if (!(opts & XRDNET_DELAY)
   &&  setsockopt(xfd, tcpprotid, TCP_NODELAY, (Sokdata_t)&one,szone))
      {rc = 1;
       if (eDest) eDest->Emsg("setOpts", errno, "set socket NODELAY");
      }

   return rc;
}

/******************************************************************************/
/*                             s e t W i n d o w                              */
/******************************************************************************/
  
int XrdNetSocket::setWindow(int xfd, int Windowsz, XrdSysError *eDest)
{
   int rc = 0;
   const SOCKLEN_t szwb  = (SOCKLEN_t)sizeof(Windowsz);

   if (setsockopt(xfd, SOL_SOCKET, SO_SNDBUF,
                       (Sokdata_t)&Windowsz, szwb))
      {rc = 1;
       if (eDest) eDest->Emsg("setWindow", errno, "set socket SNDBUF");
      }

   if (setsockopt(xfd, SOL_SOCKET, SO_RCVBUF,
                       (Sokdata_t)&Windowsz, szwb))
      {rc = 1;
       if (eDest) eDest->Emsg("setWindow", errno, "set socket RCVBUF");
      }
   return rc;
}

/******************************************************************************/
/*                            s o c k e t P a t h                             */
/******************************************************************************/

char *XrdNetSocket::socketPath(XrdSysError *Say, char *fnbuff,
                               const char *path, const char *fn, mode_t mode)
{
   const int srchOK = S_IXUSR | S_IXGRP;
   const int sfMask = (S_IFIFO | S_IFSOCK);
   int rc, i, fnlen = strlen(fnbuff);
   mode_t myMode = (mode & (S_IRWXU | S_IRWXG)) | srchOK;
   struct stat buf;
   char *sp = 0;

// Copy the const char path because makePath modifies it
//
   i = strlen(path);
   if (strlcpy(fnbuff, path, 1024) >= 1024 || (i + fnlen + 1) >= 1024)
      {Say->Emsg("createPath", "Socket path", path, "too long");
       return 0;
      }

// Check if we should separate the filename from the path
//
   if (!fn)
      {if (fnbuff[i-1] == '/') fnbuff[i-1] = '\0';
       if ((sp = rindex(fnbuff, '/'))) *sp = '\0';
      }
   
// Create the directory if it is not already there
//
   if ((rc = XrdOucUtils::makePath(fnbuff, myMode)))
      {Say->Emsg("createPath", errno, "create path", path);
       return 0;
      }

// Construct full filename
//
   if (sp) *sp = '/';
      else {if (path[i-1] != '/') fnbuff[i++] = '/';
            if (fn) strcpy(fnbuff+i, fn);
           }

// Check is we have already created it and whether we can access
//
   if (!stat(fnbuff,&buf))
      {if ((buf.st_mode & S_IFMT) != (mode & sfMask))
          {Say->Emsg("createPath","Path",fnbuff,
                     (mode & S_IFSOCK) ? "exists but is not a socket"
                                       : "exists but is not a pipe");
           return 0;
          }
       if (access(fnbuff, W_OK))
          {Say->Emsg("cratePath", errno, "access path", fnbuff);
           return 0;
          }
      } else chmod(fnbuff, mode); // This may fail on some platforms

// All set now
//
   return fnbuff;
}
