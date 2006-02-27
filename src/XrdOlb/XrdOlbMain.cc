/******************************************************************************/
/*                                                                            */
/*                         X r d O l b M a i n . c c                          */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbMainCVSID = "$Id$";
  
/* This is the distributed cache server. It can work in manager mode (-m option)
   or in sserver mode (-s option).

   olbd [options] [configfn]

   options: [-d] [-l <fname>] [-L <sec>] [-m] [-s] [-w]

Where:
   -d     Turns on debugging mode.

   -l     Specifies location of the log file. By default, error messages
          go to standard out.

   -L     How many minutes between log file closes.

   -m     function in manager scheduling moede.

   -s     Executes in server mode.

   -w     Wait for a data-point connection before connecting to the manager.

Notes:
   1.     The name of config file must either be specified on the command
          line or via the environmental variable XrdOlbCONFIGFN.
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

#include "XrdOlb/XrdOlbAdmin.hh"
#include "XrdOlb/XrdOlbCache.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbScheduler.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbPrepare.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdNet/XrdNetLink.hh"
#include "XrdNet/XrdNetWork.hh"
  
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

       int              XrdOlbSTDERR;

       XrdOlbCache      XrdOlbCache;

       XrdOlbConfig     XrdOlbConfig;

       XrdOlbPrepare    XrdOlbPrepQ;

       XrdOlbScheduler  XrdOlbSched;

       XrdOlbManager    XrdOlbSM;

       XrdNetWork      *XrdOlbNetTCPm = 0;
       XrdNetWork      *XrdOlbNetTCPr = 0;
       XrdNetWork      *XrdOlbNetTCPs = 0;

       XrdNetLink      *XrdOlbRelay = 0;

       XrdOucLogger     XrdOlbLog;

       XrdOucError      XrdOlbSay(&XrdOlbLog, "olb_");

       XrdOucTrace      XrdOlbTrace(&XrdOlbSay);

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOlbLoginServer(void *carg)
      {XrdNetLink *lp = (XrdNetLink *)carg;
       return XrdOlbSM.Login(lp);
      }

void *XrdOlbStartAdmin(void *carg)
      {XrdOlbAdmin Admin;
       return Admin.Start((XrdNetSocket *)carg);
      }

void *XrdOlbStartAnote(void *carg)
      {XrdOlbAdmin Anote;
       return Anote.Notes((XrdNetSocket *)carg);
      }

void *XrdOlbStartPandering(void *carg)
      {XrdOucTList *tp = (XrdOucTList *)carg;
       return XrdOlbSM.Pander(tp->text, tp->val);
      }

void *XrdOlbStartSupervising(void *carg)
      {EPNAME("StartSuper");
       XrdNetLink *newlink;
       while(1) if ((newlink = XrdOlbNetTCPr->Accept(XRDNET_NODNTRIM)))
                   {DEBUG("olbd: FD " <<newlink->FDnum() <<" connected to " <<newlink->Nick());
                    XrdOlbSM.Login(newlink);
                   }
       return (void *)0;
      }

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   EPNAME("main")
   XrdOucSemaphore SyncUp(0);
   pthread_t       tid;
   XrdNetLink     *newlink;
   XrdOucTList    *tp;

// Turn off sigpipe before we start any threads
//
   signal(SIGPIPE, SIG_IGN);
   sighold(SIGCHLD);

// Process configuration file
//
   if (XrdOlbConfig.Configure(argc, argv)) exit(1);

// Start the notification thread if we need to
//
   if (XrdOlbConfig.AnoteSock)
      XrdOucThread::Run(&tid, XrdOlbStartAnote, (void *)XrdOlbConfig.AnoteSock,
                        0, "Notification handler");

// Start the admin thread if we need to, we will not continue until told
// to do so by the admin interface.
//
   if (XrdOlbConfig.AdminSock)
      {XrdOlbAdmin::setSync(&SyncUp);
       XrdOucThread::Run(&tid, XrdOlbStartAdmin, (void *)XrdOlbConfig.AdminSock,
                         0, "Admin traffic");
       SyncUp.Wait();
      }

// Start the supervisor subsystem
//
   if (XrdOlbNetTCPr)
      {if (XrdOucThread::Run(&tid,XrdOlbStartSupervising, 
                             (void *)0, 0, "supervisor"))
          {XrdOlbSay.Emsg("olbd", errno, "start supervisor");
           _exit(1);
          }
      }

// Start the server subsystem
//
   if (XrdOlbConfig.Server())
      {tp = XrdOlbConfig.myManagers;
       while(tp)
            {if (!XrdOlbConfig.Manager() && !tp->next)
                XrdOlbSM.Pander(tp->text, tp->val);
                else {if (XrdOucThread::Run(&tid,XrdOlbStartPandering,(void *)tp,
                                            0, tp->text))
                         {XrdOlbSay.Emsg("olbd", errno, "start server");
                          _exit(1);
                         }
                      }
             tp = tp->next;
            }
      }

// Do manager processing now, simply loop looking for connections
//
   if (XrdOlbConfig.Manager())
      while(1) if ((newlink = XrdOlbNetTCPm->Accept(XRDNET_NODNTRIM)))
                  {DEBUG("olbd: FD " <<newlink->FDnum() <<" connected to " <<newlink->Nick());
                   XrdOucThread::Run(&tid, XrdOlbLoginServer, (void *)newlink);
                  }

// If we ever get here, just exit
//
   exit(0);
}
