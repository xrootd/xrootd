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

#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <malloc.h>
#endif
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
  
#include "XrdVersion.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdProtLoad.hh"
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdStats.hh"
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
   static const char *head =
          "<statistics tod=\"%%ld\" ver=\"" XrdVERSION "\" src=\"%s:%d\" "
                      "tos=\"%ld\" pgm=\"%s\" ins=\"%s\" pid=\"%d\" "
                      "site=\"%s\">";
   char myBuff[1024];

   XrdLog   = eP;
   XrdSched = sP;
   BuffPool = bP;

   Hlen = sprintf(myBuff, head, hname, port, tBoot, pname, iname,
                          static_cast<int>(getpid()), (site ? site : ""));
   Head = strdup(myBuff);
   buff = 0;
   blen = 0;
   myHost = hname;
   myName = iname;
   myPort = port;
}
 
/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/
  
void XrdStats::Report(char **Dest, int iVal, int Opts)
{
   static XrdNetMsg *netDest[2] = {0,0};
   static int autoSync, repOpts = Opts;
   const char *Data;
          int theOpts, Dlen;

// If we have dest then this is for initialization
//
   if (Dest)
   // Establish up to two destinations
   //
      {if (Dest[0]) netDest[0] = new XrdNetMsg(XrdLog, Dest[0]);
       if (Dest[1]) netDest[1] = new XrdNetMsg(XrdLog, Dest[1]);
       if (!(repOpts & XRD_STATS_ALL)) repOpts |= XRD_STATS_ALL;
       autoSync = repOpts & XRD_STATS_SYNCA;

   // Get and schedule a new job to report
   //
      if (netDest[0]) new XrdStatsJob(XrdSched, this, iVal);
       return;
      }

// This is a re-entry for reporting purposes, establish the sync flag
//
   if (!autoSync || XrdSched->Active() <= 30) theOpts = repOpts;
      else theOpts = repOpts & ~XRD_STATS_SYNC;

// Now get the statistics
//
   statsMutex.Lock();
   if ((Data = GenStats(Dlen, theOpts)))
      {netDest[0]->Send(Data, Dlen);
       if (netDest[1]) netDest[1]->Send(Data, Dlen);
      }
   statsMutex.UnLock();
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

void XrdStats::Stats(XrdStats::CallBack *cbP, int opts)
{
   const char *info;
   int sz;

// Lock the buffer,
//
   statsMutex.Lock();

// Obtain the stats, if we have some, do the callback
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

// If buffer is not allocated, do it now. We must defer buffer allocation
// until all components that can provide statistics have been loaded
//
   if (!(bp = buff))
      {blen = InfoStats(0,0) + BuffPool->Stats(0,0) + XrdLink::Stats(0,0)
            + ProcStats(0,0) + XrdSched->Stats(0,0) + XrdPoll::Stats(0,0)
            + XrdProtLoad::Statistics(0,0) + ovrhed + Hlen;
       buff = (char *)memalign(getpagesize(), blen+256);
       if (!(bp = buff)) {rsz = snulsz; return snul;}
      }
   bl = blen;

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

   sz = bp - buff;
   if (bl > 0) n = strlcpy(bp, tail, bl);
      else n = 0;
   rsz = sz + (n >= bl ? bl : n);
   return buff;
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
