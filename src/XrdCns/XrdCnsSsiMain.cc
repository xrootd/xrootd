/******************************************************************************/
/*                                                                            */
/*                      X r d C n s S s i M a i n . c c                       */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This is the Cluster Name Space utility. The syntax is:

   cns_ssi {diff | list | updt} [options] <path>

   options: [-f] [-h] [-m] [-n] [-p] [-s] [-S] [-v] [-l <lfile>]

Where:

   <path> The archive (i.e., backup) path to use. All sub-directories in
          <path> of the form "<path>/cns/<host>" are considered.

   -l     list: Equivalent to specifying '-h -m -n -p -s'.

   -m     list: Displays the file mode.

   -n     list: Displays the space name.

   -p     list: Displays the physical location.

   -s     list: Displays the file size.

   -S     list: Same as -s but displays size in k, m, g, or t.

   -l     updt: Specifies location of the log file. This may also come from the
          XRDLOGDIR environmental variable. By default, messages go to stderr.

   -v     updt: Increases the verbosity of messages.
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

#include "XrdCns/XrdCnsSsi.hh"
#include "XrdCns/XrdCnsSsiCfg.hh"
#include "XrdCns/XrdCnsSsiSay.hh"

#include "XrdOuc/XrdOucTList.hh"

#include "XrdSys/XrdSysError.hh"
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
extern XrdCnsSsiCfg       Config;

       XrdSysError        MLog(0,"Cns_");

       XrdCnsSsiSay       Say(&MLog);
}

using namespace XrdCns;

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   XrdSysLogger MLogger;
   XrdOucTList *tP;
   const char *xrdLogD = 0;
   char *hP;
   int rc = 0;

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
   Say.setV(Config.Verbose);

// Construct the logfile path and bind it (command line only)
//
   if (Config.logFN || (xrdLogD = getenv("XRDLOGDIR")))
      {char buff[2048];
       if (Config.logFN) strcpy(buff, Config.logFN);
          else {strcpy(buff, xrdLogD); strcat(buff, "cnsssilog");}
       MLogger.AddMsg(XrdBANNER);
       MLogger.Bind(buff, 0);
      }

// Process the request
//
   while((tP = Config.dirList))
        {hP = tP->text + tP->val;
         if (Config.Xeq == 'l') rc |= XrdCnsSsi::List(hP, tP->text);
            else {int i = XrdCnsSsi::Updt(hP, tP->text);
                  if (i) Say.M("Unable to update ", hP, " inventory.");
                  rc |= i;
                 }
         Config.dirList = tP->next;
         delete tP;
        }

// All done
//
   exit(rc);
}
