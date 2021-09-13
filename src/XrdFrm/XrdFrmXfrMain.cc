/******************************************************************************/
/*                                                                            */
/*                      X r d F r m X f r M a i n . c c                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

/* This is the "main" part of the frm_xfragent & frm_xfrd commands.
*/

/* This is the "main" part of the frm_migrd command. Syntax is:
*/
static const char *XrdFrmOpts  = ":bc:dfhk:l:n:s:S:Tvz";
static const char *XrdFrmUsage =

  " [-b] [-c <cfgfn>] [-d] [-f] [-k {num|sz{k|m|g}|sig] [-l [=]<fn>] [-n name]\n"
  " [-s pidfile] [-S site] [-T] [-v] [-z]\n";
/*
Where:

   -b     Run as a true daemon in the bacground (only for xfrd).

   -c     The configuration file. The default is '/opt/xrootd/etc/xrootd.cf'

   -d     Turns on debugging mode.

   -f     Fix orphaned files (i.e., lock and pin) by removing them.

   -k     Keeps num log files or no more that sz log files.

   -l     Specifies location of the log file. This may also come from the
          XrdOucLOGFILE environmental variable.
          By default, error messages go to standard error.

   -n     The instance name.

   -s     The pidfile name.

   -S     The site name.

   -T     Runs in test mode (no actual migration will occur).

   -v     Verbose mode, typically prints each file details.
*/

/******************************************************************************/
/*                         i n c l u d e   f i l e s                          */
/******************************************************************************/
  
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <sys/param.h>

#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmXfrAgent.hh"
#include "XrdFrm/XrdFrmXfrDaemon.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysUtils.hh"

using namespace XrdFrc;
using namespace XrdFrm;
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

       XrdFrmConfig       XrdFrm::Config(XrdFrmConfig::ssXfr,
                                         XrdFrmOpts, XrdFrmUsage);

// The following is needed to resolve symbols for objects included from xrootd
//
       XrdOucTrace       *XrdXrootdTrace;
       XrdSysError        XrdLog(0, "");
       XrdOucTrace        XrdTrace(&Say);

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   XrdSysLogger Logger;
   extern int mainConfig();
   char *pP;

// Turn off sigpipe and host a variety of others before we start any threads
//
   XrdSysUtils::SigBlock();

// Set the default stack size here
//
   if (sizeof(long) > 4) XrdSysThread::setStackSize((size_t)1048576);
      else               XrdSysThread::setStackSize((size_t)786432);

// If we are named frm_pstg then we are runnng in agent-mode
//
    if (!(pP = rindex(argv[0], '/'))) pP = argv[0];
       else pP++;
   if (strncmp("frm_xfrd", pP, 8)) Config.isAgent = 1;


// Perform configuration
//
   Say.logger(&Logger);
   XrdLog.logger(&Logger);
   if (!Config.Configure(argc, argv, &mainConfig)) exit(4);

// Fill out the dummy symbol to avoid crashes
//
   XrdXrootdTrace = new XrdOucTrace(&Say);

// All done, simply exit based on our persona
//
   exit(Config.isAgent ? XrdFrmXfrAgent::Start() : XrdFrmXfrDaemon::Start());
}

/******************************************************************************/
/*                            m a i n C o n f i g                             */
/******************************************************************************/
  
int mainConfig()
{
// Initialize the daemon, depending on who we are to be
//
   return (Config.isAgent ? 0 : !XrdFrmXfrDaemon::Init());
}
