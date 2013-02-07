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
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddr.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdNetCache        XrdNetAddr::dnsCache;

struct addrinfo    XrdNetAddr::hostHints = {AI_CANONNAME|AI_V4MAPPED,PF_INET6};
                                                  

XrdNetAddr::fmtUse XrdNetAddr::useFmt = XrdNetAddr::fmtName;

/******************************************************************************/
/*                                F o r m a t                                 */
/******************************************************************************/
  
int XrdNetAddr::Format(char *bAddr, int bLen, fmtUse theFmt, int fmtOpts)
{
   const char *pFmt = "]:%d";
   int totLen, n, pNum, addBrak = 0, omitP = (fmtOpts & noPort);

// Handle the degenerative case first
//
   if (IP.addr.sa_family == AF_UNIX)
      {n = (omitP ? snprintf(bAddr, bLen, "localhost")
                  : snprintf(bAddr, bLen, "localhost:%s", unixPipe->sun_path));
       return (n < bLen ? n : QFill(bAddr, bLen));
      }

// Preset defaults (note that the port number is the same position and
// same size regardless of address type).
//
   if (theFmt == fmtDflt) theFmt = useFmt;
   pNum = ntohs(IP.v4.sin_port);

// Resolve address if need be and return result if possible
//
   if (theFmt == fmtName || theFmt == fmtAuto)
      {if (!hostName && !(hostName = dnsCache.Find(this)) && theFmt == fmtName)
          Resolve();
       if (hostName)
          {n = (omitP ? snprintf(bAddr, bLen, "%s",    hostName)
                      : snprintf(bAddr, bLen, "%s:%d", hostName, pNum));
           return (n < bLen ? n : QFill(bAddr, bLen));
          }
       theFmt = fmtAddr;
      }

// Check if we can now produce an address format quickly
//
   if (hostName && !isalpha(*hostName))
          {n = (omitP ? snprintf(bAddr, bLen, "%s",    hostName)
                      : snprintf(bAddr, bLen, "%s:%d", hostName, pNum));
       return (n < bLen ? n : QFill(bAddr, bLen));
      }

// Format address
//
        if (IP.addr.sa_family == AF_INET6)
           {if (bLen < (INET6_ADDRSTRLEN+2)) return QFill(bAddr, bLen);
            *bAddr = '['; addBrak = 1;
            if (fmtOpts & old6Map4 && IN6_IS_ADDR_V4MAPPED(&IP.v6.sin6_addr))
               {strcpy(bAddr, "[::");
                if (!inet_ntop(AF_INET, &IP.v6.sin6_addr.s6_addr32[3],
                               bAddr+3, bLen-3)) return QFill(bAddr, bLen);
               } else if (!inet_ntop(AF_INET6,&(IP.v6.sin6_addr),bAddr+1,bLen-1))
                         return QFill(bAddr, bLen);
           }
   else if (IP.addr.sa_family == AF_INET)
           {if (theFmt != fmtAdv6) {n = 0; pFmt =  ":%d";}
               else {if (bLen < (INET_ADDRSTRLEN+9)) return QFill(bAddr, bLen);
                     if (fmtOpts & old6Map4) {strcpy(bAddr, "[::"); n = 3;}
                        else {strcpy(bAddr, "[::ffff:"); n = 8;}
                    }
            if (!inet_ntop(AF_INET, &(IP.v4.sin_addr),bAddr+n,bLen-n))
               return QFill(bAddr, bLen);
           }
   else return QFill(bAddr, bLen);

// Recalculate buffer position and length
//
   totLen = strlen(bAddr); bAddr += totLen; bLen -= totLen;

// Process when no port number wanted
//
   if (omitP)
      {if (addBrak)
          {if (bLen < 2) return QFill(bAddr, bLen);
           *bAddr++ = ']'; *bAddr = 0; totLen++;
          }
       return totLen;
      }

// Add the port number and return result
//
   if ((n = snprintf(bAddr, bLen, pFmt, pNum)) >= bLen)
      return QFill(bAddr, bLen);
   return totLen+n;
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/
  
void XrdNetAddr::Init(struct addrinfo *rP, int Port)
{

// Simply copy over the information we have
//
   memcpy(&IP.addr, rP->ai_addr, rP->ai_addrlen);
   addrSize = rP->ai_addrlen;
   if (hostName) free(hostName);
   hostName = (rP->ai_canonname ? strdup(rP->ai_canonname) : 0);
   if (sockAddr != &IP.addr) {delete unixPipe; sockAddr = &IP.addr;}
   IP.v6.sin6_port = htons(static_cast<short>(Port));
}

/******************************************************************************/
/*                            i s L o o p b a c k                             */
/******************************************************************************/
  
bool XrdNetAddr::isLoopback()
{
   static const char lbVal[13] ={0,0,0,0,0,0,0,0,0,0,0,0,0x7f};

// Check for loopback address
//
   if (IP.addr.sa_family == AF_INET)
      return !memcmp(&IP.v4.sin_addr.s_addr, &lbVal[12], 1);

   if (IP.addr.sa_family == AF_INET6)
      return !memcmp(&IP.v6.sin6_addr, &in6addr_loopback, sizeof(in6_addr))
          || !memcmp(&IP.v6.sin6_addr,  lbVal,            sizeof(lbVal));

   return false;
}

/******************************************************************************/
/*                          i s R e g i s t e r e d                           */
/******************************************************************************/
  
bool XrdNetAddr::isRegistered()
{
   const char *hName;

// Simply see if we can resolve this name
//
   if (!(hName = Name())) return false;
   return isalpha(*hName);
}
  
/******************************************************************************/
/* Private:                      L o w C a s e                                */
/******************************************************************************/
  
char *XrdNetAddr::LowCase(char *str)
{
   char *sp = str;

   while(*sp) {if (isupper((int)*sp)) *sp = (char)tolower((int)*sp); sp++;}

   return str;
}

/******************************************************************************/
/*                                  N a m e                                   */
/******************************************************************************/
  
const char *XrdNetAddr::Name(const char *eName, const char **eText)
{
   char buff[INET6_ADDRSTRLEN+8];
   void *aP;
   int n, rc, family;

// Preset errtxt to zero
//
   if (eText) *eText = 0;

// Check for unix family which is equal to localhost.
//
  if (IP.addr.sa_family == AF_UNIX) return "localhost";

// If we already translated this name, just return the translation
//
   if (hostName || (hostName = dnsCache.Find(this))) return hostName;

// Try to resolve this address
//
   if (!(rc = Resolve())) return hostName;

// We failed resolving this address
//
   if (eText) *eText = gai_strerror(rc);
   return eName;
}

/******************************************************************************/
/*                               N a m e D u p                                */
/******************************************************************************/
  
char *XrdNetAddr::NameDup(const char **eText)
{
   return strdup(Name("0.0.0.0"));
}

/******************************************************************************/
/*                                  P o r t                                   */
/******************************************************************************/

int XrdNetAddr::Port(int pNum)
{
// Make sure we have a proper address family here
//
   if (IP.addr.sa_family != AF_INET && IP.addr.sa_family != AF_INET6)
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
/* Private:                        Q F i l l                                  */
/******************************************************************************/
  
int XrdNetAddr::QFill(char *bAddr, int bLen)
{
   static const char quests[] = "????????";

// Insert up to 8 question marks
//
   if (bLen)
      {strncpy(bAddr, quests, bLen);
       bAddr[bLen-1] = 0;
      }
   return 0;
}

/******************************************************************************/
/* Private:                      R e s o l v e                                */
/******************************************************************************/

int XrdNetAddr::Resolve()
{
   char hBuff[NI_MAXHOST];
   int n, rc;

// Free up hostname here
//
   if (hostName) {free(hostName); hostName = 0;}

// Determine the actual size of the address structure
//
        if (IP.addr.sa_family == AF_INET ) n = sizeof(IP.v4);
   else if (IP.addr.sa_family == AF_INET6) n = sizeof(IP.v6);
   else if (IP.addr.sa_family == AF_UNIX )
           {hostName = strdup("localhost");
            return 0;
           }
   else return EAI_FAMILY;

// Do lookup of canonical name. Note that under certain conditions (e.g. dns
// failure) we can get a negative return code that may cause a SEGV. We check.
// Additionally, some implementations of getnameinfo() return the scopeid when
// a numeric address is returned. We check and remove it.
//
   if ((rc = getnameinfo(&IP.addr, n, hBuff+1, sizeof(hBuff)-2, 0, 0, 0)))
      {if (rc < 0) {errno = ENETUNREACH; rc = EAI_SYSTEM;}
       return rc;
      }

// Handle the case when the mapping returned an actual name or an address
// We always want numeric ipv6 addresses surrounded by brackets.
//
        if (isalpha(hBuff[1]))    hostName = strdup(LowCase(hBuff+1));
   else if (!index(hBuff+1, ':')) hostName = strdup(hBuff+1);
   else {char *perCent = index(hBuff+1, '%');
         if (perCent) *perCent = 0;
         n = strlen(hBuff+1);
         hBuff[0] = '['; hBuff[n+1] = ']'; hBuff[n+2] = 0;
         hostName = strdup(hBuff);
        }

// Add the entry to the cache and return success
//
   dnsCache.Add(this, hostName);
   return 0;
}
  
/******************************************************************************/
/*                                  S a m e                                   */
/******************************************************************************/
  
int XrdNetAddr::Same(const XrdNetAddr *ipAddr, bool plusPort)
{

// Both address families must match
//
  if (IP.addr.sa_family != ipAddr->IP.addr.sa_family) return 0;

// Now process to do the match
//
        if (IP.addr.sa_family == AF_INET)
           {if (memcmp(&IP.v4.sin_addr,  &(ipAddr->IP.v4.sin_addr),
                       sizeof(IP.v4.sin_addr))) return 0;
            return (plusPort ? IP.v4.sin_port  == ipAddr->IP.v4.sin_port  : 1);
           }
   else if (IP.addr.sa_family == AF_INET6)
           {if (memcmp(&IP.v6.sin6_addr, &(ipAddr->IP.v6.sin6_addr),
                       sizeof(IP.v6.sin6_addr))) return 0;
            return (plusPort ? IP.v6.sin6_port == ipAddr->IP.v6.sin6_port : 1);
           }
   else if (IP.addr.sa_family == AF_UNIX)
           return !strcmp(unixPipe->sun_path, ipAddr->unixPipe->sun_path);

   return 0;
}

/******************************************************************************/
/*                                  S e l f                                   */
/******************************************************************************/
  
const char *XrdNetAddr::Self(int pNum)
{
   const char *etext;
   char buff[1024];

// Get our host name and initialize this object with it
//
   gethostname(buff, sizeof(buff));
   return Set(buff, pNum);
}
  
/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
const char *XrdNetAddr::Set(const char *hSpec, int pNum)
{
   static const char *badIPv4 = "invalid IPv4 address";
   static const char *badIPv6 = "invalid IPv6 address";
   static const char *badName = "invalid host name";
   static const int   map46ID = htonl(0x0000ffff);

   const char *Colon, *iP;
   char aBuff[NI_MAXHOST+INET6_ADDRSTRLEN];
   int  aLen, n;

// Clear translation if set (note unixPipe & sockAddr are the same).
//
   if (hostName)             {free(hostName);  hostName = 0;}
   if (sockAddr != &IP.addr) {delete unixPipe; sockAddr = &IP.addr;}
   memset(&IP, 0, sizeof(IP));
   addrSize = sizeof(sockaddr_in6);

// Check for any address setting
//
   if (!hSpec)
      {IP.v6.sin6_family = AF_INET6;
       IP.v6.sin6_addr   = in6addr_any;
       if (pNum < 0) pNum= -pNum;
       IP.v6.sin6_port   = htons(static_cast<short>(pNum));
       protType          = PF_INET6;
       return 0;
      }

// Check for Unix type address here
//
   if (*hSpec == '/')
      {if (strlen(hSpec) >= sizeof(unixPipe->sun_path)) return "path too long";
       unixPipe = new sockaddr_un;
       strcpy(unixPipe->sun_path, hSpec);
       unixPipe->sun_family = IP.addr.sa_family = AF_UNIX;
       addrSize = sizeof(sockaddr_un);
       protType = PF_UNIX;
       return 0;
      }

// Do length check to see if we can fit the host name in our buffer.
//
   aLen = strlen(hSpec);
   if (aLen >= sizeof(aBuff)) return "host id too long";

// Convert the address as appropriate
//
        if (*hSpec == '[')
           {const char *Brak = index(hSpec+1, ']');
            if (!Brak) return badIPv6;
            Colon = Brak+1;
            if (!(*Colon)) Colon = 0;
               else if (*Colon != ':') return badIPv6;
            aLen = Brak - (hSpec+1);
            if (aLen >= INET6_ADDRSTRLEN) return badIPv6;
            strncpy(aBuff, hSpec+1, aLen); aBuff[aLen] = 0;
            if (inet_pton(AF_INET6,aBuff,&IP.v6.sin6_addr) != 1) return badIPv6;
            IP.v6.sin6_family = AF_INET6;
            protType = PF_UNIX;
           }
   else if (*hSpec >= '0' && *hSpec <= '9')
           {if ((Colon = index(hSpec, ':')))
               {aLen = Colon - hSpec;
                if (aLen >= INET_ADDRSTRLEN) return badIPv4;
                strncpy(aBuff, hSpec, aLen); aBuff[aLen] = 0; iP = aBuff;
               } else iP = hSpec;
            if (inet_pton(AF_INET ,iP, &IP.v6.sin6_addr.s6_addr32[3]) != 1)
               return badIPv4;
            IP.v6.sin6_addr.s6_addr32[3] = map46ID;
            IP.v6.sin6_family = AF_INET6;
            protType = PF_INET6;
           }
   else if (*hSpec == 0) return badName;

   else    {struct addrinfo *rP = 0;
            if ((Colon = index(hSpec, ':')))
               {aLen = Colon - hSpec;
                if (aLen > MAXHOSTNAMELEN) return badName;
                strncpy(aBuff, hSpec, aLen); aBuff[aLen] = 0; iP = aBuff;
               } else iP = hSpec;
            n = getaddrinfo(iP, 0, &hostHints, &rP);
            if (n || !rP)
               {if (rP) freeaddrinfo(rP);
                return (n ? gai_strerror(n) : "host not found");
               }
            memcpy(&IP.addr, rP->ai_addr, rP->ai_addrlen);
            protType = (IP.v6.sin6_family == AF_INET6 ? PF_INET6 : PF_INET);
            if (hostName) free(hostName);
            hostName = strdup(rP->ai_canonname);
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
  
const char *XrdNetAddr::Set(const char *hSpec,int &numIP,int maxIP,int pNum)
{
   struct addrinfo *rP = 0, *pP, *nP, myhints = {AI_CANONNAME};
   XrdNetAddr *aVec = this;
   const char *Colon, *iP;
   char aBuff[MAXHOSTNAMELEN+8];
   int  aLen, n;

// If only one address can be returned, just revert to standard processing
//
   if (!hSpec || !isalpha(*hSpec) || maxIP < 2)
      {const char *eMsg = Set(hSpec, pNum);
       numIP = (eMsg ? 0 : 1);
       return eMsg;
      }

// Extract out host name
//
   if ((Colon = index(hSpec, ':')))
      {aLen = Colon - hSpec;
       if (aLen > MAXHOSTNAMELEN) return "invalid host name";
       strncpy(aBuff, hSpec, aLen); aBuff[aLen] = 0; iP = aBuff;
      } else iP = hSpec;

// Get the port number we will be setting
//
   if (pNum == PortInSpec && !Colon) return "port not specified";
   if (pNum <= 0 && Colon)
      {char *eP;
       pNum = strtol(Colon+1, &eP, 10);
       if (pNum < 0 || pNum > 0xffff || *eP) return "invalid port number";
      } else if (pNum < 0) pNum = -pNum;

// Get all of the addresses
//
// n = getaddrinfo(iP, 0, (const addrinfo *)&myhints, &rP);
   n = getaddrinfo(iP, 0, (const addrinfo *)0,        &rP);
   if (n || !rP) return gai_strerror(n);
      {if (rP) freeaddrinfo(rP);
       return (n ? gai_strerror(n) : "host not found");
      }

// Now self-referentially fill out ourselves with no duplicates
//
   n = 0; pP = 0; nP = rP;
   do {if (!pP || pP->ai_addrlen != nP->ai_addrlen
       ||  memcmp((const void *)pP->ai_addr, (const void *)nP->ai_addr,
                  nP->ai_addrlen)) {aVec[n].Init(nP, pNum); n++;}
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
   if (sockAddr != &IP.addr) {delete unixPipe; sockAddr = &IP.addr;}
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
            IP.addr.sa_family = AF_UNIX;
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
   if (sockAddr != &IP.addr) {delete unixPipe; sockAddr = &IP.addr;}
   addrSize = sizeof(sockaddr_in6);
   sockNum = sockFD;

// Get the address on the other side of this socket
//
   if (getpeername(sockFD, &IP.addr, &addrSize) < 0) return strerror(errno);

// All done
//
   return 0;
}
