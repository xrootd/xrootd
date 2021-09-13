#ifndef __XrdSysTimer__
#define __XrdSysTimer__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s T i m e r . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#ifndef WIN32
#include <sys/time.h>
#else
#include <ctime>
#include <Winsock2.h>
#include "XrdSys/XrdWin32.hh"
#endif

/* This include file describes the oo elapsed time interval interface. It is
   used by the oo Real Time Monitor, among others.
*/

class XrdSysTimer {

public:
       struct timeval *Delta_Time(struct timeval &tbeg);

static time_t Midnight(time_t tnow=0);

inline int    TimeLE(time_t tsec) {return StopWatch.tv_sec <= tsec;}

       // The following routines return the current interval added to the
       // passed argument as well as returning the current Unix seconds
       //
       unsigned long Report(double &);
       unsigned long Report(unsigned long &);
       unsigned long Report(unsigned long long &);
       unsigned long Report(struct timeval &);

inline void Reset()   {gettimeofday(&StopWatch, 0);}

inline time_t Seconds() {return StopWatch.tv_sec;}

inline void Set(struct timeval &tod)
                       {StopWatch.tv_sec  = tod.tv_sec;
                        StopWatch.tv_usec = tod.tv_usec;
                       }

static void Snooze(int seconds);

static char *s2hms(int sec, char *buff, int blen);

static int  TimeZone();

static void Wait(int milliseconds);

static void Wait4Midnight();

      XrdSysTimer() {Reset();}

private:
      struct timeval StopWatch;     // Current running clock
      struct timeval LastReport;    // Total time from last report

      unsigned long Report();       // Place interval in Last Report
};
#endif
