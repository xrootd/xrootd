#ifndef __XRD_POLL_H__
#define __XRD_POLL_H__
/******************************************************************************/
/*                                                                            */
/*                            x r d _ P o l l . h                             */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

#include <sys/poll.h>
#include "XrdOuc/XrdOucPthread.hh"

#define XRD_NUMPOLLERS 3

class XrdLink;
  
class XrdPoll
{
public:

// Attach() is called when a new link needs to be assigned to a poller
//
static  int   Attach(XrdLink *lp);    // Implementation supplied

// Detach() is called when a link is being discarded
//
static  void  Detach(XrdLink *lp);   //  Implementation supplied

// Disable() is called when we need to mask interrupts from a link
//
virtual void  Disable(XrdLink *lp, const char *etxt=0) = 0;

// Enable() is called when we want to receive interrupts from a link
//
virtual int   Enable(XrdLink *lp)  = 0;

// Poll2Text() converts bits in an revents item to text
//
static  char *Poll2Text(short events); // Implementation supplied

// Setup() is called at config time to perform poller configuration
//
static  int   Setup(int numfd);        // Implementation supplied

// Start() is called via a thread for each poller that was created
//
virtual    void        Start(XrdOucSemaphore *syncp, int &rc) = 0;

// Stats() is called to provide statistics on polling
//
static  int   Stats(char *buff, int blen);

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

static     const char *TraceID;                  // For tracing

// Gets the next request on the poll pipe. This is common to all implentations.
//
           int         getRequest();             // Implementation supplied

// Exclude() called to exclude a link from a poll set
//
virtual    void        Exclude(XrdLink *lp) = 0;

// Include() called to include a link in a poll set
//
virtual    int         Include(XrdLink *lp) = 0;

// newPoller() called to get a new poll object at initialization time
//             Even though static, an implementation must be supplied.
//
static     XrdPoll   *newPoller(int pollid, int numfd)    /* = 0 */;

// The following is common to all implementations
//
XrdOucMutex   PollMutex;

XrdOucMutex   PollPipe;
struct pollfd PipePoll;
int           CmdFD;      // FD to send PipeData commands
int           ReqFD;      // FD to recv PipeData requests
struct        PipeData {enum cmd {EnFD, DiFD, RmFD};
                        cmd req;
                        int fd;
                        int ent;
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

static     XrdOucMutex  doingAttach;
           int          numAttached;    // Number of fd's attached to poller
};
#endif
