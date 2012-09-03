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

class XrdSysLogger
{
public:
         XrdSysLogger(int ErrFD=STDERR_FILENO, int xrotate=1);

        ~XrdSysLogger() {if (ePath) free(ePath);}

// Bind allows you to bind standard error to a file with an optional periodic
// closing and opening of the file.
//
int Bind(const char *path, int intsec=0);

// Flush any pending output
//
void Flush() {fsync(eFD);}

// originalFD() returns the base FD that we started with
//
int  originalFD() {return baseFD;}

// Output data and optionally prefix with date/time
//
void Put(int iovcnt, struct iovec *iov);

// Set log file keep value. A negative number means keep abs() files.
//                          A positive number means keep no more than n bytes.
//
void setKeep(long long knum) {eKeep = knum;}

// Set log file rotation on/off. Used by forked processes.
//
void setRotate(int onoff) {doLFR = onoff;}

// Return formated date/time (tbuff must be atleast 24 characters)
//
int Time(char *tbuff);

// traceBeg() obtains  the logger lock and returns a message header.
// traceEnd() releases the logger lock and returns a newline
//
char *traceBeg() {Logger_Mutex.Lock(); Time(TBuff); return TBuff;}
char  traceEnd() {Logger_Mutex.UnLock(); return '\n';}

// xlogFD() returns the FD to be used by external programs as their STDERR.
// A negative one indicates that no special FD is assigned.
//
int   xlogFD();

private:

XrdSysMutex Logger_Mutex;
static int extLFD[4];
long long  eKeep;
char       TBuff[24];        // Trace header buffer
int        eFD;
int        baseFD;
char      *ePath;
char       Filesfx[8];
time_t     eNTC;
int        eInt;
time_t     eNow;
int        doLFR;

void   putEmsg(char *msg, int msz);
int    ReBind(int dorename=1);
void   Trim();
};
#endif
