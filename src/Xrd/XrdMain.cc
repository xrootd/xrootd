/**************************************************************************************/
/*                                                                            */
/*                            X r d M a i n . c c                             */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* This is the XRootd server. The syntax is:

   xrootd [options]

   options: [-b] [-c <fname>] [-d] [-h] [-l <fname>] [-p <port>] [<oth>]

Where:
   -b     forces background execution.

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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>

#include "Xrd/XrdConfig.hh"
#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdProtLoad.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysUtils.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdMain : XrdJob
{
public:

XrdSysSemaphore  *theSem;
XrdProtocol      *theProt;
XrdInet          *theNet;
int               thePort;
static XrdConfig  Config;

void              DoIt() {XrdLink *newlink;
                          if ((newlink = theNet->Accept(0, -1, theSem)))
                             {newlink->setProtocol(theProt);
                              newlink->DoIt();
                             }
                         }

           XrdMain() : XrdJob("main accept"), theSem(0), theProt(0),
                                              theNet(0), thePort(0) {}
           XrdMain(XrdInet *nP) : XrdJob("main accept"), theSem(0),
                                  theProt(0), theNet(nP), thePort(nP->Port()) {}
          ~XrdMain() {}
};

XrdConfig XrdMain::Config;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *mainAccept(void *parg)
{  XrdMain        *Parms   = (XrdMain *)parg;
   XrdScheduler   *mySched =  Parms->Config.ProtInfo.Sched;
   XrdProtLoad     ProtSelect(Parms->thePort);
   XrdSysSemaphore accepted(0);

// Complete the parms
//
   Parms->theSem  = &accepted;
   Parms->theProt = (XrdProtocol *)&ProtSelect;

// Simply schedule new accepts
//
   while(1) {mySched->Schedule((XrdJob *)Parms);
             accepted.Wait();
            }

   return (void *)0;
}

/******************************************************************************/
/*                             m a i n A d m i n                              */
/******************************************************************************/
  
void *mainAdmin(void *parg)
{  XrdMain      *Parms   = (XrdMain *)parg;
   XrdInet      *NetADM  =  Parms->theNet;
   XrdLink      *newlink;
// static XrdProtocol_Admin  ProtAdmin;
   int           ProtAdmin;

// At this point we should be able to accept new connections. Noe that we don't
// support admin connections as of yet so the following code is superflous.
//
   while(1) if ((newlink = NetADM->Accept()))
               {newlink->setProtocol((XrdProtocol *)&ProtAdmin);
                Parms->Config.ProtInfo.Sched->Schedule((XrdJob *)newlink);
               }
   return (void *)0;
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   XrdMain   Main;
   pthread_t tid;
   char      buff[128];
   int       i, retc;

// Turn off sigpipe and host a variety of others before we start any threads
//
   XrdSysUtils::SigBlock();

// Set the default stack size here
//
   if (sizeof(long) > 4) XrdSysThread::setStackSize((size_t)1048576);
      else               XrdSysThread::setStackSize((size_t)786432);

// Process configuration file
//
   if (Main.Config.Configure(argc, argv)) _exit(1);

// Start the admin thread if an admin network is defined
//
   if (Main.Config.NetADM && (retc = XrdSysThread::Run(&tid, mainAdmin,
                             (void *)new XrdMain(Main.Config.NetADM),
                             XRDSYSTHREAD_BIND, "Admin handler")))
      {Main.Config.ProtInfo.eDest->Emsg("main", retc, "create admin thread");
       _exit(3);
      }

// At this point we should be able to accept new connections. Spawn a
// thread for each network except the first. The main thread will handle
// that network as some implementations require a main active thread.
//
   for (i = 1; i <= XrdProtLoad::ProtoMax; i++)
       if (Main.Config.NetTCP[i])
          {XrdMain *Parms = new XrdMain(Main.Config.NetTCP[i]);
           sprintf(buff, "Port %d handler", Parms->thePort);
           if (Parms->theNet == Main.Config.NetTCP[XrdProtLoad::ProtoMax])
               Parms->thePort = -(Parms->thePort);
           if ((retc = XrdSysThread::Run(&tid, mainAccept, (void *)Parms,
                                         XRDSYSTHREAD_BIND, strdup(buff))))
              {Main.Config.ProtInfo.eDest->Emsg("main", retc, "create", buff);
               _exit(3);
              }
          }

// Finally, start accepting connections on the main port
//
   Main.theNet  = Main.Config.NetTCP[0];
   Main.thePort = Main.Config.NetTCP[0]->Port();
   mainAccept((void *)&Main);

// We should never get here
//
   pthread_exit(0);
}
