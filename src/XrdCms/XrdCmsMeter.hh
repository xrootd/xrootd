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

#include "XrdCms/XrdCmsPerfMon.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucStream.hh"

class XrdCmsMeter : public XrdCmsPerfMon
{
public:

int   calcLoad(uint32_t pcpu, uint32_t pio, uint32_t pload,
               uint32_t pmem, uint32_t ppag);

int   calcLoad(int xload, uint32_t pdsk);

int   FreeSpace(int &tutil);

void  Init();

int   isOn() {return Running;}

int   Monitor(char *pgm, int itv);
int   Monitor(int itv);

void  PutInfo(XrdCmsPerfMon::PerfInfo &perfInfo, bool alert=false);

void  Record(int pcpu, int pnet, int pxeq,
             int pmem, int ppag, int pdsk);

int   Report(int &pcpu, int &pnet, int &pxeq,
             int &pmem, int &ppag, int &pdsk);

void *Run();

void *RunFS();

void *RunPM();

int   numFS() {return fs_nums;}

unsigned int TotalSpace(unsigned int &minfree);

enum  vType {manFS = 1, peerFS = 2};

void  setVirtual(vType vVal) {Virtual = vVal;}

void  setVirtUpdt() {cfsMutex.Lock(); VirtUpdt = 1; cfsMutex.UnLock();}

bool  Update(char *line, bool alert=false);

       XrdCmsMeter();
      ~XrdCmsMeter();

private:
      void calcSpace();
      char Scale(long long inval, long &outval);
      void SpaceMsg(int why);
      void UpdtSpace();

XrdOucStream  myMeter;
XrdSysMutex   cfsMutex;
XrdSysMutex   repMutex;
long long     MinFree;  // Calculated only once
long long     HWMFree;  // Calculated only once
long long     dsk_lpn;  // Calculated only once
long long     dsk_tot;  // Calculated only once
long long     dsk_free;
long long     dsk_maxf;
int           dsk_util;
int           dsk_calc;
int           fs_nums;  // Calculated only once
int           lastFree;
int           lastUtil;
int           noSpace;
int           Running;
long          MinShow;  // Calculated only once
long          HWMShow;  // Calculated only once
char          MinStype; // Calculated only once
char          HWMStype; // Calculated only once
char          Virtual;  // This is a virtual filesystem
char          VirtUpdt; // Data changed for the virtul FS

time_t        rep_tod;
char         *monpgm;
XrdCmsPerfMon *monPerf;
int           monint;
pthread_t     montid;

uint32_t      xeq_load;
uint32_t      cpu_load;
uint32_t      mem_load;
uint32_t      pag_load;
uint32_t      net_load;
int           myLoad;
int           prevLoad;
};

namespace XrdCms
{
extern    XrdCmsMeter Meter;
}
#endif
