/******************************************************************************/
/*                                                                            */
/*                       X r d O u c S o c k e t . c c                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                          All Rights Reserved                               */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Deprtment of Energy               */
/******************************************************************************/

//         $Id$

const char *XrdOucSocketCVSID = "$Id$";

#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucSocket.hh"

/******************************************************************************/
/*                         l o c a l   d e f i n e s                          */
/******************************************************************************/
  
#define XrdOucSocket_MaxBKLG  5

#define Erq(p,a,b) Err(p,a,b, (char *)0)

#define Err(p,a,b,c) (ErrCode = (eroute ? eroute->Emsg(#p, a, b, c) : ErrCode),-1)

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOucSocket::XrdOucSocket(XrdOucError *erobj, int SockFileDesc)
{
   ReqFlags = 0;
   ErrCode  = 0;
   PeerName = 0;
   SockFD   = SockFileDesc;
   eroute   = erobj;
}

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/
  
int XrdOucSocket::Accept(int timeout)
{
   int retc, ClientSock;
   struct pollfd sfd[1];

   ErrCode = 0;

   // Check if a timeout was requested
   //
   if (timeout >= 0)
      {sfd[0].fd     = SockFD;
       sfd[0].events = POLLIN | POLLRDNORM || POLLRDBAND || POLLPRI || POLLHUP;
       sfd[0].revents = 0;
       do {retc = poll(sfd, 1, timeout);}
                  while(retc < 0 && (errno = EAGAIN || errno == EINTR));
       if (!sfd[0].revents) return -1;
      }

   do {ClientSock = accept(SockFD, (struct sockaddr *)0, 0);}
      while(ClientSock < 0 && errno == EINTR);

   if (ClientSock < 0) {Erq(Accept, errno, "accept rejected");}

   // Return the socket number.
   //
   return ClientSock;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

void XrdOucSocket::Close()
{
     // Close any open file descriptor.
     //
     if (SockFD >= 0) {close(SockFD); SockFD=-1;} 

     // Free any previous peername pointer
     //
     if (PeerName) {free(PeerName); PeerName = 0;}

     // Reset values and return.
     //
     ErrCode=0;
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdOucSocket::Open(char *path, int port, int flags) {
    struct sockaddr_in InetAddr;
    struct sockaddr_un UnixAddr;
    struct sockaddr *SockAddr;
    char *action, pbuff[1024];
    int myEC, SockSize, backlog;
    int SockType = (flags & XrdOucSOCKET_UDP ? SOCK_DGRAM : SOCK_STREAM);

// Make sure this object is available for a new socket
//
   if (SockFD >= 0) return Err(Open, EADDRINUSE, "create socket for", path);

// Save the request flags, sometimes we need to check them from the local copy
//
   ReqFlags = flags;
   myEC = ErrCode = 0;

// Allocate a socket descriptor and bind connection address, if requested.
//
   if (port < 0 && path && path[0] == '/')
      {if (strlen(path) >= sizeof(UnixAddr.sun_path))
          return Err(Open, ENAMETOOLONG, "create unix socket ", path);
       if ((SockFD = socket(PF_UNIX, SockType, 0)) < 0)
          return Err(Open, errno, "create unix socket ", path);
       if (flags & XrdOucSOCKET_SERVER) unlink((const char *)path);
       UnixAddr.sun_family = AF_UNIX;
       strcpy(UnixAddr.sun_path, path);
       SockAddr = (struct sockaddr *)&UnixAddr;
       SockSize = sizeof(UnixAddr);
      } else {
       if ((SockFD = socket(PF_INET, SockType, 0)) < 0)
          return Err(Open, errno, "create inet socket to", path);
          if (port < 0)
             if ((port = GetHostAndPort(path,pbuff,sizeof(pbuff))) < 0)
                {myEC = ErrCode; Close(); ErrCode = myEC; return -1;}
                 else path = pbuff;
       if (GetHostAddr(path, InetAddr) < 0) 
          {myEC = ErrCode; Close(); ErrCode = myEC; return -1;}
       InetAddr.sin_port = port;
       SockAddr = (struct sockaddr *)&InetAddr;
       SockSize = sizeof(InetAddr);
      }

// Either do a connect or a bind.
//
   if (flags & XrdOucSOCKET_SERVER)
      {action = (char *)"bind socket";
       if ( bind(SockFD, SockAddr, SockSize) ) myEC = errno;
          else {if (port < 0) chmod(path, S_IRWXU);
                if (SockType == SOCK_STREAM)
                   {if (!(backlog = flags & XrdOucSOCKET_BKLG))
                       backlog = XrdOucSocket_MaxBKLG;
                    action = (char *)"listen on stream";
                    if (listen(SockFD, backlog)) myEC = errno;
                   }
               }
      } else {
       action = (char *)"connect socket";
       if (connect(SockFD, SockAddr, SockSize)) myEC = errno;
      }

// Check for any errors and return.
//
   if (myEC)
      {Close(); 
       ErrCode = myEC;
       return Err(Open, ErrCode, action, path);
      }
   return SockFD;
}

/******************************************************************************/
/*                              P e e r n a m e                               */
/******************************************************************************/
  
char *XrdOucSocket::Peername(int snum)
{
      struct sockaddr_in addr;
      unsigned int addrlen = sizeof(addr);
      int retc;
      char *hname, abuff[1024];
      struct hostent hostloc, *hp;

     // Make sure  we have something to look at
     //
     if (snum < 0) snum = SockFD;
     if (snum < 0) {Erq(Perrname, ENOTCONN, "socket not open");
                    return (char *)0;
                   }

     // Free any previous peername pointer
     //
     if (PeerName) {free(PeerName); PeerName = 0;}

     // Get the address on the other side of this socket
     //
     if (getpeername(snum, (struct sockaddr *)&addr, &addrlen) < 0)
        {Erq(Peername, errno, "unable to obtain peer name");
         return (char *)0;
        }

     // Convert it to a host name
     //
     if (GETHOSTBYADDR((char *)&addr.sin_addr, sizeof(addr.sin_addr), AF_INET,
                               &hostloc, abuff, sizeof(abuff), hp, &retc))
             hname = inet_ntoa(addr.sin_addr);
        else hname = hostloc.h_name;

     // Duplicate the name and set a pointe to it for later freeing
     //
        PeerName = strdup(hname);
        return PeerName;
}

/******************************************************************************/
/*                        G e t H o s t A n d P o r t                         */
/******************************************************************************/
  
int XrdOucSocket::GetHostAndPort(char *path, char *buff, int bsz)
{   int plen, pnum;

    // Extract the host name from the path
    //
    if ((plen = strlen(path))) do {plen--;} while(plen && path[plen] != ':');
    if (!plen) return Err(GetHostAndPort, EINVAL, 
                          "find socket host in", path);
    if (plen >= bsz) return Err(GetHostAndPort, ENAMETOOLONG, 
                          "use hostname in", path);
    if (path != buff) strncpy(buff, path, plen);
    buff[plen++] = '\0';

    // Extract the port number from the path
    //
    errno = 0;
    pnum = (int)strtol((const char *)(path+plen), (char **)0, 10);
    if (!pnum && errno) return Err(GetHostAndPort, errno, 
                                   "find port number in", &path[plen]);
    return pnum;
}

/******************************************************************************/
/*                           G e t H o s t A d d r                            */
/******************************************************************************/
  
int XrdOucSocket::GetHostAddr(char *hostname, struct sockaddr_in &InetAddr)
{   char abuff[1024];
    int retc, ok= 1;
    u_long addr;
    struct hostent hostloc, *hp;

    InetAddr.sin_family = AF_INET;
    if (!hostname || !hostname[0]) InetAddr.sin_addr.s_addr = INADDR_ANY;
       else {if ( (hostname[0] > '9' || hostname[0] < '0')
               && !GETHOSTBYNAME(hostname,&hostloc,abuff,sizeof(abuff),hp,&retc))
                ok = 0;
               else {if ((int)(addr = inet_addr(hostname)) == -1)
                        {ok = 0; retc = EINVAL;}
                     if (GETHOSTBYADDR((char *)&addr, sizeof(addr), AF_INET,
                                &hostloc, abuff, sizeof(abuff), hp, &retc)) ok = 0;
                    }
              if (!ok) return Err(GetHostAddr, retc, 
                                  "obtain address for", hostname);
                 else memcpy((char *)&InetAddr.sin_addr, (char *)hostloc.h_addr,
                             hostloc.h_length);
            }
    return 0;
}
