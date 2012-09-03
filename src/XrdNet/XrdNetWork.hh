#ifndef __XRDNetWork_H__
#define __XRDNetWork_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d N e t W o r k . h h                          */
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
#ifndef WIN32
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <Winsock2.h>
#endif

#include "XrdNet/XrdNet.hh"

class XrdSysError;
class XrdNetLink;
class XrdNetSecurity;

class XrdNetWork : public XrdNet
{
public:

// Accept()   processes incomming connections. When a succesful connection is
//            made, it returns an XrdNetLink object suitable for communications.
//            If a timeout occurs, or an XrdNetLink object cannot be allocated,
//            it returns 0. Options are those defined above. A timeout, in
//            seconds, may be specified.
//
XrdNetLink     *Accept(int opts=0,
                       int timeout=-1);

// Connect() Creates a socket and connects to the given host and port. Upon
//           success, it returns an XrdNetLink object suitable for peer
//           communications. Upon failure it returns zero. Options are as above.
//           A second timeout may be specified.
//
XrdNetLink     *Connect(const char *host,  // Destination host or ip address
                        int   port,        // Port number
                        int   opts=0,      // Options
                        int   timeout=-1   // Second timeout
                       );

// Relay() creates a UDP socket and optionally sets things up so that
//         messages will be routed to a particular host:port destination.
//         Upon success it returs the address of a XrdNetLink object that
//         be used to communicate with the dest. Upon failure return zero.
//
XrdNetLink     *Relay(const char  *dest=0, // Optional destination
                      int          opts=0  // Optional options as above
                     );

// When creating this object, you must specify the error routing object.
// Optionally, specify the security object to screen incomming connections.
// (if zero, no screening is done).
//
                XrdNetWork(XrdSysError *erp, XrdNetSecurity *secp=0)
                          : XrdNet(erp, secp) {}
               ~XrdNetWork() {}
};
#endif
