/******************************************************************************/
/*                                                                            */
/*                      X r d N e t C o n n e c t . c c                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdNetConnectCVSID = "$Id$";

#include "errno.h"
#include "poll.h"
#include "unistd.h"
#include <string.h>

#include "XrdNet/XrdNetConnect.hh"
#include "XrdOuc/XrdOucTimer.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

XrdOucSemaphore XrdNetConnect::tLimit(8);
  
extern "C"
{
void *XrdNetConnectXeq(void *carg) 
                      {XrdNetConnect *np = (XrdNetConnect *)carg;
                       np->ConnectXeq(); 
                       return (void *)0;
                      }
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetConnect::XrdNetConnect(int fd, const sockaddr *name, int namelen)
{

// Copy the arguments because they will go away when the creator exits
// prior to the connect() completing.
//
   myFD    = fd;
   memcpy(&myDest, name, namelen);
   myDestL = namelen;
   myRetc  = ETIMEDOUT;
   myTID   = 0;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdNetConnect::~XrdNetConnect()
{
// We would normally kill the connect thread. However, thread specific signals
// don't work on all platforms. So, we let the thread hang about. Normally,
// that won't be too many (we impose a limit, amyway).
//
// if (myTID) XrdOucThread_Kill(myTID);

// Should we kill the thread, make sure we uncomment the post here and
// get rid of the post in the ConnectXeq() method.
//
// tLimit.Post();
}
 
/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/
  
int XrdNetConnect::Connect(             int       fd,
                           const struct sockaddr *name, 
                                        int       namelen, 
                                        int       tsec)
{

// If a timeout is wanted, we create a connect object and lock it against
// deletion. We call doConnect() that spawns a thread that actually executes
// connect() and we wait on a condition variable for the timeout period. If the 
// connect occurs before the timeout, the thread will post the condition var
// and we will use the rc from the thread. Otherwise, we claim a time out.
// We then unlock the object which allows the thread to eventually delete the
// object. This is really the only way to keep from thrashing the call stack.
//
   if (tsec)
      {XrdNetConnect *myConnect = new XrdNetConnect(fd, name, namelen);
       int theRC;
       myConnect->cActv.Lock();
       myConnect->doConnect();
       if (myConnect->cDone.Wait(tsec)) theRC = ETIMEDOUT;
          else theRC = myConnect->myRetc;
       myConnect->cActv.UnLock();
       return theRC;
      }

// Otherwise, simply connect and wait the default 3 minutes (usually)
//
   if (connect(fd, name, namelen)) return errno;
   return 0;
}
 
/******************************************************************************/
/*                            C o n n e c t X e q                             */
/******************************************************************************/
  
void XrdNetConnect::ConnectXeq()
{
// Perform the connect
//
   if (connect(myFD, &myDest, myDestL)) myRetc = errno;
      else myRetc = 0;

// Indicate that this thread is complete
//
   cDone.Signal();
   tLimit.Post();

// We must now briefly lock this object to allow the initial thread to
// retreive the return code before the object is deleted. This works
// because the calling thread locked the object prior to starting this
// thread and will unlock the object only after it is done with it.
//
   cActv.Lock();
   cActv.UnLock();
   delete this;
}
 
/******************************************************************************/
/*                             d o C o n n e c t                              */
/******************************************************************************/
  
void XrdNetConnect::doConnect()
{
// Make sure we have not exceeded the maximum number of connect threads
//
   tLimit.Wait();

// Start a thread to do the connection
//
   XrdOucThread_Run(&myTID, XrdNetConnectXeq, (void *)this);
}
