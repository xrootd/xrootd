/******************************************************************************/
/*                                                                            */
/*                           X r d N e t I F . c c                            */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_GETIFADDRS
#include <net/if.h>
#include <ifaddrs.h>
#endif

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"

#include <iostream>
using namespace std;
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysError      *XrdNetIF::eDest     = 0;

char             *XrdNetIF::myDomain  = XrdNetIF::SetDomain();

char             *XrdNetIF::ifCfg[2]  = {0,0};

XrdNetIF::netType XrdNetIF::netRoutes = XrdNetIF::netLocal;

int               XrdNetIF::dfPort    = 1094;

XrdNetIF::ifData  XrdNetIF::ifNull;

/******************************************************************************/
/* Private:                     G e n A d d r s                               */
/******************************************************************************/
  
bool XrdNetIF::GenAddrs(ifAddrs &ifTab, XrdNetAddrInfo *src,
                                        const char     *hName, bool isPVT)
{
   static const int noPort  = XrdNetAddr::noPort;
   static const int old6M4  = XrdNetAddr::old6Map4;
   int n;

// If this is an IPV4 address, then format as it
//
   ifTab.ipV6 = false;
   if (src->isIPType(XrdNetAddrInfo::IPv4))
      {if (!(ifTab.hALen = src->Format(ifTab.hAddr,  sizeof(ifTab.hAddr),
                                XrdNetAddr::fmtAddr, noPort))
       ||  !(ifTab.hDLen = src->Format(ifTab.hDest,  sizeof(ifTab.hDest),
                                XrdNetAddr::fmtAdv6, old6M4))) return false;
       return true;
      }

// If this is a mapped address then we can easily generate the IPV4 version
// and the destination address is the deprecated IPV6 address.
//
   if (src->isMapped())
      {char *Colon;
       if (!src->Format(ifTab.hAddr,  sizeof(ifTab.hAddr),
                 XrdNetAddr::fmtAdv6, noPort))  return false;
       if (!(Colon = rindex(ifTab.hAddr, ':'))) return false;
       n = strlen(Colon+1);
       memmove(ifTab.hAddr,Colon+1,n); ifTab.hAddr[n-1] = 0; ifTab.hALen = n-1;
       if (!(ifTab.hDLen = src->Format(ifTab.hDest, sizeof(ifTab.hDest),
                                XrdNetAddr::fmtAdv6, old6M4))) return false;
       return true;
      }

// This is a true IPV6 address, we must check if there is an IPV4 interface.
// This will only work if we have a host name. We preferentially use IPV4
// addresses for backward compatability.
//
   if (hName)
      {XrdNetAddr *iP;
       int i, iN, p = src->Port();
       bool aOK = true;
       if (!XrdNetUtils::GetAddrs(hName,&iP,iN,XrdNetUtils::onlyIPv4, p) && iN)
          {for (i = 0; i < iN; i++) {if (!(isPVT ^ iP[i].isPrivate())) break;}
           if (i < iN)
              {if (!(ifTab.hALen = iP[i].Format(ifTab.hAddr,sizeof(ifTab.hAddr),
                                   XrdNetAddr::fmtAddr, noPort))
               ||  !(ifTab.hDLen = iP[i].Format(ifTab.hDest,sizeof(ifTab.hDest),
                                   XrdNetAddr::fmtAdv6, old6M4))) aOK = false;
               delete [] iP;
               return aOK;
              }
          }
      }

// There is no IPV4 address so use pure IPV6.
//
   ifTab.ipV6 = true;
   if (!(ifTab.hALen = src->Format(ifTab.hAddr,  sizeof(ifTab.hAddr),
                            XrdNetAddr::fmtAddr, noPort))
   ||  !(ifTab.hDLen = src->Format(ifTab.hDest,  sizeof(ifTab.hDest),
                            XrdNetAddr::fmtAdv6, old6M4))) return false;
   return true;
}

/******************************************************************************/
/* Private:                        G e n I F                                  */
/******************************************************************************/
  
#define ADDSLOT(xdst, xstr, xlen) {strcpy(ifP->iVal,xstr); ifP->iLen=xlen; \
               xdst=ifP; bP += (6 + xlen + (xlen & 0x01)); ifP = (ifData *)bP;}

#define RLOSLOT(xdst) xdst = (ifData *)(ifBuff+((char *)xdst-buff))

bool XrdNetIF::GenIF(XrdNetAddrInfo **src, int srcnum)
{
   ifAddrs ifTab;
   const char *hName;
   char buff[2048], *bP = buff;
   char hBuff[NI_MAXHOST+16];
   ifData *ifP = (ifData *)buff;
   int i, n;
   bool isPrivate, genAOK = true;

// Initialize all of the vectors and free the buffer if we allocated it
//
   ifName[Public] = ifName[Private] = &ifNull;
   ifNest[Public] = ifNest[Private] = &ifNull;
   ifDest[Public] = ifDest[Private] = &ifNull;
   if (ifBuff) {free(ifBuff); ifBuff = 0;}

for (i = 0; i < srcnum; i++)
{
// Expand out the interface names if we actually need to
//
   if (!src[i]) continue;
   isPrivate = src[i]->isPrivate();
   if ((!isPrivate && ifName[Public] != &ifNull)
   ||  ( isPrivate && ifName[Private]!= &ifNull))
      {if (eDest && src[i]->Format(hBuff, sizeof(hBuff),
                                   XrdNetAddr::fmtAddr,XrdNetAddr::noPort))
          {char eBuff[1024];
           sprintf(eBuff, "Skipping duplicate %s interface",
                          (isPrivate ? "private" : "public"));
           eDest->Emsg("SetIF", eBuff, hBuff);
          }
       continue;
      }

// If this is a public address, get the host name. We avoid reverse lookups for
// private addresses because most DNS servers will never respond unless it is a
// private server enabled for the private address space in question. This will
// always introduce a very long delay which we really don't want to have.
//
   if (!isPrivate && (hName = src[i]->Name()))
      {n = strlen(hName);
       ADDSLOT(ifName[Public], hName, n);
       n = snprintf(hBuff, sizeof(hBuff), "%s:%d", hName, src[i]->Port());
       ADDSLOT(ifNest[Public], hBuff, n);
       genAOK = GenAddrs(ifTab, src[i], hName, isPrivate);
      } else genAOK = GenAddrs(ifTab, src[i], 0, isPrivate);

// Check if we should proceed. Failure here is almost unheard of
//
   if (!genAOK)
      {if (eDest) eDest->Emsg("SetIF", "Unable to validate net interfaces!");
       return false;
      }

// If this is a private interface, then set private pointers to actual addresses
// since, technically, private addresses should not be registered.
//
   if (isPrivate)
      {ADDSLOT(ifName[Private], ifTab.hAddr, ifTab.hALen);
       ADDSLOT(ifDest[Private], ifTab.hDest, ifTab.hDLen);
       ifNest[Private] = ifDest[Private];
       v6Dest[Private] = ifTab.ipV6;
      } else {
       if (ifName[Public] == &ifNull)
          {ADDSLOT(ifName[Public], ifTab.hAddr, ifTab.hALen);}
       ADDSLOT(ifDest[Public], ifTab.hDest, ifTab.hDLen);
       if (ifNest[Public] == &ifNull) ifNest[Public] = ifDest[Public];
       v6Dest[Public] = ifTab.ipV6;
      }
}

// Allocate/Replace string storage area
//
   n = (char *)ifP - buff;
   if (!(ifBuff = (char *)malloc(n))) return false;
   memcpy(ifBuff, buff, n);

// Now relocate all the pointers in the name and dest vectors
//
   for (i = 0; i < (int)ifNum; i++)
       {if (ifName[i] != &ifNull) RLOSLOT(ifName[i]);
        if (ifNest[i] != &ifNull) RLOSLOT(ifNest[i]);
        if (ifDest[i] != &ifNull) RLOSLOT(ifDest[i]);
       }

// All done
//
   return true;
}

/******************************************************************************/
/*                                 G e t I F                                  */
/******************************************************************************/

#define prtaddr(x) cerr <<"Addr!!! " << *x <<endl;

int XrdNetIF::GetIF(XrdOucTList **ifList, const char **eText)
{
   char ipBuff[256];
   short ifIdx, sval[4] = {0, 0, 0, 0};
   short iLen;
   int   haveIF = 0;

#ifdef HAVE_GETIFADDRS

// Obtain the list of interfaces
//
   XrdNetAddr      netAddr;
   struct ifaddrs *ifBase, *ifP;
   XrdOucTList    *tLP, *tList = 0, *tLast = 0;
   int             n = 0;

   if (getifaddrs(&ifBase) < 0)
      {if (eText) *eText = strerror(errno);
       if (ifList) *ifList = 0;
       if (eDest) eDest->Emsg("GetIF", errno, "get interface addresses.");
       return 0;
      }

// Report only those interfaces that are up and are not loop-back devices and
// have been specified by actual name
//
   ifP = ifBase;
   while(ifP)
        {if ((ifP->ifa_addr != 0)
         &&  (!ifList || IsOkName(ifP->ifa_name, ifIdx))
         &&  (ifP->ifa_flags & (IFF_UP))
         &&  (ifP->ifa_flags & (IFF_RUNNING))
         && !(ifP->ifa_flags & (IFF_LOOPBACK))
         &&  ((ifP->ifa_addr->sa_family == AF_INET  &&
              !V4LinkLocal(ifP->ifa_addr))
             ||
              (ifP->ifa_addr->sa_family == AF_INET6 &&
              !(IN6_IS_ADDR_LINKLOCAL(&((sockaddr_in6 *)(ifP->ifa_addr))->sin6_addr)))
             )
            )
            {if (ifP->ifa_addr->sa_family == AF_INET) haveIF |= haveIPv4;
                else haveIF |= haveIPv6;
             if (ifList)
                {netAddr.Set(ifP->ifa_addr);
                 if ((iLen = netAddr.Format(ipBuff, sizeof(ipBuff),
                             XrdNetAddrInfo::fmtAddr,XrdNetAddrInfo::noPort)))
                    {sval[2] = ifIdx;
                     sval[1] = (netAddr.isPrivate() ? 1 : 0);
                     sval[0] = iLen;
                     tLP = new XrdOucTList(ipBuff, sval);
                     if (tList) tLast->next = tLP;
                        else    tList       = tLP;
                     tLast = tLP;
                     n++;
                    }
                }
            }
          ifP = ifP->ifa_next;
        }

// All done
//
   if (ifBase) freeifaddrs(ifBase);
   if (eText) *eText = 0;
   if (!ifList) return haveIF;
   *ifList = tList;
   return n;

#else

// If we just need to provide the interface type, indicate we cannot
//
   if (!ifList) return haveNoGI;

// For platforms that don't support getifaddrs() use our address
//
   XrdNetAddr netAddr((int)0);

// Simply return our formatted address as the interface address
//
   if ((iLen = netAddr.Format(ipBuff, sizeof(ipBuff),
                       XrdNetAddrInfo::fmtAddr,XrdNetAddrInfo::noPort)))
      {if (eText) *eText = 0;
       sval[0] = iLen;
       *ifList = new XrdOucTList(ipBuff, sval);
       return 1;
      }

// Something bad happened and it shouldn't have
//
   if (eText) *eText = "unknown error";
   if (eDest) eDest->Emsg("GetIF", "Unable to get interface address; "
                                   "check if IPV6 enabled!");
   return 0;
#endif
}

/******************************************************************************/

int XrdNetIF::GetIF(char *buff, int blen, const char **eText, bool show)
{
   XrdOucTList *ifP, *ifN;
   char *bP = buff;
   int n, bLeft = blen-8;
   bool ifOK[2] = {false, false};

#ifndef HAVE_GETIFADDRS
// Display warning on how we are getting the interface addresses
//
   if (eDest && show)
      eDest->Say("Config Warning: using DNS registered address as interface!");
#endif

// Create the interface list here
//
   *buff = 0;
   if (GetIF(&ifN, eText))
      {while((ifP = ifN))
            {n = ifP->sval[0];
             if (bLeft > n+2)
                {if (bP != buff) {*bP++ = ' '; bLeft--;}
                 strcpy(bP, ifP->text);
                 bP += n; bLeft -= (n+1);
                }
             ifOK[ifP->sval[2]] = true;
             if (show && eDest)
                {const char *kind = (ifP->sval[1] ? " private" : " public ");
                 eDest->Say("Config ", ifCfg[ifP->sval[2]], kind,
                                    " network interface: ", ifP->text);
                }
             ifN = ifP->next; delete ifP;
            }
      }

// Warn about missing interfaces
//
   if (show && eDest)
      {for (n = 0; n < 2; n++)
           {if (!ifOK[n] && ifCfg[n])
                eDest->Say("Config ", ifCfg[n],
                           " interface not found or is not usable.");
           }
      }

// Return result
//
   return bP-buff;
}

/******************************************************************************/

int XrdNetIF::GetIF(char *&ifline, const char **eText, bool show)
{
   char buff[4096];
   int  n;

   if ((n = GetIF(buff, sizeof(buff), eText, show))) ifline = strdup(buff);
      else ifline = 0;

// Warn about no interfaces
//
   if (!ifline && show && eDest)
      eDest->Say("Config ", "No usable interfaces; using DNS registered "
                            "address as the interface.");
   return n;
}

/******************************************************************************/
/*                              I n D o m a i n                               */
/******************************************************************************/
  
bool XrdNetIF::InDomain(XrdNetAddrInfo *epaddr)
{
   const char *hnP;

// Do not attempt to resolve private addresses as they are always in the domain.
//
   if (epaddr->isPrivate()) return true;

// Checkout the domain
//
   if (!myDomain || !(hnP = epaddr->Name(0)) || !(hnP = index(hnP, '.')))
      return false;

// Match the domain and returnthe result
//
   return strcmp(myDomain, hnP+1) == 0;
}

/******************************************************************************/
/*                              I s O k N a m e                               */
/******************************************************************************/

bool XrdNetIF::IsOkName(const char *ifn, short &ifIdx)
{
   if (!ifn) return false;
        if (ifCfg[0] && !strcmp(ifn, ifCfg[0])) ifIdx = 0;
   else if (ifCfg[1] && !strcmp(ifn, ifCfg[1])) ifIdx = 1;
   else return false;
   return true;
}
  
/******************************************************************************/
/*                                  P o r t                                   */
/******************************************************************************/

int XrdNetIF::Port(int pnum)
{
   int i, n, prevport = ifPort;
   char *Colon, pBuff[8];

// Check if anything is really changing
//
   pnum &= 0x0000ffff;
   if (pnum == prevport) return prevport;

// Format the port number (can't be more than 5 characters)
//
   n = sprintf(pBuff, "%d", pnum);
   ifPort = pnum;

// Insert the port number if all the buffers that have one. There is room.
//
   for (i = 0; i < (int)ifNum; i++)
       {if (ifNest[i] != &ifNull && (Colon = rindex(ifNest[i]->iVal, ':')))
           {strcpy(Colon+1, pBuff);
            ifNest[i]->iLen = (Colon - ifNest[i]->iVal) + n + 1;
           }
        if (ifDest[i] != &ifNull && (Colon = rindex(ifDest[i]->iVal, ':')))
           {strcpy(Colon+1, pBuff);
            ifDest[i]->iLen = (Colon - ifDest[i]->iVal) + n + 1;
           }
       }

// All done
//
   return prevport;
}
  
/******************************************************************************/
/*                               R o u t i n g                                */
/******************************************************************************/
  
void XrdNetIF::Routing(XrdNetIF::netType nettype)
{
   netRoutes = (nettype == netDefault ? netSplit : nettype);
}

/******************************************************************************/
/* Private:                    S e t D o m a i n                              */
/******************************************************************************/
  
char *XrdNetIF::SetDomain()
{
   XrdNetAddr myAddr((int)0);
   const char *hnP;

// Get our fully resilved name (this doesn't always work)
//
   if (!(hnP = myAddr.Name()) || !(hnP = index(hnP,'.')) || !(*(hnP+1)))
      return 0;

// Return the components after the first as the domain name
//
   return strdup(hnP+1);
}

/******************************************************************************/
/*                                 S e t I F                                  */
/******************************************************************************/
  
bool XrdNetIF::SetIF(XrdNetAddrInfo *src, const char *ifList, netType nettype)
{
   XrdNetAddrInfo *netIF[2] = {0,0};
   XrdNetAddr      netAdr[2];
   const char *ifErr = 0, *ifBeg = ifList, *ifEnd, *ifAdr, *ifBad = 0;
   int i, n, ifCnt = 1;
   char abuff[64];

// If no list is supplied then fill out based on the source address
//
   if (!(ifPort = src->Port())) ifPort = dfPort;
   if (!ifList || !(*ifList))
      {XrdNetAddrInfo *ifVec[8];
       XrdNetAddr *iP;
       const char *hName = src->Name();
       ifCnt = 0;
       if (!hName
       ||  XrdNetUtils::GetAddrs(hName, &iP, ifCnt, XrdNetUtils::allIPv64,
                                 ifPort) || !ifCnt) return GenIF(&src, 1);
       if (ifCnt > 8) ifCnt = 8;
       for (i = 0; i < ifCnt; i++) ifVec[i] = &iP[i];
       bool aOK = GenIF(ifVec, ifCnt);
       delete [] iP;
       return aOK;
      }

// Prefrentially use the connect address as the primary interface. This
// avoids using reported interfaces that may have strange routing.
//
   if (src->isPrivate()) {if (!netIF[1]) netIF[1] = src;}
      else               {if (!netIF[0]) netIF[0] = src;}

// Process the iflist (up to two interfaces)
//
   if (ifList && *ifList)
   do {while (*ifBeg && *ifBeg == ' ') ifBeg++;
       if ( !(*ifBeg)) break;
       if (!(ifEnd = index(ifBeg, ' '))) {ifAdr = ifBeg; ifBeg = "";}
          else {n = ifEnd - ifBeg;
                if (n >= (int)sizeof(abuff))
                        {ifAdr = 0; ifBad = ifBeg; ifErr = "invalid";}
                   else {strncpy(abuff, ifBeg, n); abuff[n] = 0; ifAdr = abuff;}
                ifBeg = ifEnd+1;
               }
       if (!ifAdr || (ifErr = netAdr[ifCnt].Set(ifAdr, ifPort)))
          {if (eDest)
              {if (!ifAdr) ifAdr = ifBad;
               eDest->Emsg("SetIF", "Unable to use interface", ifAdr, ifErr);
              }
           continue;
          }
       i = (netAdr[ifCnt].isPrivate() ? 1 : 0);
       if (!netIF[i] || (netAdr[ifCnt].isIPType(XrdNetAddrInfo::IPv4)
                         &&  netIF[i]->isIPType(XrdNetAddrInfo::IPv6)))
          netIF[i] = &netAdr[ifCnt--];
      } while(ifCnt >= 0);

// Set the interface data
//
   if (!GenIF(netIF, 2)) return false;

// Establish how undefined interfaces will be resolved
//
   if (nettype == netDefault) nettype = netRoutes;
   if (nettype == netSplit)   return true;

// Now set all undefined private interfaces for common and local network routing
//
   if (ifName[Private] == &ifNull) ifName[Private] = ifName[Public];
   if (ifNest[Private] == &ifNull) ifNest[Private] = ifNest[Public];
   if (ifDest[Private] == &ifNull) ifDest[Private] = ifDest[Public];

// If this is a common network then we are done
//
   if (nettype == netCommon) return true;

// Now set all undefined public  interfaces for local network routing
//
   if (ifName[Public ] == &ifNull) ifName[Public ] = ifName[Private];
   if (ifNest[Public ] == &ifNull) ifNest[Public ] = ifNest[Private];
   if (ifDest[Public ] == &ifNull) ifDest[Public ] = ifDest[Private];
   return true;
}

/******************************************************************************/
/*                            S e t I F N a m e s                             */
/******************************************************************************/

bool XrdNetIF::SetIFNames(char *ifnames)
{
   char *comma;

// Make sure there are two interface names
//
   if (!(comma = index(ifnames, ',')) || comma == ifnames || !(*(comma+1)))
      {if (eDest) eDest->Say("Config","Invalid interface name list - ",ifnames);
       return false;
      }

// Free old names, if any
//
   if (ifCfg[0]) free(ifCfg[0]);
   if (ifCfg[1]) free(ifCfg[1]);

// Copy the new names
//
   *comma = 0;
   ifCfg[0] = strdup(ifnames);
   ifCfg[1] = (strcmp(ifnames, comma+1) ? strdup(comma+1) : 0);
   *comma = ',';
   return true;
}

/******************************************************************************/
/* Private:                  V 4 L i n k L o c a l                            */
/******************************************************************************/
  
bool XrdNetIF::V4LinkLocal(struct sockaddr *saP)
{
   unsigned char *ipV4;

   ipV4 = (unsigned char *)&((sockaddr_in  *)(saP))->sin_addr.s_addr;
   return ipV4[0] == 169 && ipV4[1] == 254;
}
