#ifndef __OLB_METER__H
#define __OLB_METER__H
/******************************************************************************/
/*                                                                            */
/*                        X r d O l b M e t e r . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucStream.hh"
  
class XrdOlbMeter
{
public:

static long  FreeSpace(long &totfree);

       int   Monitor(char *pgm, int itv);

       char *Report();

       void *Run();

static int   numFS() {return fs_nums;}

static void  setParms(XrdOucTList *tlp, int mfr, int itv);

       XrdOlbMeter(XrdOucError *errp);
      ~XrdOlbMeter();

private:
       XrdOucError   *eDest;
       XrdOucStream   myMeter;
static XrdOucMutex    repMutex;
static XrdOucTList   *fs_list;
static int            dsk_calc;
static int            fs_nums;
static int            MinFree;
static long long      dsk_free;
static long long      dsk_maxf;

char          ubuff[64];
time_t        rep_tod;
time_t        rep_todfs;
char         *monpgm;
int           monint;
pthread_t     montid;

unsigned int  xeq_load;
unsigned int  cpu_load;
unsigned int  mem_load;
unsigned int  pag_load;
unsigned int  net_load;
};
#endif
