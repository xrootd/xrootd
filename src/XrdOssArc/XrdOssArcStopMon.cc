/******************************************************************************/
/*                                                                            */
/*                      X r d O s s S t o p M o n . h h                       */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdexcept>
#include <cstdlib>
#include <unistd.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdOssArc/XrdOssArcStopMon.hh"
#include "XrdOssArc/XrdOssArc.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdScheduler*   schedP;
  
extern XrdSysError     Elog;

extern XrdSysTrace     ArcTrace;

static const char*     StopFN = "STOP";
static const char*     IdleFN = "IDLE";
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOssArcStopMon::XrdOssArcStopMon(const char* aPath, int chkT, bool& aOK)
                                  : XrdJob("StopMon"),
                                    xsLock(*(new XrdSysXSLock())),
                                    admPath(aPath), chkInterval(chkT)
{
// Tty to open the admin path. This is where the STOP file should appear and
// where we plce the IDLE file once we have safely stopped all the archiving
// and restores.
//
   if ((admDirFD = XrdSysFD_Open(aPath,  O_DIRECTORY|O_RDONLY)) < 0)
      {Elog.Emsg("Config", errno, "open admin path", aPath);
       aOK = false;
       return;
      }

// Remove any idle file left over from previous run
//
   if (unlinkat(admDirFD, IdleFN, 0) && errno != ENOENT)
      {Elog.Emsg("Config", errno, "remove IDLE file in admin path", aPath);
       aOK = false;
       close(admDirFD);
       admDirFD = -1;
       return;
      }

// Now schedule ourselves to see if a stop file appears
//
   schedP->Schedule(this, time(0)+chkT);

// All done
//
   aOK = true;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOssArcStopMon::~XrdOssArcStopMon()
{
// Parent instances of this class may not be destroyed. A parent instance
// is indicated by admDirFD being non-negative.
//
   if (admDirFD >= 0)
      {Elog.Emsg("StopMon", "Invalid deletion of StopMon parent instance for",
                            admPath);
       std::abort();
      }

// We are not a parent, release the lock since the constructor obtained it.
//
   Deactivate();
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

void XrdOssArcStopMon::DoIt()
{
   static const mode_t idleMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   static const int    idleOflg = O_CREAT | O_RDWR  | O_CLOEXEC;
   struct stat Stat;

// This method is only valid for the parent class.
//
   if (admDirFD < 0)
      {Elog.Emsg("StopMon","Invalid DoIt() call on child instance for",admPath);
       return;
      }

// Check if a STOP file exists in the admpath. If it does, try to obtain a
// write lock. Only one thread may it and when we get it we know no other
// threads are actively executing a backup or restore. Once we have it,
// Issue a message and create an IDLE file.
//
   if (fstatat(admDirFD, StopFN, &Stat, 0) == 0)
      {Elog.Emsg("StopMon", "STOP file found in", admPath);
       xsLock.Lock(xs_Exclusive);
       Elog.Emsg("StopMon", "Drain complete; entering idle state...");
       int iFD = openat(admDirFD, IdleFN, idleOflg, idleMode);
       if (iFD < 0) Elog.Emsg("StopMon",errno,"create IDLE file in",admPath);
          else close(iFD);
       do {XrdSysTimer::Snooze(10);} while(!fstatat(admDirFD,StopFN,&Stat,0));
       unlinkat(admDirFD, IdleFN, 0);
       xsLock.UnLock(xs_Exclusive);
       Elog.Emsg("StopMon", "Resuming execution; STOP file removed!");
      }

// Reschedule ourselves
//
   schedP->Schedule(this, time(0)+chkInterval);
}
