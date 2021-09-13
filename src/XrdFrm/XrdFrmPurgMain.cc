/******************************************************************************/
/*                                                                            */
/*                     X r d F r m P u r g M a i n . c c                      */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This is the "main" part of the frm_purge command. Syntax is:
*/
static const char *XrdFrmOpts  = ":bc:dfhk:l:n:O:s:S:Tvz";
static const char *XrdFrmUsage =

  " [-b] [-c <cfgfile>] [-d] [-f] [-k {num|sz{k|m|g}|sig] [-l [=]<fn>] [-n name]"
  " [-O free[,hold]] [-s pidfile] [-S site] [-T] [-v] [-z] [<spaces>] [<paths>]\n";
/*
Where:

   -b     Run as a true daemon process in the background.

   -c     The configuration file. The default is '/opt/xrootd/etc/xrootd.cf'

   -d     Turns on debugging mode.

   -f     Fix orphaned files (i.e., lock and pin) by removing them.

   -k     Keeps num log files or no more that sz log files.

   -l     Specifies location of the log file. This may also come from the
          XrdOucLOGFILE environmental variable.
          By default, error messages go to standard error.

   -n     The instance name.

   -O     Run this one time only as a command. The parms are:
          {free% | sz{k|m|g}[,hold]

   -s     The pidfile name.

   -S     The site name.

   -T     Runs in test mode (no actual purge will occur).

   -v     Verbose mode, typically prints each file purged and other details.

   o-t-a  The one-time-args run this as a command only once. The args direct
          the purging process. These may only be specified when -O specified.

          Syntax is: [space] path | space [path]
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
#include "XrdFrm/XrdFrmPurge.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysUtils.hh"

using namespace XrdFrc;
using namespace XrdFrm;
  
/******************************************************************************/
/*                      G l o b a l   V a r i a b l e s                       */
/******************************************************************************/

       XrdFrmConfig       XrdFrm::Config(XrdFrmConfig::ssPurg,
                                         XrdFrmOpts, XrdFrmUsage);

// The following is needed to resolve symbols for objects included from xrootd
//
       XrdOucTrace       *XrdXrootdTrace;
       XrdSysError        XrdLog(0, "");
       XrdOucTrace        XrdTrace(&Say);

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/
  
void *mainServer(void *parg)
{
//  int udpFD = *static_cast<int *>(parg);
//  XrdFrmPurge::Server(udpFD);
    return (void *)0;
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   XrdSysLogger Logger;
   extern int mainConfig();

// Turn off sigpipe and host a variety of others before we start any threads
//
   XrdSysUtils::SigBlock();

// Set the default stack size here
//
   if (sizeof(long) > 4) XrdSysThread::setStackSize((size_t)1048576);
      else               XrdSysThread::setStackSize((size_t)786432);

// Perform configuration
//
   Say.logger(&Logger);
   XrdLog.logger(&Logger);
   if (!Config.Configure(argc, argv, &mainConfig)) exit(4);

// Fill out the dummy symbol to avoid crashes
//
   XrdXrootdTrace = new XrdOucTrace(&Say);

// Display configuration (deferred because mum might have been in effect)
//
   if (!Config.isOTO || Config.Verbose) XrdFrmPurge::Display();

// Now simply poke the server every so often
//
   if (Config.isOTO) XrdFrmPurge::Purge();
      else do {if (Config.StopPurge)
                  {int n = 0;
                   struct stat buf;
                   while(!stat(Config.StopPurge, &buf))
                        {if (!n--)
                            {Say.Emsg("PurgMain", Config.StopPurge,
                                      "exists; purging suspended."); n = 12;}
                         XrdSysTimer::Snooze(5);
                        }
                  }
               XrdFrmPurge::Purge();
               XrdSysTimer::Snooze(Config.WaitPurge);
              } while(1);

// All done
//
   exit(0);
}

/******************************************************************************/
/*                            m a i n C o n f i g                             */
/******************************************************************************/
  
int mainConfig()
{
   XrdFrmConfig::Policy *pP = Config.dfltPolicy.Next;
   XrdFrmConfig::VPInfo *vP = Config.VPList;
   XrdNetSocket *udpSock;
   pthread_t tid;
   int retc, udpFD;

// If test is in effect, remove the fix flag
//
   if (Config.Test) Config.Fix = 0;

// Go through the policy list and add each policy
//
   while((pP = Config.dfltPolicy.Next))
        {if (!XrdFrmPurge::Policy(pP->Sname))
            XrdFrmPurge::Policy(pP->Sname, pP->minFree, pP->maxFree,
                                pP->Hold,  pP->Ext);
         Config.dfltPolicy.Next = pP->Next;
         delete pP;
        }

// Make sure we have a public policy
//
   if (!XrdFrmPurge::Policy("public"))
       XrdFrmPurge::Policy("public", Config.dfltPolicy.minFree,
                               Config.dfltPolicy.maxFree,
                               Config.dfltPolicy.Hold,
                               Config.dfltPolicy.Ext);

// Now add any missing policies (we need one for every space)
//
   while(vP)
      {if (!XrdFrmPurge::Policy(vP->Name))
          XrdFrmPurge::Policy(vP->Name, Config.dfltPolicy.minFree,
                                  Config.dfltPolicy.maxFree,
                                  Config.dfltPolicy.Hold,
                                  Config.dfltPolicy.Ext);
       vP = vP->Next;
      }

// Enable the appropriate spaces and over-ride config value
//
   if (!XrdFrmPurge::Init(Config.spacList, Config.cmdFree, Config.cmdHold))
      return 1;

// We are done if this is a one-time-only call
//
   if (Config.isOTO) return 0;

// Get a UDP socket for the server
//
   if (!(udpSock = XrdNetSocket::Create(&Say, Config.AdminPath,
                   "purg.udp", Config.AdminMode, XRDNET_UDPSOCKET))) return 1;
      else {udpFD = udpSock->Detach(); delete udpSock;}

// Start the Server thread
//
   if ((retc = XrdSysThread::Run(&tid, mainServer, (void *)&udpFD,
                                  XRDSYSTHREAD_BIND, "Server")))
      {Say.Emsg("main", retc, "create server thread"); return 1;}

// All done
//
   return 0;
}
