/******************************************************************************/
/*                                                                            */
/*                       X r d S y s L o g g e r . c c                        */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <dirent.h>
#include <unistd.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/termios.h>
#include <sys/uio.h>
#endif // WIN32

#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysUtils.hh"
  
/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define BLAB(x) cerr <<"Logger " <<x <<"!!!" <<endl

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/

void  *XrdSysLoggerMN(void *carg)
      {XrdSysLogger::Task *tP = (XrdSysLogger::Task *)carg;
       while(tP) {tP->Ring(); tP = tP->Next();}
       return (void *)0;
      }

struct XrdSysLoggerRP
      {XrdSysLogger   *logger;
       XrdSysSemaphore active;

                       XrdSysLoggerRP(XrdSysLogger *lp) : logger(lp), active(0)
                                        {}
                      ~XrdSysLoggerRP() {}
      };
  
void  *XrdSysLoggerRT(void *carg)
      {XrdSysLoggerRP *rP = (XrdSysLoggerRP *)carg;
       XrdSysLogger   *lp = rP->logger;
       rP->active.Post();
       lp->zHandler();
       return (void *)0;
      }

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSysLogger::XrdSysLogger(int ErrFD, int dorotate)
{
   char * logFN;

   ePath = 0;
   eInt  = 0;
   eFD   = ErrFD;
   eKeep = 0;
   doLFR = (dorotate != 0);
   msgList = 0;
   taskQ   = 0;
   lfhTID  = 0;
   hiRes   = false;
   fifoFN  = 0;
   reserved1 = 0;

// Establish default log file name
//
   if (!(logFN = getenv("XrdSysLOGFILE"))) logFN = getenv("XrdOucLOGFILE");

// Establish message routing
//
   if (ErrFD != STDERR_FILENO) baseFD = ErrFD;
      else {baseFD = XrdSysFD_Dup(ErrFD);
            Bind(logFN, 1);
           }
}
  
/******************************************************************************/
/*                                A d d M s g                                 */
/******************************************************************************/

void XrdSysLogger::AddMsg(const char *msg)
{
   mmMsg *tP, *nP = new mmMsg;

// Fill out new message
//
   nP->next = 0;
   nP->msg  = strdup(msg);
   nP->mlen = strlen(msg);

// Add new line character if one is missing (we steal the null byte for this)
//
   if (nP->mlen > 1 && nP->msg[nP->mlen-1] != '\n')
      {nP->msg[nP->mlen] = '\n'; nP->mlen += 1;}

// Add this message to the end of the list
//
   Logger_Mutex.Lock();
   if (!(tP = msgList)) msgList = nP;
      else {while(tP->next) tP = tP->next;
            tP->next = nP;
           }
   Logger_Mutex.UnLock();
}
  
/******************************************************************************/
/*                            A t M i d n i g h t                             */
/******************************************************************************/
  
void XrdSysLogger::AtMidnight(XrdSysLogger::Task *mnTask)
{

// Place this task on the task queue
//
   Logger_Mutex.Lock();
   mnTask->next = taskQ;
   taskQ = mnTask;
   Logger_Mutex.UnLock();
}

/******************************************************************************/
/*                                  B i n d                                   */
/******************************************************************************/
  
int XrdSysLogger::Bind(const char *path, int lfh)
{
   XrdSysLoggerRP rtParms(this);
   int rc;

// Kill logfile handler thread if parameters will be changing
//
   if (lfh > 0) lfh = 1;
   if (lfhTID && (eInt != lfh || !path))
      {XrdSysThread::Kill(lfhTID);
       lfhTID = 0;
      }

// Bind to stderr if no path specified
//
   if (ePath) free(ePath);
   eInt   = 0;
   ePath  = 0;
   if (fifoFN) free(fifoFN);
   fifoFN = 0;
   if (!path) return 0;

// Bind to a log file
//
   eInt  = lfh;
   ePath = strdup(path);
   doLFR = (lfh > 0);
   if ((rc = ReBind(0))) return rc;

// Handle specifics of lofile rotation
//
   if (eInt == onFifo) {if ((rc = FifoMake())) return -rc;}
      else if (eInt < 0 && !XrdSysUtils::SigBlock(-eInt))
              {rc = errno;
               BLAB("Unable to block logfile signal " <<-eInt <<"; "
                    <<strerror(rc));
               eInt = 0;
               return -rc;
              }

// Start a log rotation thread
//
   rc = XrdSysThread::Run(&lfhTID, XrdSysLoggerRT, (void *)&rtParms, 0,
                          "Logfile handler");
   if (!rc) rtParms.active.Wait();
   return (rc > 0 ? -rc : rc);
}

/******************************************************************************/
/*                             P a r s e K e e p                              */
/******************************************************************************/

int XrdSysLogger::ParseKeep(const char *arg)
{
   char *eP;

// First check to see if this is a sig type
//
   eKeep = 0;
   if (isalpha(*arg))
      {if (!strcmp(arg, "fifo")) return onFifo;
       return -XrdSysUtils::GetSigNum(arg);
      }

// Process an actual keep count
//
   eKeep = strtoll(arg, &eP, 10);
   if (!(*eP) || eKeep < 0) {eKeep = -eKeep; return 1;}

// Process an actual keep size
//
   if (*(eP+1)) return 0;
         if (*eP == 'k' || *eP == 'K') eKeep *= 1024LL;
    else if (*eP == 'm' || *eP == 'M') eKeep *= 1024LL*1024LL;
    else if (*eP == 'g' || *eP == 'G') eKeep *= 1024LL*1024LL*1024LL;
    else if (*eP == 't' || *eP == 'T') eKeep *= 1024LL*1024LL*1024LL*1024LL;
    else return 0;

// All done
//
   return 1;
}
  
/******************************************************************************/
/*                                   P u t                                    */
/******************************************************************************/
  
void XrdSysLogger::Put(int iovcnt, struct iovec *iov)
{
    int retc;
    char tbuff[32];

// Prefix message with time if calle wants it so
//
   if (!iov[0].iov_base)
      {iov[0].iov_base = tbuff;
       iov[0].iov_len  = (int)Time(tbuff);
      }

// Obtain the serailization mutex if need be
//
   Logger_Mutex.Lock();

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
  
int XrdSysLogger::Time(char *tbuff)
{
    struct timeval tVal;
    const int minblen = 32;
    struct tm tNow;
    int i;

// Get the current time
//
   gettimeofday(&tVal, 0);

// Format the time in human terms
//
   tbuff[minblen-1] = '\0'; // tbuff must be at least 32 bytes long
   localtime_r((const time_t *) &tVal.tv_sec, &tNow);

// Choose appropriate output
//
   if (hiRes)
      {i = snprintf(tbuff, minblen, "%02d%02d%02d %02d:%02d:%02d.%06d %03ld ",
                    tNow.tm_year-100, tNow.tm_mon+1, tNow.tm_mday,
                    tNow.tm_hour,     tNow.tm_min,   tNow.tm_sec,
                    static_cast<int>(tVal.tv_usec), XrdSysThread::Num());
      } else {
       i = snprintf(tbuff, minblen, "%02d%02d%02d %02d:%02d:%02d %03ld ",
                    tNow.tm_year-100, tNow.tm_mon+1, tNow.tm_mday,
                    tNow.tm_hour,     tNow.tm_min,   tNow.tm_sec,
                    XrdSysThread::Num());
      }
   return (i >= minblen ? minblen-1 : i);
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              F i f o M a k e                               */
/******************************************************************************/
  
int XrdSysLogger::FifoMake()
{
   struct stat Stat;
   char buff[2048], *slash;
   int n, rc, saveInt = eInt;

// Assume failure (just to keep down the code)
//
   eInt = 0;

// Construct the fifo name
//
   if (!(slash = rindex(ePath, '/')))
      {*buff = '.';
       strcpy(buff+1, ePath);
      } else {
       n = slash - ePath + 1;
       strncpy(buff, ePath, n);
       buff[n] = '.';
       strcpy(&buff[n+1], slash+1);
      }

// Check if the fifo exists and is usable or that we can create it
//
   if (!stat(buff, &Stat))
      {if (!S_ISFIFO(Stat.st_mode))
          {BLAB("Logfile fifo " <<buff <<" exists but is not a fifo");
           return EEXIST;
          }
       if (access(buff, R_OK))
          {BLAB("Unable to access " <<buff);
           return EACCES;
          }
       } else {
       if (errno != ENOENT)
          {rc = errno;
           BLAB("Unable to stat " <<buff <<"; " <<strerror(rc));
           return rc;
          }
       if (mkfifo(buff, S_IRUSR|S_IWUSR))
          {rc = errno;
           BLAB("Unable to create logfile fifo " <<buff <<"; " <<strerror(rc));
           return rc;
          }
       }

// Save the fifo path restore eInt
//
   fifoFN = strdup(buff);
   eInt = saveInt;
   return 0;
}

/******************************************************************************/
/*                              F i f o W a i t                               */
/******************************************************************************/
  
void XrdSysLogger::FifoWait()
{
   char buff[64];
   int pipeFD, rc;

// Open the fifo. We can't have this block as we need to make sure it is
// closed on EXEC as fast as possible (Linux has a non-portable solution).
//
   if ((pipeFD = XrdSysFD_Open(fifoFN, O_RDONLY)) < 0)
      {rc = errno;
       BLAB("Unable to open logfile fifo " <<fifoFN <<"; " <<strerror(rc));
       eInt = 0;
       free(fifoFN); fifoFN = 0;
       return;
      }

// Wait for read, this will block. If we got an EOF then something went wrong!
//
   if (!read(pipeFD, buff, sizeof(buff)))
      {BLAB("Unexpected EOF on logfile fifo " <<fifoFN);
       eInt = 0;
      }
   close(pipeFD);
}

/******************************************************************************/
/*                               p u t E m s g                                */
/******************************************************************************/
  
// This internal logging method is used when the caller already has the mutex!

void XrdSysLogger::putEmsg(char *msg, int msz)
{
    struct iovec eVec[2];
    int retc;
    char tbuff[24];

// Prefix message with time
//
   eVec[0].iov_base = tbuff;
   eVec[0].iov_len  = (int)Time(tbuff);
   eVec[1].iov_base = msg;
   eVec[1].iov_len  = msz;

// In theory, writev may write out a partial list. This rarely happens in
// practice and so we ignore that possibility (recovery is pretty tough).
//
   do { retc = writev(eFD, (const struct iovec *)eVec, 2);}
               while (retc < 0 && errno == EINTR);
}

/******************************************************************************/
/*                                R e B i n d                                 */
/******************************************************************************/
  
int XrdSysLogger::ReBind(int dorename)
{
   const char seq[] = "0123456789";
   unsigned int i;
   int newfd;
   struct tm nowtime;
   char *bp, buff[MAXPATHLEN+MAXNAMELEN];
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
       if (i < sizeof(seq)) rename(ePath, buff);
      }

// Compute the suffix for the file
//
   if (doLFR)
      {time_t eNow = time(0);
       localtime_r((const time_t *) &eNow, &nowtime);
       sprintf(buff, "%4d%02d%02d", nowtime.tm_year+1900, nowtime.tm_mon+1,
                                    nowtime.tm_mday);
       strncpy(Filesfx, buff, 8);
      }

// Open the file for output. Note that we can still leak a file descriptor
// if a thread forks a process before we are able to do the fcntl(), sigh.
//
   if ((newfd = XrdSysFD_Open(ePath,O_WRONLY|O_APPEND|O_CREAT,0644)) < 0)
      return -errno;

// Now set the file descriptor to be the same as the error FD. This will
// close the previously opened file, if any.
//
   if (dup2(newfd, eFD) < 0)
      {int rc = errno;
       close(newfd);
       return -rc;
      }
   close(newfd);

// Check if we should trim log files
//
   if (eKeep && doLFR) Trim();
   return 0;
}

/******************************************************************************/
/*                                  T r i m                                   */
/******************************************************************************/

#ifndef WIN32
void XrdSysLogger::Trim()
{
   struct LogFile 
          {LogFile *next;
           char    *fn;
           off_t    sz;
           time_t   tm;

           LogFile(char *xfn, off_t xsz, time_t xtm)
                  {fn = (xfn ? strdup(xfn) : 0); sz = xsz; tm = xtm; next = 0;}
          ~LogFile() 
                  {if (fn)   free(fn);
                   if (next) delete next;
                  }
          } logList(0,0,0);

   struct LogFile *logEnt, *logPrev, *logNow;
   char eBuff[2048], logFN[MAXNAMELEN+8], logDir[MAXPATHLEN+8], *logSfx;
   struct dirent *dp;
   struct stat buff;
   long long totSz = 0;
   int n,rc, totNum= 0;
   DIR *DFD;

// Ignore this call if we are not deleting log files
//
   if (!eKeep) return;

// Construct the directory path
//
   if (!ePath) return;
   strcpy(logDir, ePath);
   if (!(logSfx = rindex(logDir, '/'))) return;
   *logSfx = '\0';
   strcpy(logFN, logSfx+1);
   n = strlen(logFN);

// Open the directory
//
   if (!(DFD = opendir(logDir)))
      {int msz = sprintf(eBuff, "Error %d (%s) opening log directory %s\n",
                                errno, strerror(errno), logDir);
       putEmsg(eBuff, msz);
       return;
      }
    *logSfx++ = '/';

// Record all of the log files currently in this directory
//
   errno = 0;
   while((dp = readdir(DFD)))
        {if (strncmp(dp->d_name, logFN, n)) continue;
         strcpy(logSfx, dp->d_name);
         if (stat(logDir, &buff) || !(buff.st_mode & S_IFREG)) continue;

         totNum++; totSz += buff.st_size;
         logEnt = new LogFile(dp->d_name, buff.st_size, buff.st_mtime);
         logPrev = &logList; logNow = logList.next;
         while(logNow && logNow->tm < buff.st_mtime)
              {logPrev = logNow; logNow = logNow->next;}

         logPrev->next = logEnt; 
         logEnt->next  = logNow;
        }

// Check if we received an error
//
   rc = errno; closedir(DFD);
   if (rc)
      {int msz = sprintf(eBuff, "Error %d (%s) reading log directory %s\n",
                                rc, strerror(rc), logDir);
       putEmsg(eBuff, msz);
       return;
      }

// If there is only one log file here no need to
//
   if (totNum <= 1) return;

// Check if we need to trim log files
//
   if (eKeep < 0)
      {if ((totNum += eKeep) <= 0) return;
      } else {
       if (totSz <= eKeep)         return;
       logNow = logList.next; totNum = 0;
       while(logNow && totSz > eKeep)
            {totNum++; totSz -= logNow->sz; logNow = logNow->next;}
      }

// Now start deleting log files
//
   logNow = logList.next;
   while(logNow && totNum--)
        {strcpy(logSfx, logNow->fn);
         if (unlink(logDir))
            rc = sprintf(eBuff, "Error %d (%s) removing log file %s\n",
                                errno, strerror(errno), logDir);
            else rc = sprintf(eBuff, "Removed log file %s\n", logDir);
         putEmsg(eBuff, rc);
         logNow = logNow->next;
        }
}
#else
void XrdSysLogger::Trim()
{
}
#endif

/******************************************************************************/
/*                              z H a n d l e r                               */
/******************************************************************************/
#include <poll.h>

void XrdSysLogger::zHandler()
{
   mmMsg   *mP;
   sigset_t sigset;
   pthread_t tid;
   int      signo, rc;
   Task     *tP;

// If we will be handling via signals, set it up now
//
   if (eInt < 0 && !fifoFN)
      {signo = -eInt;
       if ((sigemptyset(&sigset) == -1)
       ||  (sigaddset(&sigset,signo) == -1))
          {rc = errno;
           BLAB("Unable to use logfile signal " <<signo <<"; " <<strerror(rc));
           eInt = 0;
          }
      }

// This is a perpetual loop to handle the log file
//
   while(1)
        {     if (fifoFN)    FifoWait();
         else if (eInt >= 0) XrdSysTimer::Wait4Midnight();
         else if ((sigwait(&sigset, &signo) == -1))
                 {rc = errno;
                  BLAB("Unable to wait on logfile signal " <<signo
                       <<"; " <<strerror(rc));
                  eInt = 0;
                  continue;
                 }

         Logger_Mutex.Lock();
         ReBind();

         mP = msgList;
         while(mP)
              {putEmsg(mP->msg, mP->mlen);
               mP = mP->next;
              }
         tP = taskQ;
         Logger_Mutex.UnLock();

         if (tP)
            {if (XrdSysThread::Run(&tid, XrdSysLoggerMN, (void *)tP, 0,
                                   "Midnight Ringer Task"))
                {char eBuff[256];
                 rc = sprintf(eBuff, "Error %d (%s) running ringer task.\n",
                                     errno, strerror(errno));
                 putEmsg(eBuff, rc);
                }
            }
        }
}
