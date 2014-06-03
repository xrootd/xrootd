/******************************************************************************/
/*                                                                            */
/*                        X r d N e t U t i l s . c c                         */
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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysPlatform.hh"
#ifndef HAVE_PROTOR
#include "XrdSys/XrdSysPthread.hh"
#endif

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

int  XrdNetUtils::autoFamily;

// The following also sets autoFamily!
//
int  XrdNetUtils::autoHints  = XrdNetUtils::SetAuto(XrdNetUtils::prefAuto);
  
/******************************************************************************/
/*                                D e c o d e                                 */
/******************************************************************************/
  
int XrdNetUtils::Decode(XrdNetSockAddr *sadr, const char *buff, int blen)
{
   static const int ipv4Sz = sizeof(struct in_addr)*2+4;
   static const int ipv6Sz = sizeof(struct in6_addr)*2+4;
   char bval[sizeof(struct in6_addr)+2];
   int isv6, n, i = 0, Odd = 0;

// Determine if this will be IPV4 or IPV6 (only ones allowed)
//
        if (blen == ipv6Sz) isv6 = 1;
   else if (blen == ipv4Sz) isv6 = 0;
   else return -1;

// Convert the whole string to a temporary
//
   while(blen--)
        {     if (*buff >= '0' && *buff <= '9') n = *buff-48;
         else if (*buff >= 'a' && *buff <= 'f') n = *buff-87;
         else if (*buff >= 'A' && *buff <= 'F') n = *buff-55;
         else return -1;
         if (Odd) bval[i++] |= n;
            else  bval[i  ]  = n << 4;
         buff++; Odd = ~Odd;
        }

// Clear the address
//
   memset(sadr, 0, sizeof(XrdNetSockAddr));

// Copy out the data, as needed
//
   if (isv6)
      {sadr->v6.sin6_family = AF_INET6;
       memcpy(&(sadr->v6.sin6_port), bval, 2);
       memcpy(&(sadr->v6.sin6_addr), &bval[2], sizeof(struct in6_addr));
      } else {
       sadr->v4.sin_family  = AF_INET;
       memcpy(&(sadr->v4.sin_port), bval, 2);
       memcpy(&(sadr->v4.sin_addr), &bval[2], sizeof(struct in_addr));
      }

// Return the converted port (it's the same for v4/v6)
//
   return static_cast<int>(ntohs(sadr->v6.sin6_port));
}

/******************************************************************************/
/*                                E n c o d e                                 */
/******************************************************************************/
  
int XrdNetUtils::Encode(const XrdNetSockAddr *sadr, char *buff, int blen,
                                                                int port)
{
   static const char *hv = "0123456789abcdef";
   char *src, bval[sizeof(struct in6_addr)+2];
   int asz, i, j = 0;

// Compute the size we need for the buffer (note we only support IP4/6)
//
        if (sadr->Addr.sa_family == AF_INET6)
           {src = (char *)&(sadr->v6.sin6_addr); asz = sizeof(struct in6_addr);}
   else if (sadr->Addr.sa_family == AF_INET)
           {src = (char *)&(sadr->v4.sin_addr);  asz = sizeof(struct in_addr); }
   else return 0;
   if (blen < (asz*2)+5) return -((asz*2)+5);

// Get the port value in the first two bytes followed by the address.
//
   if (port < 0) memcpy(bval, &(sadr->v6.sin6_port), 2);
      else {short sPort = htons(static_cast<short>(port));
            memcpy(bval, &sPort, 2);
           }
   memcpy(&bval[2], src, asz);
   asz += 2;

// Now convert to hex
//
   for (i = 0; i < asz; i++)
       {buff[j++] = hv[(bval[i] >> 4) & 0x0f];
        buff[j++] = hv[ bval[i]       & 0x0f];
       }
   buff[j] = '\0';

// All done
//
   return asz*2;
}

/******************************************************************************/
/*                              G e t A d d r s                               */
/******************************************************************************/
  
const char  *XrdNetUtils::GetAddrs(const char            *hSpec,
                                   XrdNetAddr            *aListP[], int &aListN,
                                   XrdNetUtils::AddrOpts  opts,     int  pNum)
{
   static const char *badHS = "invalid host specification";
   struct addrinfo hints, *nP, *rP = 0;
   XrdNetAddr *aVec;
   struct {char ipMap[7];                // ::ffff: (length = 7)
           char ipAdr[MAXHOSTNAMELEN+15];
         } aBuff;
   const char *ipAddr, *hnBeg, *hnEnd, *pnBeg, *pnEnd;
   int   n, map426 = 0;

// Prep the returned fields
//
   *aListP = 0;
    aListN = 0;

// Copy the host specification
//
   if (!hSpec) return badHS;
   strlcpy(aBuff.ipAdr, hSpec, sizeof(aBuff.ipAdr));

// Parse the host specification
//
   if (pNum == NoPortRaw)
      {hnBeg = aBuff.ipAdr;
       pNum  = 0;
      } else {
       if (!Parse(aBuff.ipAdr, &hnBeg, &hnEnd, &pnBeg, &pnEnd)) return badHS;
       aBuff.ipAdr[hnEnd-aBuff.ipAdr] = 0;
       if (pnBeg == hnEnd)
          {if (pNum == PortInSpec) return "port not specified";
           if (pNum < 0) pNum = -pNum;
          } else {
           const char *eText;
           aBuff.ipAdr[pnEnd-aBuff.ipAdr] = 0;
           n = ServPort(pnBeg, opts & onlyUDP, &eText);
           if (!n) return eText;
           if (pNum < 0) pNum = n;
          }
      }

// Setup the hints
//
   memset(&hints, 0, sizeof(hints));
   hints.ai_socktype = (opts & onlyUDP ? SOCK_DGRAM : SOCK_STREAM);
   opts = opts & ~onlyUDP;
   switch(opts)
         {case allIPMap: hints.ai_family = AF_INET6;
                         hints.ai_flags  = AI_V4MAPPED | AI_ALL;
                         break;
          case allIPv64: hints.ai_family = AF_UNSPEC;
                         break;
          case allV4Map: hints.ai_family = AF_INET;
                         map426 = 1;
                         break;
          case onlyIPv6: hints.ai_family = AF_INET6;
                         break;
          case onlyIPv4: hints.ai_family = AF_INET;
                         break;
          case prefIPv6: hints.ai_family = AF_INET6;
                         hints.ai_flags  = AI_V4MAPPED;
                         break;
          case prefAuto: hints.ai_family = autoFamily;
                         hints.ai_flags  = autoHints;
                         break;
          default:       hints.ai_family = AF_INET6;
                         hints.ai_flags  = AI_V4MAPPED | AI_ALL;
                         break;
         }

// Check if we need to convert an ipv4 address to an ipv6 one
//
   if (hints.ai_family == AF_INET6
   &&  isdigit(aBuff.ipAdr[0]) && index(aBuff.ipAdr, '.'))
      {strncpy(aBuff.ipMap, "::ffff:", 7);
       ipAddr = aBuff.ipMap;
      } else ipAddr = hnBeg;

// Get all of the addresses
//
   n = getaddrinfo(ipAddr, 0, &hints, &rP);
   if (n || !rP)
      {if (rP) freeaddrinfo(rP);
       return (n ? gai_strerror(n) : "host not found");
      }

// Count the number of entries we will return and get the addr vector
//
   n = 0; nP = rP;
   do {if (nP->ai_family == AF_INET6 || nP->ai_family == AF_INET) n++;
       nP = nP->ai_next;
      } while(nP);
   if (!n) return 0;
   aVec = new XrdNetAddr[n];
   *aListP = aVec; aListN = n;

// Now fill out the addresses
//
   n = 0; nP = rP;
   do {if (nP->ai_family == AF_INET6 || nP->ai_family == AF_INET)
          {aVec[n].Set(nP, pNum, map426); n++;}
       nP = nP->ai_next;
      } while(nP);

// All done
//
   if (rP) freeaddrinfo(rP);
   return 0;
}

/******************************************************************************/
/*                                 H o s t s                                  */
/******************************************************************************/
  
XrdOucTList *XrdNetUtils::Hosts(const char  *hSpec, int hPort, int  hWant,
                                int *sPort, const char **eText)
{
   static const int hMax = 8;
   XrdNetAddr   myAddr(0), aList[hMax];
   XrdOucTList *tList = 0;
   const char  *etext, *hName;
   int numIP, i, k;

// Check if the port must be in the spec and set maximum
//
   if (hPort < 0) hPort = XrdNetAddr::PortInSpec;
   if (hWant > hMax) hWant = hMax;
      else if (hWant < 1) hWant = 1;

// Initialze the list of addresses
//
   if ((etext = aList[0].Set(hSpec, numIP, hWant, hPort)))
      {if (eText) *eText = etext;
       return 0;
      }

// Create the tlist object list without duplicates. We may have duplicates as
// this may be a multi-homed node and we don't want to show that here.
//
   for (i = 0; i < numIP; i++)
       {if (sPort && myAddr.Same(&aList[i]))
           {*sPort = aList[i].Port(); sPort = 0;}
        hName = aList[i].Name("");
        for (k = 0; k < i; k++) {if (!strcmp(hName, aList[k].Name(""))) break;}
        if (k >= i) tList = new XrdOucTList(hName, aList[i].Port(), tList);
       }

// All done, return the result
//
   if (eText) *eText = (tList ? 0 : "unknown processing error");
   return tList;
}

/******************************************************************************/
/*                              I P F o r m a t                               */
/******************************************************************************/

int XrdNetUtils::IPFormat(const struct sockaddr *sAddr,
                          char *bP, int bL, int opts)
{
   XrdNetAddr theAddr;
   int fmtopts = (opts & oldFmt ? XrdNetAddrInfo::old6Map4 : 0);

// Set the address
//
   if (theAddr.Set(sAddr)) return 0;

// Now format the address
//
   if (opts & noPort) fmtopts |=  XrdNetAddrInfo::noPort;
   return theAddr.Format(bP, bL, XrdNetAddrInfo::fmtAdv6, fmtopts);
}
  
/******************************************************************************/

int XrdNetUtils::IPFormat(int fd, char *bP, int bL, int opts)
{
   XrdNetSockAddr theIP;
   SOCKLEN_t addrSize = sizeof(theIP);
   int rc;

// The the address wanted
//
   rc = (fd > 0 ? getpeername( fd, &theIP.Addr, &addrSize)
                : getsockname(-fd, &theIP.Addr, &addrSize));
   if (rc) return 0;

// Now format it
//
   return IPFormat(&theIP.Addr, bP, bL, opts);
}

/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/
  
bool XrdNetUtils::Match(const char *HostName, const char *HostPat)
{
   static const int maxIP = 16;
   const char *mval;
   int i, j, k;

// First check if this will match right away
//
   if (!strcmp(HostPat, HostName)) return true;

// Check for an asterisk do prefix/suffix match
//
   if ((mval = index(HostPat, (int)'*')))
      { i = mval - HostPat; mval++;
       k = strlen(HostName); j = strlen(mval);
       if ((i+j) > k
       || strncmp(HostName,      HostPat,i)
       || strncmp((HostName+k-j),mval,j)) return false;
       return 1;
      }

// Now check for host expansion
//
    i = strlen(HostPat);
    if (i && HostPat[i-1] == '+')
       {XrdNetAddr InetAddr[maxIP];
        char hBuff[264];
        if (i >= (int)sizeof(hBuff)) return false;
        strncpy(hBuff, HostPat, i-1);
        hBuff[i-1] = 0;
        if (InetAddr[0].Set(hBuff, i, maxIP, 0)) return false;
        while(i--) if ((mval = InetAddr[i].Name()) && !strcmp(mval, HostName))
                      return true;
       }

// No matches
//
   return false;
}
  
/******************************************************************************/
/*                            M y H o s t N a m e                             */
/******************************************************************************/
  
char *XrdNetUtils::MyHostName(const char *eName, const char **eText)
{
   XrdNetAddr myAddr;
   const char *etext, *myName;
   char buff[1024];

// Get our host name and initialize this object with it
//
   gethostname(buff, sizeof(buff));
   if ((etext = myAddr.Set(buff,0))) myName = eName;
      else myName = myAddr.Name(eName, &etext);

// Return result, we will always have something
//
   if (eText) *eText = etext;
   return (myName ? strdup(myName) : 0);
}

/******************************************************************************/
/*                             N e t C o n f i g                              */
/******************************************************************************/
  
XrdNetUtils::NetProt XrdNetUtils::NetConfig(XrdNetUtils::NetType netquery,
                                            const char **eText)
{
  XrdNetAddr *myAddrs;
  const char *eMsg;
  char buff[1024];
  NetProt hasProt = hasNone;
  int aCnt;

// Make sure we support this query
//
   if (netquery != qryINET)
      {if (eText) *eText = "unsupported NetType query";
       return hasNone;
      }

// Get our host name and initialize this object with it
//
   gethostname(buff, sizeof(buff));

// Now get all of the addresses associated with this hostname
//
   if ((eMsg = GetAddrs(buff, &myAddrs, aCnt, allIPv64, NoPortRaw)))
      {if (eText) *eText = eMsg;
       return hasNone;
      }

// Now run through all of the addresses to see what we have
//
   for (int i = 0; i < aCnt && hasProt != hasIP64; i++)
       {     if (myAddrs[i].isIPType(XrdNetAddrInfo::IPv6))
                hasProt = NetProt(hasProt | hasIPv6);
        else if (myAddrs[i].isIPType(XrdNetAddrInfo::IPv4))
                hasProt = NetProt(hasProt | hasIPv4);
       }

// Delete the array and return what we have
//
   delete [] myAddrs;
   if (hasProt == hasNone && eText) *eText = "";
   return hasProt;
}

/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/
  
bool XrdNetUtils::Parse(const char *hSpec, 
                        const char **hName, const char **hNend,
                        const char **hPort, const char **hPend)
{
   const char *asep = 0;

// Parse the specification
//
   if (*hSpec == '[')
      {if (!(*hNend = index(hSpec+1, ']'))) return false;
       *hName = hSpec+1; asep = (*hNend)+1;
      } else {
       *hName = hSpec;
       if (!(*hNend = index(hSpec, ':'))) *hNend = hSpec + strlen(hSpec);
          else asep = *hNend;
      }

// See if we have a port to parse. We stop on a non-alphameric.
//
   if (asep && *asep == ':')
      {*hPort = ++asep;
       while(isalnum(*asep)) asep++;
       if (*hPort == asep) return false;
       *hPend = asep;
      } else *hPort = *hPend = *hNend;

// All done
//
   return true;
}

/******************************************************************************/
/*                                  P o r t                                   */
/******************************************************************************/

int XrdNetUtils::Port(int fd, char **eText)
{
   XrdNetSockAddr Inet;
   SOCKLEN_t slen = (socklen_t)sizeof(Inet);
   int rc;

   if ((rc = getsockname(fd, &Inet.Addr, &slen)))
      {rc = errno;
       if (eText) setET(eText, errno);
       return -rc;
      }

   return static_cast<int>(ntohs(Inet.v6.sin6_port));
}

/******************************************************************************/
/*                               P r o t o I D                                */
/******************************************************************************/

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

int XrdNetUtils::ProtoID(const char *pname)
{
#ifdef HAVE_PROTOR
    struct protoent pp;
    char buff[1024];
#else
    static XrdSysMutex protomutex;
    struct protoent *pp;
    int    protoid;
#endif

// Note that POSIX did include getprotobyname_r() in the last minute. Many
// platforms do not document this variant but just about all include it.
//
#ifdef __solaris__
    if (!getprotobyname_r(pname, &pp, buff, sizeof(buff))) 
       return IPPROTO_TCP;
    return pp.p_proto;
#elif !defined(HAVE_PROTOR)
    protomutex.Lock();
    if (!(pp = getprotobyname(pname))) protoid = IPPROTO_TCP;
       else protoid = pp->p_proto;
    protomutex.UnLock();
    return protoid;
#else
    struct protoent *ppp;
    if (getprotobyname_r(pname, &pp, buff, sizeof(buff), &ppp))
       return IPPROTO_TCP;
    return pp.p_proto;
#endif
}

/******************************************************************************/
/*                              S e r v P o r t                               */
/******************************************************************************/
  
int XrdNetUtils::ServPort(const char *sName, bool isUDP, const char **eText)
{
   struct addrinfo *rP = 0, myHints;
   int rc, portnum = 0;

// First check if this is a plain number
//
   if (isdigit(*sName))
      {char *send;
       portnum = strtol(sName, &send, 10);
       if (portnum > 0 && portnum < 65536 && *send == 0) return portnum;
       if (eText) *eText = "invalid port number";
       return 0;
      }

// Fill out the hints
//
   memset(&myHints, 0, sizeof(myHints));
   myHints.ai_socktype = (isUDP ? SOCK_DGRAM : SOCK_STREAM);

// Try to find the port number
//
   rc = getaddrinfo(0, sName, &myHints, &rP);
   if (rc || !rP)
      {if (eText) *eText = (rc ? gai_strerror(rc) : "service not found");
       if (rP) freeaddrinfo(rP);
       return 0;
      }

// Return the port number
//
   portnum = int(ntohs(((struct sockaddr_in *)(rP->ai_addr))->sin_port));
   freeaddrinfo(rP);
   if (!portnum && eText) *eText = "service has no port";
   return portnum;
}
 
/******************************************************************************/
/*                               S e t A u t o                                */
/******************************************************************************/
  
int XrdNetUtils::SetAuto(XrdNetUtils::AddrOpts aOpts)
{

// If a specific family is not specified, then determine which families to use
//
   if (aOpts != onlyIPv4 && aOpts != allIPMap)
      {int ifTypes = XrdNetIF::GetIF(0);
            if (ifTypes & XrdNetIF::haveIPv6) aOpts = allIPMap;
       else if (ifTypes & XrdNetIF::haveIPv4) aOpts = onlyIPv4;
       else {autoFamily = AF_UNSPEC; autoHints = AI_V4MAPPED | AI_ADDRCONFIG;
             return AI_V4MAPPED | AI_ADDRCONFIG;
            }
      }

// If this is forced IPv4 then we know how to set the hints
//
   if (aOpts == onlyIPv4)
      {autoFamily = AF_INET; autoHints = 0; return 0;}

// So, this is IPv6. Be as flexible as possible.
//
   autoFamily = AF_INET6;
   autoHints  = AI_V4MAPPED | AI_ALL;
   return AI_V4MAPPED | AI_ALL;
}
  
/******************************************************************************/
/* Private:                        s e t E T                                  */
/******************************************************************************/
  
int XrdNetUtils::setET(char **errtxt, int rc)
{
    if (rc) *errtxt = strerror(rc);
       else *errtxt = (char *)"unexpected error";
    return 0;
}
