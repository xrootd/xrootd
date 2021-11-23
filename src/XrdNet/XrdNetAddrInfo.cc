/******************************************************************************/
/*                                                                            */
/*                     X r d N e t A d d r I n f o . c c                      */
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
  
#include <cctype>
#include <cerrno>
#include <netdb.h>
#include <cstdio>
#include <arpa/inet.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdNet/XrdNetCache.hh"

/******************************************************************************/
/*                 P l a t f o r m   D e p e n d e n c i e s                  */
/******************************************************************************/
  
// Linux defines s6_addr32 but MacOS does not and Solaris defines it only when
// compiling the kernel. This is really standard stuff that should be here.
//
#ifndef s6_addr32
#if   defined(__solaris__)
#define s6_addr32 _S6_un._S6_u32
#elif defined(__APPLE__) || defined(__FreeBSD__)
#define s6_addr32 __u6_addr.__u6_addr32
#endif
#endif

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdNetCache           *XrdNetAddrInfo::dnsCache = 0;

namespace
{
   static const char lbVal[13] ={0,0,0,0,0,0,0,0,0,0,0,0,0x7f};
};

/******************************************************************************/
/*                                F o r m a t                                 */
/******************************************************************************/
  
int XrdNetAddrInfo::Format(char *bAddr, int bLen, fmtUse theFmt, int fmtOpts)
{
   const char *pFmt = "]:%d";
   int totLen, n, pNum, addBrak = 0;
   int omitP = (fmtOpts & (noPort|noPortRaw));
   int ipRaw = (fmtOpts & noPortRaw);
   int ipOld = fmtOpts & (old6Map4 | prefipv4);

// Handle the degenerative case first
//
   if (IP.Addr.sa_family == AF_UNIX)
      {n = (omitP ? snprintf(bAddr, bLen, "localhost")
                  : snprintf(bAddr, bLen, "localhost:%s", unixPipe->sun_path));
       return (n < bLen ? n : QFill(bAddr, bLen));
      }

// Grab the potr. The port number is the same position and same size regardless
// of address type.
//
   pNum = ntohs(IP.v4.sin_port);

// Resolve address if need be and return result if possible
//
   if (theFmt == fmtName || theFmt == fmtAuto)
      {if (!hostName && dnsCache && !(hostName = dnsCache->Find(this))
       &&  theFmt == fmtName) Resolve();
       if (hostName)
          {n = (omitP ? snprintf(bAddr, bLen, "%s",    hostName)
                      : snprintf(bAddr, bLen, "%s:%d", hostName, pNum));
           return (n < bLen ? n : QFill(bAddr, bLen));
          }
       theFmt = fmtAddr;
      }

// Check if we can now produce an address format quickly. We used assume that
// the hostname was an address if it started with a non-alpha. But this does
// not correspond to RFC 1178 and RFC 3696, among others. We now only use
// the optimization path if the name is actually an IPV6/mapped4 address.
//
   if (hostName && *hostName == '[' && !ipOld)
          {n = (omitP ? snprintf(bAddr, bLen, "%s",    hostName)
                      : snprintf(bAddr, bLen, "%s:%d", hostName, pNum));
       return (n < bLen ? n : QFill(bAddr, bLen));
      }

// Format address
//
        if (IP.Addr.sa_family == AF_INET6)
           {if (bLen < (INET6_ADDRSTRLEN+2)) return QFill(bAddr, bLen);
            if (ipOld && IN6_IS_ADDR_V4MAPPED(&IP.v6.sin6_addr))
               {     if (fmtOpts & prefipv4)  {n = 0; pFmt = ":%d";}
                else if (ipRaw) {strcpy(bAddr,  "::"); n = 2;}
                else    {strcpy(bAddr, "[::"); n = 3; addBrak=1;}
                if (!inet_ntop(AF_INET, &IP.v6.sin6_addr.s6_addr32[3],
                               bAddr+n, bLen-n)) return QFill(bAddr, bLen);
               } else {
                if (!ipRaw) {*bAddr = '['; n = 1; addBrak = 1;}
                   else n = 0;
                if (!inet_ntop(AF_INET6,&(IP.v6.sin6_addr),bAddr+n,bLen-n))
                    return QFill(bAddr, bLen);
               }
           }
   else if (IP.Addr.sa_family == AF_INET)
           {if (theFmt != fmtAdv6) {n = 0; pFmt =  ":%d";}
               else {if (bLen < (INET_ADDRSTRLEN+9)) return QFill(bAddr, bLen);
                     if (fmtOpts & old6Map4) {strcpy(bAddr, "[::"); n = 3;}
                        else {strcpy(bAddr, "[::ffff:"); n = 8;}
                     if (ipRaw) {strcpy(bAddr, bAddr+1); n--;}
                     addBrak = 1;
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
/*                            i s L o o p b a c k                             */
/******************************************************************************/
  
bool XrdNetAddrInfo::isLoopback()
{

// Check for loopback address
//
   if (IP.Addr.sa_family == AF_INET)
      return !memcmp(&IP.v4.sin_addr.s_addr, &lbVal[12], 1);

   if (IP.Addr.sa_family == AF_INET6)
      return !memcmp(&IP.v6.sin6_addr, &in6addr_loopback, sizeof(in6_addr))
          || !memcmp(&IP.v6.sin6_addr,  lbVal,            sizeof(lbVal));

   return false;
}
  
/******************************************************************************/
/*                             i s P r i v a t e                              */
/******************************************************************************/
  
bool XrdNetAddrInfo::isPrivate()
{
   unsigned char *ipV4 = 0;

// For IPV6 addresses we will use the macro unless it is mapped
//
   if (IP.Addr.sa_family == AF_INET6)
      {if ((IN6_IS_ADDR_V4MAPPED(&IP.v6.sin6_addr)))
          ipV4 = (unsigned char *)&IP.v6.sin6_addr.s6_addr32[3];
          else  {if ((IN6_IS_ADDR_LINKLOCAL(&IP.v6.sin6_addr))
                 ||  (IN6_IS_ADDR_SITELOCAL(&IP.v6.sin6_addr))
                 ||  (IN6_IS_ADDR_LOOPBACK (&IP.v6.sin6_addr))) return true;
                 return false;
                }
      }

// If this is not an IPV4 address then we will consider it private
//
   if (!ipV4)
      {if (IP.Addr.sa_family != AF_INET) return true;
       ipV4 = (unsigned char *)&IP.v4.sin_addr.s_addr;
      }

// For IPV4 we use the RFC definition of private. Note that this includes
// mapped addresses which, as odd as it is, we could get.
//
   if (ipV4[0] ==  10
   || (ipV4[0] == 172 && ipV4[1] >=  16 && ipV4[1] <= 31)
   || (ipV4[0] == 192 && ipV4[1] == 168)
   || (ipV4[0] == 169 && ipV4[1] == 254)
   ||  ipV4[0] == 127) return true;

// Not a local address
//
   return false;
}

/******************************************************************************/
/*                          i s R e g i s t e r e d                           */
/******************************************************************************/
  
bool XrdNetAddrInfo::isRegistered()
{
   const char *hName;

// Simply see if we can resolve this name
//
   if (!(hName = Name())) return false;
   return isHostName(hName);
}
  
/******************************************************************************/
/*                            i s U s i n g T L S                             */
/******************************************************************************/
  
bool XrdNetAddrInfo::isUsingTLS()
{
   return (protFlgs & isTLS) != 0;
}
  
/******************************************************************************/
/* Private:                      L o w C a s e                                */
/******************************************************************************/
  
char *XrdNetAddrInfo::LowCase(char *str)
{
   char *sp = str;

   while(*sp) {if (isupper((int)*sp)) *sp = (char)tolower((int)*sp); sp++;}

   return str;
}
  
/******************************************************************************/
/*                            i s H o s t N a m e                             */
/******************************************************************************/

bool XrdNetAddrInfo::isHostName(const char *name)
{
   const char *dot;
   int dnum;

// First check for Iv6 format or hostname
//
   if (*name == '[')    return false;
   if (!isdigit(*name)) return true;

// The IPv4 case is more complicated. The fastest way here is this is a
// host name if there are no dots or if the last component is not a digit
// according to the RFC spec.
//
   if (!(dot = rindex(name, '.')) || !isdigit(*(dot+1))) return true;

// We are not out of the woods yet. Now we need to do a full check.
//
   name++; dnum = 0;
   while(*name)
        {if (*name == '.') dnum++;
            else if (!isdigit(*name)) return true;
         name++;
        }
   return (dnum == 3 ? false : true);
}
  
/******************************************************************************/
/*                                  N a m e                                   */
/******************************************************************************/
  
const char *XrdNetAddrInfo::Name(const char *eName, const char **eText)
{
   int rc;

// Preset errtxt to zero
//
   if (eText) *eText = 0;

// Check for unix family which is equal to localhost.
//
  if (IP.Addr.sa_family == AF_UNIX) return "localhost";

// If we already translated this name, just return the translation
//
   if (hostName || (dnsCache && (hostName = dnsCache->Find(this))))
      return hostName;

// Try to resolve this address
//
   if (!(rc = Resolve())) return hostName;

// We failed resolving this address
//
   if (eText) *eText = gai_strerror(rc);
   return eName;
}

/******************************************************************************/
/*                                  P o r t                                   */
/******************************************************************************/

int XrdNetAddrInfo::Port()
{
// Make sure we have a proper address family here
//
   if (IP.Addr.sa_family != AF_INET && IP.Addr.sa_family != AF_INET6)
      return -1;

// Return port number
//
   return ntohs(IP.v6.sin6_port);
}

/******************************************************************************/
/* Private:                        Q F i l l                                  */
/******************************************************************************/
  
int XrdNetAddrInfo::QFill(char *bAddr, int bLen)
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

int XrdNetAddrInfo::Resolve()
{
   char hBuff[NI_MAXHOST];
   int n, rc;

// Free up hostname here
//
   if (hostName) {free(hostName); hostName = 0;}

// Determine the actual size of the address structure
//
        if (IP.Addr.sa_family == AF_INET ) n = sizeof(IP.v4);
   else if (IP.Addr.sa_family == AF_INET6) n = sizeof(IP.v6);
   else if (IP.Addr.sa_family == AF_UNIX )
           {hostName = strdup("localhost");
            return 0;
           }
   else return EAI_FAMILY;

// Do lookup of canonical name. If an error is returned we simply assume that
// the name is not resolvable and return the address as the host name.
//
   if ((rc = getnameinfo(&IP.Addr, n, hBuff+1, sizeof(hBuff)-2, 0, 0, 0)))
      {int ec = errno;
       if (Format(hBuff, sizeof(hBuff), fmtAddr, noPort))
          {hostName = strdup(hBuff); return 0;}
       errno = ec;
       return rc;
      }

// Handle the case when the mapping returned an actual name or an address
// We always want numeric ipv6 addresses surrounded by brackets. Additionally,
// some implementations of getnameinfo() return the scopeid when a numeric
// address is returned. We check and remove it.
//
        if (!index(hBuff+1, ':')) hostName = strdup(LowCase(hBuff+1));
   else {char *perCent = index(hBuff+1, '%');
         if (perCent) *perCent = 0;
         n = strlen(hBuff+1);
         hBuff[0] = '['; hBuff[n+1] = ']'; hBuff[n+2] = 0;
         hostName = strdup(hBuff);
        }

// Add the entry to the cache and return success
//
   if (dnsCache) dnsCache->Add(this, hostName);
   return 0;
}
  
/******************************************************************************/
/*                                  S a m e                                   */
/******************************************************************************/

#define IS_INET(x) (x == AF_INET || x == AF_INET6)

#define MY_FAMILY IP.Addr.sa_family

#define UR_FAMILY ipAddr->IP.Addr.sa_family

int XrdNetAddrInfo::Same(const XrdNetAddrInfo *ipAddr, bool plusPort)
{
   static const int ipv4ASZ = sizeof(IP.v4.sin_addr);

   bool isINet = IS_INET(MY_FAMILY) && IS_INET(UR_FAMILY);

// Do port comparison if requested and makes sense to do it
//
   if (plusPort && isINet)
      {in_port_t port1, port2;
       port1 = (MY_FAMILY == AF_INET ? IP.v4.sin_port : IP.v6.sin6_port);
       port2 = (UR_FAMILY == AF_INET ? ipAddr->IP.v4.sin_port
                                     : ipAddr->IP.v6.sin6_port);
       if (port1 != port2) return 0;
      }

// If address families do not match, they are the same if the hostnames match.
// If we don't have a hostname and the addresses are convertable, we can
// compare the actual addresses.
//
   if (MY_FAMILY != UR_FAMILY)
      {if (!isINet) return 0;
       if (hostName && ipAddr->hostName)
          return !strcmp(hostName,ipAddr->hostName);
       if (MY_FAMILY == AF_INET && ipAddr->isMapped())
          return !memcmp(&IP.v4.sin_addr,
                         &(ipAddr->IP.v6.sin6_addr.s6_addr32[3]), ipv4ASZ);
       if (isMapped() && UR_FAMILY == AF_INET)
          return !memcmp(&IP.v6.sin6_addr.s6_addr32[3],
                         &(ipAddr->IP.v4.sin_addr), ipv4ASZ);
       return 0;
      }
 
// Now process to do the match
//
        if (MY_FAMILY == AF_INET)
           return !memcmp(&IP.v4.sin_addr, &(ipAddr->IP.v4.sin_addr), ipv4ASZ);
   else if (MY_FAMILY == AF_INET6)
           return !memcmp(&IP.v6.sin6_addr, &(ipAddr->IP.v6.sin6_addr),
                          sizeof(IP.v6.sin6_addr));
   else if (MY_FAMILY == AF_UNIX)
           return !strcmp(unixPipe->sun_path, ipAddr->unixPipe->sun_path);

   return 0;
}
