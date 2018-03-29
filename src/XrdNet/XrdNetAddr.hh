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
//! Determine if IPV4 mode has been set.
//!
//! @return True     IPV4 mode has     been set.
//!         False    IPV4 mode has not been set.
//------------------------------------------------------------------------------

static bool IPV4Set() {return useIPV4;}

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
//! Set the IP address and possibly the port number.
//!
//! @param  hSpec    0 -> address is set to in6addr_any for binding via bind()
//!                       (INADDR_ANY in IPV4 mode).
//!                 !0 -> convert specification to an address. Valid formats:
//!                       IPv4: nnn.nnn.nnn.nnn[:<port>]
//!                       IPv6: [ipv6_addr][:<port>]  **addr brackets required**
//!                       IPvx: name[:port] x is determined by getaddrinfo()
//!                       Unix: /<path>
//! @param  pNum     >= 0 uses the value as the port number regardless of what
//!                       is in hSpec, should it be supplied. However, if is
//!                       present, it must be a valid port number or name.
//!                  <  0 uses the positive value as the port number if the
//!                       port number has not been specified in hSpec.
//!                  **** When set to PortInSpec(the default, see below) the
//!                       port number/name must be specified in hSpec. If it is
//!                       not, an error is returned.
//!
//! @return Success: 0.
//!         Failure: Error message text describing the error. The message is in
//!                  persistent storage and cannot be modified.
//------------------------------------------------------------------------------

static const int PortInSpec = (int)0x80000000;

const char *Set(const char *hSpec, int pNum=PortInSpec);

//------------------------------------------------------------------------------
//! Return multiple addresses. This form can only be used on the first element
//! of this object that has been allocated as an array. This method is useful
//! for getting all of the aliases assigned to a dns entry.
//! The file descriptor association is set to a negative value.
//!
//! @param  hSpec    0 -> address is set to in6addr_any for binding via bind()
//!                 !0 -> convert specification to an address. Valid formats:
//!                       IP.v4:   nnn.nnn.nnn.nnn[:<port>]
//!                       IP.v6:   [ipv6_addr][:<port>]
//!                       IP.xx:   name[:port] xx is determined by getaddrinfo()
//! @param  maxIP    number of elements in the array.
//! @param  numIP    the number of IP addresses actually set (returned value).
//! @param  pNum     >= 0 uses the value as the port number regardless of what
//!                       is in hSpec, should it be supplied. However, if is
//!                       present, it must be a valid port number or name.
//!                  <  0 uses the positive value as the port number if the
//!                       port number has not been specified in hSpec.
//!                  **** When set to PortInSpec(the default, see below) the
//!                       port number/name must be specified in hSpec. If it is
//!                       not, an error is returned.
//! @param  forUDP   when true addresses are usable for UDP connections.
//!                  Otherwise, they are for TCP connections.
//!
//! @return Success: 0 with numIP set to the number of elements set.
//!         Failure: the error message text describing the error and
//!                  numIP is set to zero. The message is in persistent
//!                  storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Set(const char *hSpec, int &numIP, int maxIP,
                int pNum=PortInSpec, bool forUDP=false);

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
//! Set our address from the supplied socket file descriptor.
//!
//! @param  sockFD   a connected socket file descriptor. The value is also
//!                  recorded as the associated file descriptor.
//! @param  peer     When true  the address is set from getpeername()
//!                  When false the address is set from getsockname()
//!
//! @return Success: Returns 0.
//!         Failure: Returns the error message text describing the error. The
//!                  message is in persistent storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Set(int sockFD, bool peer=true);

//------------------------------------------------------------------------------
//! Set our address via and addrinfo structure and initialize the port.
//!
//! @param  rP       pointer to an addrinfo structure.
//! @param  port     the port number to set.
//! @param  mapit    when true maps IPv4 addresses to IPv6. Otherwise, does not.
//!
//! @return Success: Returns 0.
//!         Failure: Returns the error message text describing the error. The
//!                  message is in persistent storage and cannot be modified.
//------------------------------------------------------------------------------

const char *Set(struct addrinfo *rP, int port, bool mapit=false);

//------------------------------------------------------------------------------
//! Set the cache time for address to name resolutions. This method should only
//! be called during initialization time. The default is to not use the cache.
//------------------------------------------------------------------------------

static void SetCache(int keeptime);

//------------------------------------------------------------------------------
//! Force this object to work in IPV4 mode only. This method permanently sets
//! IPV4 mode which cannot be undone without a restart. It is meant to bypass
//! broken IPV6 stacks on those unfortunate hosts that have one. It should be
//! called before any other calls to this object (e.g. initialization time).
//------------------------------------------------------------------------------

static void SetIPV4();

//------------------------------------------------------------------------------
//! Force this object to work in IPV6 mode using IPV6 or mapped IPV4 addresses.
//! This method permanently sets IPV6 mode which cannot be undone without a
//! restart. It is meant to disable the default mode which determines which
//! address to use based on which address types are configured on the host
//! (i.e. getaddrinfo() with hints AI_ADDRCONFIG|AI_V4MAPPED).
//------------------------------------------------------------------------------

static void SetIPV6();

//------------------------------------------------------------------------------
//! Set the location for this address
//!
//! @param  loc  pointer to the structure that describes the location. See
//!              XrdnetAddrInfo for the definition of the stucture.
//------------------------------------------------------------------------------

void        SetLocation(XrdNetAddrInfo::LocInfo &loc) {addrLoc = loc;}

//------------------------------------------------------------------------------
//! Assignment operator and copy constructor are inherited, no need to define
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  addr     A pointer to an initialized and valid sockaddr or sockaddr
//!                  compatible structure used to initialize the address.
//! @param  port     Uses the address of the current host and the speoified
//!                  port number to initilize the address.
//------------------------------------------------------------------------------

            XrdNetAddr() : XrdNetAddrInfo() {}

            XrdNetAddr(const XrdNetAddr   *addr) : XrdNetAddrInfo(addr) {}

            XrdNetAddr(const sockaddr     *addr) : XrdNetAddrInfo()
                                                   {Set(addr);}

            XrdNetAddr(const sockaddr_in  *addr) : XrdNetAddrInfo()
                                                   {Set((sockaddr *)addr);}

            XrdNetAddr(const sockaddr_in6 *addr) : XrdNetAddrInfo()
                                                   {Set((sockaddr *)addr);}

            XrdNetAddr(int port);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

           ~XrdNetAddr() {}
private:
static struct addrinfo    *Hints(int htype, int stype);
bool                       Map64();

static struct addrinfo    *hostHints;
static struct addrinfo    *huntHintsTCP;
static struct addrinfo    *huntHintsUDP;
static bool                useIPV4;
};
#endif
