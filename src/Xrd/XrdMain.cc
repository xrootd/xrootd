/******************************************************************************/
/*                                                                            */
/*                            X r d M a i n . c c                             */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//           $Id$

const char *XrdMainCVSID = "$Id$";

/* This is the XRootd server. The syntax is:

   xrootd [options]

   options: [-c <fname>] [-d] [-h] [-l <fname>] [-p <port>] [<oth>]

Where:
   -c     specifies the configuration file. This may also come from the
          XrdCONFIGFN environmental variable.

   -d     Turns on debugging mode (equivalent to xrd.trace all)

   -h     Displays usage line and exits.

   -l     Specifies location of the log file. This may also come from the
          XrdOucLOGFILE environmental variable or from the oofs layer. By
          By default, error messages go to standard error.

   -p     Is the port to use either as a service name or an actual port number.
          The default port is 1094.

   <oth>  Are other protocol specific options.

*/

/******************************************************************************/
/*                         i n c l u d e   f i l e s                          */
/******************************************************************************/
  
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <iostream.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdConfig.hh"
#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdScheduler.hh"
#define  TRACELINK newlink
#include "Xrd/XrdTrace.hh"

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucPthread.hh"
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

       XrdConfig          XrdConf;

       XrdInet           *XrdNetTCP = 0;
       XrdInet           *XrdNetADM = 0;

       XrdScheduler       XrdSched;

       XrdOucLogger       XrdLogger;

       XrdOucError        XrdLog(&XrdLogger, "Xrd");

       XrdOucThread      *XrdThread;

       XrdOucTrace        XrdTrace(&XrdLog);

/******************************************************************************/
/*                             m a i n A d m i n                              */
/******************************************************************************/
  
void *mainAdmin(void *parg)
{
   XrdLink *newlink;
// static XrdProtocol_Admin  ProtAdmin;
   long ProtAdmin;

// At this point we should be able to accept new connections
//
   while(1) if ((newlink = XrdNetADM->Accept()))
               {newlink->setProtocol((XrdProtocol *)&ProtAdmin);
                XrdSched.Schedule((XrdJob *)newlink);
               }
   return (void *)0;
}

/******************************************************************************/
/*                              m a i n U s e r                               */
/******************************************************************************/
  
void *mainUser(void *parg)
{
   XrdLink *newlink;
   static XrdProtocol_Select ProtSelect;

// At this point we should be able to accept new connections
//
   while(1) if ((newlink = XrdNetTCP->Accept()))
               {newlink->setProtocol((XrdProtocol *)&ProtSelect);
                XrdSched.Schedule((XrdJob *)newlink);
               }
   return (void *)0;
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   sigset_t myset;

// Turn off sigpipe and host a variety of others before we start any threads
//
   signal(SIGPIPE, SIG_IGN);
   sigemptyset(&myset);
   sigaddset(&myset, SIGUSR1);
   sigaddset(&myset, SIGUSR2);
   sigaddset(&myset, SIGCHLD);
#ifndef __macos__
   sigaddset(&myset, SIGRTMIN);
   sigaddset(&myset, SIGRTMIN+1);
#endif
   pthread_sigmask(SIG_BLOCK, &myset, NULL);

// Process configuration file
//
   if (XrdConf.Configure(argc, argv)) _exit(1);

// Start the admin thread if an admin network is defined
//
   if (XrdNetADM)
      {pthread_t tid;
       int retc;
       if ((retc = XrdOucThread::Run(&tid, mainAdmin, (void *)0,
                                     XRDOUCTHREAD_BIND, "Admin handler")))
          {XrdLog.Emsg("main", retc, "create admin thread"); _exit(3);}
      }

// Start the user thread (a foible w.r.t. Solaris unbound threads)
//
      {pthread_t tid;
       int retc;
       if ((retc = XrdOucThread::Run(&tid, mainUser,(void *)0,
                                     XRDOUCTHREAD_BIND, "User handler")))
          {XrdLog.Emsg("main", retc, "create user thread"); _exit(3);}
      }

// All done with the initial thread. However, in some versions of Linux, we
// will be put into "stealth" mode if the main thread exits. In some versions
// of Solaris, we will get signal handling anomolies unless the main thread
// exits. So, we do the platform dependent thing here.
//
#ifdef __linux__
   while(1) {sleep(1440*999);}
#endif
   pthread_exit(0);
}
