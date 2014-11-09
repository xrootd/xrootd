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

#include <ctype.h>
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
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
// Selection mask values
//
const char hasPub4 = 0x01;
const char hasPrv4 = 0x02;
const char hasPub6 = 0x04;
const char hasPrv6 = 0x08;
const char hasAny4 = hasPub4 | hasPrv4;
const char hasAny6 = hasPub6 | hasPrv6;
const int  hasNum  = 4;

// Name translation table
//
const char  sMask[hasNum] = {hasPub4,  hasPrv4,   hasPub6,  hasPrv6};
const char *sName[hasNum] = {"pub4 ",  "prv4 ",   "pub6 ",  "prv6"};

// common -> Only private addresses may select any node. Public must select
//           a node that has a public address.
//
   const char ifMaskComm [XrdNetIF::ifMax] = {hasPub4,  hasAny4,
                                              hasPub6,  hasAny6,
                                              hasPub4 | hasPub6,
                                              hasAny6 | hasAny4,
                                              hasPub6 | hasPub4,
                                              hasAny4 | hasAny6
                                             };

// local  -> Public-private address is immaterial for node selection.
//
   const char ifMaskLocal[XrdNetIF::ifMax] = {hasAny4,  hasAny4,
                                              hasAny6,  hasAny6,
                                              hasAny4 | hasAny6,
                                              hasAny4 | hasAny6,
                                              hasAny6 | hasAny4,
                                              hasAny6 | hasAny4
                                             };

// split  -> Address type may only select node with that address type.
//
   const char ifMaskSplit[XrdNetIF::ifMax] = {hasPub4,  hasPrv4,
                                              hasPub6,  hasPrv6,
                                              hasPub4 | hasPub6,
                                              hasPrv4 | hasPrv6,
                                              hasPub6 | hasPub4,
                                              hasPrv6 | hasPrv4
                                             };
}

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

XrdSysError      *XrdNetIF::eDest     = 0;

char             *XrdNetIF::myDomain  = XrdNetIF::SetDomain();

char             *XrdNetIF::ifCfg[2]  = {0,0};

const char       *XrdNetIF::ifTName[ifMax] = {"public IPv4",   // 01
                                              "private IPv4",  // 02
                                              "public IPv6",   // 04
                                              "private IPv6",  // 08
                                              "public",
                                              "private",
                                              "public",
                                              "private"
                                             };


// The following vector is suitable only for local routing. It is reset
// to the appropriate selection bits when Routing() is called.
//
const char       *XrdNetIF::ifMaskVec = ifMaskLocal;

XrdNetIF::netType XrdNetIF::netRoutes = XrdNetIF::netLocal;

int               XrdNetIF::dfPort    = 1094;

XrdNetIF::ifData  XrdNetIF::ifNull;

/******************************************************************************/
/*                               D i s p l a y                                */
/******************************************************************************/
  
void XrdNetIF::Display(const char *pfx)
{
   static const char *ifN[] = {"pub4", "prv4", "pub6", "prv6"};
   static const char *ifT[] = {"all4", 0,      "all6", 0};
   static const char *nNM[] = {"local", "split", "common", "local"};
          const char *iHX[hasNum] = {"", "", "", ""};
          const char *ifRType, *hName = "";
   char buff[256];
   bool nameOK = false;

// If we have no error routing object, just return
//
   if (!eDest) return;

// Get a hostname
//
   for (int i = 0; i < (int)ifNum; i++)
       {if (ifName[i] != &ifNull)
           {hName = ifName[i]->iVal;
            if (ifxDNS[i]) {nameOK = true; break;}
           }
       }

// Compute selection mask
//
   for (int i = 0; i < hasNum; i++)
       if (ifMask & sMask[i]) iHX[i] = sName[i];

// Print results
//
   sprintf(buff, ": %s %s%s%s%s", nNM[ifRoute], iHX[0],iHX[1],iHX[2],iHX[3]);
   eDest->Say(pfx, "Routing for ", hName, buff);

   for (int i = 0; i < (int)ifNum; i++)
       {if (ifName[i] != &ifNull)
           {if (ifT[i] && ifDest[i] == ifDest[i+1]) {ifRType = ifT[i]; i++;}
               else ifRType = ifN[i];
            sprintf(buff, "Route %s: ", ifRType);
            eDest->Say(pfx, buff, (nameOK ? hName : ifName[i]->iVal),
                       " Dest=", ifDest[i]->iVal, portSfx.val);
           }
       }
}

/******************************************************************************/
/* Private:                     G e n A d d r s                               */
/******************************************************************************/
  
bool XrdNetIF::GenAddrs(ifAddrs &ifTab, XrdNetAddrInfo *src)
{
   static const int noPort  = XrdNetAddr::noPort;
   static const int old6M4  = XrdNetAddr::noPort | XrdNetAddr::old6Map4;
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
// and the locate destination address is the deprecated IPV6 address.
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

// There is no IPV4 address so use pure IPV6.
//
   ifTab.ipV6 = true;
   if (!(ifTab.hALen = src->Format(ifTab.hAddr,  sizeof(ifTab.hAddr),
                            XrdNetAddr::fmtAdv6, noPort))
   ||  !(ifTab.hDLen = src->Format(ifTab.hDest,  sizeof(ifTab.hDest),
                            XrdNetAddr::fmtAdv6, noPort))) return false;
   return true;
}

/******************************************************************************/

bool XrdNetIF::GenAddrs(ifAddrs &ifTab, const char *hName, bool wantV6)
{
   XrdNetAddr *iP;
   XrdNetUtils::AddrOpts aOpts = (wantV6 ? XrdNetUtils::onlyIPv6
                                         : XrdNetUtils::onlyIPv4);
   int i, iN, iPVT = -1;
   bool aOK = false;

// Find alternate addresses in the desired protocol family for this host.
//
   if (!XrdNetUtils::GetAddrs(hName, &iP, iN, aOpts, 0) && iN)
      {for (i = 0; i < iN; i++)
           {if (iP[i].isPrivate()) iPVT = i;
               else break;
           }
       if (i < iN) ifTab.prvt = false;
          else if (iPVT >= 0) {i = iPVT; ifTab.prvt = true;}
       if (i > iN) aOK = GenAddrs(ifTab, &iP[i]);
       delete [] iP;
      }

// All done
//
   return aOK;
}

/******************************************************************************/
/* Private:                        G e n I F                                  */
/******************************************************************************/
  
#define ADDSLOT(xdst, xstr, xlen) {strcpy(ifBP->iVal,xstr);ifBP->iLen=xlen; \
               xdst=ifBP; bP += (6 + xlen + (xlen & 0x01));ifBP = (ifData *)bP;}

#define RLOSLOT(xdst) xdst = (ifData *)(ifBuff+((char *)xdst-buff))

bool XrdNetIF::GenIF(XrdNetAddrInfo **src, int srcnum)
{
   ifAddrs ifTab;
   const char *hName;
   char buff[4096], *bP = buff;
   ifData *ifBP = (ifData *)buff;
   ifType ifT;
   int i, n;
   bool isPrivate;

// Initialize all of the vectors and free the buffer if we allocated it
//
   for (i = 0; i < (int)ifMax; i++)
      {ifName[i] = ifDest[i] = &ifNull;
       ifxDNS[i] = false;
      }
   if (ifBuff) {free(ifBuff); ifBuff = 0;}

for (i = 0; i < srcnum; i++)
{

// Generate interface addresses. Failure here is almost impossible.
//
   if (!src[i]) continue;
   isPrivate = src[i]->isPrivate();
   if (!GenAddrs(ifTab, src[i]))
      {if (eDest) eDest->Emsg("SetIF", "Unable to validate net interfaces!");
       return false;
      }

// Determine interface type
//
   if (isPrivate) ifT = (ifTab.ipV6 ? PrivateV6 : PrivateV4);
      else        ifT = (ifTab.ipV6 ? PublicV6  : PublicV4);

// We can now check if we have a duplicate interface here
//
   if (ifDest[ifT] != &ifNull && eDest)
      {char eBuff[64];
       sprintf(eBuff, "Skipping duplicate %s interface",
                      (isPrivate ? "private" : "public"));
       eDest->Emsg("SetIF", eBuff, ifTab.hDest);
       continue;
      }

// Set the locate destination, always an address
//
   ADDSLOT(ifDest[ifT], ifTab.hDest, ifTab.hDLen);

// If this is a private interface, then set private pointers to actual addresses
// since, technically, private addresses should not be registered. Otherwise,
// fill in the public interface information. We also set unregistered public
// addresses (what a pain).
//
   if (isPrivate)
      {ADDSLOT(ifName[ifT], ifTab.hAddr, ifTab.hALen);
      } else {
       if ((hName = src[i]->Name()) && src[i]->isRegistered())
          {ADDSLOT(ifName[ifT], hName, strlen(hName));
           ifxDNS[ifT] = true;
          } else  ifName[ifT]  = ifDest[ifT];
      }
}

// At this point we have set all of the advertised interfaces. If this is a
// registered host then we know we have the name and nest information but not
// necessarily the locate destination for each protocol. So, we will try to
// find them via DNS. If the host does not have an IPv6 address then we will
// use the mapped IPv4 address and hope that the client is dual stacked.
//
   if (ifDest[PublicV4] == &ifNull && ifxDNS[PublicV6]
   &&  GenAddrs(ifTab, ifName[PublicV6]->iVal, false))
      {if (!ifTab.prvt)
          {ADDSLOT(ifDest[PublicV4], ifTab.hDest, ifTab.hDLen);
           ifName[PublicV4]  = ifName[PublicV6];
          } else if (ifDest[PrivateV4] == &ifNull)
          {ADDSLOT(ifDest[PrivateV4], ifTab.hDest, ifTab.hDLen);
           ifName[PrivateV4] = ifName[PublicV6];
          }
      }

   if (ifDest[PublicV6] == &ifNull && ifxDNS[PublicV4]
   &&  GenAddrs(ifTab, ifName[PublicV4]->iVal, true))
      {if (!ifTab.prvt)
          {ADDSLOT(ifDest[PublicV6], ifTab.hDest, ifTab.hDLen);
           ifName[PublicV6]  = ifName[PublicV4];
          } else if (ifDest[PrivateV6] == &ifNull)
          {ADDSLOT(ifDest[PrivateV6], ifTab.hDest, ifTab.hDLen);
           ifName[PrivateV6] = ifName[PublicV4];
          }
      }

// Allocate/Replace string storage area
//
   n = (char *)ifBP - buff;
   if (!(ifBuff = (char *)malloc(n))) return false;
   memcpy(ifBuff, buff, n);

// Now relocate all the pointers in the name and dest vectors
//
   for (n = 0; n < (int)ifNum; n++)
       {if (ifName[n] != &ifNull) RLOSLOT(ifName[n]);
        if (ifDest[n] != &ifNull) RLOSLOT(ifDest[n]);
       }

// All done
//
   return true;
}

/******************************************************************************/
/*                               G e t D e s t                                */
/******************************************************************************/

int XrdNetIF::GetDest(char *dest, int dlen, ifType ifT, bool prefn)
{
   ifType  ifX = (ifT >= ifAny ? static_cast<ifType>(ifAvail) : ifT);
   ifData *ifP = (prefn && ifxDNS[ifX] ? ifName[ifX] : ifDest[ifX]);
   int n;

// Compute length and make sure we don't overflow
//
   n = ifP->iLen + portSfx.len;
   if (!(ifP->iLen) || n >= dlen) return 0;

// Return result with port appended
//
   strcpy(dest, ifP->iVal);
   strcpy(dest +ifP->iLen, portSfx.val);
   return n;
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
   int prevport = ifPort;

// Check if anything is really changing
//
   pnum &= 0x0000ffff;
   if (pnum == prevport) return prevport;

// Format the port number (can't be more than 5 characters)
//
   portSfx.len = sprintf(portSfx.val, ":%d", pnum);
   ifPort = pnum;

// All done
//
   return prevport;
}
  
/******************************************************************************/
/*                               R o u t i n g                                */
/******************************************************************************/
  
void XrdNetIF::Routing(XrdNetIF::netType nettype)
{

// Set the routing type
//
   netRoutes = (nettype == netDefault ? netLocal : nettype);

// Based on the routing we need to set the appropriate selection mask vector
//
        if (netRoutes == netLocal) ifMaskVec = ifMaskLocal;
   else if (netRoutes == netSplit) ifMaskVec = ifMaskSplit;
   else                            ifMaskVec = ifMaskComm;
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
  
bool XrdNetIF::SetIF(XrdNetAddrInfo *src, const char *ifList, int port,
                     netType nettype)
{
   XrdNetAddrInfo *netIF[4] = {0,0,0,0}; //pub 0:v4, prv 1:v4 pub 2:v6 prv 3:v6
   XrdNetAddr      netAdr[4];
   const char *ifErr = 0, *ifBeg = ifList, *ifEnd, *ifAdr, *ifBad = 0;
   int i, n, ifCnt = 1;
   char abuff[64];

// Setup the port number (this sets ifPort)
//
   if (port >= 0) Port((port ? port : dfPort));

// Set routing mode for this interface
//
   ifRoute = static_cast<short>(nettype == netDefault ? netRoutes : nettype);

// If no list is supplied then fill out based on the source address
//
   if (!ifList || !(*ifList))
      {XrdNetAddrInfo *ifVec[8];
       XrdNetAddr *iP;
       const char *hName = src->Name();
       ifCnt = 0;
       if (!hName
       ||  XrdNetUtils::GetAddrs(hName,&iP,ifCnt,XrdNetUtils::allIPv64,ifPort)
       || !ifCnt) return SetIF64(GenIF(&src, 1));
       if (ifCnt > 8) ifCnt = 8;
       for (i = 0; i < ifCnt; i++) ifVec[i] = &iP[i];
       bool aOK = GenIF(ifVec, ifCnt);
       delete [] iP;
       return SetIF64(aOK);
      }

// Prefrentially use the connect address as the primary interface. This
// avoids using reported interfaces that may have strange routing.
//
   i = (src->isIPType(XrdNetAddrInfo::IPv4) || src->isMapped() ? 0 : 2);
   if (src->isPrivate()) i |= 1;
   netIF[i] = src;

// Process the iflist (up to four interfaces)
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
       i = (netAdr[ifCnt].isIPType(XrdNetAddrInfo::IPv4) ||
            netAdr[ifCnt].isMapped() ? 0 : 2);
       if (netAdr[ifCnt].isPrivate()) i |= 1;
       if (!netIF[i]) netIF[i] = &netAdr[ifCnt--];
      } while(ifCnt >= 0);

// Set the interface data
//
   return SetIF64(GenIF(netIF, 4));
}

/******************************************************************************/
/*                            S e t I F N a m e s                             */
/******************************************************************************/

bool XrdNetIF::SetIFNames(char *ifnames)
{
   char *comma;

// Parse the interface names
//
   if ((comma = index(ifnames, ',')))
      {if (comma == ifnames || !(*(comma+1)))
          {if (eDest) eDest->Say("Config","Invalid interface name - ",ifnames);
           return false;
          }
      }

// Free old names, if any
//
   if (ifCfg[0]) free(ifCfg[0]);
   if (ifCfg[1]) free(ifCfg[1]);

// Copy the new names
//
   if (comma)
      {*comma = 0;
       ifCfg[1] = (strcmp(ifnames, comma+1) ? strdup(comma+1) : 0);
       *comma = ',';
      } else ifCfg[1] = 0;
   ifCfg[0] = strdup(ifnames);
   return true;
}

/******************************************************************************/
/* Private:                      S e t I F P P                                */
/******************************************************************************/
  
void XrdNetIF::SetIFPP()
{
   int i, j;

// For split network we use what we have
//
   if (netSplit == (netType)ifRoute) return;

// Now set all undefined private interfaces for common and local network routing
//
   i = (int)PrivateV4; j = PublicV4;
   do {if (ifName[i] == &ifNull) ifName[i]=ifName[j];
       if (ifDest[i] == &ifNull) ifDest[i]=ifDest[j];
       if (i == (int)PrivateV6) break;
       i = (int)PrivateV6; j = (int)PublicV6;
      } while(true);

// If this is a common network then we are done
//
   if (netCommon == (netType)ifRoute) return;

// Now set all undefined public  interfaces for local network routing
//
   i = (int)PublicV4; j = PrivateV4;
   do {if (ifName[i] == &ifNull) ifName[i]=ifName[j];
       if (ifDest[i] == &ifNull) ifDest[i]=ifDest[j];
       if (i == (int)PublicV6) break;
       i = (int)PublicV6; j = (int)PrivateV6;
      } while(true);
}

/******************************************************************************/
/* Private:                      S e t I F 6 4                                */
/******************************************************************************/
  
bool XrdNetIF::SetIF64(bool retVal)
{
   static const  int ifN46= 4;
   static ifType ifSet[ifN46] = {Public46, Private46, Public64, Private64};
   static ifType ifChk[ifN46] = {PublicV4, PrivateV4, PublicV6, PrivateV6};
   static ifType eqSel[ifN46] = {PublicV6, PrivateV6, PublicV4, PrivateV4};
   static ifType neSel[ifN46] = {PublicV4, PrivateV4, PublicV6, PrivateV6};

// Readjust routing tables if this is not a split network
//
   if (netSplit != (netType)ifRoute) SetIFPP();

// Fill out the 4/6 6/4 tables and compute the selection mask
//
   ifMask = 0;
   for (int i = 0; i < ifN46; i++)
      {ifName[ifSet[i]] = (ifName[ifChk[i]] == &ifNull ? ifName[eqSel[i]]
                                                       : ifName[neSel[i]]);
       ifDest[ifSet[i]] = (ifDest[ifChk[i]] == &ifNull ? ifDest[eqSel[i]]
                                                       : ifDest[neSel[i]]);
       ifxDNS[ifSet[i]] = ifName[ifSet[i]] != &ifNull &&
                          isalpha(*(ifName[ifSet[i]]->iVal));
       if (ifDest[ifChk[i]] != &ifNull)  ifMask |= sMask[i];
      }

// Record the one that is actually present
//
   if (ifName[Public64] != &ifNull) ifAvail = static_cast<char>(Public64);
      else                          ifAvail = static_cast<char>(Private64);

// Return wha the caller wants us to return
//
   return retVal;
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
