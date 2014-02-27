/******************************************************************************/
/*                                                                            */
/*                         X r d N e t A d d r . c c                          */
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
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetCache.hh"
#include "XrdNet/XrdNetUtils.hh"

/******************************************************************************/
/*                 P l a t f o r m   D e p e n d e n c i e s                  */
/******************************************************************************/
  
// Linux defines s6_addr32 but MacOS does not and Solaris defines it only when
// compiling the kernel. This is really standard stuff that should be here.
//
#ifndef s6_addr32
#if   defined(__solaris__)
#define s6_addr32 _S6_un._S6_u32
#elif defined(__APPLE__)
#define s6_addr32 __u6_addr.__u6_addr32
#endif
#endif

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

struct addrinfo   *XrdNetAddr::hostHints    = XrdNetAddr::Hints(0, 0);

struct addrinfo   *XrdNetAddr::huntHintsTCP = XrdNetAddr::Hints(1, SOCK_STREAM);

struct addrinfo   *XrdNetAddr::huntHintsUDP = XrdNetAddr::Hints(2, SOCK_DGRAM);

bool               XrdNetAddr::useIPV4      = false;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetAddr::XrdNetAddr(int port) : XrdNetAddrInfo()
{
   char buff[1024];

// Get our host name and initialize this object with it
//
   gethostname(buff, sizeof(buff));
   Set(buff, port);
}

/******************************************************************************/
/* Private:                        H i n t s                                  */
/******************************************************************************/
  
struct addrinfo *XrdNetAddr::Hints(int htype, int stype)
{
   static struct addrinfo theHints[3];

// Return properly initialized hint structure. We need to do this dynamically
// in a static constructor since the addrinfo layout differs by OS-type.
//
   memset(&theHints[htype], 0, sizeof(struct addrinfo));;
   if (htype) theHints[htype].ai_flags    = AI_V4MAPPED | AI_ALL;
       else   theHints[htype].ai_flags    = AI_V4MAPPED | AI_CANONNAME;
   theHints[htype].ai_family   = AF_INET6;
   theHints[htype].ai_socktype = stype;
   return &theHints[htype];
}

/******************************************************************************/
/* Private:                        M a p 6 4                                  */
/******************************************************************************/

bool XrdNetAddr::Map64()
{

// The address must be a mapped IPV4 address
//
   if (!IN6_IS_ADDR_V4MAPPED(&IP.v6.sin6_addr)) return false;

// Now convert this down to an IPv4 address
//
   IP.v4.sin_addr.s_addr = IP.v6.sin6_addr.s6_addr32[3];
   IP.v4.sin_family      = AF_INET;
   protType              = PF_INET;
   addrSize              = sizeof(sockaddr_in);
   return true;
}

/******************************************************************************/
/*                                  P o r t                                   */
/******************************************************************************/

int XrdNetAddr::Port(int pNum)
{
// Make sure we have a proper address family here
//
   if (IP.Addr.sa_family != AF_INET && IP.Addr.sa_family != AF_INET6)
      return -1;

// Return port number if so wanted. The port location is the same regardless of
// the address family.
//
   if (pNum < 0) return ntohs(IP.v6.sin6_port);

// Set port number if we have a valid address. The location of the port
// is the same regardless of address family.
//
   if (pNum > 0xffff) return -1;
   IP.v6.sin6_port = htons(static_cast<short>(pNum));
   return pNum;
}
  
/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
const char *XrdNetAddr::Set(const char *hSpec, int pNum)
{
   static const char *badIPv4 = "invalid IPv4 address";
   static const char *badIPv6 = "invalid IPv6 address";
   static const char *badIP64 = "IPv6 address not IPv4 representable";
   static const char *badName = "invalid host name";
   static const int   map46ID = htonl(0x0000ffff);

   const char *Colon, *iP;
   char aBuff[NI_MAXHOST+INET6_ADDRSTRLEN];
   int  aLen, n;
   bool mapIt;

// Clear translation if set (note unixPipe & sockAddr are the same).
//
   if (hostName)             {free(hostName);  hostName = 0;}
   if (sockAddr != &IP.Addr) {delete unixPipe; sockAddr = &IP.Addr;}
   memset(&IP, 0, sizeof(IP));
   addrSize = sizeof(sockaddr_in6);

// Check for any address setting
//
   if (!hSpec)
      {if (useIPV4)
          {IP.v4.sin_family      = AF_INET;
           IP.v4.sin_addr.s_addr = INADDR_ANY;
           protType              = PF_INET;
           addrSize              = sizeof(sockaddr_in);
          } else {
           IP.v6.sin6_family     = AF_INET6;
           IP.v6.sin6_addr       = in6addr_any;
           protType              = PF_INET6;
          }
       if (pNum < 0) pNum= -pNum;
       IP.v6.sin6_port   = htons(static_cast<short>(pNum));
       return 0;
      }

// Check for Unix type address here
//
   if (*hSpec == '/')
      {if (strlen(hSpec) >= sizeof(unixPipe->sun_path)) return "path too long";
       unixPipe = new sockaddr_un;
       strcpy(unixPipe->sun_path, hSpec);
       unixPipe->sun_family = IP.Addr.sa_family = AF_UNIX;
       addrSize = sizeof(sockaddr_un);
       protType = PF_UNIX;
       return 0;
      }

// Do length check to see if we can fit the host name in our buffer.
//
   aLen = strlen(hSpec);
   if (aLen >= (int)sizeof(aBuff)) return "host id too long";

// Convert the address as appropriate. Note that we do accept RFC5156 deprecated
// IPV4 mapped IPV6 addresses(i.e. [::a.b.c.d]. This is historical.
//
        if (*hSpec == '[')
           {const char *Brak = index(hSpec+1, ']');
            if (!Brak) return badIPv6;
            Colon = Brak+1;
            if (!(*Colon)) Colon = 0;
               else if (*Colon != ':') return badIPv6;
            aLen = Brak - (hSpec+1);
            if (aLen >= INET6_ADDRSTRLEN) return badIPv6;
            mapIt =    (*(hSpec+1) == ':' && *(hSpec+2) == ':'
                     && *(hSpec+3) >= '0' && *(hSpec+3) <= '9'
                     && (iP = index(hSpec+4, '.')) && iP < Brak);
            strncpy(aBuff, hSpec+1, aLen); aBuff[aLen] = 0;
            if (inet_pton(AF_INET6,aBuff,&IP.v6.sin6_addr) != 1) return badIPv6;
            if (mapIt) IP.v6.sin6_addr.s6_addr32[2] = map46ID;
            IP.v6.sin6_family = AF_INET6;
            protType = PF_INET6;
            if (useIPV4 && !Map64()) return badIP64;
           }
   else if (*hSpec >= '0' && *hSpec <= '9')
           {if ((Colon = index(hSpec, ':')))
               {aLen = Colon - hSpec;
                if (aLen >= INET_ADDRSTRLEN) return badIPv4;
                strncpy(aBuff, hSpec, aLen); aBuff[aLen] = 0; iP = aBuff;
               } else iP = hSpec;
            if (inet_pton(AF_INET ,iP, &IP.v6.sin6_addr.s6_addr32[3]) != 1)
               return badIPv4;
            IP.v6.sin6_addr.s6_addr32[2] = map46ID;
            IP.v6.sin6_family = AF_INET6;
            protType = PF_INET6;
            if (useIPV4 && !Map64()) return badIPv4;
           }
   else if (*hSpec == 0) return badName;

   else    {struct addrinfo *rP = 0;
            if ((Colon = index(hSpec, ':')))
               {aLen = Colon - hSpec;
                if (aLen > MAXHOSTNAMELEN) return badName;
                strncpy(aBuff, hSpec, aLen); aBuff[aLen] = 0; iP = aBuff;
               } else iP = hSpec;
            n = getaddrinfo(iP, 0, hostHints, &rP);
            if (n || !rP)
               {if (rP) freeaddrinfo(rP);
                return (n ? gai_strerror(n) : "host not found");
               }
            memcpy(&IP.Addr, rP->ai_addr, rP->ai_addrlen);
            protType = (IP.v6.sin6_family == AF_INET6 ? PF_INET6 : PF_INET);
            freeaddrinfo(rP);
           }

// Now set the port number as needed (v4 & v6 port locations are the same)
//
   if (pNum == PortInSpec && !Colon) return "port not specified";
   if (pNum <= 0 && Colon)
      {char *eP;
       pNum = strtol(Colon+1, &eP, 10);
       if (pNum < 0 || pNum > 0xffff || *eP) return "invalid port number";
      } else if (pNum < 0) pNum = -pNum;
   IP.v6.sin6_port = htons(static_cast<short>(pNum));

// All done
//
   return 0;
}

/******************************************************************************/
  
const char *XrdNetAddr::Set(const char *hSpec, int &numIP, int maxIP,
                            int pNum, bool optUDP)
{
   struct addrinfo *hP, *rP = 0, *pP, *nP;
   XrdNetAddr *aVec = this;
   const char *hnBeg, *hnEnd, *pnBeg, *pnEnd;
   char hBuff[MAXHOSTNAMELEN+8];
   int  hLen, n;

// If only one address can be returned, just revert to standard processing
//
   if (!hSpec || !isalpha(*hSpec) || maxIP < 2)
      {const char *eMsg = Set(hSpec, pNum);
       numIP = (eMsg ? 0 : 1);
       return eMsg;
      }

// Extract out host name
//
   if (!XrdNetUtils::Parse(hSpec, &hnBeg, &hnEnd, &pnBeg, &pnEnd))
      return "invalid host specification";
   hLen = hnEnd - hnBeg;
   if (hLen > MAXHOSTNAMELEN) return "host name too long";
   strncpy(hBuff, hSpec, hLen); hBuff[hLen] = 0;

// Get the port number we will be setting
//
   if (pnBeg == hnEnd)
      {if (pNum == PortInSpec) return "port not specified";
       if (pNum < 0) pNum = -pNum;
      } else {
       if (*pnEnd || !(n = XrdNetUtils::ServPort(pnBeg, optUDP)))
          return "invalid port";
       if (pNum < 0) pNum = n;
      }

// Get all of the addresses
//
   hP = (optUDP ? huntHintsUDP : huntHintsTCP);
   n = getaddrinfo(hBuff, 0, hP, &rP);
   if (n || !rP)
      {if (rP) freeaddrinfo(rP);
       return (n ? gai_strerror(n) : "host not found");
      }

// Now self-referentially fill out ourselves with no duplicates
//
   n = 0; pP = 0; nP = rP;
   do {if (!pP || pP->ai_addrlen != nP->ai_addrlen
       ||  memcmp((const void *)pP->ai_addr, (const void *)nP->ai_addr,
                  nP->ai_addrlen)) {aVec[n].Set(nP, pNum); n++;}
       pP = nP; nP = nP->ai_next;
      } while(n < maxIP && nP);

// All done
//
   numIP = n;
   if (rP) freeaddrinfo(rP);
   return 0;
}

/******************************************************************************/

const char *XrdNetAddr::Set(const struct sockaddr *sockP, int sockFD)
{
// Clear translation if set
//
   if (hostName)             {free(hostName);  hostName = 0;}
   if (sockAddr != &IP.Addr) {delete unixPipe; sockAddr = &IP.Addr;}
   sockNum = sockFD;

// Copy the address based on address family
//
        if (sockP->sa_family == AF_INET6) addrSize = sizeof(IP.v6);
   else if (sockP->sa_family == AF_INET)  addrSize = sizeof(IP.v4);
   else if (sockP->sa_family == AF_UNIX)
           {unixPipe = new sockaddr_un;
            memcpy(unixPipe, sockP, sizeof(struct sockaddr_un));
            unixPipe->sun_path[sizeof(unixPipe->sun_path)-1] = 0;
            addrSize = sizeof(sockaddr_un);
            memset(&IP, 0, sizeof(IP));
            IP.Addr.sa_family = AF_UNIX;
            return 0;
           }
   else return "invalid address family";

// Copy the address and return
//
   memcpy(&IP, sockP, addrSize);
   return 0;
}

/******************************************************************************/
  
const char *XrdNetAddr::Set(int sockFD)
{

// Clear translation if set
//
   if (hostName)             {free(hostName);  hostName = 0;}
   if (sockAddr != &IP.Addr) {delete unixPipe; sockAddr = &IP.Addr;}
   addrSize = sizeof(sockaddr_in6);
   sockNum = sockFD;

// Get the address on the other side of this socket
//
   if (getpeername(sockFD, &IP.Addr, &addrSize) < 0)
      {addrSize = 0;
       return strerror(errno);
      }

// Set the correct address size
//
   addrSize = (IP.Addr.sa_family == AF_INET ? sizeof(sockaddr_in)
                                            : sizeof(sockaddr_in6));

// All done
//
   return 0;
}

/******************************************************************************/
  
const char *XrdNetAddr::Set(struct addrinfo *rP, int Port, bool mapit)
{
   static const int   map46ID = htonl(0x0000ffff);

// See if we need to convert this address otherwise just copy it
//
   if (mapit && rP->ai_family == AF_INET)
      {memset(&IP.Addr, 0, sizeof(IP.Addr));
       IP.v6.sin6_family = AF_INET6;
       memcpy(&IP.v6.sin6_addr.s6_addr32[3], (rP->ai_addr->sa_data)+2, 4);
       IP.v6.sin6_addr.s6_addr32[2] = map46ID;
       addrSize = sizeof(IP.v6);
       protType = PF_INET6;
      } else {
       memcpy(&IP.Addr, rP->ai_addr, rP->ai_addrlen);
       addrSize = rP->ai_addrlen;
       protType = rP->ai_protocol;
      }

// Cleanup pre-existing information
//
   if (hostName) free(hostName);
   hostName = (rP->ai_canonname ? strdup(rP->ai_canonname) : 0);
   if (sockAddr != &IP.Addr) {delete unixPipe; sockAddr = &IP.Addr;}
   IP.v6.sin6_port = htons(static_cast<short>(Port));
   sockNum = -1;
   return 0;
}

/******************************************************************************/
/*                              S e t C a c h e                               */
/******************************************************************************/
  
void XrdNetAddr::SetCache(int keeptime)
{
   static XrdNetCache theCache;

// Set the cache keep time
//
   theCache.SetKT(keeptime);
   dnsCache = (keeptime > 0 ? &theCache : 0);
}

/******************************************************************************/
/*                               S e t I P V 4                                */
/******************************************************************************/
  
void XrdNetAddr::SetIPV4()
{

// To force IPV4 mode we merely change the hints structure and set the IPV4
// mode flag to reject IPV6 address unless they are mapped.
//
   hostHints->ai_flags    = AI_CANONNAME;
   hostHints->ai_family   = AF_INET;

   huntHintsTCP->ai_flags    = AI_ADDRCONFIG;
   huntHintsTCP->ai_family   = AF_INET;

   huntHintsUDP->ai_flags    = AI_ADDRCONFIG;
   huntHintsUDP->ai_family   = AF_INET;

   useIPV4 = true;
}
