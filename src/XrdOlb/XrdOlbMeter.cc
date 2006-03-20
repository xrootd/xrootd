/******************************************************************************/
/*                                                                            */
/*                        X r d O l b M e t e r . c c                         */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbMeterCVSID = "$Id$";
  
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/vfs.h>
#elif defined( __macos__)
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statvfs.h>
#endif

#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbMeter.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucPthread.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOlbConfig  XrdOlbConfig;
 
extern XrdOucError   XrdOlbSay;

extern XrdOlbManager XrdOlbSM;

extern XrdOucTrace   XrdOlbTrace;


/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOlbMeterRun(void *carg)
      {XrdOlbMeter *mp = (XrdOlbMeter *)carg;
       return mp->Run();
      }

void *XrdOlbMeterRunFS(void *carg)
      {XrdOlbMeter *mp = (XrdOlbMeter *)carg;
       return mp->RunFS();
      }

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdOlbMeterFS
{
public:

XrdOlbMeterFS *Next;
dev_t          Dnum;
ino_t          Inum;

               XrdOlbMeterFS(XrdOlbMeterFS *curP, dev_t dn, ino_t in)
                            {Next = curP; Dnum = dn; Inum = in;}
              ~XrdOlbMeterFS() {}
};

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOlbMeter::XrdOlbMeter() : myMeter(&XrdOlbSay)
{
    Running  = 0;
    fs_list  = 0;
    dsk_calc = 0;
    fs_nums  = 0;
    noSpace  = 0;
    MinFree  = 0;
    HWMFree  = 0;
    dsk_free = 0;
    dsk_maxf = 0;
    monpgm   = 0;
    monint   = 0;
    montid   = 0;
    rep_tod  = 0;
    rep_todfs= 0;
    xeq_load = 0;
    cpu_load = 0;
    mem_load = 0;
    pag_load = 0;
    net_load = 0;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOlbMeter::~XrdOlbMeter()
{
   if (monpgm) free(monpgm);
   if (montid) XrdOucThread::Kill(montid);
}
  
/******************************************************************************/
/*                              c a l c L o a d                               */
/******************************************************************************/

int XrdOlbMeter::calcLoad(int pcpu, int pio, int pload, int pmem, int ppag)
{
   return   (XrdOlbConfig.P_cpu  * pcpu /100)
          + (XrdOlbConfig.P_io   * pio  /100)
          + (XrdOlbConfig.P_load * pload/100)
          + (XrdOlbConfig.P_mem  * pmem /100)
          + (XrdOlbConfig.P_pag  * ppag /100);
}

/******************************************************************************/
/*                             F r e e S p a c e                              */
/******************************************************************************/
  
long XrdOlbMeter::FreeSpace(long &tot_free)
{
   long long fsavail, fstotav;

// The values are calculated periodically so use the last available ones
//
   cfsMutex.Lock();
   fsavail = dsk_maxf;
   fstotav = dsk_free;
   cfsMutex.UnLock();

// Now adjust the values to fit
//
   if (fsavail >> 31) fsavail = 0x7fffffff;
   if (fstotav >> 31) fstotav = 0x7fffffff;

// Set the quantity and return it
//
   tot_free = static_cast<long>(fstotav);
   return static_cast<long>(fsavail);
}

/******************************************************************************/
/*                               M o n i t o r                                */
/******************************************************************************/
  
int XrdOlbMeter::Monitor(char *pgm, int itv)
{
   char *mp, pp;

// Isolate the program name
//
   mp = monpgm = strdup(pgm);
   while(*mp && *mp != ' ') mp++;
   pp = *mp; *mp ='\0';

// Make sure the program is executable by us
//
   if (access(monpgm, X_OK))
      {XrdOlbSay.Emsg("Meter", errno, "find executable", monpgm);
       return -1;
      }

// Start up the program. We don't really need to serialize Restart() because
// Monitor() is a one-time call (otherwise unpredictable results may occur).
//
   *mp = pp; monint = itv;
   XrdOucThread::Run(&montid, XrdOlbMeterRun, (void *)this, 0, "Perf meter");
   Running = 1;
   return 0;
}
 
/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/
  
char *XrdOlbMeter::Report()
{
   long maxfree, totfree;

// Force restart the monitor program if it hasn't reported within 2 intervals
//
   if (montid && (time(0) - rep_tod > monint*2)) myMeter.Drain();

// Format a usage line
//
   repMutex.Lock();
   maxfree = FreeSpace(totfree);
   snprintf(ubuff, sizeof(ubuff), "%d %d %d %d %d %ld %ld",
            cpu_load, net_load, xeq_load, mem_load,
            pag_load, maxfree, totfree);
   repMutex.UnLock();

// All done
//
   return ubuff;
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/
  
void *XrdOlbMeter::Run()
{
   const struct timespec rqtp = {30, 0};
   int i, myLoad, prevLoad = -1;
   char *lp = 0;

// Execute the program (keep restarting and keep reading the output)
//
   while(1)
        {if (myMeter.Exec(monpgm) == 0)
             while((lp = myMeter.GetLine()))
                  {repMutex.Lock();
                   i = sscanf(lp, "%d %d %d %d %d",
                       &xeq_load, &cpu_load, &mem_load, &pag_load, &net_load);
                   rep_tod = time(0);
                   repMutex.UnLock();
                   if (i != 5) break;
                   myLoad = calcLoad(cpu_load,net_load,xeq_load,mem_load,pag_load);
                   if (prevLoad >= 0)
                      {prevLoad = prevLoad - myLoad;
                       if (prevLoad < 0) prevLoad = -prevLoad;
                       if (prevLoad > XrdOlbConfig.P_fuzz) informLoad();
                      }
                   prevLoad = myLoad;
                  }
         if (lp) XrdOlbSay.Emsg("Meter","Perf monitor returned invalid output:",lp);
            else XrdOlbSay.Emsg("Meter","Perf monitor died.");
         nanosleep(&rqtp, 0);
         XrdOlbSay.Emsg("Meter", "Restarting monitor:", monpgm);
        }
   return (void *)0;
}

/******************************************************************************/
/*                                 r u n F S                                  */
/******************************************************************************/
  
void *XrdOlbMeter::RunFS()
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
             XrdOlbSM.Space(noSpace);
            }
            else if (noSpace && !nowlim) SpaceMsg(noNewSpace);
         nowlim = (nowlim ? nowlim-1 : mlim);
        }
   return (void *)0;
}

/******************************************************************************/
/*                              s e t P a r m s                               */
/******************************************************************************/
  
void  XrdOlbMeter::setParms(XrdOucTList *tlp)
{
    pthread_t monFStid;
    XrdOlbMeterFS *fsP, baseFS(0,0,0);
    XrdOucTList *plp, *nlp;
    char buff[1024], sfx1, sfx2;
    long maxfree, totfree;
    struct stat buf;
    int rc;

// Set values (disk space values are in kilobytes)
// 
    fs_list = tlp; 
    MinFree = XrdOlbConfig.DiskMin;
    HWMFree = XrdOlbConfig.DiskHWM;
    dsk_calc = (XrdOlbConfig.DiskAsk < 5 ? 5 : XrdOlbConfig.DiskAsk);

// Calculate number of filesystems without duplication
//
    fs_nums = 0; plp = 0;
    if ((nlp = tlp)) do
       {if ((rc = stat(nlp->text, &buf)) || isDup(buf, &baseFS))
           {XrdOucTList *xlp = nlp->next;
            const char *fault = (rc ? "Missing filesystem '"
                                    : "Duplicate filesystem '");
            XrdOlbSay.Emsg("Meter", fault, nlp->text, "' skipped for free space.");
            if (plp) plp->next = xlp;
               else  fs_list   = xlp;
            delete nlp;
            if ((nlp = xlp)) continue;
            break;
           } else fs_nums++;
       } while((nlp = nlp->next));

// Calculate the initial free space and start the FS monitor thread
//
   if (!fs_nums) 
      {noSpace = 1;
       XrdOlbSM.Space(1,0);
       XrdOlbSay.Emsg("Meter", "Warning! No writable filesystems found; "
                            "write access and staging prohibited.");
      } else {
       calcSpace();
       if ((noSpace = (dsk_maxf < MinFree))) XrdOlbSM.Space(1,0);
       XrdOucThread::Run(&monFStid,XrdOlbMeterRunFS,(void *)this,0,"FS meter");
      }

// Delete any additional MeterFS objects we allocated
//
   while((fsP = baseFS.Next)) {baseFS.Next = fsP->Next; delete fsP;}

// Document what we have
//
   if (fs_nums)
      {sfx1 = Scale(dsk_maxf, maxfree);
       sfx2 = Scale(dsk_free, totfree);
       sprintf(buff,"Found %d filesystem(s); %ld%c total bytes free; %ld%c available",
                    fs_nums, totfree, sfx2, maxfree, sfx1);
       XrdOlbSay.Emsg("Meter", buff);
       if (noSpace)
          {sprintf(buff, "%lldK minimum", MinFree);
           XrdOlbSay.Emsg("Meter", "Warning! Available space <", buff);
          }
      }
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                            i n f o r m L o a d                             */
/******************************************************************************/
  
void XrdOlbMeter::informLoad()
{
   long maxfree, totfree;
   char mybuff[64];
   int i;

   maxfree = FreeSpace(totfree);
   i = snprintf(mybuff, sizeof(mybuff), "load %d %d %d %d %d %ld %ld\n",
                cpu_load, net_load, xeq_load, mem_load,
                pag_load, maxfree, totfree);

   XrdOlbSM.Inform(mybuff, i);
}

/******************************************************************************/
/*                                 i s D u p                                  */
/******************************************************************************/
  
int XrdOlbMeter::isDup(struct stat &buf, XrdOlbMeterFS *baseFS)
{
  XrdOlbMeterFS *fsp = baseFS->Next;

// Search for matching filesystem
//
   while(fsp) if (fsp->Dnum == buf.st_dev && fsp->Inum == buf.st_ino) return 1;
                 else fsp = fsp->Next;

// New filesystem
//
   baseFS->Next = new XrdOlbMeterFS(baseFS->Next, buf.st_dev, buf.st_ino);
   return 0;
}

/******************************************************************************/
/*                             c a l c S p a c e                              */
/******************************************************************************/
  
void XrdOlbMeter::calcSpace()
{
   EPNAME("calcSpace")
   long long fsbsize;
   long long bytes, fsavail = 0, fstotav = 0;
   XrdOucTList *tlp = fs_list;
   STATFS_BUFF fsdata;

// For each file system, do a statvfs() or equivalent. We define free space
// as the largest amount available in one filesystem since we can't allocate
// across filesystems. The correct filesystem blocksize is very OS specific
//
   while(tlp)
        {if (!STATFS(tlp->text, &fsdata))
#if defined(__solaris__) || defined(_STATFS_F_FRSIZE)
            {fsbsize = (fsdata.f_frsize ? fsdata.f_frsize : fsdata.f_bsize);
#else
            {fsbsize = fsdata.f_bsize;
#endif
             bytes = fsdata.f_bavail * ( fsbsize ?  fsbsize : FS_BLKFACT);
             if (bytes >= MinFree)
                {fstotav += bytes;
                 if (bytes > fsavail) fsavail = bytes;
                }
            }
         tlp = tlp->next;
        }

// Update the stats and release the lock
//
   cfsMutex.Lock();
   bytes    = dsk_maxf;
   dsk_maxf = fsavail/1024;
   dsk_free = fstotav/1024;
   cfsMutex.UnLock();
   if (bytes != dsk_maxf)
      DEBUG("New fs info; maxfree=" <<dsk_maxf <<"K totfree=" <<dsk_free <<"K");
}

/******************************************************************************/
/*                                 S c a l e                                  */
/******************************************************************************/
  
const char XrdOlbMeter::Scale(long long inval, long &outval)
{
    const char sfx[] = {'K', 'M', 'G', 'T', 'P'};
    unsigned int i;

    for (i = 0; i < sizeof(sfx) && inval > 9999; i++) inval = inval/1024;

    outval = static_cast<long>(inval);
    return sfx[i];
}
 
/******************************************************************************/
/*                              S p a c e M s g                               */
/******************************************************************************/

void XrdOlbMeter::SpaceMsg(int why)
{
   char buff[1024];
   if (why)
      sprintf(buff, "Insufficient space; %lldK available < %lld %s",
                    dsk_maxf, (noSpace ? HWMFree : MinFree),
                              (noSpace ? "high watermark" : "minimum"));
      else 
      sprintf(buff, "  Sufficient space; %lldK available > %lldK high watermak",
                    dsk_maxf, HWMFree);
      XrdOlbSay.Emsg("Meter", buff);
}
