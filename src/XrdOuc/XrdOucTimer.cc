/******************************************************************************/
/*                                                                            */
/*                        X r d O u c T i m e r . c c                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

const char *XrdOucTimerCVSID = "$Id$";

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include "XrdOuc/XrdOucTimer.hh"

/******************************************************************************/
/*                            D e l t a _ T i m e                             */
/******************************************************************************/

struct timeval *XrdOucTimer::Delta_Time(struct timeval &tbeg)
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

time_t XrdOucTimer::Midnight(time_t tnow)
{
   struct tm midtime;

// Compute time at midnight
//
   if (!tnow) tnow = time((time_t)0);
   localtime_r((const time_t *) &tnow, &midtime);
   midtime.tm_hour = midtime.tm_min = midtime.tm_sec = 0;
   return mktime(&midtime);
}
  
/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/

unsigned long XrdOucTimer::Report()
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

unsigned long XrdOucTimer::Report(double &Total_Time)
{
    unsigned long report_time = Report();

// Add up the time as a double
//
    Total_Time += (double)LastReport.tv_sec +
                 ((double)(LastReport.tv_usec/1000))/1000.0;

// Return time
//
   return report_time;
}

/******************************************************************************/

unsigned long XrdOucTimer::Report(unsigned long &Total_Time)
{
    unsigned long report_time = Report();

// Add up the time as a 32-bit value to nearest microsecond (max = 24 days)
//
    Total_Time += (unsigned long)LastReport.tv_sec*1000 +
                  (unsigned long)(LastReport.tv_usec/1000);

// Return time
//
   return report_time;
}

/******************************************************************************/

unsigned long XrdOucTimer::Report(unsigned long long &Total_Time)
{
    unsigned long report_time = Report();

// Add up the time as a 64-bit value to nearest microsecond
//
    Total_Time += (unsigned long long)LastReport.tv_sec*1000 +
                  (unsigned long long)(LastReport.tv_usec/1000);

// Return time
//
   return report_time;
}

/******************************************************************************/

unsigned long XrdOucTimer::Report(struct timeval &Total_Time)
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
/*                                 s 2 h m s                                  */
/******************************************************************************/
  
char *XrdOucTimer::s2hms(int sec, char *buff, int blen)
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
/*                                  W a i t                                   */
/******************************************************************************/
  
void XrdOucTimer::Wait(int mills)
{
 struct timespec naptime, waketime;

// Calculate nano sleep time
//
   naptime.tv_sec  =  mills/1000;
   naptime.tv_nsec = (mills%1000)*1000000;

// Initialize the pollfd structure in a way to prevent it from ever catching
// an event. All we really want is to wait for x milliseconds.
//
   while(nanosleep(&naptime, &waketime) && EINTR == errno)
        {naptime.tv_sec  =  waketime.tv_sec;
         naptime.tv_nsec =  waketime.tv_nsec;
        }
}
