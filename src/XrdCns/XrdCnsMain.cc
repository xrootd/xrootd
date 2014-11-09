/******************************************************************************/
/*                                                                            */
/*                         X r d G n s M a i n . c c                          */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This is the Cluster Name Space interface. The syntax is:

   XrdCnsd [options] [[xroot://]<host[:port]>[/[/prefix]] . . .]

   options: [-a <apath>] [-b <bpath>] [-B <bpath>] [-c] [-d] [-e <epath>]

            [-i <tspec>] [-I <tspec>] [-l <lfile>] [-p <port>] [-q <lim>] [-R]
Where:
   -a     The admin path where the event log is placed and where named
          sockets are created. If not specified, the admin path comes from
          the XRDADMINPATH env variable. Otherwise, /tmp is used. This option
          is valid only for command line use.

   -b     The archive (i.e., backup) path to use. If not specified, no backup
          is done. Data is  written to "<bpath>/cns/<host>". By default, the
          backups are written to each redirector. By prefixing <bpath> with
          <host[:port]:> then backups are written to the specified host:port.
          If <port> is omitted the the specified or default -p value is used.
          Note that this backup can be used to create an inventory file.

   -B     Same as -b *except* that only the inventory is maintained (i.e., no
          composite name space is created).

   -c     Specified the config file name. By defaults this comes from the envar
          XRDCONFIGFN set by the underlying xrootd. Note that if -R is specified
          then -c must be specified as there is no underlying xrootd.

   -d     Turns on debugging mode. Valid only via command line.

   -D     Sets the client library debug value. Specify an number -2 to 3.

   -e     The directory where the event logs are to be written. By default
          this is whatever <apath> becomes.

   -i     The interval between forced log archiving. Default is 20m (minutes).

   -I     The time interval, in seconds, between checks for the inventory file.

   -l     Specifies location of the log file. This may also come from the
          XRDLOGDIR environmental variable. Valid only via command line.
          By default, error messages go to standard error.

   -L     The local root (ignored except when -R specified).

   -N     The name2name library and parms (ignored except when -R specified).

   -p     The default port number to use for the xrootd that can be used to
          create/maintain the name space as well as hold archived logs. The
          number 1095 is used bt default.

   -q     Maximum number of log records before the log is closed and archived.
          Specify 1 to 1024. The default if 512.

   -R     Run is stand-alone mode and recreate the name space and, perhaps,
          the inventory file.

<host>    Is the hostname of the server managing the cluster name space. You
          may specify more than one if they are replicated. The default is to
          use the hosts specified via the "all.manager" directive.
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

#include "XrdVersion.hh"

#include "Xrd/XrdTrace.hh"

#include "XrdCns/XrdCnsConfig.hh"
#include "XrdCns/XrdCnsDaemon.hh"

#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysUtils.hh"
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

#define XrdBANNER "Copr.  2004-2013 Stanford University, cns version " XrdVSTRING

namespace XrdCns
{
extern XrdCnsConfig       Config;

extern XrdCnsDaemon       XrdCnsd;

       XrdSysError        MLog(0,"Cns_");

       XrdOucTrace        XrdTrace(&MLog);
}

using namespace XrdCns;

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   XrdSysLogger MLogger;
   XrdOucStream stdinEvents;    // STDIN fed events
   char *xrdLogD = 0;

// Establish message routing
//
   MLog.logger(&MLogger);

// Turn off sigpipe and host a variety of others before we start any threads
//
   XrdSysUtils::SigBlock();

// Set the default stack size here
//
   if (sizeof(long) > 4) XrdSysThread::setStackSize((size_t)1048576);
      else               XrdSysThread::setStackSize((size_t)786432);

// Process the options and arguments
//
   if (!Config.Configure(argc, argv)) exit(1);

// Construct the logfile path and bind it
//
   if (Config.logfn || (xrdLogD = getenv("XRDLOGDIR")))
      {char buff[2048];
       if (Config.logfn) strcpy(buff, Config.logfn);
          else {strcpy(buff, xrdLogD); strcat(buff, "cnsdlog");}
       MLogger.AddMsg(XrdBANNER);
       MLogger.AddMsg("XrdCnsd - Cluster Name Space Daemon");
       MLogger.Bind(buff, Config.bindArg);
      }

// Complete configuration. We do it this way so that we can easily run this
// either as a plug-in or as a command.
//
   if (!Config.Configure()) _exit(1);

// At this point we should be able to accept new requests
//
   stdinEvents.Attach(STDIN_FILENO, 32*1024);
   XrdCnsd.getEvents(stdinEvents, "xrootd");

// We should never get here
//
   _exit(8);
}
