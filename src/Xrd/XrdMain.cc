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
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdNetwork.hh"
#include "Xrd/XrdScheduler.hh"
#define  TRACELINK newlink
#include "Xrd/XrdTrace.hh"

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucPthread.hh"
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

       XrdConfig          XrdConfig;

       XrdNetwork        *XrdNetTCP = 0;
       XrdNetwork        *XrdNetADM = 0;

       XrdScheduler       XrdScheduler;

       XrdOucLogger         XrdLogger;

       XrdOucError          XrdLog(&XrdLogger, "Xrd");

       XrdOucTrace          XrdTrace(&XrdLog);

/******************************************************************************/
/*                             m a i n A d m i n                              */
/******************************************************************************/
  
extern "C"
{
void *mainAdmin(void *parg)
{
   XrdLink *newlink;
// static XrdProtocol_Admin  ProtAdmin;
   long ProtAdmin;
   char *TraceID = (char *)"Admin";

// At this point we should be able to accept new connections
//
   while(1) if (newlink = XrdNetADM->Accept())
               {TRACEI(CONN, "admin connection accepted");
                newlink->setProtocol((XrdProtocol *)&ProtAdmin);
                XrdScheduler.Schedule((XrdJob *)newlink);
               }
   return (void *)0;
}
}

/******************************************************************************/
/*                              m a i n U s e r                               */
/******************************************************************************/
  
extern "C"
{
void *mainUser(void *parg)
{
   XrdLink *newlink;
   static XrdProtocol_Select ProtSelect;
   char *TraceID = (char *)"User";


// At this point we should be able to accept new connections
//
   while(1) if (newlink = XrdNetTCP->Accept())
               {TRACEI(CONN, "connection accepted");
                newlink->setProtocol((XrdProtocol *)&ProtSelect);
                XrdScheduler.Schedule((XrdJob *)newlink);
               }
   return (void *)0;
}
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
main(int argc, char *argv[])
{
   char *TraceID = (char *)"Main";

// Turn off sigpipe before we start any threads
//
   sigignore(SIGPIPE);
   sigignore(SIGUSR2);
   sighold(SIGCHLD);

// Process configuration file
//
   if (XrdConfig.Configure(argc, argv)) _exit(1);

// Start the admin thread if an admin network is defined
//
   if (XrdNetADM)
      {pthread_t tid;
       int retc;
        if (retc = XrdOucThread_Sys(&tid,mainAdmin,(void *)0))
           {XrdLog.Emsg("main", retc, "creating admin thread"); _exit(3);}
        TRACE(DEBUG, "thread " << tid <<" assigned to admin handler");
      }

// Start the user thread (a foible w.r.t. Solaris unbound threads)
//
      {pthread_t tid;
       int retc;
        if (retc = XrdOucThread_Sys(&tid,mainUser,(void *)0))
           {XrdLog.Emsg("main", retc, "creating user thread"); _exit(3);}
        TRACE(DEBUG, "thread " << tid <<" assigned to user handler");
      }

// All done with the initial thread
//
   pthread_exit(0);
}
