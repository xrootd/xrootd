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
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/vfs.h>
#else
#include <sys/statvfs.h>
#endif

#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOlb/XrdOlbMeter.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucPthread.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOucTrace XrdOlbTrace;

XrdOucMutex    XrdOlbMeter::repMutex;

XrdOucTList   *XrdOlbMeter::fs_list = 0;
int            XrdOlbMeter::dsk_calc= 0;
int            XrdOlbMeter::fs_nums = 0;
long           XrdOlbMeter::MinFree = 0;
long long      XrdOlbMeter::dsk_free= 0;
long long      XrdOlbMeter::dsk_maxf= 0;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
extern "C"
{
void *XrdOlbMeterRun(void *carg)
      {XrdOlbMeter *mp = (XrdOlbMeter *)carg;
       return mp->Run();
      }
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOlbMeter::XrdOlbMeter(XrdOucError *errp) : myMeter(errp)
{
    monpgm   = 0;
    monint   = 0;
    montid   = 0;
    eDest    = errp;
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
   if (montid) XrdOucThread_Kill(montid);
}
  
/******************************************************************************/
/*                             F r e e S p a c e                              */
/******************************************************************************/
  
long XrdOlbMeter::FreeSpace(long &tot_free)
{
     const char *epname = "FreeSpace";
     static XrdOucMutex fsMutex;
     static int Now, lastCalc = 0;
     long long bytes, fsavail = 0, fstotav = 0;
     XrdOucTList *tlp = fs_list;
     STATFS_BUFF fsdata;


// Check if we should calculate space
//
   Now = time(0);
   fsMutex.Lock();
   if ((Now - lastCalc) < dsk_calc)
      {fsavail = dsk_maxf; tot_free = (long)dsk_free;
       fsMutex.UnLock();
       return (long)fsavail;
      }
   lastCalc = Now;

// For each file system, do a statvfs() or equivalent. We define free space
// as the largest amount available in one filesystem since we can't allocate
// across filesystems.
//
   while(tlp)
        {if (!STATFS((const char *)tlp->text, &fsdata))
            {bytes = fsdata.f_bavail * ( fsdata.f_bsize ?
                                         fsdata.f_bsize : FS_BLKFACT);
             if (bytes >= MinFree)
                {fstotav += bytes/1024;
                 if (bytes > fsavail) fsavail = bytes;
                }
            }
         tlp = tlp->next;
        }

// Adjust to fit
//
   fsavail = fsavail / 1024;
   if (fsavail >> 31) fsavail = 0x7fffffff;
   if (fstotav >> 31) fstotav = 0x7fffffff;
   DEBUG("Updated fs info; old=" <<dsk_free <<"K new=" <<fsavail <<"K tot=" <<fstotav <<"K");

// Set the quantity and return it
//
   dsk_maxf = fsavail;
   dsk_free = fstotav;
   tot_free = (long)fstotav;
   fsMutex.UnLock();
   return (long)fsavail;
}

/******************************************************************************/
/*                               M o n i t o r                                */
/******************************************************************************/
  
int XrdOlbMeter::Monitor(char *pgm, int itv)
{
   const char *epname = "Monitor";
   char *mp, pp;

// Isolate the program name
//
   mp = monpgm = strdup(pgm);
   while(*mp && *mp != ' ') mp++;
   pp = *mp; *mp ='\0';

// Make sure the program is executable by us
//
   if (access((const char *)monpgm, X_OK))
      {eDest->Emsg("Meter", errno, (char *)"find executable", monpgm);
       return -1;
      }

// Start up the program. We don't really need to serialize Restart() because
// Monitor() is a one-time call (otherwise unpredictable results may occur).
//
   *mp = pp; monint = itv;
   XrdOucThread_Run(&montid, XrdOlbMeterRun, (void *)this);
   DEBUG("Thread " <<montid <<" handling perf meter " <<monpgm);
   return 0;
}
 
/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/
  
char *XrdOlbMeter::Report()
{
   long totfree;

// Force restart the monitor program if it hasn't reported within 2 intervals
//
   if (montid && (time(0) - rep_tod > monint*2)) myMeter.Drain();

// Format a usage line
//
   repMutex.Lock();
   snprintf(ubuff, sizeof(ubuff), "%ld %ld %ld %ld %ld %ld %ld",
            cpu_load, net_load, xeq_load, mem_load,
            pag_load, FreeSpace(totfree), totfree);
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
   int i;
   char *lp = 0;

// Execute the program (keep restarting and keep reading the output)
//
   while(1)
        {if (myMeter.Exec(monpgm) == 0)
             while((lp = myMeter.GetLine()))
                  {repMutex.Lock();
                   i = sscanf(lp, "%ld %ld %ld %ld %ld",
                       &xeq_load, &cpu_load, &mem_load, &pag_load, &net_load);
                   rep_tod = time(0);
                   repMutex.UnLock();
                   if (i != 5) break;
                  }
         if (lp) eDest->Emsg("Meter","Perf monitor returned invalid output:",lp);
            else eDest->Emsg("Meter","Perf monitor died.");
         nanosleep(&rqtp, 0);
         eDest->Emsg("Meter", "Restarting monitor:", monpgm);
        }
   return (void *)0;
}

/******************************************************************************/
/*                              s e t P a r m s                               */
/******************************************************************************/

void  XrdOlbMeter::setParms(XrdOucTList *tlp, long mfr, int itv)
{
    XrdOucTList *nlp = tlp;
    fs_list = tlp; 
    MinFree = mfr; 
    dsk_calc = itv;
    fs_nums = 0;
    if (tlp) do {fs_nums++;} while((nlp = nlp->next));
}
