#ifndef __XRDNETMSG_H__
#define __XRDNETMSG_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d N e t M s g . h h                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <cstdlib>
#include <cstring>
#ifndef WIN32
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <Winsock2.h>
#endif

#include "XrdNet/XrdNetAddr.hh"

class XrdSysError;

class XrdNetMsg
{
public:

//------------------------------------------------------------------------------
//! Send a UDP message to an endpoint.
//!
//! @param  buff     The data to send.
//! @param  blen     Length of the data in buff. If not specified, the length is
//!                  computed as strlen(buff).
//! @param  dest     The endpint name which can be host:port or a named socket.
//!                  If dest is zero, uses dest specified in the constructor.
//! @param  timeout  maximum seconds to wait for a idle socket. When negative,
//!                  the default, no time limit applies.
//! @return <0       Message not sent due to error.
//! @return =0       Message send (well as defined by UDP)
//! @return >0       Message not sent, timeout occurred.
//------------------------------------------------------------------------------

int           Send(const char *buff,          // The data to be send
                         int   blen=0,        // Length (strlen(buff) if zero)
                   const char *dest=0,        // Hostname to send UDP datagram
                         int   tmo=-1);       // Timeout in ms (-1 = none)

//------------------------------------------------------------------------------
//! Send a UDP message to an endpoint using an I/O vector.
//!
//! @param  iov      The vector of data to send. Total amount be <= 4096 bytes.
//! @param  iovcnt   The number of elements in the vector.
//! @param  dest     The endpint name which can be host:port or a named socket.
//!                  If dest is zero, uses dest specified in the constructor.
//! @param  timeout  maximum seconds to wait for a idle socket. When negative,
//!                  the default, no time limit applies.
//! @return <0       Message not sent due to error.
//! @return =0       Message send (well as defined by UDP)
//! @return >0       Message not sent, timeout occurred.
//------------------------------------------------------------------------------

int           Send(const struct  iovec iov[], // Remaining parms as above
                         int     iovcnt,      // Number of elements in iovec
                   const char   *dest=0,      // Hostname to send UDP datagram
                         int     tmo=-1);     // Timeout in ms (-1 = none)
//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  erp      The error message object for routing error messages.
//! @param  aOK      If supplied, set to true upon success; false otherwise.
//! @param  dest     The endpint name which can be host:port or a named socket.
//!                  This becomes the default endpoint. Any specified endpoint
//!                  to send must be in the same family (e.g. UNIX). If not
//!                  specified, then an endpoint must always be specified with
//!                  send and is restricted to be in the INET family.
//------------------------------------------------------------------------------

                XrdNetMsg(XrdSysError *erp, const char *dest=0, bool *aOK=0);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

               ~XrdNetMsg() {if (FD >= 0) close(FD);}

protected:
int OK2Send(int timeout, const char *dest);
int retErr(int ecode, const char *theDest);
int retErr(int ecode, XrdNetAddr *theDest);

XrdSysError       *eDest;
XrdNetAddr         dfltDest;
XrdNetAddr         specDest;
int                destOK;
int                FD;
};
#endif
