/******************************************************************************/
/*                                                                            */
/*                           X r d S t a t s . c c                            */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$ 

const char *XrdStatsCVSID = "$Id$";

#ifndef __macos__
#include <malloc.h>
#endif
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
  
#include "XrdVersion.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdProtocol.hh"
#include "Xrd/XrdStats.hh"
#include "XrdOuc/XrdOucPlatform.hh"

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

extern XrdBuffManager    XrdBuffPool;

extern XrdScheduler      XrdSched;

/******************************************************************************/
/*                           C o n s t r c u t o r                            */
/******************************************************************************/
  
XrdStats::XrdStats(char *hname, int port)
{
   myHost = hname;
   myPort = port;
   myPid  = getpid();
   buff   = 0;   // Allocated on first Stats() call
   blen   = 0;
}
 
/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
char *XrdStats::Stats(int opts)   // statsMutex must be locked!
{
#define XRDSHEAD "<statistics tod=\"%ld\" ver=\"" XrdVSTRING "\">"

   static XrdProtocol_Select Protocols;
   static const char head[] = XRDSHEAD;
   static const char tail[] = "</statistics>";
   static const int  ovrhed = strlen(head)+16+sizeof(XrdVSTRING)+strlen(tail);
   char *bp;
   int   bl, sz, do_sync = opts & XRD_STATS_SYNC;

// If buffer is not allocated, do it now. We must defer buffer allocation
// until all components that can provide statistics have been loaded
//
   if (!(bp = buff))
      {bl = Protocols.Stats(0,0) + ovrhed;
       if (getBuff(bl)) bp = buff;
          else return (char *)XRDSHEAD "</<statistics>";
      }
   bl = blen;

// Insert the heading
//
   sz = sprintf(buff, head, static_cast<long>(time(0)));
   bl -= sz; bp += sz;

// Extract out the statistics, as needed
//
   if (opts & XRD_STATS_INFO)
      {sz = InfoStats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_BUFF)
      {sz = XrdBuffPool.Stats(bp, bl, do_sync);
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
      {sz = Protocols.Stats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   if (opts & XRD_STATS_SCHD)
      {sz = XrdSched.Stats(bp, bl, do_sync);
       bp += sz; bl -= sz;
      }

   strcpy(bp, tail);
   return buff;
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               g e t B u f f                                */
/******************************************************************************/
  
int XrdStats::getBuff(int xtra)
{

// Calculate the number of bytes needed for all stats
//
   blen  = xtra;
   blen += XrdBuffPool.Stats(0,0);
   blen += InfoStats(0,0);
   blen += XrdLink::Stats(0,0);
   blen += XrdPoll::Stats(0,0);
   blen += ProcStats(0,0);
   blen += XrdSched.Stats(0,0);

// Allocate a buffer of this size
//
   buff = (char *)memalign(getpagesize(), blen+256);
   return buff != 0;
}

/******************************************************************************/
/*                             I n f o S t a t s                              */
/******************************************************************************/
  
int XrdStats::InfoStats(char *bfr, int bln, int do_sync)
{
   static const char statfmt[] = "<stats id=\"info\"><host>%s</host>"
                     "<port>%d</port></stats>";

// Check if actual length wanted
//
   if (!bfr) return sizeof(statfmt)+16 + strlen(myHost);

// Format the statistics
//
   return snprintf(bfr, bln, statfmt, myHost, myPort);
}
 
/******************************************************************************/
/*                             P r o c S t a t s                              */
/******************************************************************************/
  
int XrdStats::ProcStats(char *bfr, int bln, int do_sync)
{
   static const char statfmt[] = "<stats id=\"proc\"><pid>%d</pid>"
          "<utime><s>%ld</s><u>%ld</u></utime>"
          "<stime><s>%ld</s><u>%ld</u></stime>"
          "<maxrss>%ld</maxrss><majflt>%ld</majflt><nswap>%ld</nswap>"
          "<inblock>%ld</inblock><oublock>%ld</oublock>"
          "<msgsnd>%ld</msgsnd><msgrcv>%ld</msgrcv>"
          "<nsignals>%ld</nsignals></stats>";
   struct rusage r_usage;
   long utime_sec, utime_usec, stime_sec, stime_usec;

// Check if actual length wanted
//
   if (!bfr) return sizeof(statfmt)+16*13;

// Get the statistics
//
   if (getrusage(RUSAGE_SELF, &r_usage)) return 0;

// Convert fields to correspond to the format we are using
//
   utime_sec  = static_cast<long>(r_usage.ru_utime.tv_sec);
   utime_usec = static_cast<long>(r_usage.ru_utime.tv_usec);
   stime_sec  = static_cast<long>(r_usage.ru_stime.tv_sec);
   stime_usec = static_cast<long>(r_usage.ru_stime.tv_usec);

// Format the statistics
//
   return snprintf(bfr, bln, statfmt, myPid,
          utime_sec, utime_usec, stime_sec, stime_usec,
          r_usage.ru_maxrss, r_usage.ru_majflt, r_usage.ru_nswap,
          r_usage.ru_inblock, r_usage.ru_oublock,
          r_usage.ru_msgsnd, r_usage.ru_msgrcv, r_usage.ru_nsignals);
}
