#ifndef __XRD_STATS_H__
#define __XRD_STATS_H__
/******************************************************************************/
/*                                                                            */
/*                           x r d _ S t a t s . h                            */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

#include <stdlib.h>

#include "XrdOuc/XrdOucPthread.hh"

#define XRD_STATS_ALL  0x00FF
#define XRD_STATS_INFO 0x0001
#define XRD_STATS_BUFF 0x0002
#define XRD_STATS_LINK 0x0004
#define XRD_STATS_POLL 0x0008
#define XRD_STATS_PROC 0x0010
#define XRD_STATS_PROT 0x0020
#define XRD_STATS_SCHD 0x0040
  
class XrdStats
{
public:

void  Lock() {statsMutex.Lock();}       // Call before doing Stats()

char *Stats(int opts);

void  UnLock() {statsMutex.UnLock();}   // Call after inspecting buffer

      XrdStats(char *hn, int port);
     ~XrdStats() {if (buff) free(buff);}

private:

int        getBuff(int xtra);
int        InfoStats(char *buff, int blen);
int        ProcStats(char *buff, int blen);

XrdOucMutex statsMutex;

char      *buff;        // Used by all callers
int        blen;
char      *myHost;
int        myPort;
int        myPid;
};
#endif
