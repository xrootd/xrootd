/******************************************************************************/
/*                                                                            */
/*                      X r d F r m X f r M a i n . c c                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//           $Id$

const char *XrdFrmXfrMainCVSID = "$Id$";

/* This is the "main" part of the frm_migr, frm_pstg & frm_xfrd commands.
*/

/* This is the "main" part of the frm_migrd command. Syntax is:
*/
static const char *XrdFrmOpts  = ":c:dfhk:l:n:Tv";
static const char *XrdFrmUsage =

  " [-c <cfgfn>] [-d] [-f] [-k {num | sz{k|m|g}] [-l <lfile>] [-n name] [-T] [-v]\n";
/*
Where:

   -c     The configuration file. The default is '/opt/xrootd/etc/xrootd.cf'

   -d     Turns on debugging mode.

   -f     Fix orphaned files (i.e., lock and pin) by removing them.

   -k     Keeps num log files or no more that sz log files.

   -l     Specifies location of the log file. This may also come from the
          XrdOucLOGFILE environmental variable.
          By default, error messages go to standard error.

   -n     The instance name.

   -T     Runs in test mode (no actual migration will occur).

   -v     Verbose mode, typically prints each file details.
*/

/******************************************************************************/
/*                         i n c l u d e   f i l e s                          */
/******************************************************************************/
  
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>

#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmMigrate.hh"
#include "XrdFrm/XrdFrmReqAgent.hh"
#include "XrdFrm/XrdFrmReqBoss.hh"
#include "XrdFrm/XrdFrmReqFile.hh"
#include "XrdFrm/XrdFrmTrace.hh"
#include "XrdFrm/XrdFrmTransfer.hh"
#include "XrdFrm/XrdFrmXfrQueue.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdFrm;
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

       XrdSysLogger       XrdFrm::Logger;

       XrdSysError        XrdFrm::Say(&Logger, "");

       XrdOucTrace        XrdFrm::Trace(&Say);

       XrdFrmConfig       XrdFrm::Config(XrdFrmConfig::ssXfr,
                                         XrdFrmOpts, XrdFrmUsage);

       XrdFrmReqBoss      XrdFrm::GetFiler("getf", XrdFrmXfrQueue::getQ);

       XrdFrmReqBoss      XrdFrm::Migrated("migr", XrdFrmXfrQueue::migQ);

       XrdFrmReqBoss      XrdFrm::PreStage("pstg", XrdFrmXfrQueue::stgQ);

       XrdFrmReqBoss      XrdFrm::PutFiler("putf", XrdFrmXfrQueue::putQ);

// The following is needed to resolve symbols for objects included from xrootd
//
       XrdOucTrace       *XrdXrootdTrace;
       XrdSysError        XrdLog(&Logger, "");
       XrdOucTrace        XrdTrace(&Say);

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   extern int mainConfig();
   sigset_t myset;
   char *pP;

// Turn off sigpipe and host a variety of others before we start any threads
//
   signal(SIGPIPE, SIG_IGN);  // Solaris optimization
   sigemptyset(&myset);
   sigaddset(&myset, SIGPIPE);
   sigaddset(&myset, SIGCHLD);
   pthread_sigmask(SIG_BLOCK, &myset, NULL);

// Set the default stack size here
//
   if (sizeof(long) > 4) XrdSysThread::setStackSize((size_t)1048576);
      else               XrdSysThread::setStackSize((size_t)786432);

// If we are named frm_pstg then we are runnng in agent-mode
//
    if (!(pP = rindex(argv[0], '/'))) pP = argv[0];
       else pP++;
   if (strcmp("frm_xfrd", pP)) Config.isAgent = 1;

// Perform configuration
//
   if (!Config.Configure(argc, argv, &mainConfig)) exit(4);

// Fill out the dummy symbol to avoid crashes
//
   XrdXrootdTrace = new XrdOucTrace(&Say);

// If we are running in agent mode scadadle to that (it's simple). Otherwise,
// start the thread that fields udp-based requests.
//
   if (Config.isAgent) exit(XrdFrmReqAgent::Start());
   XrdFrmReqAgent::Pong();

// Now simply poke the each server every so often
//
   while(1)
        {PreStage.Wakeup(); GetFiler.Wakeup();
         Migrated.Wakeup(); PutFiler.Wakeup();
         XrdSysTimer::Snooze(Config.WaitQChk);
        }

// We get here is we failed to initialize
//
   exit(255);
}

/******************************************************************************/
/*                            m a i n C o n f i g                             */
/******************************************************************************/
  
int mainConfig()
{
   char buff[1024];

// If we are a true server then first start the transfer agents and migrator
// Note that if we are not an agent then only one instance may run at a time.
//
   if (!Config.isAgent)
      {sprintf(buff, "%sfrm_xfrd.lock", Config.AdminPath);
       if (!XrdFrmReqFile::Unique(buff) || !XrdFrmTransfer::Init()) return 1;
       if (Config.WaitMigr < Config.IdleHold) Config.WaitMigr = Config.IdleHold;
       if (Config.pathList)
          {if (!Config.xfrOUT)
              Say.Emsg("Config","Output copy command not specified; "
                                "auto-migration disabled!");
              else XrdFrmMigrate::Migrate();
          } else Say.Emsg("Config","No migratable paths; "
                                   "auto-migration disabled!");
      }

// Start the external interfaces
//
   if (!PreStage.Start() || !Migrated.Start()
   ||  !GetFiler.Start() || !PutFiler.Start()) return 1;

// All done
//
   return 0;
}
