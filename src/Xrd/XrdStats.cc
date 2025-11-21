/******************************************************************************/
/*                                                                            */
/*                           X r d S t a t s . c c                            */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "XrdVersion.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdMonitor.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdProtLoad.hh"
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdStats.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

       long  XrdStats::tBoot = static_cast<long>(time(0));

/******************************************************************************/
/*               L o c a l   C l a s s   X r d S t a t s J o b                */
/******************************************************************************/

class XrdStatsJob : XrdJob
{
public:

     void DoIt() {Stats->Report();
                  Sched->Schedule((XrdJob *)this, time(0)+iVal);
                 }

          XrdStatsJob(XrdScheduler *schP, XrdStats *sP, int iV)
                     : XrdJob("stats reporter"),
                       Sched(schP), Stats(sP), iVal(iV)
                     {Sched->Schedule((XrdJob *)this, time(0)+iVal);}
         ~XrdStatsJob() {}
private:
XrdScheduler *Sched;
XrdStats     *Stats;
int           iVal;
};

/******************************************************************************/
/*                           C o n s t r c u t o r                            */
/******************************************************************************/

XrdStats::XrdStats(XrdSysError *eP, XrdScheduler *sP, XrdBuffManager *bP,
                   const char *hname, int port,
                   const char *iname, const char *pname, const char *site)
{
   const char *head =
          "<statistics tod=\"%%ld\" ver=\"" XrdVERSION "\" src=\"%s:%d\" "
                      "tos=\"%ld\" pgm=\"%s\" ins=\"%s\" pid=\"%d\" "
                      "site=\"%s\">";
               Hend = "</statistics>";
               Htln = strlen(Hend);

   const char *jead =
          "{\"statistics\":{\"tod\":%%ld,\"ver\":\"" XrdVERSION "\",\"src\":\"%s:%d\","
                      "\"tos\":%ld,\"pgm\":\"%s\",\"ins\":\"%s\",\"pid\":%d,"
                      "\"site\":\"%s\",";
               Jend = "}}";
               Jtln = 2;

   char myBuff[1024];

   XrdLog   = eP;
   XrdSched = sP;
   BuffPool = bP;

   sprintf(myBuff, head, hname, port, tBoot, pname, iname,
                   static_cast<int>(getpid()), (site ? site : ""));
   Head = strdup(myBuff);

   sprintf(myBuff, jead, hname, port, tBoot, pname, iname,
                   static_cast<int>(getpid()), (site ? site : ""));
   Jead = strdup(myBuff);

// Allocate a shared buffer. Buffer use is serialized via the statsMutex.
//
   blen = 64*1024; // 64K which is the largest allowed UDP packet
   if (posix_memalign((void **)&buff, getpagesize(), blen)) buff = 0;

   myHost = hname;
   myName = iname;
   myPort = port;

   theMon = new XrdMonitor;
}

/******************************************************************************/
/*                                E x p o r t                                 */
/******************************************************************************/

void XrdStats::Export(XrdOucEnv& theEnv)
{
   XrdMonRoll* monRoll = new XrdMonRoll(*theMon);
   theEnv.PutPtr("XrdMonRoll*", monRoll);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

void XrdStats::Init(char **Dest, int iVal, int xOpts, int jOpts)
{

// Establish up to two destinations
//
   if (Dest[0]) netDest[0] = new XrdNetMsg(XrdLog, Dest[0]);
   if (Dest[1]) netDest[1] = new XrdNetMsg(XrdLog, Dest[1]);

// Establish auto reporting options
//
   if (!(jOpts & XRD_STATS_ALLJ) && !(xOpts & XRD_STATS_ALLX))
      xOpts |= XRD_STATS_ALLX; // ALLX includes ALLJ
   jsonOpts = (jOpts & XRD_STATS_ALLJ) | XRD_STATS_JSON; xmlOpts = xOpts;
   autoSync = xOpts & XRD_STATS_SYNCA;

// Get and schedule a new job to report
//
   if (netDest[0]) new XrdStatsJob(XrdSched, this, iVal);
   return;
}

/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/

void XrdStats::Report()
{
   char udpBuff[64*1024];
   const char *Data;
          int theOpts, Dlen;

// This is an entry for reporting purposes, establish the sync flag
//
   if (!autoSync || XrdSched->Active() <= 30) theOpts = xmlOpts;
      else theOpts = xmlOpts & ~XRD_STATS_SYNC;

// Now get the statistics in xml format. Note that there is only one buufer
// so we lock this code path to protect it. Skip this if no specific reports
// in the xml category are requested.
//
   if (theOpts)
      {statsMutex.Lock();
       if ((Data = GenStats(Dlen, theOpts)))
          {netDest[0]->Send(Data, Dlen);
           if (netDest[1]) netDest[1]->Send(Data, Dlen);
          }
       statsMutex.UnLock();
      }

// Check if we have additional data registered via addons and plugins that
// we need in JSON format. These are sent as separate udp packets.
//
   theOpts = XrdMonitor::F_JSON;
   if (jsonOpts & XRD_STATS_ADON) theOpts |= XrdMonitor::X_ADON;
   if (jsonOpts & XRD_STATS_PLUG) theOpts |= XrdMonitor::X_PLUG;
   if (!(theOpts & ~XrdMonitor::F_JSON) || !theMon->Registered()) return;

// Format the header and setup for sending packets
//
   int   hL = sprintf(udpBuff, Jead, time(0));
   int   bL = sizeof(udpBuff) - hL - Jtln - 8;
   char* bP = udpBuff + hL;

// Get each item and send it off
//
   struct iovec ioV[3];
   ioV[0].iov_base = udpBuff;
   ioV[0].iov_len  = hL;
   ioV[1].iov_base = bP;
   ioV[2].iov_base = (void*)Jend;
   ioV[2].iov_len  = Jtln;
   int uL, sItem = 0;
   while((uL = theMon->Format(bP, bL, sItem, theOpts)))
        {ioV[1].iov_len = uL;
         netDest[0]->Send(ioV, 3);
         if (netDest[1]) netDest[1]->Send(ioV, 3);
        }
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

void XrdStats::Stats(XrdStats::CallBack *cbP, int xOpts, int jOpts)
{
   const char *info;
   int sz, opts;

// Note that currently we do not support json for client requests, so we
// ignore the jOpts as they should never be set.
//
   opts = xOpts;

// Lock the buffer,
//
   statsMutex.Lock();

// Obtain the stats, if we have some, do the callback. We currently do not
// support return of JSON format as some statistics can't provide it.
//
   if ((info = GenStats(sz, opts))) cbP->Info(info, sz);

// Unlock the buffer
//
   statsMutex.UnLock();
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              G e n S t a t s                               */
/******************************************************************************/

const char *XrdStats::GenStats(int &rsz, int opts) // statsMutex must be locked!
{
   static const char *sgen = "<stats id=\"sgen\">"
                             "<as>%d</as><et>%lu</et><toe>%ld</toe></stats>";
   static const char *tail = "</statistics>";
   static const char *snul = "<statistics tod=\"0\" ver=\"" XrdVSTRING "\">"
                            "</statistics>";

   static const int  snulsz =     strlen(snul);
   static const int  ovrhed = 256+strlen(sgen)+strlen(tail);
   XrdSysTimer myTimer;
   char *bp;
   int   n, bl, sz, do_sync = (opts & XRD_STATS_SYNC ? 1 : 0);

// If buffer is not allocated we cannot generate a report (not likely)
//
   if (!(bp = buff)) {rsz = snulsz; return snul;}
   bl = blen - ovrhed;

// Start the time if need be
//
   if (opts & XRD_STATS_SGEN) myTimer.Reset();

// Insert the heading
//
   sz = sprintf(buff, Head, static_cast<long>(time(0)));
   bl -= sz; bp += sz;

// Extract out the statistics, as needed
//
   if (opts & XRD_STATS_INFO)
      {sz = InfoStats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_BUFF)
      {sz = BuffPool->Stats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_LINK)
      {sz = XrdLink::Stats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_POLL)
      {sz = XrdPoll::Stats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_PROC)
      {sz = ProcStats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_PROT)
      {sz = XrdProtLoad::Statistics(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_SCHD)
      {sz = XrdSched->Stats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_SGEN)
      {unsigned long totTime = 0;
       myTimer.Report(totTime);
       sz = snprintf(bp,bl,sgen,do_sync==0,totTime,static_cast<long>(time(0)));
       bp += sz; bl -= sz;
      }

// Set the type of object we are interested in
//
   int fOpts = 0;
   if (opts & XRD_STATS_ADON) fOpts |= XrdMonitor::X_ADON;
   if (opts & XRD_STATS_PLUG) fOpts |= XrdMonitor::X_PLUG;
   if (fOpts)
      {int uL, sItem = 0;
       while(bl > 0 && (uL = theMon->Format(bp, bl, sItem, fOpts)))
            {bp += uL; bl -= uL;}
      }

   sz = bp - buff;
   if (bl > 0) n = strlcpy(bp, tail, bl);
      else n = 0;
   rsz = sz + (n >= bl ? bl : n);
   return buff;
}

/******************************************************************************/


void XrdStats::GenStats(std::vector<struct iovec>& ioVec, int opts)
{
   const char* sTail;
   char *sbP, sBuff[64*1024];  // Maximum size of UDP packet
   std::vector<struct iovec> ioV;
   int sTLen, sbFree, sdSZ, fOpts, sItem = 0;

// Insert the header in the buffer
//
   if (opts & XRD_STATS_JSON)
      {int Jlen = sprintf(sBuff, Jead, time(0));
       sdSZ = sbFree = sizeof(sBuff) - Jlen - 64;  // Generous extra for tail
       sbP = sBuff + Jlen;
       sTail = Jend;
       sTLen = Jtln;
       fOpts = XrdMonitor::F_JSON;
      } else {
       int Hlen = sprintf(sBuff, Head, time(0));
       sdSZ = sbFree = sizeof(sBuff) - Hlen - 64;  // Generous extra for tail
       sbP = sBuff + Hlen;
       sTail = Hend;
       sTLen = Htln;
       fOpts = 0;
      }

// Set the type of object we are interested in
//
   if (opts & XRD_STATS_ADON) fOpts |= XrdMonitor::X_ADON;
   if (opts & XRD_STATS_PLUG) fOpts |= XrdMonitor::X_PLUG;

// Generate all plugin statistics, one at a time
//
   while((sdSZ = theMon->Format(sbP, sbFree, sItem, fOpts)))
        {if (sdSZ > 0 && sdSZ <= sbFree)
            {char* bP = sbP + sdSZ;
             struct iovec ioV;
             strcpy(bP, sTail);
             strcpy(bP+sTLen, "\n");
             bP++;
             ioV.iov_base = strdup(sBuff);
             ioV.iov_len  = bP - sBuff + sTLen;
             ioVec.push_back(ioV);
            }
        }
}

/******************************************************************************/
/*                             I n f o S t a t s                              */
/******************************************************************************/

int XrdStats::InfoStats(char *bfr, int bln, int do_sync)
{
   static const char statfmt[] = "<stats id=\"info\"><host>%s</host>"
                     "<port>%d</port><name>%s</name></stats>";

// Check if actual length wanted
//
   if (!bfr) return sizeof(statfmt)+24 + strlen(myHost);

// Format the statistics
//
   return snprintf(bfr, bln, statfmt, myHost, myPort, myName);
}

/******************************************************************************/
/*                             P r o c S t a t s                              */
/******************************************************************************/

int XrdStats::ProcStats(char *bfr, int bln, int do_sync)
{
   static const char statfmt[] = "<stats id=\"proc\">"
          "<usr><s>%lld</s><u>%lld</u></usr>"
          "<sys><s>%lld</s><u>%lld</u></sys>"
          "</stats>";
   struct rusage r_usage;
   long long utime_sec, utime_usec, stime_sec, stime_usec;
// long long ru_maxrss, ru_majflt, ru_nswap, ru_inblock, ru_oublock;
// long long ru_msgsnd, ru_msgrcv, ru_nsignals;

// Check if actual length wanted
//
   if (!bfr) return sizeof(statfmt)+16*13;

// Get the statistics
//
   if (getrusage(RUSAGE_SELF, &r_usage)) return 0;

// Convert fields to correspond to the format we are using. Commented out fields
// are either not uniformaly reported or are incorrectly reported making them
// useless across multiple platforms.
//
//
   utime_sec   = static_cast<long long>(r_usage.ru_utime.tv_sec);
   utime_usec  = static_cast<long long>(r_usage.ru_utime.tv_usec);
   stime_sec   = static_cast<long long>(r_usage.ru_stime.tv_sec);
   stime_usec  = static_cast<long long>(r_usage.ru_stime.tv_usec);
// ru_maxrss   = static_cast<long long>(r_usage.ru_maxrss);
// ru_majflt   = static_cast<long long>(r_usage.ru_majflt);
// ru_nswap    = static_cast<long long>(r_usage.ru_nswap);
// ru_inblock  = static_cast<long long>(r_usage.ru_inblock);
// ru_oublock  = static_cast<long long>(r_usage.ru_oublock);
// ru_msgsnd   = static_cast<long long>(r_usage.ru_msgsnd);
// ru_msgrcv   = static_cast<long long>(r_usage.ru_msgrcv);
// ru_nsignals = static_cast<long long>(r_usage.ru_nsignals);

// Format the statistics
//
   return snprintf(bfr, bln, statfmt,
          utime_sec, utime_usec, stime_sec, stime_usec
//        ru_maxrss, ru_majflt, ru_nswap, ru_inblock, ru_oublock,
//        ru_msgsnd, ru_msgrcv, ru_nsignals
         );
}
