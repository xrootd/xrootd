#ifndef __CMS_METER__H
#define __CMS_METER__H
/******************************************************************************/
/*                                                                            */
/*                        X r d C m s M e t e r . h h                         */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucStream.hh"

class XrdCmsMeterFS;
  
class XrdCmsMeter
{
public:

int   calcLoad(int pcpu, int pio, int pload, int pmem, int ppag);

int   calcLoad(int xload,int pdsk);

int   FreeSpace(int &tutil);

int   isOn() {return Running;}

int   Monitor(char *pgm, int itv);

int   Report(int &pcpu, int &pnet, int &pxeq,
             int &pmem, int &ppag, int &pdsk);

void *Run();

void *RunFS();

int   numFS() {return fs_nums;}

void  setParms(XrdOucTList *tlp, int warnDups);

       XrdCmsMeter();
      ~XrdCmsMeter();

private:
      void calcSpace();
      int  isDup(struct stat &buf, XrdCmsMeterFS *baseFS);
const char Scale(long long inval, long &outval);
      void SpaceMsg(int why);

XrdOucStream  myMeter;
XrdSysMutex   cfsMutex;
XrdSysMutex   repMutex;
XrdOucTList  *fs_list;
long long     MinFree;  // Calculated only once
long long     HWMFree;  // Calculated only once
long long     dsk_tot;  // Calculated only once
long long     dsk_free;
long long     dsk_maxf;
int           dsk_util;
int           dsk_calc;
int           fs_nums;  // Calculated only once
int           noSpace;
int           Running;
long          MinShow;  // Calculated only once
long          HWMShow;  // Calculated only once
char          MinStype; // Calculated only once
char          HWMStype; // Calculated only once

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

namespace XrdCms
{
extern    XrdCmsMeter Meter;
}
#endif
