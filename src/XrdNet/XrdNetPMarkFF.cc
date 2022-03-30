/******************************************************************************/
/*                                                                            */
/*                     X r d N e t P M a r k C f g . h h                      */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdNet/XrdNetPMarkFF.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTrace.hh"
  
/******************************************************************************/
/*                          L o c a l   M a c r o s                           */
/******************************************************************************/

#define TRACE(txt) if (doTrace) SYSTRACE(Trace->, tident, epName, 0, txt)

#define DEBUG(txt) if (doDebug) SYSTRACE(Trace->, tident, epName, 0, txt)
  
#define EPName(ep) const char *epName = ep
  
/******************************************************************************/
/*               F i r e f l y   P a c k e t   T e m p l a t e                */
/******************************************************************************/

namespace
{
const char *ffFmt0 =
"<134>1 - %s xrootd - firefly-json - " //RFC5424 syslog header (abbreviated)
"{"
  "\"version\":1,"
  "\"flow-lifecycle\":{"
    "\"state\":\"%%s\","            //-> start | ongoing | end
    "\"current-time\":\"%%s\","     //-> yyyy-mm-ddThh:mm:ss.uuuuuu+00:00
    "\"start-time\":\"%s\""
    "%%s"                           //-> ,"end-time":"<date-time>"
  "},"
  "\"usage\":{\"received\":%%llu,\"sent\":%%llu},"
  "\"netlink\":{\"rtt\":%%u.%%.03u},";

const char *ffFmt1 =
  "\"context\":{"
    "\"experiment-id\":%d,"
    "\"activity-id\":%d"
    "%s"                            //-> ,application:<appname>
  "},";

const char *ffFmt2 =
  "\"flow-id\":{"
    "\"afi\":\"ipv%c\","            //-> ipv4 | ipv6
    "\"src-ip\":\"%s\","            //   source which is always server (us)
    "\"dst-ip\":\"%s\","            //   dest   which is always client
    "\"protocol\":\"tcp\","
    "\"src-port\":%d,"
    "\"dst-port\":%d"
  "}"
"}";

const char *ffApp = ",\"application\":\"%.*s\"";

const char *ffEnd = ",\"end-time\":\"%s\"";
}
  
/******************************************************************************/
/*                        s t a t i c   O b j e c t s                         */
/******************************************************************************/

namespace XrdNetPMarkConfig
{

// Other configuration values
//
extern XrdSysError  *eDest;
extern XrdNetMsg    *netMsg;
extern XrdNetMsg    *netOrg;
extern XrdScheduler *Sched;
extern XrdSysTrace  *Trace;

extern char         *ffDest;
extern int           ffPortO;
extern int           ffEcho;
extern bool          doDebug;
extern bool          doTrace;

extern const  char  *myHostName;
}
using namespace XrdNetPMarkConfig;

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/
/*
namespace
{
void *Refresh(void *carg)
      {int intvl = *(int *)carg;
       while(true) {XrdSysTimer::Snooze(intvl); XrdNetPMarkCfg::Ping();}
      }
XrdSysMutex ffMutex;
}
*/

/******************************************************************************/
/* Private:                         E m i t                                   */
/******************************************************************************/
  
bool XrdNetPMarkFF::Emit(const char *state, const char *cT, const char *eT)
{
   EPName("Emit");
   struct sockStats ss;
   char msgBuff[1024];

   SockStats(ss);

   int n = snprintf(msgBuff, sizeof(msgBuff), ffHdr, state, cT, eT,
                             ss.bRecv, ss.bSent, ss.msRTT, ss.usRTT);

   if (n + ffTailsz >= (int)sizeof(msgBuff))
      {eDest->Emsg("PMarkFF", "invalid json; msgBuff truncated.");
       fdOK = odOK = false;
       return false;
      }

   memcpy(msgBuff+n, ffTail, ffTailsz+1);

   if (fdOK)
      {DEBUG("Sending pmark s-msg: " <<msgBuff);
       if (netMsg->Send(msgBuff, n+ffTailsz) < 0)
          {fdOK = false;
           return false;
          }
      }

   if (odOK)
      {DEBUG("Sending pmark o-msg: " <<(netMsg ? "=s-msg" : msgBuff));
       if (netOrg->Send(oDest, *mySad, msgBuff, n+ffTailsz) < 0)
          {odOK = false;
           return false;
          }
      }

   return true;
}

/******************************************************************************/
/* Private:                       g e t U T C                                 */
/******************************************************************************/
  
const char *XrdNetPMarkFF::getUTC(char *utcBuff, int utcBLen)
{
   struct timeval tod;
   struct tm utcDT;
   char *bP;

// Get the current time in UTC
//
   gettimeofday(&tod, 0);
   gmtime_r(&tod.tv_sec, &utcDT);

// Format this ISO-style
//
   size_t n = strftime(utcBuff, utcBLen, "%FT%T", &utcDT);
   bP = utcBuff + n; utcBLen -= n;
   snprintf(bP, utcBLen, ".%06u+00:00", static_cast<unsigned int>(tod.tv_usec));

// Return result
//
   return utcBuff;
}

/******************************************************************************/
/*                                  P i n g                                   */
/******************************************************************************/
/*
void XrdNetPMarkCfg::Ping()
{
// Tell every registered task to send out a continuation
//
   ffMutex.Lock();
   for (std::set<XdNetPMarkFF*> it = ffTasks.begin(); it!= ffTasks.end(); it++)
???
   ffMutex.UnLock();
}
*/
/******************************************************************************/
/*                              R e g i s t r y                               */
/******************************************************************************/
/*
XrdNetMsg                          *XrdNetPMarkCfg::netMsg  = 0;
std::set<XrdNetPMarkFF*>            XrdNetPMarkCfg::ffTasks;
  
void XrdNetPMarkCfg::Registry(XrdNetPMarkFF *ffobj, bool doadd)
{
// Add or delete ityem from task list
//
   ffMutex.Lock();
   if (doadd) ffTasks.insert(ffObj);
      else    ffTasks.erase(ffObj);
   ffMutex.UnLock();
}

// This is firefly so we must get a netmsg object
//
   bool aOK;
   netMsg = new XrdNetMsg(eLog, ffDest, aOK);
   if (!aOK)
      {eLog->Emsg("Config", "Unable to create UDP tunnel to", ffDest);
       return 0;
      }

// If there is an interval, start a thread to handle continuations
//
   if (ffIntvl && XrdSysThread::Run(&tid,Refresh,(void *)&ffIntvl,0,"pmark")
      {eDest->Emsg(epname, errno, "start pmark refresh timer");
       return 0;
      }
*/

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdNetPMarkFF::~XrdNetPMarkFF()
{
// If all is well, emit the closing message
//
   if (fdOK || odOK)
      {char utcBuff[40], endBuff[80];
       snprintf(endBuff, sizeof(endBuff), ffEnd,
                         getUTC(utcBuff, sizeof(utcBuff)));
       Emit("end", utcBuff, endBuff);
      }

// Cleanup
//
   if (mySad)  delete(mySad);
   if (oDest)  free(oDest);
   if (ffHdr)  free(ffHdr);
   if (ffTail) free(ffTail);
   if (xtraFH) delete xtraFH;
};

/******************************************************************************/
/*                             S o c k S t a t s                              */
/******************************************************************************/

#ifdef __linux__
#include <linux/tcp.h>
#endif

void XrdNetPMarkFF::SockStats(struct sockStats &ss)
{
#ifndef __linux__
   memset(&ss, 0, sizeof(struct sockStats));
#else
   struct tcp_info tcpInfo;
   socklen_t  tiLen = sizeof(tcpInfo);

   if (getsockopt(sockFD, IPPROTO_TCP, TCP_INFO, (void *)&tcpInfo, &tiLen) == 0)
      {ss.bRecv = static_cast<uint64_t>(tcpInfo.tcpi_bytes_received);
       ss.bSent = static_cast<uint64_t>(tcpInfo.tcpi_bytes_acked);
       ss.msRTT = static_cast<uint32_t>(tcpInfo.tcpi_rtt/1000);
       ss.usRTT = static_cast<uint32_t>(tcpInfo.tcpi_rtt%1000);
      } else memset(&ss, 0, sizeof(struct sockStats));
#endif
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
bool XrdNetPMarkFF::Start(XrdNetAddrInfo &addr)
{
   char appInfo[128], clIP[INET6_ADDRSTRLEN+2], svIP[INET6_ADDRSTRLEN+2];
   int  clPort, svPort;
   char clType, svType;
   bool fdok = false, odok = false;

// Preform app if we need to
//
   if (!appName) *appInfo = 0;
      else snprintf(appInfo,sizeof(appInfo),ffApp,sizeof(appInfo)-20,appName);

// Get the file descriptor for the socket
//
   sockFD = addr.SockFD();

// Obtain connectivity information about the peer and ourselves. We really
// should obtain our external address and use that but the issue is that
// we may have multiple external addresses and the client determines which
// one actually gets used. So, it's complicated. A TODO.
//
   clPort = XrdNetUtils::GetSokInfo( sockFD, clIP, sizeof(clIP), clType);
   if (clPort < 0)
      {eDest->Emsg("PMarkFF", clPort, "get peer information.");
       return false;
      }

   svPort = XrdNetUtils::GetSokInfo(-sockFD, svIP, sizeof(svIP), svType);
   if (svPort < 0)
      {eDest->Emsg("PMarkFF", clPort, "get self information.");
       return false;
      }

// If there is no special collector, indicate so
//
   if (netMsg) fdok = true;

// If the messages need to flow to the origin, get the destination information
//
   if (netOrg)
      {const XrdNetSockAddr *urSad = addr.NetAddr();
       if (!urSad) eDest->Emsg("PMarkFF", "unable to get origin address.");
          else {char buff[1024];
                mySad = new XrdNetSockAddr;
                memcpy(mySad, urSad, sizeof(XrdNetSockAddr));
                mySad->v4.sin_port = htons(static_cast<uint16_t>(ffPortO));
                snprintf(buff, sizeof(buff), "%s:%d", clIP, ffPortO);
                oDest = strdup(buff);
                odok  = true;
               }
      }

// If we cannot report anywhere then indicate we failed
//
   if (!fdok && !odok) return false;

// Format the base firefly template. Note that the client determines the
// address family that is being used.
//
   char utcBuff[40], bseg0[512];
   int len0 = snprintf(bseg0, sizeof(bseg0), ffFmt0, myHostName,
                                             getUTC(utcBuff, sizeof(utcBuff)));
   if (len0 >= (int)sizeof(bseg0))
      {eDest->Emsg("PMarkFF", "invalid json; bseg0 truncated.");
       return false;
      }

   ffHdr = strdup(bseg0);

   char bseg1[256];
   int len1 = snprintf(bseg1, sizeof(bseg1), ffFmt1, eCode, aCode, appInfo);
   if (len1 >= (int)sizeof(bseg1))
      {eDest->Emsg("PMarkFF", "invalid json; bseg1 truncated.");
       return false;
      }

   char bseg2[256];
   int len2 = snprintf(bseg2, sizeof(bseg2), ffFmt2,
                              clType, svIP, clIP, svPort, clPort);
   if (len2 >= (int)sizeof(bseg2))
      {eDest->Emsg("PMarkFF", "invalid json; cl bseg2 truncated.");
       return false;
      }

   ffTailsz = len1 + len2;
   ffTail   = (char *)malloc(ffTailsz + 1);
   strcpy(ffTail,      bseg1);
   strcpy(ffTail+len1, bseg2);

// OK, we now can emit the starting packet
//
   fdOK = fdok;
   odOK = odok;
   return Emit("start", utcBuff, "");
}
