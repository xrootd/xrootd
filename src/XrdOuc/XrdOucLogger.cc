/******************************************************************************/
/*                                                                            */
/*                       X r d O u c L o g g e r . c c                        */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC03-76-SFO0515 with the Deprtment of Energy                  */
/******************************************************************************/

//       $Id$ 

const char *XrdOucLoggerCVSID = "$Id$";

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/uio.h>
#ifndef __macos__
#include <stropts.h>
#endif

#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucTimer.hh"
 
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOucLogger::XrdOucLogger(int ErrFD, int dorotate, int mask)
{
   ePath = 0;
   eNTC  = 0;
   eInt  = 0;
   eNow  = 0;
   eFD   = ErrFD;
   doLFR = dorotate;
   logMask = mask;

// Establish message routing
//
   if (ErrFD != STDERR_FILENO) baseFD = ErrFD;
      else {baseFD = dup(ErrFD);
            fcntl(baseFD, F_SETFD, FD_CLOEXEC);
            Bind(getenv("XrdOucLOGFILE"), 86400);
           }
}
  
/******************************************************************************/
/*                                  B i n d                                   */
/******************************************************************************/
  
int XrdOucLogger::Bind(const char *path, int isec)
{

// Compute time at midnight
//
   eNow = time(0);
   eNTC = XrdOucTimer::Midnight(eNow);

// Bind to the logfile as needed
//
   if (path) 
      {eInt  = isec;
       if (ePath) free(ePath);
       ePath = strdup(path);
       return ReBind(0);
      }
   eInt = 0;
   ePath = 0;
   return 0;
}

/******************************************************************************/
/*                                   P u t                                    */
/******************************************************************************/
  
void XrdOucLogger::Put(int iovcnt, struct iovec *iov)
{
    int retc;
    char tbuff[24];

// Prefix message with time if calle wants it so
//
   if (iov[0].iov_base) eNow = time(0);
      else {iov[0].iov_base = tbuff;
            iov[0].iov_len  = (int)Time(tbuff);
           }

// Obtain the serailization mutex if need be
//
   Logger_Mutex.Lock();

// Check if we should close and reopen the output
//
   if (eInt && eNow >= eNTC) ReBind();

// In theory, writev may write out a partial list. This rarely happens in
// practice and so we ignore that possibility (recovery is pretty tough).
//
   do { retc = writev(eFD, (const struct iovec *)iov, iovcnt);}
               while (retc < 0 && errno == EINTR);

// Release the serailization mutex if need be
//
   Logger_Mutex.UnLock();
}

/******************************************************************************/
/*                                  T i m e                                   */
/******************************************************************************/
  
int XrdOucLogger::Time(char *tbuff)
{
    eNow = time(0);
    struct tm tNow;

// Format the header
//
   tbuff[23] = '\0'; // tbuff must be at least 24 bytes long
   localtime_r((const time_t *) &eNow, &tNow);
   return snprintf(tbuff, 23, "%02d%02d%02d %02d:%02d:%02d %03ld ",
                  tNow.tm_year-100, tNow.tm_mon+1, tNow.tm_mday,
                  tNow.tm_hour,     tNow.tm_min,   tNow.tm_sec,
                  XrdOucThread::Num());
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                R e B i n d                                 */
/******************************************************************************/
  
int XrdOucLogger::ReBind(int dorename)
{
   const char seq[] = "0123456789";
   unsigned int i;
   int newfd;
   struct tm nowtime;
   char *bp, buff[1280];
   struct stat bf;

// Rename the file to be of the form yyyymmdd corresponding to the date it was
// opened. We will add a sequence number (.x) if a conflict occurs.
//
   if (dorename && doLFR)
      {strcpy(buff, ePath);
       bp = buff+strlen(ePath);
       *bp++ = '.';
       strncpy(bp, Filesfx, 8);
       bp += 8;
       *bp = '\0'; *(bp+2) = '\0';
       for (i = 0; i < sizeof(seq) && !stat(buff, &bf); i++)
           {*bp = '.'; *(bp+1) = (char)seq[i];}
       if (i < sizeof(seq)) rename((const char *)ePath, (const char *)buff);
      }

// Compute the new suffix
//
   localtime_r((const time_t *) &eNow, &nowtime);
   sprintf(buff, "%4d%02d%02d", nowtime.tm_year+1900, nowtime.tm_mon+1,
                                nowtime.tm_mday);
   strncpy(Filesfx, buff, 8);

// Set new close interval
//
   if (eInt > 0) while(eNTC <= eNow) eNTC += eInt;

// Open the file for output
//
   if ((newfd = open(ePath,O_WRONLY|O_APPEND|O_CREAT,0644)) < 0) return -errno;

// Now set the file descriptor to be the same as the error FD. This will
// close the previously opened file, if any.
//
   if (dup2(newfd, eFD) < 0) return -errno;
   close(newfd);
   return 0;
}
