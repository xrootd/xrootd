/******************************************************************************/
/*                                                                            */
/*                        X r d S y s T i m e r . c c                         */
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
#include <unistd.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include "XrdSys/XrdSysTimer.hh"
#include <iostream>

/******************************************************************************/
/*                            D e l t a _ T i m e                             */
/******************************************************************************/

struct timeval *XrdSysTimer::Delta_Time(struct timeval &tbeg)
{
    gettimeofday(&LastReport, 0);
    LastReport.tv_sec  = LastReport.tv_sec  - tbeg.tv_sec;
    LastReport.tv_usec = LastReport.tv_usec - tbeg.tv_usec;
    if (LastReport.tv_usec < 0) {LastReport.tv_sec--; LastReport.tv_usec += 1000000;}
    return &LastReport;
}

/******************************************************************************/
/*                              M i d n i g h t                               */
/******************************************************************************/

time_t XrdSysTimer::Midnight(time_t tnow)
{
   struct tm midtime;
   time_t add_time;

// Compute time at midnight
//
   if (tnow == 0 || tnow == 1) {add_time = tnow; tnow = time(0);}
      else add_time = 0;
   localtime_r((const time_t *) &tnow, &midtime);
   if (add_time) {midtime.tm_hour = 23; midtime.tm_min = midtime.tm_sec = 59;}
      else        midtime.tm_hour =     midtime.tm_min = midtime.tm_sec = 0;
   return mktime(&midtime) + add_time;
}
  
/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/

unsigned long XrdSysTimer::Report()
{
   unsigned long current_time;

// Get current time of day 
//
    gettimeofday(&LastReport, 0);
    current_time = (unsigned long)LastReport.tv_sec;

// Calculate the time interval thus far
//
   LastReport.tv_sec  = LastReport.tv_sec  - StopWatch.tv_sec;
   LastReport.tv_usec = LastReport.tv_usec - StopWatch.tv_usec;
   if (LastReport.tv_usec < 0) 
       {LastReport.tv_sec--; LastReport.tv_usec += 1000000;}

// Return the current time
//
    return current_time;
}

/******************************************************************************/

unsigned long XrdSysTimer::Report(double &Total_Time)
{
    unsigned long report_time = Report();

// Add up the time as a double
//
    Total_Time += static_cast<double>(LastReport.tv_sec) +
                  static_cast<double>(LastReport.tv_usec/1000)/1000.0;

// Return time
//
   return report_time;
}

/******************************************************************************/

unsigned long XrdSysTimer::Report(unsigned long &Total_Time)
{
    unsigned long report_time = Report();

// Add up the time as a 32-bit value to nearest milliseconds (max = 24 days)
//
    Total_Time += (unsigned long)LastReport.tv_sec*1000 +
                  (unsigned long)(LastReport.tv_usec/1000);

// Return time
//
   return report_time;
}

/******************************************************************************/

unsigned long XrdSysTimer::Report(unsigned long long &Total_Time)
{
    unsigned long report_time = Report();

// Add up the time as a 64-bit value to nearest milliseconds
//
    Total_Time += (unsigned long long)LastReport.tv_sec*1000 +
                  (unsigned long long)(LastReport.tv_usec/1000);

// Return time
//
   return report_time;
}

/******************************************************************************/

unsigned long XrdSysTimer::Report(struct timeval &Total_Time)
{
    unsigned long report_time = Report();

// Add the interval to the interval total time so far
//
   Total_Time.tv_sec  += LastReport.tv_sec;
   Total_Time.tv_usec += LastReport.tv_usec;
   if (Total_Time.tv_usec > 1000000) {Total_Time.tv_sec++;
                                      Total_Time.tv_usec -= 1000000;}

// Return time
//
   return report_time;
}

/******************************************************************************/
/*                                S n o o z e                                 */
/******************************************************************************/
  
void XrdSysTimer::Snooze(int sec)
{
#ifndef WIN32
 struct timespec naptime, waketime;

// Calculate nano sleep time
//
   naptime.tv_sec  =  sec;
   naptime.tv_nsec =  0;

// Wait for a lsoppy number of seconds
//
   while(nanosleep(&naptime, &waketime) && EINTR == errno)
        {naptime.tv_sec  =  waketime.tv_sec;
         naptime.tv_nsec =  waketime.tv_nsec;
        }
#else
   ::Sleep(sec*1000);
#endif
}
/******************************************************************************/
/*                                 s 2 h m s                                  */
/******************************************************************************/
  
char *XrdSysTimer::s2hms(int sec, char *buff, int blen)
{
   int hours, minutes;

   minutes = sec/60;
   sec     = sec%60;
   hours   = minutes/60;
   minutes = minutes%60;

   snprintf(buff, blen-1, "%d:%02d:%02d", hours, minutes, sec);
   buff[blen-1] = '\0';
   return buff;
}

/******************************************************************************/
/*                              T i m e Z o n e                               */
/******************************************************************************/

int XrdSysTimer::TimeZone()
{
  time_t currTime    = time(0);
  time_t currTimeGMT = 0;
  tm ptm;

  gmtime_r( &currTime, &ptm );
  currTimeGMT = mktime( &ptm );
  currTime /= 60*60;
  currTimeGMT /= 60*60;
  return currTime - currTimeGMT;
}
  
/******************************************************************************/
/*                                  W a i t                                   */
/******************************************************************************/
  
void XrdSysTimer::Wait(int mills)
{
#ifndef WIN32
 struct timespec naptime, waketime;

// Calculate nano sleep time
//
   naptime.tv_sec  =  mills/1000;
   naptime.tv_nsec = (mills%1000)*1000000;

// Wait for exactly x milliseconds
//
   while(nanosleep(&naptime, &waketime) && EINTR == errno)
        {naptime.tv_sec  =  waketime.tv_sec;
         naptime.tv_nsec =  waketime.tv_nsec;
        }
#else
   ::Sleep(mills);
#endif
}

/******************************************************************************/
/*                         W a i t 4 M i d n i g h t                          */
/******************************************************************************/
  
void XrdSysTimer::Wait4Midnight()
{

// Wait until midnight arrives
//
#ifndef __APPLE__
   timespec Midnite = {Midnight(1), 0};
   while(clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&Midnite,0) == EINTR) {}
#else
   timespec tleft, Midnite = {Midnight(1) - time(0), 0};
   int ntpWait = 60;
do{while(nanosleep(&Midnite, &tleft) && EINTR == errno)
        {Midnite.tv_sec  = tleft.tv_sec;
         Midnite.tv_nsec = tleft.tv_nsec;
        }
   if (Midnight(1) - time(0) >= 60) break;
   Midnite.tv_sec  = 1;
   Midnite.tv_nsec = 0;
  } while(ntpWait--);  // This avoids multiple wakeups when NTP adjusts clock
#endif
}
