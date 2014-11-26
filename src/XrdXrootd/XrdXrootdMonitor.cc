/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n i t o r . c c                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <cstdio>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <malloc.h>
#endif

#include "XrdVersion.hh"

#include "XrdNet/XrdNetMsg.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "Xrd/XrdScheduler.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

/******************************************************************************/
/*                     S t a t i c   A l l o c a t i o n                      */
/******************************************************************************/

XrdScheduler      *XrdXrootdMonitor::Sched      = 0;
XrdSysError       *XrdXrootdMonitor::eDest      = 0;
char              *XrdXrootdMonitor::idRec      = 0;
int                XrdXrootdMonitor::idLen      = 0;
char              *XrdXrootdMonitor::Dest1      = 0;
int                XrdXrootdMonitor::monMode1   = 0;
XrdNetMsg         *XrdXrootdMonitor::InetDest1  = 0;
char              *XrdXrootdMonitor::Dest2      = 0;
int                XrdXrootdMonitor::monMode2   = 0;
XrdNetMsg         *XrdXrootdMonitor::InetDest2  = 0;
XrdXrootdMonitor  *XrdXrootdMonitor::altMon     = 0;
XrdSysMutex        XrdXrootdMonitor::windowMutex;
kXR_int32          XrdXrootdMonitor::startTime  = 0;
int                XrdXrootdMonitor::monRlen    = 0;
XrdXrootdMonitor::MonRdrBuff
                   XrdXrootdMonitor::rdrMon[XrdXrootdMonitor::rdrMax];
XrdXrootdMonitor::MonRdrBuff
                  *XrdXrootdMonitor::rdrMP      = 0;
XrdSysMutex        XrdXrootdMonitor::rdrMutex;
int                XrdXrootdMonitor::monBlen    = 0;
int                XrdXrootdMonitor::lastEnt    = 0;
int                XrdXrootdMonitor::lastRnt    = 0;
int                XrdXrootdMonitor::isEnabled  = 0;
int                XrdXrootdMonitor::numMonitor = 0;
int                XrdXrootdMonitor::autoFlash  = 0;
int                XrdXrootdMonitor::autoFlush  = 600;
int                XrdXrootdMonitor::FlushTime  = 0;
int                XrdXrootdMonitor::monIdent   = 3600;
kXR_int32          XrdXrootdMonitor::currWindow = 0;
int                XrdXrootdMonitor::rdrTOD     = 0;
int                XrdXrootdMonitor::rdrWin     = 0;
int                XrdXrootdMonitor::rdrNum     = 3;
kXR_int32          XrdXrootdMonitor::sizeWindow = 60;
long long          XrdXrootdMonitor::mySID      = 0;
char               XrdXrootdMonitor::sidName[16]= {0};
short              XrdXrootdMonitor::sidSize    = 0;
char               XrdXrootdMonitor::monINFO    = 0;
char               XrdXrootdMonitor::monIO      = 0;
char               XrdXrootdMonitor::monFILE    = 0;
char               XrdXrootdMonitor::monREDR    = 0;
char               XrdXrootdMonitor::monUSER    = 0;
char               XrdXrootdMonitor::monAUTH    = 0;
char               XrdXrootdMonitor::monACTIVE  = 0;
char               XrdXrootdMonitor::monFSTAT   = 0;
char               XrdXrootdMonitor::monCLOCK   = 0;

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern          XrdOucTrace       *XrdXrootdTrace;

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define setTMark(TM_mb, TM_en, TM_tm) \
           TM_mb->info[TM_en].arg0.val    = mySID; \
           TM_mb->info[TM_en].arg0.id[0]  = XROOTD_MON_WINDOW; \
           TM_mb->info[TM_en].arg1.Window = \
           TM_mb->info[TM_en].arg2.Window = static_cast<kXR_int32>(ntohl(TM_tm));

#define setTMurk(TM_mb, TM_en, TM_tm) \
           TM_mb->info[TM_en].arg0.Window = rdrWin; \
           TM_mb->info[TM_en].arg1.Window = static_cast<kXR_int32>(TM_tm);
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                X r d X r o o t d M o n i t o r _ I d e n t                 */
/******************************************************************************/
  
class XrdXrootdMonitor_Ident : public XrdJob
{
public:

void          DoIt() {
                      XrdXrootdMonitor::Ident();
                      Sched->Schedule((XrdJob *)this, time(0)+idInt);
                     }

      XrdXrootdMonitor_Ident(XrdScheduler *sP, int idt)
                            : XrdJob("monitor ident"), Sched(sP), idInt(idt) {}
     ~XrdXrootdMonitor_Ident() {}

private:
XrdScheduler  *Sched;     // System scheduler
int            idInt;
};

/******************************************************************************/
/*           C l a s s   X r d X r o o t d M o n i t o r _ T i c k            */
/******************************************************************************/

class XrdXrootdMonitor_Tick : public XrdJob
{
public:

void          DoIt() {
#ifndef NODEBUG
                      const char *TraceID = "MonTick";
#endif
                      time_t Now = XrdXrootdMonitor::Tick();
                      if (Window && Now)
                         Sched->Schedule((XrdJob *)this, Now+Window);
                         else {TRACE(DEBUG, "Monitor clock stopping.");}
                     }

void          Set(XrdScheduler *sp, int intvl) {Sched = sp; Window = intvl;}

      XrdXrootdMonitor_Tick() : XrdJob("monitor window clock"),
                                Sched(0), Window(0) {}
     ~XrdXrootdMonitor_Tick() {}

private:
XrdScheduler  *Sched;     // System scheduler
int            Window;
};

/******************************************************************************/
/*            C l a s s   X r d X r o o t d M o n i t o r L o c k             */
/******************************************************************************/
  
class XrdXrootdMonitorLock
{
public:

static void Lock()   {monLock.Lock();}

static void UnLock() {monLock.UnLock();}

       XrdXrootdMonitorLock(XrdXrootdMonitor *theMonitor)
                {if (theMonitor != XrdXrootdMonitor::altMon) unLock = 0;
                    else {unLock = 1; monLock.Lock();}
                }
      ~XrdXrootdMonitorLock() {if (unLock) monLock.UnLock();}

private:

static XrdSysMutex monLock;
       char        unLock;
};

XrdSysMutex XrdXrootdMonitorLock::monLock;

/******************************************************************************/
/*       X r d X r o o t d M o n i t o r : : U s e r : : D i s a b l e        */
/******************************************************************************/

void XrdXrootdMonitor::User::Disable()
{
   if (Agent)
      {XrdXrootdMonitor::unAlloc(Agent); Agent = 0;}
   Fops = Iops = 0;
}
  
/******************************************************************************/
/*        X r d X r o o t d M o n i t o r : : U s e r : : E n a b l e         */
/******************************************************************************/

void XrdXrootdMonitor::User::Enable()
{
   if (Agent || (Agent = XrdXrootdMonitor::Alloc(1)))
      {Iops = XrdXrootdMonitor::monIO;
       Fops = XrdXrootdMonitor::monFILE;
      } else Iops = Fops = 0;
}
  
/******************************************************************************/
/*      X r d X r o o t d M o n i t o r : : U s e r : : R e g i s t e r       */
/******************************************************************************/
  
void XrdXrootdMonitor::User::Register(const char *Uname, 
                                      const char *Hname,
                                      const char *Pname)
{
   const char *colonP, *atP;
   char  uBuff[1024], *uBP;
   int n;

// The identification always starts with the protocol being used
//
   n = sprintf(uBuff, "%s/", Pname);
   uBP = uBuff + n;

// Decode the user name as a.b:c@d
//
   if ((colonP = index(Uname, ':')) && (atP = index(colonP+1, '@')))
      {n = colonP - Uname + 1;
       strncpy(uBP, Uname, n);
       strcpy(uBP+n, sidName);
       n += sidSize; *(uBP+n) = '@'; n++;
       strcpy(uBP+n, Hname);
      } else strcpy(uBP, Uname);

// Generate a monitor identity for this user. We do not assign a dictioary
// identifier unless this entry is reported.
//
   Agent = XrdXrootdMonitor::Alloc();
   Did   = 0;
   Len   = strlen(uBuff);
   Name  = strdup(uBuff);
   Iops  = XrdXrootdMonitor::monIO;
   Fops  = XrdXrootdMonitor::monFILE;
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdMonitor::XrdXrootdMonitor()
{
   kXR_int32 localWindow;

// Initialize last window to force a mark as well as the local window
//
   lastWindow  = 0;
   localWindow = currWindow;

// Allocate a monitor buffer
//
   if (!(monBuff = (XrdXrootdMonBuff *)memalign(getpagesize(), monBlen)))
      eDest->Emsg("Monitor", "Unable to allocate monitor buffer.");
      else {nextEnt = 1;
            setTMark(monBuff, 0, localWindow);
           }
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdXrootdMonitor::~XrdXrootdMonitor()
{
// Release buffer
   if (monBuff) {Flush(); free(monBuff);}
}

/******************************************************************************/
/*                                 a p p I D                                  */
/******************************************************************************/
  
void XrdXrootdMonitor::appID(char *id)
{
   static const int apInfoSize = sizeof(XrdXrootdMonTrace)-4;

// Application ID's are only meaningful for io event recording
//
   if (this == altMon || !*id) return;

// Fill out the monitor record
//
   if (lastWindow != currWindow) Mark();
      else if (nextEnt == lastEnt) Flush();
   monBuff->info[nextEnt].arg0.id[0]  = XROOTD_MON_APPID;
   strncpy((char *)(&(monBuff->info[nextEnt])+4), id, apInfoSize);
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdXrootdMonitor *XrdXrootdMonitor::Alloc(int force)
{
   XrdXrootdMonitor *mp;
   int lastVal;

// If enabled, create a new object (if possible). If we are not monitoring
// i/o then return the global object.
//
   if (!isEnabled || (isEnabled < 0 && !force)) mp = 0;
      else if (!monIO) mp = altMon;
              else if ((mp = new XrdXrootdMonitor()))
                      if (!(mp->monBuff)) {delete mp; mp = 0;}

// Check if we should turn on the monitor clock
//
   if (mp && isEnabled < 0)
      {windowMutex.Lock();
       lastVal = numMonitor; numMonitor++;
       if (!lastVal && !monREDR) startClock();
       windowMutex.UnLock();
      }

// All done
//
   return mp;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

void XrdXrootdMonitor::Close(kXR_unt32 dictid, long long rTot, long long wTot)
{
  XrdXrootdMonitorLock mLock(this);
  unsigned int rVal, wVal;

// Fill out the monitor record (we allow the compiler to correctly cast data)
//
   if (lastWindow != currWindow) Mark();
      else if (nextEnt == lastEnt) Flush();
   monBuff->info[nextEnt].arg0.id[0]    = XROOTD_MON_CLOSE;
   monBuff->info[nextEnt].arg0.id[1]    = do_Shift(rTot, rVal);
   monBuff->info[nextEnt].arg0.rTot[1]  = htonl(rVal);
   monBuff->info[nextEnt].arg0.id[2]    = do_Shift(wTot, wVal);
   monBuff->info[nextEnt].arg0.id[3]    = 0;
   monBuff->info[nextEnt].arg1.wTot     = htonl(wVal);
   monBuff->info[nextEnt++].arg2.dictid = dictid;

// Check if we need to duplicate this entry
//
   if (altMon && this != altMon) altMon->Dup(&monBuff->info[nextEnt-1]);
}

/******************************************************************************/
/*                              D e f a u l t s                               */
/******************************************************************************/

// This version must be called after the subsequent version!

void XrdXrootdMonitor::Defaults(char *dest1, int mode1, char *dest2, int mode2)
{
   int mmode;

// Make sure if we have a dest1 we have mode
//
   if (!dest1)
      {mode1 = (dest1 = dest2) ? mode2 : 0;
       dest2 = 0; mode2 = 0;
      } else if (!dest2) mode2 = 0;


// Set the default destinations (caller supplied strdup'd strings)
//
   if (Dest1) free(Dest1);
   Dest1 = dest1; monMode1 = mode1;
   if (Dest2) free(Dest2);
   Dest2 = dest2; monMode2 = mode2;

// Set overall monitor mode
//
   mmode     = mode1 | mode2;
   monACTIVE = (mmode                   ? 1 : 0);
   isEnabled = (mmode & XROOTD_MON_ALL  ? 1 :-1);
   monIO     = (mmode & XROOTD_MON_IO   ? 1 : 0);
   monIO     = (mmode & XROOTD_MON_IOV  ? 2 : monIO);
   monINFO   = (mmode & XROOTD_MON_INFO ? 1 : 0);
   monFILE   = (mmode & XROOTD_MON_FILE ? 1 : 0) | monIO;
   monREDR   = (mmode & XROOTD_MON_REDR ? 1 : 0);
   monUSER   = (mmode & XROOTD_MON_USER ? 1 : 0);
   monAUTH   = (mmode & XROOTD_MON_AUTH ? 1 : 0);
   monFSTAT  = (mmode & XROOTD_MON_FSTA && monFSTAT ? 1 : 0);

// Compute whether or not we need the clock running
//
   if (monREDR || (isEnabled > 0 && (monIO || monFILE))) monCLOCK = 1;

// Check where user information should go
//
   if (((mode1 & XROOTD_MON_IO) && (mode1 & XROOTD_MON_USER))
   ||  ((mode2 & XROOTD_MON_IO) && (mode2 & XROOTD_MON_USER)))
      {if ((!(mode1 & XROOTD_MON_IO) && (mode1 & XROOTD_MON_USER))
       ||  (!(mode2 & XROOTD_MON_IO) && (mode2 & XROOTD_MON_USER))) monUSER = 3;
          else monUSER = 2;
      }

// If we are monitoring redirections then set an envar saying how often idents
// should be sent (this also tips off other layers to handle such monitoring)
//
   if (monREDR) XrdOucEnv::Export("XRDMONRDR", monIdent);

// Do final check
//
   if (Dest1 == 0 && Dest2 == 0) isEnabled = 0;
}

/******************************************************************************/

void XrdXrootdMonitor::Defaults(int msz,   int rsz,   int wsz,
                                int flush, int flash, int idt, int rnm,
                                int fsint, int fsopt, int fsion)
{

// Set default window size and flush time
//
   sizeWindow = (wsz <= 0 ? 60 : wsz);
   autoFlush  = (flush <= 0 ? 600 : flush);
   autoFlash  = (flash <= 0 ?   0 : flash);
   monIdent   = (idt   <  0 ?   0 : idt);
   rdrNum     = (rnm   <= 0 || rnm > rdrMax ? 3 : rnm);
   rdrWin     = (sizeWindow > 16777215 ? 16777215 : sizeWindow);
   rdrWin     = htonl(rdrWin);

// Set the fstat defaults
//
   XrdXrootdMonFile::Defaults(fsint, fsopt, fsion);
   monFSTAT = fsint != 0;

// Set default monitor buffer size
//
   if (msz <= 0) msz = 16384;
      else if (msz < 1024) msz = 1024;
              else msz = msz/sizeof(XrdXrootdMonTrace)*sizeof(XrdXrootdMonTrace);
   lastEnt = (msz-sizeof(XrdXrootdMonHeader))/sizeof(XrdXrootdMonTrace);
   monBlen = (lastEnt*sizeof(XrdXrootdMonTrace))+sizeof(XrdXrootdMonHeader);
   lastEnt--;

// Set default monitor redirect buffer size
//
   if (rsz <= 0) rsz = 32768;
      else if (rsz < 2048) rsz = 2048;
   lastRnt = (rsz-(sizeof(XrdXrootdMonHeader) + 16))/sizeof(XrdXrootdMonRedir);
   monRlen =  (lastRnt*sizeof(XrdXrootdMonRedir))+sizeof(XrdXrootdMonHeader)+16;
   lastRnt--;
}

/******************************************************************************/
/*                                  D i s c                                   */
/******************************************************************************/

void XrdXrootdMonitor::Disc(kXR_unt32 dictid, int csec, char Flags)
{
  XrdXrootdMonitorLock mLock(this);

// Check if this should not be included in the io trace
//
   if (this != altMon && monUSER == 1 && altMon)
      {altMon->Disc(dictid, csec); return;}

// Fill out the monitor record (let compiler cast the data correctly)
//
   if (lastWindow != currWindow) Mark();
      else if (nextEnt == lastEnt) Flush();
   monBuff->info[nextEnt].arg0.rTot[0]  = 0;
   monBuff->info[nextEnt].arg0.id[0]    = XROOTD_MON_DISC;
   monBuff->info[nextEnt].arg0.id[1]    = Flags;
   monBuff->info[nextEnt].arg1.wTot     = htonl(csec);
   monBuff->info[nextEnt++].arg2.dictid = dictid;

// Check if we need to duplicate this entry
//
   if (altMon && this != altMon && monUSER == 3)
      altMon->Dup(&monBuff->info[nextEnt-1]);
}
  
/******************************************************************************/
/*                                   D u p                                    */
/******************************************************************************/
  
void XrdXrootdMonitor::Dup(XrdXrootdMonTrace *mrec)
{
  XrdXrootdMonitorLock mLock(this);

// Fill out the monitor record
//
   if (lastWindow != currWindow) Mark();
      else if (nextEnt == lastEnt) Flush();
   memcpy(&monBuff->info[nextEnt],(const void *)mrec,sizeof(XrdXrootdMonTrace));
   nextEnt++;
}

/******************************************************************************/
/* Private:                        F e t c h                                  */
/******************************************************************************/
  
XrdXrootdMonitor::MonRdrBuff *XrdXrootdMonitor::Fetch()
{
   MonRdrBuff *bP;

// Get the next available stream and promote another one
//
   rdrMutex.Lock();
   if ((bP = rdrMP)) rdrMP = rdrMP->Next;
   rdrMutex.UnLock();
   return bP;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdXrootdMonitor::Init(XrdScheduler *sp,    XrdSysError *errp,
                           const char   *iHost, const char  *iProg,
                           const char   *iName, int Port)
{
   static     XrdXrootdMonitor_Ident MonIdent(sp, monIdent);
   XrdXrootdMonMap *mP;
   char       iBuff[1024], iPuff[1024], *sName, *cP;
   int        i, Now = time(0);
   bool       aOK;

// Set static variables
//
   Sched = sp;
   eDest = errp;
   startTime = htonl(Now);

// Generate our server ID
//
   strcpy(iBuff, "=/");
   sprintf(iPuff, "%s&ver=%s", iProg, XrdVERSION);
   sName = XrdOucUtils::Ident(mySID, iBuff+2, sizeof(iBuff)-2,
                              iHost, iPuff, iName, Port);
   cP = (char *)&mySID; *cP = 0; *(cP+1) = 0;
   sidSize = strlen(sName);
   if (sidSize >= (int)sizeof(sidName)) sName[sizeof(sidName)-1] = 0;
   strcpy(sidName, sName);
   free(sName);

// There is nothing to do unless we have been enabled via Defaults()
//
   if (!isEnabled) return 1;

// Setup the primary destination
//
   InetDest1 = new XrdNetMsg(eDest, Dest1, &aOK);
   if (!aOK)
      {eDest->Emsg("Monitor", "Unable to setup primary monitor collector.");
       return 0;
      }

// Setup the secondary destination
//
   if (Dest2)
      {InetDest2 = new XrdNetMsg(eDest, Dest2, &aOK);
       if (!aOK)
          {eDest->Emsg("Monitor","Unable to setup secondary monitor collector.");
           return 0;
          }
      }

// If there is a destination that is only collecting file events, then
// allocate a global monitor object but don't start the timer just yet.
//
   if ((monMode1 && !(monMode1 & XROOTD_MON_IO))
   ||  (monMode2 && !(monMode2 & XROOTD_MON_IO)))
       if (!(altMon = new XrdXrootdMonitor()) || !altMon->monBuff)
          {if (altMon) {delete altMon; altMon = 0;}
           eDest->Emsg("Monitor","allocate monitor; insufficient storage.");
           return 0;
          }

// Turn on the monitoring clock if we need it running all the time
//
   if (monCLOCK) startClock();

// Create identification record
//
   idLen = strlen(iBuff) + sizeof(XrdXrootdMonHeader) + sizeof(kXR_int32);
   idRec = (char *)malloc(idLen+1);
   mP = (XrdXrootdMonMap *)idRec;
   fillHeader(&(mP->hdr), XROOTD_MON_MAPIDNT, idLen);
   mP->hdr.pseq = 0;
   mP->dictid   = 0;
   strcpy(mP->info, iBuff);

// Now schedule the first identification record
//
   if (Sched && monIdent) Sched->Schedule((XrdJob *)&MonIdent);

// If we are monitoring file stats then start that up
//
   if (!Sched || !monFSTAT) monFSTAT = 0;
      else if (!XrdXrootdMonFile::Init(Sched, eDest)) return 0;

// If we are not monitoring redirections, we are done!
//
   if (!monREDR) return 1;

// Allocate as many redirection monitors as requested
//
   for (i = 0; i < rdrNum; i++)
       {rdrMon[i].Buff = (XrdXrootdMonBurr *)memalign(getpagesize(),monRlen);
        if (!rdrMon[i].Buff)
           {eDest->Emsg("Monitor", "Unable to allocate monitor rdr buffer.");
            return 0;
           }
        rdrMon[i].Buff->sID    = mySID;
        rdrMon[i].Buff->sXX[0] = XROOTD_MON_REDSID;
        rdrMon[i].Next = (i ? &rdrMon[i-1] : &rdrMon[0]);
        rdrMon[i].nextEnt = 0;
        rdrMon[i].flushIt = Now + autoFlush;
        rdrMon[i].lastTOD = 0;
       }
   rdrMon[0].Next = &rdrMon[i-1];
   rdrMP = &rdrMon[0];

// All done
//
   return 1;
}

/******************************************************************************/
/* Private:                    G e t D i c t I D                              */
/******************************************************************************/
  
kXR_unt32 XrdXrootdMonitor::GetDictID()
{
   static XrdSysMutex  seqMutex;
   static unsigned int monSeqID = 1;
   unsigned int        mySeqID;

// Assign a unique ID for this entry
//
   seqMutex.Lock();
   mySeqID = monSeqID++;
   seqMutex.UnLock();

// Return the ID
//
   return htonl(mySeqID);
}

/******************************************************************************/
/* Private:                          M a p                                    */
/******************************************************************************/
  
kXR_unt32 XrdXrootdMonitor::Map(char  code, XrdXrootdMonitor::User &uInfo,
                                const char *path)
{
   XrdXrootdMonMap     map;
   int                 size, montype;

// Copy in the username and path
//
   map.dictid = GetDictID();
   strcpy(map.info, uInfo.Name);
   size = uInfo.Len;
   if (path)
      {*(map.info+size) = '\n';
       strlcpy(map.info+size+1, path, sizeof(map.info)-size-1);
       size = size + strlen(path) + 1;
      }

// Fill in the header
//
   size = sizeof(XrdXrootdMonHeader)+sizeof(kXR_int32)+size;
   fillHeader(&map.hdr, code, size);

// Route the packet to all destinations that need them
//
        if (code == XROOTD_MON_MAPPATH) montype = XROOTD_MON_PATH;
   else if (code == XROOTD_MON_MAPUSER) montype = XROOTD_MON_USER;
   else                                 montype = XROOTD_MON_INFO;
   Send(montype, (void *)&map, size);

// Return the dictionary id
//
   return map.dictid;
}
  
/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
void XrdXrootdMonitor::Open(kXR_unt32 dictid, off_t fsize)
{
  XrdXrootdMonitorLock mLock(this);

  if (lastWindow != currWindow) Mark();
     else if (nextEnt == lastEnt) Flush();
  h2nll(fsize, monBuff->info[nextEnt].arg0.val);
  monBuff->info[nextEnt].arg0.id[0]    = XROOTD_MON_OPEN;
  monBuff->info[nextEnt].arg1.buflen   = 0;
  monBuff->info[nextEnt++].arg2.dictid = dictid;

// Check if we need to duplicate this entry
//
   if (altMon && this != altMon) altMon->Dup(&monBuff->info[nextEnt-1]);
}

/******************************************************************************/
/*                              R e d i r e c t                               */
/******************************************************************************/
  
int XrdXrootdMonitor::Redirect(kXR_unt32 mID, const char *hName, int Port,
                               char      opC, const char *Path)
{
   XrdXrootdMonRedir *mtP;
   MonRdrBuff        *mP = Fetch();
   int n, slots, hLen, pLen;
   char *dest;

// Take care of the server's name which might actually be a path
//
   if (*hName == '/') {Path = hName; hName = ""; hLen = 0;}
      else {const char *quest = index(hName, '?');
            hLen = (quest ? quest - hName : strlen(hName));
            if (hLen >  256) hLen =  256;
           }

// Take care of the path
//
   pLen = strlen(Path);
   if (pLen > 1024) pLen = 1024;

// Compute number of entries needed here
//
   n = (hLen + 1 + pLen + 1);    // "<host>:<path>\0"
   slots = n / sizeof(XrdXrootdMonRedir);
   if (n % sizeof(XrdXrootdMonRedir)) slots++;
   pLen = slots * sizeof(XrdXrootdMonRedir) - (hLen+1);

// Obtain a lock on this buffer
//
   if (!mP) return 0;
   mP->Mutex.Lock();

// If we don't have enough slots, flush this buffer. Note that we account for
// the ending timing mark here (an extra slot).
//
   if (mP->nextEnt + slots + 2 >= lastRnt) Flush(mP);

// Check if we need a timing mark
//
   if (mP->lastTOD != rdrTOD)
      {mP->lastTOD = rdrTOD;
       setTMurk(mP->Buff, mP->nextEnt, mP->lastTOD);
       mP->nextEnt++;
      }

// Fill out the buffer
//
   mtP = &(mP->Buff->info[mP->nextEnt]);
   mtP->arg0.rdr.Type = XROOTD_MON_REDIRECT | opC;
   mtP->arg0.rdr.Dent = static_cast<char>(slots);
   mtP->arg0.rdr.Port = htons(static_cast<short>(Port));
   mtP->arg1.dictid   = mID;
   dest = (char *)(mtP+1);
   strncpy(dest, hName,hLen); dest += hLen; *dest++ = ':';
   strncpy(dest, Path, pLen);

// Adjust pointer and return
//
   mP->nextEnt = mP->nextEnt + (slots+1);
   mP->Mutex.UnLock();
   return 0;
}

/******************************************************************************/
/*                                  T i c k                                   */
/******************************************************************************/
  
time_t XrdXrootdMonitor::Tick()
{
   time_t Now = time(0);
   int    nextFlush;

// We can safely set the window as we are the only ones doing so and memory
// access is atomic as long as it sits within a cache line (which it does).
//
   currWindow = static_cast<kXR_int32>(Now);
   rdrTOD     = htonl(currWindow);
   nextFlush  = currWindow + autoFlush;

// Check to see if we should flush the alternate monitor
//
   if (altMon && currWindow >= FlushTime)
      {XrdXrootdMonitorLock::Lock();
       if (currWindow >= FlushTime)
          {if (altMon->nextEnt > 1) altMon->Flush();
              else FlushTime = nextFlush;
          }
       XrdXrootdMonitorLock::UnLock();
      }

// Now check to see if we need to flush redirect buffers
//
   if (monREDR)
      {int n = rdrNum;
       while(n--)
            {rdrMon[n].Mutex.Lock();
             if (rdrMon[n].nextEnt == 0) rdrMon[n].flushIt = nextFlush;
                else if (rdrMon[n].flushIt <= currWindow) Flush(&rdrMon[n]);
             rdrMon[n].Mutex.UnLock();
            }
      }

// All done. Stop the clock if there is no reason for it to be running. The
// clock always runs if we are monitoring redirects or all clients. Otherwise,
// the clock only runs if we have a one or more client-specific monitors.
//
   if (!monREDR && isEnabled < 0)
      {windowMutex.Lock();
       if (!numMonitor) Now = 0;
       windowMutex.UnLock();
      }
   return Now;
}

/******************************************************************************/
/*                               u n A l l o c                                */
/******************************************************************************/
  
void XrdXrootdMonitor::unAlloc(XrdXrootdMonitor *monp)
{

// We must delete this object if we are de-allocating the local monitor.
//
   if (monp != altMon) delete monp;

// Decrease number being monitored if in selective mode
//
   if (isEnabled < 0)
      {windowMutex.Lock();
       numMonitor--;
       windowMutex.UnLock();
      }
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              d o _ S h i f t                               */
/******************************************************************************/
  
unsigned char XrdXrootdMonitor::do_Shift(long long xTot, unsigned int &xVal)
{
  const long long smask = 0x7fffffff00000000LL;
  unsigned char xshift = 0;

  while(xTot & smask) {xTot = xTot >> 1LL; xshift++;}
  xVal = static_cast<unsigned int>(xTot);

  return xshift;
}

/******************************************************************************/
/*                            f i l l H e a d e r                             */
/******************************************************************************/
  
void XrdXrootdMonitor::fillHeader(XrdXrootdMonHeader *hdr,
                                  const char          id, int size)
{  static XrdSysMutex seqMutex;
   static int         seq = 0;
          int         myseq;

// Generate a new sequence number
//
   seqMutex.Lock();
   myseq = 0x00ff & (seq++);
   seqMutex.UnLock();

// Fill in the header
//
   hdr->code = static_cast<kXR_char>(id);
   hdr->pseq = static_cast<kXR_char>(myseq);
   hdr->plen = htons(static_cast<uint16_t>(size));
   hdr->stod = startTime;
}
  
/******************************************************************************/
/*                                 F l u s h                                  */
/******************************************************************************/
  
void XrdXrootdMonitor::Flush()
{
   int       size;
   kXR_int32 localWindow, now;

// Do not flush if the buffer is empty
//
   if (nextEnt <= 1) return;

// Get the current window marker. No need for locks as simple memory accesses
// are sufficiently synchrnozed for our purposes.
//
   localWindow = currWindow;

// Fill in the header and in the process we will have the current time
//
   size = (nextEnt+1)*sizeof(XrdXrootdMonTrace)+sizeof(XrdXrootdMonHeader);
   fillHeader(&monBuff->hdr, XROOTD_MON_MAPTRCE, size);

// Punt on the right ending time. We are trying to keep same-sized windows
// This was corrected by Matevz Tadel, as before we were using real time which
// could have been far into the future due to simple inactivity. So, Place the
// computed ending timing mark.
//
   now = lastWindow + sizeWindow;
   setTMark(monBuff, nextEnt, now);

// Send off the buffer and reinitialize it
//
   if (this != altMon) Send(XROOTD_MON_IO, (void *)monBuff, size);
      else {Send(XROOTD_MON_FILE, (void *)monBuff, size);
            FlushTime = localWindow + autoFlush;
           }
   setTMark(monBuff, 0, localWindow);
   nextEnt = 1;
}

/******************************************************************************/

void XrdXrootdMonitor::Flush(XrdXrootdMonitor::MonRdrBuff *mP)
{
   int size;

// Reset flush time but do not flush an empty buffer. We use the current time
// to make sure a record atleast sits in the buffer a full flush period.
//
   mP->flushIt = static_cast<int>(time(0)) + autoFlush;
   if (mP->nextEnt <= 1) return;

// Set ending timing mark and force a new one on the next fill
//
   setTMurk(mP->Buff, mP->nextEnt, rdrTOD);
   mP->lastTOD = 0;

// Fill in the header and in the process we will have the current time
//
   size = (mP->nextEnt+1)*sizeof(XrdXrootdMonRedir)+sizeof(XrdXrootdMonHeader)+8;
   fillHeader(&(mP->Buff->hdr), XROOTD_MON_MAPREDR, size);

// Send off the buffer and reinitialize it
//
   Send(XROOTD_MON_REDR, (void *)(mP->Buff), size);
   mP->nextEnt = 0;
}

/******************************************************************************/
/*                                  M a r k                                   */
/******************************************************************************/
  
void XrdXrootdMonitor::Mark()
{
   kXR_int32 localWindow;

// Get the current window marker. Since simple memory accesses are sufficiently
// synchronized, no need to lock this.
//
   localWindow = currWindow;

// Using an update provided by Matevz Tadel, UCSD, if this is an I/O buffer
// mark then we will also flush the I/O buffer if all the following hold:
// a) flushing enabled, b) buffer not empty, and c) covers the flush time.
// We would normally do this during Tick() but that would require too much
// locking in the middle of an I/O path, so we do psudo timed flushing.
//
   if (this != altMon && autoFlash && nextEnt > 1)
      {kXR_int32 bufStartWindow = 
                 static_cast<kXR_int32>(ntohl(monBuff->info[0].arg2.Window));
       if (localWindow - bufStartWindow >= autoFlash)
          {Flush();
           lastWindow = localWindow;
           return;
          }
      }

// Now, optimize placing the window mark in the buffer. Using another MT fix we
// set the end of the previous window to be lastwindow + sizeWindow (instead of
// localWindow) to prevent windows from being wrongly zero sized.
//
        if (monBuff->info[nextEnt-1].arg0.id[0] == XROOTD_MON_WINDOW)
   {
       monBuff->info[nextEnt-1].arg2.Window =
                static_cast<kXR_int32>(htonl(localWindow));
   }
   else if (nextEnt+8 > lastEnt)
   {
      Flush();
   }
   else
   {
      monBuff->info[nextEnt].arg0.val     = mySID;
      monBuff->info[nextEnt].arg0.id[0]   = XROOTD_MON_WINDOW;
      monBuff->info[nextEnt].arg1.Window  =
               static_cast<kXR_int32>(htonl(lastWindow + sizeWindow));
      monBuff->info[nextEnt].arg2.Window  =
               static_cast<kXR_int32>(htonl(localWindow));
      nextEnt++;
   }
   lastWindow = localWindow;
}
 
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdXrootdMonitor::Send(int monMode, void *buff, int blen)
{
#ifndef NODEBUG
    const char *TraceID = "Monitor";
#endif
    static XrdSysMutex sendMutex;
    int rc1, rc2;

    sendMutex.Lock();
    if (monMode & monMode1 && InetDest1)
       {rc1  = InetDest1->Send((char *)buff, blen);
        TRACE(DEBUG,blen <<" bytes sent to " <<Dest1 <<" rc=" <<rc1);
       }
       else rc1 = 0;
    if (monMode & monMode2 && InetDest2)
       {rc2  = InetDest2->Send((char *)buff, blen);
        TRACE(DEBUG,blen <<" bytes sent to " <<Dest2 <<" rc=" <<rc2);
       }
       else rc2 = 0;
    sendMutex.UnLock();

    return (rc1 ? rc1 : rc2);
}

/******************************************************************************/
/*                            s t a r t C l o c k                             */
/******************************************************************************/
  
void XrdXrootdMonitor::startClock()
{
   static XrdXrootdMonitor_Tick MonTick;
          time_t Now;

// Start the clock, Caller must have windowMutex locked, if necessary.
//
   Now = time(0);
   currWindow = static_cast<kXR_int32>(Now);
   rdrTOD     = htonl(currWindow);
   MonTick.Set(Sched, sizeWindow);
   FlushTime = autoFlush + currWindow;
   if (Sched) Sched->Schedule((XrdJob *)&MonTick, Now+sizeWindow);
}
