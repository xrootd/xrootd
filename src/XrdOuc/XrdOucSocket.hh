#ifndef __OOUC_SOCKET__
#define __OOUC_SOCKET__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c S o c k e t . h h                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/
 
//         $Id$
  
#include <sys/socket.h>

/******************************************************************************/
/*                     C o n s t r u c t o r   F l a g s                      */
/******************************************************************************/

// Bind socket to the specified file path or host_path and port ans issue a
// a listen request to prime socket for incomming messages. Otherwise, a
// connection is made to the specified path:port (i.e., client actions).
//
#define XrdOucSOCKET_SERVER 0x1000
#define XrdOucSOCKET_UDP    0x0100

// Maximum backlog for incomming connections. The backlog value goes in low
// order byte and is used only when XrdOucSOKSERVER is specified.
//
#define XrdOucSOCKET_BKLG   0x00FF

class XrdOucError;

class XrdOucSocket
{
public:

// When creating a socket object, you may pass an optional error routing object.
// If you do so, error messages will be writen via the error object. Otherwise,
// errors will be returned quietly. Addionally, you can attach a file descriptor
// to the socket object. This is useful when creating an object for accepted
// connections, e.g., ClientSock = new XrdOucSocket("", ServSock.Accept()).
//
            XrdOucSocket(XrdOucError *erobj=0, int SockFileDesc=-1);

           ~XrdOucSocket() {Close();}

// Open a socket. Returns socket number upon success otherwise a -1. Use
// LastError() to find out the reason for failure. Only one socket at a time
// may be created. Use Close() to close the socket before creating a new one.

//         |<-------- C l i e n t -------->|  |<-------- S e r v e r -------->|
//         Unix Socket       Internet Socket  Unix Socket       Internet Socket
// path  = Filname           hostname.        filename          0 or ""
// port  = -1                port number      -1                port number
// flags = ~SOCKET_SERVER    ~SOCKET_SERVER   SOCKET_SERVER     SOCKET_SERVER

// If the client path does not start with a slash and the port number is -1
// then hostname must be of the form hostname:port.
//
       int  Open(char *path, int port=-1, int flags=0);

// Issue accept on the created socket. Upon success return socket FD, upon
// failure return -1. Use LastError() to obtain reason for failure. Note that
// Accept() is valid only for Server Sockets. An optional millisecond
// timeout may be specified. If no new connection is attempted within the
// millisecond time limit, a return is made with -1 and an error code of 0.
//
       int  Accept(int ms=-1);

// Obtain the name of the host on the other side of a socket. Upon success,
// a pointer to the hostname is returned. Otherwise null is returned. An
// optional socket number may be provided to obtain the hostname for it.
//
       char *Peername(int snum=-1);

// Close a socket.
//
       void Close();

// Detach the socket filedescriptor without closing it. Usefule when you
// will be attaching the descriptor to a stream. Returns the descriptor so
// you can do something like Stream.Attach(Socket.Detach()).
//
       int  Detach() {int oldFD = SockFD; SockFD = -1; return oldFD;}

// Return last errno.
//
inline int  LastError() {return ErrCode;}

// Return socket file descriptor number (useful when attaching to a stream).
//
inline int  SockNum() {return SockFD;}

/******************************************************************************/
  
private:
int         SockFD;
int         ErrCode;
int         ReqFlags;
char       *PeerName;
XrdOucError *eroute;

       int GetHostAndPort(char *path, char *buff, int bsz);
       int GetHostAddr(char *hostname, struct sockaddr_in &InetAddr);
};
#endif
