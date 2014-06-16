#ifndef __XRDNETUTILS_HH__
#define __XRDNETUTILS_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d N e t U t i l s . h h                         */
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

#include "XrdOuc/XrdOucEnum.hh"

class XrdOucTList;
class XrdNetAddr;
union XrdNetSockAddr;
  
class XrdNetUtils
{
public:

//------------------------------------------------------------------------------
//! Decode an "encoded" ipv6/4 address and place it "sockaddr" type structure.
//!
//! @param  sadr     address of the union that will hold the results.
//! @param  buff     address of buffer that holds the encoding.
//! @param  blen     length of the string (it need not be null terminated).
//!
//! @return > 0      the port number in host byte order.
//!         = 0      the port number was not set.
//!         < 0      the encoding was not correct.
//------------------------------------------------------------------------------

static int  Decode(XrdNetSockAddr *sadr, const char *buff, int blen);

//------------------------------------------------------------------------------
//! Encode the address and return it in a supplied buffer.
//!
//! @param  sadr     address of the union that holds the IPV4/6 address.
//! @param  buff     address of buffer to hold the null terminated encoding.
//! @param  blen     length of the buffer. It6 should be at least 40 bytes.
//! @param  port     optional port value to use as opposed to the one present
//!                  in sockaddr sadr. The port must be in host order.
//!
//! @return > 0      the length of the encoding less the null byte.
//!         = 0      current address format not supported for encoding.
//!         < 0      buffer is too small; abs(retval) bytes needed.
//------------------------------------------------------------------------------

static int  Encode(const XrdNetSockAddr *sadr, char *buff, int blen, int port=-1);


//------------------------------------------------------------------------------
//! Return multiple addresses associated with a host or IP address. This form
//! allows complete control of address handling. See XrdNetAddr::Set() for an
//! alternate form that returns addresses suitable for use on the local host.
//! The file descriptor association in each returned XrdNetAddr object is set
//! to a negative value.
//!
//! @param  hSpec    -> convert specification to an address. Valid formats:
//!                     IP.v4:   nnn.nnn.nnn.nnn[:<port>]
//!                     IP.v6:   [ipv6_addr][:<port>]
//!                     IP.xx:   name[:port] xx is determined by getaddrinfo()
//! @param  aListP   place where the pointer to the returned array of XrdNetAddr
//!                  objects is to be placed. Set to zero if none returned. The
//!                  caller must delete this array when no longer needed.
//! @param  aListN   place where the number of elements in aListP are to be
//!                  returned.
//! @param  opts     Options on what to return. Choose one of:
//!                  allIPMap - all  IPv6 and   mapped IPv4 addrs (default)
//!                  allIPv64 - all  IPv6 and unmapped IPv4 addrs
//!                  allV4Map - all    mapped IPV4 addrs.
//!                  onlyIPv6 - only          IPv6 addrs
//!                  onlyIPv4 - only unmapped IPv4 addrs
//!                  prefIPv6 - only IPv6 addrs; if none, mapped IPv4 addrs
//!                  prefAuto - Returns addresses based on configured non-local
//!                             interfaces. The returned addresses will be
//!                             normally useable on this host and may be IPv4,
//!                             IPv6, mapped IPv4, or a mixture.
//!                  The above may be or'd with one or more of the following:
//!                  onlyUDP  - only addrs valid for UDP connections else TCP
//! @param  pNum     >= 0 uses the value as the port number regardless of what
//!                       is in hSpec, should it be supplied. However, if is
//!                       present, it must be a valid port number.
//!                  <  0 uses the positive value as the port number if the
//!                       port number has not been specified in hSpec.
//!                  **** When set to PortInSpec(the default, see below) the
//!                       port number/name must be specified in hSpec. If it is
//!                       not, an error is returned.
//!                  **** When set to NoPortRaw then hSpec does not contain a
//!                       port number and is a host name, IPv4 address, or an
//!                       IPv6 address *without* surrounding brackets.
//!
//! @return Success: 0 with aListN set to the number of elements in aListP.
//!         Failure: the error message text describing the error and aListP
//!                  and aListN is set to zero. The message is in persistent
//!                  storage and cannot be modified.
//------------------------------------------------------------------------------

enum AddrOpts {allIPMap=  0, allIPv64=  1, allV4Map=  2,
               onlyIPv6=  3, onlyIPv4=  4, prefIPv6=  8,
               prefAuto= 16, onlyUDP =128
              };

static const int PortInSpec = (int)0x80000000;
static const int NoPortRaw  = (int)0xC0000000;

static
const char  *GetAddrs(const char *hSpec, XrdNetAddr *aListP[], int &aListN,
                      AddrOpts    opts=allIPMap, int pNum=PortInSpec);

//------------------------------------------------------------------------------
//! Obtain an easily digestable list of hosts. This is the list of up to eight
//! unique aliases (i.e. with different addresses) assigned to a base hostname.
//!
//! @param  sPort    If not nil, the *sPort will be set to hPort if and only if
//!                  the IP address in one of the entries matches the host
//!                  address. Otherwise, the value is unchanged.
//! @param  hName    the host specification suitable for XrdNetAddr.Set().
//! @param  hPort    When >= 0 specified the port to use regardless of hSpec.
//!                  When <  0 the port must be present in hSpec.
//!         hWant    Maximum number of list entries wanted. If hWant is greater
//!                  that eight it is set eigth.
//! @param  eText    When not nil, is where to place error message text.
//!
//! @return Success: Pointer to a list of XrdOucTList objects where
//!                  p->val  is the port number
//!                  p->text is the host name.
//!                  The list of objects belongs to the caller.
//!         Failure: A nil pointer is returned. If eText is supplied, the error
//!                  message, in persistent storage, is returned.
//------------------------------------------------------------------------------

static
XrdOucTList *Hosts(const char  *hSpec, int hPort=-1, int hWant=8, int *sPort=0,
                               const char **eText=0);

//------------------------------------------------------------------------------
//! Convert an IP address/port (V4 or V6) into the standard V6 RFC ASCII
//! representation: "[address]:port".
//!
//! @param  sAddr    Address to convert. This is either sockaddr_in or
//!                  sockaddr_in6 cast to struct sockaddr.
//! @param  bP       points to a buffer large enough to hold the result.
//!                  A buffer 64 characters long will always be big enough.
//! @param  bL       the actual size of the buffer.
//! @param  opts     Formating options:
//!                  noPort  - does not suffix the port number with ":port".
//!                  oldFmt  - use the deprecated format for an IPV4 mapped
//!                            address: [::d.d.d.d] vs  [::ffff:d.d.d.d].
//!
//! @return Success: The length of the formatted address is returned.
//! @return Failure: Zero is returned and the buffer state is undefined.
//!                  Failure occurs when the buffer is too small or the address family
//!                  (sAddr->sa_family) is neither AF_INET nor AF_INET6.
//------------------------------------------------------------------------------

static const int noPort = 1;
static const int oldFmt = 2;

static int IPFormat(const struct sockaddr *sAddr, char *bP, int bL, int opts=0);

//------------------------------------------------------------------------------
//! Convert an IP socket address/port (V4 or V6) into the standard V6 RFC ASCII
//! representation: "[address]:port".
//!
//! @param  fd       The file descriptor of the socket whose address is to be
//!                  converted. The sign of the fd indicates which address:
//!                  fd > 0 the peer  address is used (i.e. getpeername)
//!                  fd < 0 the local address is used (i.e. getsockname)
//! @param  bP       points to a buffer large enough to hold the result.
//!                  A buffer 64 characters long will always be big enough.
//! @param  bL       the actual size of the buffer.
//! @param  opts     Formating options:
//!                  noPort  - does not suffix the port number with ":port".
//!                  oldFmt  - use the deprecated format for an IPV4 mapped
//!                            address: [::d.d.d.d] vs  [::ffff:d.d.d.d].
//!
//! @return Success: The length of the formatted address is returned.
//! @return Failure: Zero is returned and the buffer state is undefined.
//!                  Failure occurs when the buffer is too small or the file
//!                  descriptor does not refer to an open socket.
//------------------------------------------------------------------------------

static int IPFormat(int fd, char *bP, int bL, int opts=0);

//------------------------------------------------------------------------------
//! Determine if a hostname matches a pattern.
//!
//! @param  hName    the name of the host.
//! @param  pattern  the pattern to match against. The pattern may contain one
//!                  If the pattern contains a single asterisk, then the prefix
//!                  of hName is compared with the characters before the '*' and
//!                  the suffix of hName is compared with the character after.
//!                  If the pattern ends with a plus, the all then pattern is
//!                  taken as a hostname (less '+') and expanded to all possible
//!                  hostnames and each one is compared with hName. If the
//!                  pattern contains both, the asterisk rule is used first.
//!                  If it contains neither then strict equality is used.
//!
//! @return Success: True,  the pattern matches.
//!         Failure: False, no match found.
//------------------------------------------------------------------------------

static bool Match(const char *hName, const char *pattern);

//------------------------------------------------------------------------------
//! Get the fully qualified name of the current host.
//!
//! @param  eName    The name to be returned when the host name or its true
//!                  address could not be returned. The pointer may be nil.
//! @param  eText    When supplied will hold 0 if no errors occurred or error
//!                  message text, in persistent storage, describing why the
//!                  error-triggered alternate name was returned.
//!                  If it contains neither then strict equality is used.
//!
//! @return An strdup() copy of the host name, address , or eName; unless eName
//!         is nil, in which case a nil pointer is returned. The caller is
//!         responsible for freeing any returned string using free().
//------------------------------------------------------------------------------

static char *MyHostName(const char *eName="*unknown*", const char **eText=0);

//------------------------------------------------------------------------------
//! Get the supported network protocols.
//!
//! @param  netqry   An NetType enum specifying the protocol to inspect.
//! @param  eText    When not nil, is where to place error message text.
//!
//! @return One the the NetProt enums (see below). When hasNone is returned
//!         and eText is not nill it will point to a static string that gives
//!         the reason. If the reason is a null string, the query was successful
//!         but returned no matching protocols.
//------------------------------------------------------------------------------

enum NetProt {hasNone  = 0, //!< Unable to determine available protocols
              hasIPv4  = 1, //<! Has only IPv4 capability
              hasIPv6  = 2, //<! Has only IPv6 capability
              hasIP64  = 3  //<! Has IPv4 IPv6 capability (dual stack)
             };

enum NetType {qryINET  = 0 //!< Only consider internet protocols
             };

static NetProt      NetConfig(NetType netquery=qryINET, const char **eText=0);

//------------------------------------------------------------------------------
//! Parse an IP or host name specification.
//!
//! @param  hSpec    the name or IP address of the host. As one of the following
//!                  "[<ipv6>]:<port>", "<ipv4>:<port>", or "<name>:<port>".
//! @param  hName    place where the starting address of the host is placed.
//! @param  hNend    place where the ending   address+1 is placed. This will
//!                  point to either ']', ':', or a null byte.
//! @param  hPort    place where the starting address of the port is placed.
//!                  If no ":port" was found, this will contain *hNend.
//! @param  hNend    place where the ending   address+1 is placed. If no port
//!                  If no ":port" was found, this will contain *hNend.
//!
//! @return Success: True.
//!         Failure: False, hSpec is not valid. Some output parameters may have
//!                  been set but shlould be ignored.
//------------------------------------------------------------------------------

static bool Parse(const char *hSpec, const char **hName, const char **hNend,
                                     const char **hPort, const char **hPend);

//------------------------------------------------------------------------------
//! Obtain the numeric port associated with a file descriptor.
//!
//! @param  fd       the file descriptor number.
//! @param  eText    when not null, the reason for a failure is returned.
//!
//! @return Success: The positive port number.
//!         Failure: 0 is returned and if eText is not null, the error message.
//------------------------------------------------------------------------------

static int  Port(int fd, char **eText=0);

//------------------------------------------------------------------------------
//! Obtain the protocol identifier.
//!
//! @param  pName    the name of the protocol (e.g. "tcp").
//! @param  eText    when not null, the reason for a failure is returned.
//!
//! @return The protocol identifier.
//------------------------------------------------------------------------------

static int  ProtoID(const char *pName);

//------------------------------------------------------------------------------
//! Obtain the numeric port corresponding to a symbolic name.
//!
//! @param  sName    the name of the service or a numeric port number.
//! @param  isUDP    if true, returns the UDP service port o/w the TCP service
//! @param  eText    when not null, the reason for a failure is returned.
//!
//! @return Success: The positive port number.
//!         Failure: 0 is returned and if eText is not null, the error message.
//------------------------------------------------------------------------------

static int  ServPort(const char *sName, bool isUDP=false, const char **eText=0);

//------------------------------------------------------------------------------
//! Set the family and hints to be used in GetAddrs() with prefAuto. This is
//! used within this class and by XrdNetAddr when the IP mode changes.  It is
//! meant for internal use only.
//!
//! @param  ipType   Is one of the following from the AddrOpts enum:
//!                  allIPMap - Use IPv6 and mapped IPv4 addrs (default)
//!                  onlyIPv4 - Use only IPv4 addresses.
//!                  prefAuto - Determine proper options based on configuration.
//!
//! @return The getaddrinfo() hints value that should be used.
//------------------------------------------------------------------------------

static int  SetAuto(AddrOpts aOpts=allIPMap);

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

            XrdNetUtils() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

           ~XrdNetUtils() {}
private:

static int setET(char **errtxt, int rc);
static int autoFamily;
static int autoHints;
};

XRDOUC_ENUM_OPERATORS(XrdNetUtils::AddrOpts)

#endif
