#ifndef __XRDNETIF_HH__
#define __XRDNETIF_HH__
/******************************************************************************/
/*                                                                            */
/*                           X r d N e t I F . h h                            */
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

#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//! The XrdNetIF class handles host interfaces. It is used to obtain the
//! available interface addresses, encode them to be easily transmitted, and
//! decode them to set the interface variations that can be used.
//------------------------------------------------------------------------------

class XrdNetAddrInfo;
class XrdOucTList;
class XrdSysError;

struct sockaddr;

class XrdNetIF
{
public:

//------------------------------------------------------------------------------
//! The enum that is used to index into ifData to get appropriate interface.
//------------------------------------------------------------------------------

       enum ifType {Public = 0, Private = 1, ifNum = 2};

//------------------------------------------------------------------------------
//! Check if destination type is a true IPV6 address.
//!
//! @param  dtype -> Either Public or Private ifType
//!
//! @return true  GetDest() will return a true   IPV6 address.
//! @return false GetDest() will return a mapped IPV4 address.
//------------------------------------------------------------------------------

inline bool DestIPv6(ifType dtype) {return v6Dest[dtype];}

//------------------------------------------------------------------------------
//! Get the interface address with a port number.
//!
//! @param  dtype -> Either Public or Private ifType
//! @param  dest  -> Reference to where a pointer to the dest will be placed
//! @param  prefn -> When true, a hostname:port is returned if possible
//!
//! @return The length of the name whose pointer is placed in name.
//!         A value of zero indicates that no such interface exists.
//------------------------------------------------------------------------------

inline int  GetDest(ifType dtype, const char *&dest, bool prefn=false)
                   {ifData *ifP = (prefn ? ifNest[dtype] : ifDest[dtype]);
                    dest = ifP->iVal; return ifP->iLen;
                   }

//------------------------------------------------------------------------------
//! Get the interface name without a port number.
//!
//! @param  ntype -> Either Public or Private ifType
//! @param  name  -> Reference to where a pointer to the name will be placed
//!
//! @return The length of the name whose pointer is placed in name.
//!         A value of zero indicates that no such interface exists.
//------------------------------------------------------------------------------

inline int  GetName(ifType ntype, const char *&name)
                   {name = ifName[ntype]->iVal; return ifName[ntype]->iLen;}

//------------------------------------------------------------------------------
//! Copy the interface name and return port number.
//!
//! @param  ntype -> Either Public or Private ifType
//! @param  nbuff -> Reference to buffer where the name will be placed. It must
//!                  be atleast 256 bytes in length.
//! @param  nport -> Place where the port number will be placed.
//!
//! @return The length of the name copied into the buffer.
//!         A value of zero indicates that no such interface exists.
//------------------------------------------------------------------------------

inline int  GetName(ifType ntype, char *nbuff, int &nport)
                   {strcpy(nbuff, ifName[ntype]->iVal); nport = ifPort;
                    return ifName[ntype]->iLen;
                   }

//------------------------------------------------------------------------------
//! Obtain an easily digestable list of IP routable interfaces to this machine.
//!
//! @param  ifList   Place where the list of interfaces will be placed.
//! @param  eText    When not nil, is where to place error message text.
//!
//! @return Success: Returns the count of interfaces in the list.
//!                  *ifList->sval[0] strlen(ifList->text)
//!                  *ifList->sval[1] when != 0 the address is private.
//!                  *ifList->text    the interface address is standard format.
//!                  The list of objects belongs to the caller and must be
//!                  deleted when no longer needed.
//!         Failure: Zero is returned. If eText is supplied, the error message,
//!                  in persistent storage, is returned.
//------------------------------------------------------------------------------

static int  GetIF(XrdOucTList **ifList, const char **eText=0);

//------------------------------------------------------------------------------
//! Obtain an easily transmittable IP routable interfaces to this machine.
//!
//! @param  buff     Pointer to buffer to hold result which can be fed to SetIF.
//! @param  blen     The length of the buffer (4K is really sufficient).
//! @param  eText    When not nil, is where to place error message text.
//! @param  show     When true configured interfaces are also displayed.
//!
//! @return Success: Number of bytes placed in buff, excluding the null.
//!         Failure: Zero is returned. If eText is supplied, the error message,
//!                  in persistent storage, is returned.
//------------------------------------------------------------------------------

static int  GetIF(char *buff, int blen, const char **eText=0, bool show=false);

//------------------------------------------------------------------------------
//! Obtain an easily transmittable IP routable interfaces to this machine.
//!
//! @param  ifline   Reference to a char * pointer that will get the result.
//! @param  eText    When not nil, is where to place error message text.
//! @param  show     When true configured interfaces are also displayed.
//!
//! @return Success: Number of bytes in the returned string ecluding the null.
//!                  The caller is responsible for unallocating it via free().
//!         Failure: Zero is returned. If eText is supplied, the error message,
//!                  in persistent storage, is returned. *ifline is set to 0.
//------------------------------------------------------------------------------

static int  GetIF(char *&ifline, const char **eText=0, bool show=false);

//------------------------------------------------------------------------------
//! Determine if an endpoint is this domain based on hostname.
//!
//! @param  epAddr   Pointer to the endpoint NetAddrInfo object.
//!
//! @result true     The endpoint is in this domain.
//! @result false    Either the endpoint is not in this domain, is a private
//!                  address, or is not registered in DNS.
//------------------------------------------------------------------------------

static bool InDomain(XrdNetAddrInfo *epaddr);

//------------------------------------------------------------------------------
//! Routing() and SetIF() parameter.
//!
//! netDefault - netSplit for Routing() and Routing() value for SetIF().
//! netSplit   - public and private addresses are routed separately so that
//!              substitution of one type of address for another is not allowed.
//! netCommon  - clients with private addresses also have public addresses.
//!              Source and target addresses should match but a public address
//!              may be used in the absence of a private address.
//! netLocal   - private addresses are registered and can be used by public
//!              clients within this domain. Clients with public addresses can
//!              be routed to private addresses.
//------------------------------------------------------------------------------

       enum netType {netDefault = 0, netSplit, netCommon, netLocal};

//------------------------------------------------------------------------------
//! Get the assigned port number
//!
//! @return The port number.
//------------------------------------------------------------------------------

inline int  Port() {return ifPort;}

//------------------------------------------------------------------------------
//! Set the assigned port number. This method is not thread safe!
//!
//! @param  pnum     The port number.
//!
//! @return The previous port number.
//------------------------------------------------------------------------------

       int  Port(int pnum);

//------------------------------------------------------------------------------
//! Set the default assigned port number.
//!
//! @param  pnum     The port number.
//!
//! @return The previous port number.
//------------------------------------------------------------------------------

static void PortDefault(int pnum=1094) {dfPort = pnum;}

//------------------------------------------------------------------------------
//! Set default interface network routing.
//!
//! @param  nettype  Network routing (see netType definition).
//------------------------------------------------------------------------------

static void Routing(netType nettype);

//------------------------------------------------------------------------------
//! Set the ifData structure based on the interface string generated by GetIF().
//!
//! @param  src      The network information of host supplying the if string.
//! @param  ifList   The interface string, it must be null terminated.
//! @param  nettype  Determine how undefined interfaces are resolved. See
//!                  the netType definition.
//!
//! @return Success: True.
//!         Failure: False and if eText is supplied, the error message,
//!                  in persistent storage, is returned.
//------------------------------------------------------------------------------

       bool SetIF(XrdNetAddrInfo *src, const char *ifList,
                  netType nettype=netDefault);

//------------------------------------------------------------------------------
//! Set the public and private network interface names.
//!
//! @param  ifnames  Pointer to the comma seperated interface names. This
//!                  string is modified.
//!
//! @return true     Names have been set.
//! @return false    Invalid interface name list.
//------------------------------------------------------------------------------

static bool SetIFNames(char *ifnames);

//------------------------------------------------------------------------------
//! Specify where messages are to be sent.
//!
//! @param  erp      Pointer to the error message object. By default, no error
//!                  messages are printed. This is not a thread-safe call and
//!                  the err disposition must be set at initialization time.
//------------------------------------------------------------------------------

static void SetMsgs(XrdSysError *erp) {eDest = erp;}

//------------------------------------------------------------------------------
//! Constructor and Destructor
//------------------------------------------------------------------------------

       XrdNetIF() : ifBuff(0) {}

      ~XrdNetIF() {if (ifBuff) free(ifBuff);}

private:

struct ifAddrs
      {short hALen;
       short hDLen;
       bool  ipV6;
       char  hAddr[64];    // IPV6      address ([])
       char  hDest[64];    // IPV6:port in deprecated format
      };

bool  GenAddrs(ifAddrs &ifTab,XrdNetAddrInfo *src,const char *hName,bool isPVT);
bool  GenIF(XrdNetAddrInfo **src, int srcnum);
static
bool  IsOkName(const char *ifn, short &ifNum);
static
char *SetDomain();
static
bool  V4LinkLocal(struct sockaddr *saP);

struct ifData
{
       short  iLen;
       char   iVal[256]; // Actually of size iLen

       ifData() : iLen(0) {*iVal = 0;}
      ~ifData() {}
};

ifData        *ifName[ifNum];
ifData        *ifNest[ifNum];
ifData        *ifDest[ifNum];
int            ifPort;
bool           v6Dest[ifNum];
char          *ifBuff;

static
XrdSysError   *eDest;
static
char          *myDomain;
static
char          *ifCfg[2];
static
netType        netRoutes;
static int     dfPort;
static ifData  ifNull;
};
#endif
