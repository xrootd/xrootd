#ifndef __SYS_LOGGER_H__
#define __SYS_LOGGER_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S y s L o g g e r . h h                        */
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

#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#include <string.h>
#include <strings.h>
#else
#include <string.h>
#include <io.h>
#include "XrdSys/XrdWin32.hh"
#endif

#include "XrdSys/XrdSysPthread.hh"

//-----------------------------------------------------------------------------
//! XrdSysLogger is the object that is used to route messages to wherever they
//! need to go and also handles log file rotation and trimming.
//-----------------------------------------------------------------------------

class XrdSysLogger
{
public:

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  errFD     is the filedescriptor of where error messages normally
//!                   go if this class is not used. Default is stderr.
//! @param  xrotate   when not zero performs internal log rotatation. Otherwise,
//!                   log rotation is suppressed. See also setRotate().
//-----------------------------------------------------------------------------

         XrdSysLogger(int ErrFD=STDERR_FILENO, int xrotate=1);

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

        ~XrdSysLogger() {if (ePath) free(ePath);}

//-----------------------------------------------------------------------------
//! Add a message to be printed at midnight.
//!
//! @param  msg       The message to be printed. A copy of the message is saved.
//-----------------------------------------------------------------------------

void  AddMsg(const char *msg);

//-----------------------------------------------------------------------------
//! Add a task to be run at midnight. Tasks are run sequentially lifo.
//!
//! @param  mnTask    Pointer to an instance of the task object below.
//-----------------------------------------------------------------------------

class Task
{
public:
friend class XrdSysLogger;

virtual void  Ring() = 0; //!< This method gets called at midnight

inline  Task *Next() {return next;}

              Task() : next(0) {}
virtual      ~Task() {}

private:
Task *next;
};

void  AtMidnight(Task *mnTask);

//-----------------------------------------------------------------------------
//! Bind allows you to bind the file descriptor passed at construction time to
//! a file with an optional periodic closing and opening of the file.
//!
//! @param  path      The log file path. The file is created, if need be.
//!                   If path is null, messages are routed to stderr and the
//!                   lfh argument is ignored.
//! @param  lfh       Log file handling:
//!                   >0 file is to be closed and opened at midnight.
//!                      This implies automatic log rotation.
//!                   =0 file is to be left open all the time.
//!                      This implies no        log rotation.
//!                   <0 file is to be closed and opened only on signal abs(lfh)
//!                      unless the value equals onFifo. In this case a fifo is
//!                      used to control log file rotation. The name of the fifo
//!                      is path with the filename component prefixed by dot.
//!                      This implies manual    log rotation. Warning! Using
//!                      signals requires that Bind() be called before starting
//!                      any threads so that the signal is properly blocked.
//!
//! @return  0        Processing successful.
//! @return <0        Unable to bind, returned value is -errno of the reason.
//-----------------------------------------------------------------------------

static const int onFifo = (int)0x80000000;

int Bind(const char *path, int lfh=0);

//-----------------------------------------------------------------------------
//! Flush any pending output
//-----------------------------------------------------------------------------

void Flush() {fsync(eFD);}

//-----------------------------------------------------------------------------
//! Get the file descriptor passed at construction time.
//!
//! @return the file descriptor passed to the constructor.
//-----------------------------------------------------------------------------

int  originalFD() {return baseFD;}

//-----------------------------------------------------------------------------
//! Parse the keep option argument.
//!
//! @param  arg       Pointer to the argument. The argument syntax is:
//!                   <count> | <size> | fifo | <signame>
//!
//! @return !0        Parsing succeeded. The return value is the argument that
//!                   must be passed as the lfh parameter to Bind().
//! @return =0        Invalid keep argument.
//-----------------------------------------------------------------------------

int  ParseKeep(const char *arg);

//-----------------------------------------------------------------------------
//! Output data and optionally prefix with date/time
//!
//! @param  iovcnt    The number of elements in iov vector.
//! @param  iov       The vector describing what to print. If iov[0].iov_base
//!                   is zero, the message is prefixed by date and time.
//-----------------------------------------------------------------------------

void Put(int iovcnt, struct iovec *iov);

//-----------------------------------------------------------------------------
//! Set log file timstamp to high resolution (hh:mm:ss.uuuu).
//-----------------------------------------------------------------------------

void setHiRes() {hiRes = true;}

//-----------------------------------------------------------------------------
//! Set log file keep value.
//!
//! @param  knum      The keep value. If knum < 0 then abs(knum) files are kept.
//!                   Otherwise, only knum bytes of log files are kept.
//-----------------------------------------------------------------------------

void setKeep(long long knum) {eKeep = knum;}

//-----------------------------------------------------------------------------
//! Set log file rotation on/off.
//!
//! @param  onoff     When !0 turns on log file rotations. Otherwise, rotation
//!                   is turned off.
//-----------------------------------------------------------------------------

void setRotate(int onoff) {doLFR = onoff;}

//-----------------------------------------------------------------------------
//! Start trace message serialization. This method must be followed by a call
//! to traceEnd().
//!
//! @return pointer to the time buffer to be used as the msg timestamp.
//-----------------------------------------------------------------------------

char *traceBeg() {Logger_Mutex.Lock(); Time(TBuff); return TBuff;}

//-----------------------------------------------------------------------------
//! Stop trace message serialization. This method must be preceeded by a call
//! to traceBeg().
//!
//! @return pointer to a new line character to terminate the message.
//-----------------------------------------------------------------------------

char  traceEnd() {Logger_Mutex.UnLock(); return '\n';}

//-----------------------------------------------------------------------------
//! Get the log file routing.
//!
//! @return the filename of the log file or "stderr".
//-----------------------------------------------------------------------------

const char *xlogFN() {return (ePath ? ePath : "stderr");}

//-----------------------------------------------------------------------------
//! Internal method to handle the logfile. This is public because it needs to
//! be called by an external thread.
//-----------------------------------------------------------------------------

void        zHandler();

private:
int         FifoMake();
void        FifoWait();
int         Time(char *tbuff);

struct mmMsg
      {mmMsg *next;
       int    mlen;
       char  *msg;
      };
mmMsg     *msgList;
Task      *taskQ;
XrdSysMutex Logger_Mutex;
long long  eKeep;
char       TBuff[32];        // Trace header buffer
int        eFD;
int        baseFD;
char      *ePath;
char       Filesfx[8];
int        eInt;
int        reserved1;
char      *fifoFN;
bool       hiRes;
bool       doLFR;
pthread_t  lfhTID;

void   putEmsg(char *msg, int msz);
int    ReBind(int dorename=1);
void   Trim();
};
#endif
