/******************************************************************************/
/*                                                                            */
/*                        X r d C m s M e t e r . c c                         */
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
  
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsConfig.hh"
#include "XrdCms/XrdCmsMeter.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsState.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdCms/XrdCmsUtils.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdCms;
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

       XrdCmsMeter   XrdCms::Meter;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/

namespace
{
void *MeterRun(void *carg)
      {XrdCmsMeter *mp = (XrdCmsMeter *)carg;
       return mp->Run();
      }

void *MeterRunFS(void *carg)
      {XrdCmsMeter *mp = (XrdCmsMeter *)carg;
       return mp->RunFS();
      }

void *MeterRunPM(void *carg)
      {XrdCmsMeter *mp = (XrdCmsMeter *)carg;
       return mp->RunPM();
      }
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdCmsMeter::XrdCmsMeter() : myMeter(&Say)
{
    Running  = 0;
    dsk_calc = 0;
    fs_nums  = 0;
    noSpace  = 0;
    MinFree  = 0;
    HWMFree  = 0;
    dsk_lpn  = 0;
    dsk_tot  = 0;
    dsk_free = 0;
    dsk_maxf = 0;
    lastFree = 0;
    lastUtil = 0;
    monpgm   = 0;
    monPerf  = 0;
    monint   = 0;
    montid   = 0;
    rep_tod  = time(0);
    xeq_load = 0;
    cpu_load = 0;
    mem_load = 0;
    pag_load = 0;
    net_load = 0;
    myLoad   = 0;
    prevLoad = -1;
    Virtual  = 0;
    VirtUpdt = 1;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdCmsMeter::~XrdCmsMeter()
{
   if (monpgm) free(monpgm);
   if (montid) XrdSysThread::Kill(montid);
}
  
/******************************************************************************/
/*                              c a l c L o a d                               */
/******************************************************************************/

int XrdCmsMeter::calcLoad(uint32_t pcpu, uint32_t pio, uint32_t pload,
                          uint32_t pmem, uint32_t ppag)
{
   if (pcpu  > 100) pcpu  = 100;
   if (pio   > 100) pio   = 100;
   if (pload > 100) pload = 100;
   if (pmem  > 100) pmem  = 100;
   if (ppag  > 100) ppag  = 100;

   return   (Config.P_cpu  * pcpu /100)
          + (Config.P_io   * pio  /100)
          + (Config.P_load * pload/100)
          + (Config.P_mem  * pmem /100)
          + (Config.P_pag  * ppag /100);
}

/******************************************************************************/

int XrdCmsMeter::calcLoad(int nowload, uint32_t pdsk)
{
   if (pdsk > 100) pdsk = 100;
   return   (Config.P_dsk  * pdsk /100) + nowload;
}

/******************************************************************************/
/*                             F r e e S p a c e                              */
/******************************************************************************/
  
int XrdCmsMeter::FreeSpace(int &tot_util)
{
   long long fsavail;

// If we are a virtual filesystem, do virtual stats
//
   if (Virtual)
      {if (Virtual == peerFS) {tot_util = 0; return 0x7fffffff;}
       if (VirtUpdt) UpdtSpace();
       tot_util = lastUtil;
       return lastFree;
      }

// The values are calculated periodically so use the last available ones
//
   cfsMutex.Lock();
   fsavail = dsk_maxf;
   tot_util= dsk_util;
   cfsMutex.UnLock();

// Now adjust the values to fit
//
   if (fsavail >> 31LL) fsavail = 0x7fffffff;

// Return amount available
//
   return static_cast<int>(fsavail);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
void  XrdCmsMeter::Init()
{
    XrdOssVSInfo vsInfo;
    pthread_t monFStid;
    char buff[1024], sfx1, sfx2, sfx3;
    long maxfree, totfree, totDisk;
    int rc;

// Get initial free space
//
        if ((rc = Config.ossFS->StatVS(&vsInfo, 0, 1)))
           {Say.Emsg("Meter", rc, "calculate file system space");
            noSpace = 1;
           }
   else if (!(fs_nums = vsInfo.Extents))
           {Say.Emsg("Meter", "Warning! No writable filesystems found.");
            noSpace = 1;
           }
   else    {dsk_tot = vsInfo.Total >> 20LL; // in MB
            dsk_lpn = vsInfo.Large >> 20LL;
           }

// Check if we should bother to continue
//
   if (noSpace)
      {if (!Config.asSolo()) CmsState.Update(XrdCmsState::Space, 0);
       Say.Emsg("Meter", "Write access and staging prohibited.");
       return;
      }

// Set values (disk space values are in megabytes)
// 
    if (Config.DiskMinP) MinFree = dsk_lpn * Config.DiskMinP / 100;
    if (Config.DiskMin > MinFree) MinFree  = Config.DiskMin;
    MinStype= Scale(MinFree, MinShow);
    if (Config.DiskHWMP) HWMFree = dsk_lpn * Config.DiskHWMP / 100;
    if (Config.DiskHWM > HWMFree) HWMFree  = Config.DiskHWM;
    HWMStype= Scale(HWMFree, HWMShow);
    dsk_calc = (Config.DiskAsk < 5 ? 5 : Config.DiskAsk);

// Calculate the initial free space and start the FS monitor thread
//
   calcSpace();
   if ((noSpace = (dsk_maxf < MinFree)) && !Config.asSolo())
      CmsState.Update(XrdCmsState::Space, 0);
   if ((rc = XrdSysThread::Run(&monFStid,MeterRunFS,(void *)this,0,"FS meter")))
      Say.Emsg("Meter", rc, "start filesystem meter.");

// Document what we have
//
   sfx1 = Scale(dsk_maxf, maxfree);
   sfx2 = Scale(dsk_tot,  totDisk);
   sfx3 = Scale(dsk_free, totfree);
   sprintf(buff,"Found %d filesystem(s); %ld%cB total (%d%% util);"
                " %ld%cB free (%ld%cB max)", fs_nums, totDisk, sfx2,
                dsk_util, totfree, sfx3, maxfree, sfx1);
   Say.Emsg("Meter", buff);
   if (noSpace)
      {sprintf(buff, "%ld%cB minimum", MinShow, MinStype);
       Say.Emsg("Meter", "Warning! Available space <", buff);
      }
}

/******************************************************************************/
/*                               M o n i t o r                                */
/******************************************************************************/
  
int XrdCmsMeter::Monitor(char *pgm, int itv)
{
   char *mp, pp;
   int rc;

// Isolate the program name
//
   mp = monpgm = strdup(pgm);
   while(*mp && *mp != ' ') mp++;
   pp = *mp; *mp ='\0';

// Make sure the program is executable by us
//
   if (access(monpgm, X_OK))
      {Say.Emsg("Meter", errno, "find executable", monpgm);
       return -1;
      }

// Start up the program. We don't really need to serialize Restart() because
// Monitor() is a one-time call (otherwise unpredictable results may occur).
//
   *mp = pp; monint = itv;
   if ((rc = XrdSysThread::Run(&montid,MeterRun,(void *)this,0,"Perf meter")))
      Say.Emsg("Meter", rc, "start performance meter.");
   Running = 1;
   return 0;
}

/******************************************************************************/
  
int XrdCmsMeter::Monitor(int itv)
{
   XrdCmsPerfMon *monPerf;
   int rc;

// Load the plugin
//
   monPerf = XrdCmsUtils::loadPerfMon(&Say, Config.prfLib, *Config.myVInfo);

// Configure it if loaded.
//
   if (!monPerf || !monPerf->Configure(Config.ConfigFN, Config.prfParms,
                                       *(Say.logger()), *this, 0, true))
      {Say.Emsg("Meter", "Unable to configure performance monitor plugin.");
       return -1;
      }

// Start the monitor thread for this plugin unless strictly async
// reporting is wanted (i.e. the interval is zero).
//
   if (monint)
      {if ((rc = XrdSysThread::Run(&montid,MeterRunPM,(void *)this,0,"Perf monitor")))
          {Say.Emsg("Meter", rc, "start performance meter.");
           return -1;
          }
      }

   montid = 0;
   Running = 1;
   return 0;
}

/******************************************************************************/
/*                               P u t I n f o                                */
/******************************************************************************/
  
void XrdCmsMeter::PutInfo(XrdCmsPerfMon::PerfInfo &perfInfo, bool alert)
{

   repMutex.Lock();
   cpu_load = (perfInfo.cpu_load <= 100 ? perfInfo.cpu_load : 100);
   mem_load = (perfInfo.mem_load <= 100 ? perfInfo.mem_load : 100);
   net_load = (perfInfo.net_load <= 100 ? perfInfo.net_load : 100);
   pag_load = (perfInfo.pag_load <= 100 ? perfInfo.pag_load : 100);
   xeq_load = (perfInfo.xeq_load <= 100 ? perfInfo.xeq_load : 100);

   myLoad = calcLoad(cpu_load,net_load,xeq_load,mem_load,pag_load);

   if (prevLoad >= 0)
      {prevLoad = prevLoad - myLoad;
       if (prevLoad < 0) prevLoad = -prevLoad;
       if (prevLoad > Config.P_fuzz) alert = true;
      }
   prevLoad = myLoad;
   repMutex.UnLock();

   if (alert) XrdCmsNode::Report_Usage(0);
}

/******************************************************************************/
/*                                R e c o r d                                 */
/******************************************************************************/
  
void XrdCmsMeter::Record(int pcpu, int pnet, int pxeq,
                         int pmem, int ppag, int pdsk)
{
   int temp;

   repMutex.Lock();
   temp = cpu_load + cpu_load/2;
   cpu_load = (cpu_load + (pcpu > temp ? temp : pcpu))/2;
   temp = net_load + net_load/2;
   net_load = (net_load + (pnet > temp ? temp : pnet))/2;
   temp = xeq_load + xeq_load/2;
   xeq_load = (xeq_load + (pxeq > temp ? temp : pxeq))/2;
   temp = mem_load + mem_load/2;
   mem_load = (mem_load + (pmem > temp ? temp : pmem))/2;
   temp = pag_load + pag_load/2;
   pag_load = (pag_load + (ppag > temp ? temp : ppag))/2;
   repMutex.UnLock();
}
 
/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/
  
int XrdCmsMeter::Report(int &pcpu, int &pnet, int &pxeq, 
                        int &pmem, int &ppag, int &pdsk)
{
   int maxfree;

// Force restart the monitor program if it hasn't reported within 2 intervals
//
   if (!Virtual && montid && (time(0) - rep_tod > monint*2)) myMeter.Drain();

// Format a usage line
//
   repMutex.Lock();
   maxfree = FreeSpace(pdsk);
   if (!Running && !Virtual) pcpu = pnet = pmem = ppag = pxeq = 0;
      else {pcpu = cpu_load; pnet = net_load; pmem = mem_load; 
            ppag = pag_load; pxeq = xeq_load;
           }
   repMutex.UnLock();

// All done
//
   return maxfree;
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/
  
void *XrdCmsMeter::Run()
{
   static const int snoozeTime = 30;
   char *lp = 0;

// Execute the program (keep restarting and keep reading the output)
//
   while(1)
        {if (myMeter.Exec(monpgm) == 0)
             while((lp = myMeter.GetLine()) && Update(lp)) {}
         if (lp) Say.Emsg("Meter","Perf monitor returned invalid output:",lp);
            else Say.Emsg("Meter","Perf monitor died.");
         XrdSysTimer::Snooze(snoozeTime);
         Say.Emsg("Meter", "Restarting monitor:", monpgm);
        }
   return (void *)0;
}

/******************************************************************************/
/*                                 r u n F S                                  */
/******************************************************************************/
  
void *XrdCmsMeter::RunFS()
{
   const struct timespec rqtp = {dsk_calc, 0};
   int noNewSpace;
   int mlim = 60/dsk_calc, nowlim = 0;
  
   while(1)
        {nanosleep(&rqtp, 0);
         calcSpace();
         noNewSpace = dsk_maxf < (noSpace ? HWMFree : MinFree);
         if (noSpace != noNewSpace)
            {SpaceMsg(noNewSpace);
             noSpace = noNewSpace;
             if (!Config.asSolo()) CmsState.Update(XrdCmsState::Space,!noSpace);
            }
            else if (noSpace && !nowlim) SpaceMsg(noNewSpace);
         nowlim = (nowlim ? nowlim-1 : mlim);
        }
   return (void *)0;
}
  
/******************************************************************************/
/*                                 R u n P M                                  */
/******************************************************************************/
  
void *XrdCmsMeter::RunPM()
{
   XrdCmsPerfMon::PerfInfo perfInfo;

// Keep asking the plugin for statistics.
//
   while(1)
        {monPerf->GetInfo(perfInfo);
         PutInfo(perfInfo);
         perfInfo.Clear();
         XrdSysTimer::Snooze(monint);
        }
   return (void *)0;
}
  
/******************************************************************************/
/*                            T o t a l S p a c e                             */
/******************************************************************************/

unsigned int XrdCmsMeter::TotalSpace(unsigned int &minfree)
{
   long long fstotal, fsminfr;

// If we are a virtual filesystem, do virtual stats
//
   if (Virtual)
      {if (Virtual == peerFS) {minfree = 0; return 0x7fffffff;}
       if (VirtUpdt) UpdtSpace();
      }

// The values are calculated periodically so use the last available ones
//
   cfsMutex.Lock();
   fstotal = dsk_tot;
   fsminfr = MinFree;
   cfsMutex.UnLock();

// Now adjust the values to fit
//
   if (fsminfr >> 31LL) minfree = 0x7fffffff;
      else              minfree = static_cast<unsigned int>(fsminfr);
   if (fstotal == 0) fstotal = 1;
      else if (fstotal >> 31LL) fstotal = 0x7fffffff;

// Return amount available
//
   return static_cast<unsigned int>(fstotal);
}
  
/******************************************************************************/
/*                                U p d a t e                                 */
/******************************************************************************/

bool XrdCmsMeter::Update(char *line, bool alert)
{
   int n;

// Parse the information
//
   repMutex.Lock();
   n = sscanf(line, "%u %u %u %u %u",
       &xeq_load, &cpu_load, &mem_load, &pag_load, &net_load);
   rep_tod = time(0);

// Make sure we have the correct number here
//
   if (n != 5)
      {repMutex.UnLock();
       return false;
      }

// Calculate load and check if there has been a significant change.
//
   myLoad = calcLoad(cpu_load,net_load,xeq_load,mem_load,pag_load);
   if (prevLoad >= 0)
      {prevLoad = prevLoad - myLoad;
       if (prevLoad < 0) prevLoad = -prevLoad;
       if (prevLoad > Config.P_fuzz) alert = true;
      }
   prevLoad = myLoad;
   repMutex.UnLock();

// Do an immediate performance update if needed
//
   if (alert) XrdCmsNode::Report_Usage(0);
   return true;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                             c a l c S p a c e                              */
/******************************************************************************/
  
void XrdCmsMeter::calcSpace()
{
   EPNAME("calcSpace")
   XrdOssVSInfo vsInfo;
   int       old_util, rc;
   long long fsutil;

// Get free space statistics. On error, all fields will be zero, which is
// what we really want to kill space allocation. Note that some DFS's are
// unreliable (e.g. Lustre) and the total space may be returned as zero.
// If so, we wait a second and try again, up to 3 times.
//
   for (int i = 0; i < 3; i++)
       {if ((rc = Config.ossFS->StatVS(&vsInfo, 0, 1)))
           {Say.Emsg("Meter", rc, "calculate file system space"); break;}
        if (vsInfo.Total) break;
        XrdSysTimer::Snooze(1);
       }

// Calculate the disk utilization (note that dsk_tot is in MB)
//
   fsutil = (dsk_tot ? 100-(((vsInfo.Free >> 20LL)*100)/dsk_tot) : 100);
   if (fsutil < 0) fsutil = 0;
      else if (fsutil > 100) fsutil = 100;

// Update the stats and release the lock
//
   cfsMutex.Lock();
   old_util = dsk_util;
   dsk_maxf = vsInfo.LFree >> 20LL; // In MB
   dsk_free = vsInfo.Free  >> 20LL; // In MB
   dsk_util = static_cast<int>(fsutil);
   cfsMutex.UnLock();
   if (old_util != dsk_util)
      TRACE(Space, "New fs info; maxfree=" <<dsk_maxf
                   <<"MB utilized=" <<dsk_util <<"%");
}

/******************************************************************************/
/*                                 S c a l e                                  */
/******************************************************************************/
  
// Note: Input quantity is always in megabytes!

char XrdCmsMeter::Scale(long long inval, long &outval)
{
    const char sfx[] = {'M', 'G', 'T', 'P'};
    unsigned int i;

    for (i = 0; i < sizeof(sfx)-1 && inval > 1024; i++) inval = inval/1024;

    outval = static_cast<long>(inval);
    return sfx[i];
}
 
/******************************************************************************/
/*                              S p a c e M s g                               */
/******************************************************************************/

void XrdCmsMeter::SpaceMsg(int why)
{
   const char *What;
   char sfx, buff[1024];
   long maxfree;

   sfx = Scale(dsk_maxf, maxfree);

   if (why)
      {What = "Insufficient space; ";
       if (noSpace)
          sprintf(buff, "%ld%cB available < %ld%cB high watermark",
                                maxfree, sfx, HWMShow, HWMStype);
          else
          sprintf(buff, "%ld%cB available < %ld%cB minimum",
                                maxfree, sfx, MinShow, MinStype);
      } else {
       What = "  Sufficient space; ";
       sprintf(buff, "%ld%cB available > %ld%cB high watermak",
                                maxfree, sfx, HWMShow, HWMStype);
      }
      Say.Emsg("Meter", What, buff);
}

/******************************************************************************/
/*                             U p d t S p a c e                              */
/******************************************************************************/
  
void XrdCmsMeter::UpdtSpace()
{
   static const SMask_t allNodes(~0);
   SpaceData mySpace;

// Get new space values for the cluser
//
   Cluster.Space(mySpace, allNodes);

// Update out local information
//
   cfsMutex.Lock();
   if (mySpace.wFree > mySpace.sFree)
      {lastFree = mySpace.wFree; lastUtil = mySpace.wUtil;
      } else {
       lastFree = mySpace.sFree; lastUtil = mySpace.sUtil;
      }
   dsk_tot = static_cast<long long>(mySpace.Total)<<10LL; // In MB
   MinFree = mySpace.wMinF;
   VirtUpdt = 0;
   cfsMutex.UnLock();
}
