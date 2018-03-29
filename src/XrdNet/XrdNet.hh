#ifndef __XRDNET_H__
#define __XRDNET_H__
/******************************************************************************/
/*                                                                            */
/*                             X r d N e t . h h                              */
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

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <Winsock2.h>
#endif

#include "XrdNet/XrdNetOpts.hh"

class XrdNetAddr;
class XrdNetBufferQ;
class XrdNetPeer;
class XrdNetSecurity;
class XrdSysError;

class XrdNet
{
public:

//------------------------------------------------------------------------------
//! Accept incomming TCP connection. This is the preferred method for TCP.
//!
//! @param  myAddr   the address object to contain connection information.
//! @param  opts     processing options:
//!                  XRDNET_DELAY     - do not set nodelay on socket.
//!                  XRDNET_KEEPALIVE - use TCP keep alive on socket.
//!                  XRDNET_NOCLOSEX  - do not allow socket to close on exec()
//!                  XRDNET_NOEMSG    - supress printing error messages
//!                  XRDNET_NOLINGER  - Do not linger when closing socket.
//! @param  timeout  maximum seconds to wait for a conection. When negative,
//!                  the default, no time limit applies.
//! @return !0       Successful connection occurred, myAddr holds information.
//! @return =0       Failure, a timeout or permanent error occurred.
//------------------------------------------------------------------------------

int             Accept(XrdNetAddr &myAddr,
                       int opts=0,
                       int timeout=-1);

//------------------------------------------------------------------------------
//! Accept incomming TCP or UDP connection. This method should only be used for
//! UDP-style networks. The previous method is preferred for TCP-style networks.
//!
//! @param  myPeer   the peer object to contain connection information.
//! @param  opts     processing options:
//!                  XRDNET_DELAY     - TCP: do not set nodelay on socket.
//!                  XRDNET_KEEPALIVE - TCP: use TCP keep alive on socket.
//!                  XRDNET_NEWFD     - UDP: obtain new file descriptor
//!                  XRDNET_NOCLOSEX  - ALL: keep socket across exec() calls
//!                  XRDNET_NODNTRIM  - ALL: don't trim domain name in myPeer
//!                  XRDNET_NOEMSG    - ALL: supress printing error messages
//!                  XRDNET_NORLKUP   - ALL: avoid doing reverse DNS look-up
//! @param  timeout  maximum seconds to wait for a conection. When negative,
//!                  the default, no time limit applies.
//! @return !0       Successful connection occurred, myPeer holds information.
//! @return =0       Failure, a timeout or permanent error occurred.
//------------------------------------------------------------------------------

int             Accept(XrdNetPeer &myPeer,
                       int opts=0,
                       int timeout=-1);

//------------------------------------------------------------------------------
//! Bind a network object to a TCP or UDP port number.
//!
//! @param  port     the port number to bind to. Use 0 for arbitrary port.
//! @param  contype  Either "tcp" for TCP networks or "udp" for UDP networks.
//!
//! @return  0       Successfully bound the port.
//! @return !0       Failure, return value is -errno describing the error.
//------------------------------------------------------------------------------

int             Bind(      int   port,             // Port number
                     const char *contype="tcp"     // "tcp" or "udp"
                    );

//------------------------------------------------------------------------------
//! Bind a network object to a Unix named socket.
//!
//! @param  path     the file system path to a named socket to bind with.
//! @param  contype  Either "stream" to use TCP-tyle streaming protocol or
//!                  "datagram" to use UDP-style messaging.
//!
//! @return  0       Successfully bound the port.
//! @return !0       Failure, return value is -errno describing the error.
//------------------------------------------------------------------------------

int             Bind(      char *path,             // Unix path < |109|
                     const char *contype="stream"  // stream | datagram
                    );

//------------------------------------------------------------------------------
//! Create a TCP socket and connect it to the given host and port. This is the
//! preferred method for making TCP based connections.
//!
//! @param  myAddr   address object where connection information is returned.
//! @param  dest     destination hostname or IP address.
//! @param  port     the port number to connect to. If < 0 then the dest param
//!                  must contain the port number preceeded by a colon.
//! @param  opts     processing options:
//!                  XRDNET_NOCLOSEX  - do not allow socket to close on exec()
//!                  XRDNET_NOEMSG    - supress printing error messages
//!                  XRDNET_NORLKUP   - avoid doing reverse DNS look-up
//! @param  timeout  the maximum number of seconds to wait for the connection to
//!                  complete. A negative value waits forever. Values greater
//!                  than 255 seconds are set to 255.
//!
//! @return true     Connection completed, myAddr holds connection information.
//! @return false    Connection failed.
//------------------------------------------------------------------------------

int             Connect(XrdNetAddr &myAddr,
                        const char *dest,  // Destination host or ip address
                        int   port=-1,     // Port number
                        int   opts=0,      // Options
                        int   timeout=-1   // Second timeout
                       );

//------------------------------------------------------------------------------
//! Create a TCP or UDP socket and connect it to the given host and port. The
//! previous method is preferred for creating TCP sockets.
//!
//! @param  myPeer   peer object where connection information is returned.
//! @param  dest     destination hostname or IP address.
//! @param  port     the port number to connect to. If < 0 then the dest param
//!                  must contain the port number preceeded by a colon.
//! @param  opts     processing options:
//!                  XRDNET_NOCLOSEX  - do not allow socket to close on exec()
//!                  XRDNET_NODNTRIM  - do not trim domain name in myPeer.
//!                  XRDNET_NOEMSG    - supress printing error messages
//!                  XRDNET_NORLKUP   - avoid doing reverse DNS look-up
//!                  XRDNET_UDPSOCKET - create a UDP socket (o/w use TCP).
//! @param  timeout  the maximum number of seconds to wait for the connection to
//!                  complete. A negative value waits forever. Values greater
//!                  than 255 seconds are set to 255.
//!
//! @return true     Connection completed, myPeer holds connection information.
//! @return false    Connection failed.
//------------------------------------------------------------------------------

int             Connect(XrdNetPeer &myPeer,
                        const char *dest,  // Destination host or ip address
                        int   port=-1,     // Port number
                        int   opts=0,      // Options
                        int   timeout=-1   // Second timeout
                       );

//------------------------------------------------------------------------------
//! Get the port number, if any, bound to this network.
//!
//! @return  >0      The bound port number.
//! @return <=0      The network is not bound to a port.
//------------------------------------------------------------------------------

int             Port() {return Portnum;}

// Relay() creates a UDP socket and optionally decomposes a destination
//         of the form host:port. Upon success it fills in the Peer object
//         and return true (1). Upon failure, it returns false (0).
//
int             Relay(XrdNetPeer &Peer,   // Peer object to be initialized
                      const char *dest,   // Optional destination
                      int         opts=0  // Optional options as above
                     );

int             Relay(const char *dest);  // Optional destination

//------------------------------------------------------------------------------
//! Add a NetSecurity object to the existing accept() security constraints.
//!
//! @param  secp     Pointer to the network security object. This object must
//!                  not be deleted nor directly used after the call as this
//!                  object assumes its ownership and may delete it at any time.
//------------------------------------------------------------------------------

virtual void    Secure(XrdNetSecurity *secp);

//------------------------------------------------------------------------------
//! Set network defaults.
//!
//! @param  options  The options to be added to Accept(), Bind() and Connect()
//!                  calls. These options cannot be turned off, so be careful.
//! @param  buffsz   The UDP buffer size (the initial default is 32K) or the TCP
//!                  window size (initial default is OS dependent).
//------------------------------------------------------------------------------

void            setDefaults(int options, int buffsz=0)
                           {netOpts = options; Windowsz = buffsz;}

//------------------------------------------------------------------------------
//! Set network domain name.
//!
//! @param  dname    The domain name which indicates to Trim() what part of the
//!                  host name is so common that it can be trimmed.
//------------------------------------------------------------------------------

void            setDomain(const char *dname)
                         {if (Domain) free(Domain);
                          Domain = strdup(dname);
                          Domlen = strlen(dname);
                         }

//------------------------------------------------------------------------------
//! Trims off the domain name in a host name.
//!
//! @param  hname    The host name to be trimmed (it is modified).
//------------------------------------------------------------------------------

void            Trim(char *hname);

//------------------------------------------------------------------------------
//! Unbind the network from any bound resouces.
//------------------------------------------------------------------------------

void            unBind();

//------------------------------------------------------------------------------
//! Get the current TCP RCVBUF window size.
//!
//! @return   >0     The current window size.
//! @return  <=0     Either the network is not bound to a port or an error has
//!                  occurred. Window size is unavailable.
//------------------------------------------------------------------------------

int            WSize();

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  erp      The error object for printing error messages. It must be
//!                  supplied.
//! @param  secp     The initial NetSecurity object. This secp object must not
//!                  be deleted nor directly used after the call as this
//!                  object assumes its ownership and may delete it at any time.
//------------------------------------------------------------------------------

                XrdNet(XrdSysError *erp, XrdNetSecurity *secp=0);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual        ~XrdNet();

protected:

XrdSysError       *eDest;
XrdNetSecurity    *Police;
char              *Domain;
int                Domlen;
int                iofd;
int                Portnum;
int                PortType;
int                Windowsz;
int                netOpts;
int                BuffSize;
XrdNetBufferQ     *BuffQ;

private:

int                do_Accept_TCP(XrdNetAddr &myAddr, int opts);
int                do_Accept_TCP(XrdNetPeer &myPeer, int opts);
int                do_Accept_UDP(XrdNetPeer &myPeer, int opts);
};
#endif
