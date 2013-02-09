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
  
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddrInfo.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdNetCache            XrdNetAddrInfo::dnsCache;

XrdNetAddrInfo::fmtUse XrdNetAddrInfo::useFmt = XrdNetAddrInfo::fmtName;

/******************************************************************************/
/*                                F o r m a t                                 */
/******************************************************************************/
  
int XrdNetAddrInfo::Format(char *bAddr, int bLen, fmtUse theFmt, int fmtOpts)
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
/*                            i s L o o p b a c k                             */
/******************************************************************************/
  
bool XrdNetAddrInfo::isLoopback()
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
  
bool XrdNetAddrInfo::isRegistered()
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
  
char *XrdNetAddrInfo::LowCase(char *str)
{
   char *sp = str;

   while(*sp) {if (isupper((int)*sp)) *sp = (char)tolower((int)*sp); sp++;}

   return str;
}
  
/******************************************************************************/
/*                                  N a m e                                   */
/******************************************************************************/
  
const char *XrdNetAddrInfo::Name(const char *eName, const char **eText)
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
/*                                  P o r t                                   */
/******************************************************************************/

int XrdNetAddrInfo::Port()
{
// Make sure we have a proper address family here
//
   if (IP.addr.sa_family != AF_INET && IP.addr.sa_family != AF_INET6)
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
  
int XrdNetAddrInfo::Same(const XrdNetAddrInfo *ipAddr, bool plusPort)
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
