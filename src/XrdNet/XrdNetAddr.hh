#ifndef __XRDNETADDR_HH__
#define __XRDNETADDR_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d N e t A d d r . h h                          */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include "XrdNet/XrdNetAddrInfo.hh"
  
//------------------------------------------------------------------------------
//! The XrdNetAddr class implements the manipulators for XrdNetAddrInfo.
//------------------------------------------------------------------------------

struct addrinfo;

class XrdNetAddr : public XrdNetAddrInfo
{
public:

//------------------------------------------------------------------------------
//! Optionally set and also returns the port number for our address.
//!
//! @param pNum      when negative it only returns the current port. Otherwise,
//!                  it is taken as the value to be set.
//!
//! @return Success: The port number, which may be 0 if not set.
//!         Failure: -1 address is not an internet address or port is invalid.
//------------------------------------------------------------------------------

int         Port(int pNum=-1);

//------------------------------------------------------------------------------
//! Set the IP address and possibly the port number of the current node. This
//! method is useful in obtaining the fully qualified name of the current host.
//! The file descriptor association, if any, is reset to a negative value.
//!
//!
//! @param  pNum     The port number associated with this node (may be 0).
//!
//! @return Success: 0.
//!         Failure: Error message text describing the error. The message is in
//!                  persistent storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Self(int pNum=0);

//------------------------------------------------------------------------------
//! Set the IP address and possibly the port number.
//!
//! @param  hSpec    0 -> address is set to in6addr_any for binding via bind()
//!                 !0 -> convert specification to an address. Valid formats:
//!                       IPv4: nnn.nnn.nnn.nnn[:<port>]
//!                       IPv6: [ipv6_addr][:<port>]  **addr brackets required**
//!                       IPvx: name[:port] x is determined by getaddrinfo()
//!                       Unix: /<path>
//!
//! @param  pNum     When positive, uses the value as the port number regardless
//!                  of what is in hSpec, should it be supplied.
//!                  When set to PortInSpec(see below) the port number must be
//!                  specified in hSpec. If it is not, an error is returned.
//!                  When negative, uses the positive value as the port number
//!                  only when a port number has not been specified in hSpec.
//!
//! @return Success: 0.
//!         Failure: Error message text describing the error. The message is in
//!                  persistent storage and cannot be modified.
//------------------------------------------------------------------------------

static const int PortInSpec = 0x80000000;

const char *Set(const char *hSpec, int pNum=0);

//------------------------------------------------------------------------------
//! Return multiple addresses. This form can only be used on the first element
//! of this object that has been allocated as an array. This method is useful
//! for getting all of the aliases assigned to a dns entry.
//! The file descriptor association, if any, is reset to a negative value.
//!
//! @param  hSpec    0 -> address is set to in6addr_any for binding via bind()
//!                 !0 -> convert specification to an address. Valid formats:
//!                       IP.v4:   nnn.nnn.nnn.nnn[:<port>]
//!                       IP.v6:   [ipv6_addr][:<port>]
//!                       IP.xx:   name[:port] xx is determined by getaddrinfo()
//!                       IP.Unix: /<path>
//! @param  maxIP    number of elements in the array.
//! @param  numIP    the number of IP addresses actually set (returned value).
//! @param  pNum     When positive, uses the value as the port number regardless
//!                  of what is in hSpec, should it be supplied.
//!                  When set to PortInSpec(see above) the port number must be
//!                  specified in hSpec. If it is not, an error is returned.
//!                  When negative, uses the positive value as the port number
//!                  only when a port number has not been specified in hSpec.
//!
//! @return Success: 0 with numIP set to the number of elements set.
//!         Failure: the error message text describing the error and
//!                  numIP is set to zero. The message is in persistent
//!                  storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Set(const char *hSpec, int &numIP, int maxIP, int pNum=0);

//------------------------------------------------------------------------------
//! Set our address via a sockaddr structure.
//!
//! @param  sockP    a pointer to an initialized and valid sockaddr structure.
//! @param  sockFD   the associated file descriptor and can be used to record
//!                  the file descriptor returned by accept().
//!
//! @return Success: Returns 0.
//!         Failure: Returns the error message text describing the error. The
//!                  message is in persistent storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Set(const struct sockaddr *sockP, int sockFD=-1);

//------------------------------------------------------------------------------
//! Set our address via getpeername() from the supplied socket file descriptor.
//!
//! @param  sockFD   a connected socket file descriptor. The value is also
//!                  recorded as the associated file descriptor.
//!
//! @return Success: Returns 0.
//!         Failure: Returns the error message text describing the error. The
//!                  message is in persistent storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Set(int sockFD);

//------------------------------------------------------------------------------
//! Force this object to work in IPV4 mode only. This method permanently sets
//! IPV4 mode which cannot be undone without a restart. It is meant to bypass
//! broken IPV6 stacks on those unfortunate hosts that have one. It should be
//! called before any other calls to this object (e.g. initialization time).
//------------------------------------------------------------------------------

static void  SetIPV4();

//------------------------------------------------------------------------------
//! Assignment operator
//------------------------------------------------------------------------------

XrdNetAddr &operator=(XrdNetAddr const &rhs)
            {if (this != &rhs) XrdNetAddrInfo(rhs); return *this;}

//------------------------------------------------------------------------------
//! Copy constructor
//------------------------------------------------------------------------------

            XrdNetAddr(XrdNetAddr const &aP) : XrdNetAddrInfo(aP) {}

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

            XrdNetAddr() : XrdNetAddrInfo() {}

            XrdNetAddr(const XrdNetAddr *addr) : XrdNetAddrInfo(addr) {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

           ~XrdNetAddr() {}
private:
static struct addrinfo    *Hints(int htype);
void                       Init(struct addrinfo *rP, int Port);
bool                       Map64();

static struct addrinfo    *hostHints;
static struct addrinfo    *huntHints;
static bool                useIPV4;
};
#endif
