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

// Initialize our dynamic data
//
   myFD    = fd;
   memcpy(&myDest, name, namelen);
   myDestL = namelen;
   myRetc  = ETIMEDOUT;
   myTID   = 0;

// Make sure we have not exceeded the maximum number of connect threads
//
   tLimit.Wait();

// Start a thread to do the connection
//
   XrdOucThread_Run(&myTID, XrdNetConnectXeq, (void *)this);
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

// If a timeout is wanted, then do the complicated thing
//
   if (tsec)
      {XrdNetConnect myConnect(fd, name, namelen);
       if (myConnect.cDone.Wait(tsec)) return ETIMEDOUT;
       return myConnect.myRetc;
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
   tLimit.Post();
}
