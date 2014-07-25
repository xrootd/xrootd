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
//! Display the final interface configuration.
//!
//! @param  pfx   The desired message prefix (default is as shown).
//------------------------------------------------------------------------------

       void Display(const char *pfx="=====> ");

//------------------------------------------------------------------------------
//! The enum that is used to index into ifData to get appropriate interface.
//------------------------------------------------------------------------------

       enum ifType {PublicV4  = 0,  //<! Public  IPv4 network
                    PrivateV4 = 1,  //<! Private IPv4 network
                    PublicV6  = 2,  //<! Public  IPv6 network
                    PrivateV6 = 3,  //<! Private IPv6 network
                    PrivateIF = 1,  //<! Bit to change PublicVx -> PrivateVx
                    ifNum     = 4,  //<! Count of actual interface types
                    Public46  = 4,  //<! Public  v4|6 network (dual stack)
                    Private46 = 5,  //<! Private v4|6 network (dual stack)
                    Public64  = 6,  //<! Public  v6|4 network (dual stack)
                    Private64 = 7,  //<! Private v6|4 network (dual stack)
                    ifMax     = 8,  //<! Total elements in if vector
                    ifAny     = 8}; //<! Used to select any avilable i/f

//------------------------------------------------------------------------------
//! Get the interface address with a port number.
//!
//! @param  dest  Pointer to the buffer where dest will be placed.
//! @param  dlen  The length of the buffer.
//! @param  ifT   Desired ifType (PublicV6 is the default)
//! @param  prefn When true, a hostname:port is returned if possible
//!
//! @return The length of the name whose pointer is placed in name.
//!         A value of zero indicates that no such interface exists or
//!         the buffer was too small.
//------------------------------------------------------------------------------

       int  GetDest(char *dest, int dlen, ifType ifT=PublicV6, bool prefn=false);

//------------------------------------------------------------------------------
//! Get the interface name without a port number.
//!
//! @param  name  Reference to where a pointer to the name will be placed
//! @param  ifT   Desired ifType (PublicV6 is the default)
//!
//! @return The length of the name whose pointer is placed in name.
//!         A value of zero indicates that no such interface exists.
//------------------------------------------------------------------------------

inline int  GetName(const char *&name, ifType ifT=PublicV6)
                   {if (ifT >= ifAny) ifT = static_cast<ifType>(ifAvail);
                    name = ifName[ifT]->iVal;
                    return ifName[ifT]->iLen;
                   }

//------------------------------------------------------------------------------
//! Copy the interface name and return port number.
//!
//! @param  nbuff Reference to buffer where the name will be placed. It must
//!               be atleast 256 bytes in length.
//! @param  nport Place where the port number will be placed.
//! @param  ifT   Desired ifType (PublicV6 is the default)
//!
//! @return The length of the name copied into the buffer.
//!         A value of zero indicates that no such interface exists.
//------------------------------------------------------------------------------

inline int  GetName(char *nbuff, int &nport, ifType ifT=PublicV6)
                   {if (ifT >= ifAny) ifT = static_cast<ifType>(ifAvail);
                    strcpy(nbuff, ifName[ifT]->iVal); nport = ifPort;
                    return ifName[ifT]->iLen;
                   }

//------------------------------------------------------------------------------
//! Obtain an easily digestable list of IP routable interfaces to this machine.
//!
//! @param  ifList   Place where the list of interfaces will be placed. If
//!                  ifList is null, returns configured interface types.
//! @param  eText    When not nil, is where to place error message text.
//!
//! @return Success: ifList != 0: returns the count of interfaces in the list.
//!                  *ifList->sval[0] strlen(ifList->text)
//!                  *ifList->sval[1] when != 0 the address is private.
//!                  *ifList->text    the interface address is standard format.
//!                  The list of objects belongs to the caller and must be
//!                  deleted when no longer needed.
//!
//!                  ifList == 0: returns types of configured non-local i/f.
//!                  This is or'd values of the static const ints haveXXXX.
//!
//!         Failure: Zero is returned. If eText is supplied, the error message,
//!                  in persistent storage, is returned.
//------------------------------------------------------------------------------

static
const  int  haveNoGI = 0;  //!< ifList == 0 && getifaddrs() is not supported
static
const  int  haveIPv4 = 1;  //!< ifList == 0 && non-local ipv4 i/f found (or'd)
static
const  int  haveIPv6 = 2;  //!< ifList == 0 && non-local ipv6 i/f found (or'd)

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
//! Get the ifType for client connection.
//!
//! @param  conIPv4  True if connected via IPv4, false means IPv6.
//! @param  hasIP64  True if the client has an IPv4 and IPv6 address.
//! @param  pvtIP    True if the ip address is private.
//!
//! @return The ifType correspodning to the passed arguments.
//------------------------------------------------------------------------------

static ifType GetIFType(bool conIPv4, bool hasIP64, bool pvtIP)
                       {ifType ifT;
                        if (conIPv4) ifT = (hasIP64 ? Public46 : PublicV4);
                           else      ifT = (hasIP64 ? Public64 : PublicV6);
                        if (pvtIP) Privatize(ifT);
                        return ifT;
                       }

//------------------------------------------------------------------------------
//! Determine whether or not an interface exists.
//!
//! @param  ifT   -> Desired ifType (PublicV6 is the default)
//!
//! @return true  -> desired dest exists.
//!         false -> desired dest does not exist.
//------------------------------------------------------------------------------

inline bool HasDest(ifType ifT=PublicV6)
                   {return ifT >= ifAny || ifDest[ifT]->iLen != 0;}

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
//! Get the ifType selection mask for this object.
//!
//! @return A single char that represents the selection mask.
//------------------------------------------------------------------------------

       char Mask() {return ifMask;}

//------------------------------------------------------------------------------
//! Convert an ifType to its corresponding selection mask.
//!
//! @param  ifT   The ifType to convert.
//!
//! @return A single char that represents the selection mask.
//------------------------------------------------------------------------------

static char Mask(ifType ifT)
                {if (ifT >= ifAny) return 0x0f;
                 return ifMaskVec[ifT];
                }

//------------------------------------------------------------------------------
//! Get the human readable for for an ifType.
//!
//! @param  ifT   The ifType to convert.
//!
//! @return A pointer to the human readable name. The string resides in static
//!         storage and is always valid.
//------------------------------------------------------------------------------
static
const char *Name(ifType ifT) {if (ifT >= ifAny) return "any";
                              return ifTName[ifT];
                             }

//------------------------------------------------------------------------------
//! Get the assigned port number
//!
//! @return The port number.
//------------------------------------------------------------------------------

inline int  Port() {return ifPort;}

//------------------------------------------------------------------------------
//! Make an iofType refer to the private network.
//!
//! @param x     The iftype variable that will have the private bit set.
//------------------------------------------------------------------------------
static
inline void Privatize(ifType &x) {x = ifType(x | PrivateIF);}

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
//! @param  port     The port associated with the interfaces; as follows:
//!                  <0 -> Use previous setting if any.
//!                  =0 -> use default port (the default).
//!                  >0 -> Use the number passed.
//! @param  nettype  Determines how undefined interfaces are resolved. See
//!                  the netType definition.
//!
//! @return Success: True.
//!         Failure: False and if eText is supplied, the error message,
//!                  in persistent storage, is returned.
//------------------------------------------------------------------------------

       bool SetIF(XrdNetAddrInfo *src, const char *ifList, int port=0,
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

       XrdNetIF() : ifBuff(0), ifMask(0), ifAvail(0)  {}

      ~XrdNetIF() {if (ifBuff) free(ifBuff);}

private:

struct ifAddrs
      {short hALen;
       short hDLen;
       bool  ipV6;
       bool  prvt;
       char  hAddr[64];    // address
       char  hDest[64];    // address possibly in deprecated format
      };

bool  GenAddrs(ifAddrs &ifTab, XrdNetAddrInfo *src);
bool  GenAddrs(ifAddrs &ifTab, const char *hName, bool wantV6);
bool  GenIF(XrdNetAddrInfo **src, int srcnum);
static
bool  IsOkName(const char *ifn, short &ifIdx);
static
char *SetDomain();
void  SetIFPP();
bool  SetIF64(bool retVal);
static
bool  V4LinkLocal(struct sockaddr *saP);

struct ifData
{
       short  iLen;
       char   iVal[6]; // Actually of size iLen

       ifData() : iLen(0) {*iVal = 0;}
      ~ifData() {}
};

ifData        *ifName[ifMax];
ifData        *ifDest[ifMax];
bool           ifxDNS[ifMax];
char          *ifBuff;

struct pInfo {char   len;
              char   val[7]; // Contains ":12345\0"
                     pInfo() : len(0) {*val = 0;}
             } portSfx;

int            ifPort;
short          ifRoute;
char           ifMask;
char           ifAvail;

static
XrdSysError   *eDest;
static char   *myDomain;
static char   *ifCfg[2];
static
const char    *ifTName[ifMax];
static
const char    *ifMaskVec;
static
netType        netRoutes;
static int     dfPort;
static ifData  ifNull;
};
#endif
