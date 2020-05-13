#ifndef __XRD_POLL_H__
#define __XRD_POLL_H__
/******************************************************************************/
/*                                                                            */
/*                            X r d P o l l . h h                             */
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

#include <sys/poll.h>
#include "XrdSys/XrdSysPthread.hh"

#define XRD_NUMPOLLERS 3

class XrdPollInfo;
class XrdSysSemaphore;
  
class XrdPoll
{
public:

// Attach() is called when a new link needs to be assigned to a poller
//
static  int   Attach(XrdPollInfo &pInfo);

// Detach() is called when a link is being discarded
//
static  void  Detach(XrdPollInfo &pInfo);

// Disable() is called when we need to mask interrupts from a link
//
virtual void  Disable(XrdPollInfo &pInfo, const char *etxt=0) = 0;

// Enable() is called when we want to receive interrupts from a link
//
virtual int   Enable(XrdPollInfo &pInfo)  = 0;

// Finish() is called to allow a link to gracefully terminate when scheduled
//
static  int   Finish(XrdPollInfo &pInfo, const char *etxt=0); //Implementation supplied

// Poll2Text() converts bits in an revents item to text
//
static  char *Poll2Text(short events); // Implementation supplied

// Setup() is called at config time to perform poller configuration
//
static  int   Setup(int numfd);        // Implementation supplied

// Start() is called via a thread for each poller that was created
//
virtual void  Start(XrdSysSemaphore *syncp, int &rc) = 0;

// Stats() is called to provide statistics on polling
//
static  int   Stats(char *buff, int blen, int do_sync=0);

// Identification of the thread handling this object
//
           int         PID;       // Poller ID
           pthread_t   TID;       // Thread ID

// The following table reference the pollers in effect
//
static     XrdPoll   *Pollers[XRD_NUMPOLLERS];

           XrdPoll();
virtual   ~XrdPoll() {}

protected:

static     const char   *TraceID;                  // For tracing

// Gets the next request on the poll pipe. This is common to all implentations.
//
           int         getRequest();             // Implementation supplied

// Exclude() called to exclude a link from a poll set
//
virtual    void        Exclude(XrdPollInfo &pInfo) = 0;

// Include() called to include a link in a poll set
//
virtual    int         Include(XrdPollInfo &pInfo) = 0;

// newPoller() called to get a new poll object at initialization time
//             Even though static, an implementation must be supplied.
//
static     XrdPoll   *newPoller(int pollid, int numfd)    /* = 0 */;

// The following is common to all implementations
//
XrdSysMutex   PollPipe;
struct pollfd PipePoll;
int           CmdFD;      // FD to send PipeData commands
int           ReqFD;      // FD to recv PipeData requests
struct        PipeData {union {XrdSysSemaphore  *theSem;
                               struct {int fd;
                                       int ent;} Arg;
                              } Parms;
                        enum cmd {EnFD, DiFD, RmFD, Post};
                        cmd req;
                       };
              PipeData ReqBuff;
char         *PipeBuff;
int           PipeBlen;

// The following are statistical counters each implementation must maintain
//
           int         numEnabled;     // Count of Enable() calls
           int         numEvents;      // Count of poll fd's dispatched
           int         numInterrupts;  // Number of interrupts (e.g., signals)

private:

static     XrdSysMutex  doingAttach;
           int          numAttached;    // Number of fd's attached to poller
};
#endif
