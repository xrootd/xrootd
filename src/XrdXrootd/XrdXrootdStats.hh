#ifndef __XROOTD_STATS_H__
#define __XROOTD_STATS_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d S t a t s . h h                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//      $Id$

#include "XrdOuc/XrdOucPthread.hh"

class XrdStats;
class XrdXrootdResponse;

class XrdXrootdStats
{
public:
long             Count;        // Stats: Number of matches
long             errorCnt;     // Stats: Number of errors returned
long             redirCnt;     // Stats: Number of redirects
long             stallCnt;     // Stats: Number of stalls
long             getfCnt;      // Stats: Number of getfiles
long             putfCnt;      // Stats: Number of putfiles
long             openCnt;      // Stats: Number of opens
long             readCnt;      // Stats: Number of reads
long             prerCnt;      // Stats: Number of reads (pre)
long             writeCnt;     // Stats: Number of writes
long             syncCnt;      // Stats: Number of sync
long             miscCnt;      // Stats: Number of miscellaneous
long             AsyncNum;     // Stats: Number of async ops
long             AsyncMax;     // Stats: Number of async max
long             AsyncRej;     // Stats: Number of async rejected
long             AsyncNow;     // Stats: Number of async now (not locked)
long             Refresh;      // Stats: Number of refresh requests

XrdOucMutex      statsMutex;   // Mutex to serialize updates

int              Stats(char *buff, int blen, int do_sync=0);

int              Stats(XrdXrootdResponse &resp, char *opts);

                 XrdXrootdStats(XrdStats *sp);
                ~XrdXrootdStats() {}
private:

XrdStats *xstats;
};
#endif
