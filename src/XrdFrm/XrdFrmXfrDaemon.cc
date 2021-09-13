/******************************************************************************/
/*                                                                            */
/*                    X r d F r m X f r D a e m o n . c c                     */
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdFrc/XrdFrcRequest.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmMigrate.hh"
#include "XrdFrm/XrdFrmTransfer.hh"
#include "XrdFrm/XrdFrmXfrAgent.hh"
#include "XrdFrm/XrdFrmXfrDaemon.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdFrc;
using namespace XrdFrm;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/

XrdFrmReqBoss XrdFrmXfrDaemon::GetBoss("getf", XrdFrcRequest::getQ);

XrdFrmReqBoss XrdFrmXfrDaemon::MigBoss("migr", XrdFrcRequest::migQ);

XrdFrmReqBoss XrdFrmXfrDaemon::StgBoss("pstg", XrdFrcRequest::stgQ);

XrdFrmReqBoss XrdFrmXfrDaemon::PutBoss("putf", XrdFrcRequest::putQ);

/******************************************************************************/
/* Private:                         B o s s                                   */
/******************************************************************************/

XrdFrmReqBoss *XrdFrmXfrDaemon::Boss(char bType)
{

// Return the boss corresponding to the type
//
   switch(bType)
         {case 0  :
          case '+': return &StgBoss;
          case '^':
          case '&': return &MigBoss;
          case '<': return &GetBoss;
          case '=':
          case '>': return &PutBoss;
          default:  break;
         }
   return 0;
}

/******************************************************************************/
/* Public:                          I n i t                                   */
/******************************************************************************/

int XrdFrmXfrDaemon::Init()
{
   char buff[80];

// Make sure we are the only daemon running
//
   sprintf(buff, "%s/frm_xfrd.lock", Config.QPath);
   if (!XrdFrcUtils::Unique(buff, Config.myProg)) return 0;

// Initiliaze the transfer processor (it need to be active now)
//
   if (!XrdFrmTransfer::Init()) return 0;

// Fix up some values that might not make sense
//
   if (Config.WaitMigr < Config.IdleHold) Config.WaitMigr = Config.IdleHold;

// Check if it makes any sense to migrate and, if so, initialize migration
//
   if (Config.pathList)
      {if (!Config.xfrOUT)
          Say.Emsg("Config","Output copy command not specified; "
                            "auto-migration disabled!");
          else XrdFrmMigrate::Migrate();
      } else Say.Emsg("Config","No migratable paths; "
                               "auto-migration disabled!");

// Start the external interfaces
//
   if (!StgBoss.Start(Config.QPath, Config.AdminMode)
   ||  !MigBoss.Start(Config.QPath, Config.AdminMode)
   ||  !GetBoss.Start(Config.QPath, Config.AdminMode)
   ||  !PutBoss.Start(Config.QPath, Config.AdminMode)) return 0;

// All done
//
   return 1;
}
  
/******************************************************************************/
/* Public:                          P o n g                                   */
/******************************************************************************/
  
void *XrdFrmXfrDaemonPong(void *parg)
{
    XrdFrmXfrDaemon::Pong();
    return (void *)0;
}
  
void XrdFrmXfrDaemon::Pong()
{
   EPNAME("Pong");
   static int udpFD = -1;
   XrdOucStream Request(&Say);
   XrdFrmReqBoss *bossP;
   char *tp;

// Get a UDP socket for the server if we haven't already done so and start
// a thread to re-enter this code and wait for messages from an agent.
//
   if (udpFD < 0)
      {XrdNetSocket *udpSock;
       pthread_t tid;
       int retc;
       if ((udpSock = XrdNetSocket::Create(&Say, Config.QPath,
                   "xfrd.udp", Config.AdminMode, XRDNET_UDPSOCKET)))
          {udpFD = udpSock->Detach(); delete udpSock;
           if ((retc = XrdSysThread::Run(&tid, XrdFrmXfrDaemonPong, (void *)0,
                                         XRDSYSTHREAD_BIND, "Pong")))
              Say.Emsg("main", retc, "create udp listner");
          }
       return;
      }

// Hookup to the udp socket as a stream
//
   Request.Attach(udpFD, 64*1024);

// Now simply get requests (see XrdFrmXfrDaemon for details). Here we screen
// out ping and list requests.
//
   while((tp = Request.GetLine()))
        {DEBUG(": '" <<tp <<"'");
         switch(*tp)
               {case '?': break;
                case '!': if ((tp = Request.GetToken()))
                             while(*tp++)
                                  {if ((bossP = Boss(*tp))) bossP->Wakeup(1);}
                          break;
                default:  XrdFrmXfrAgent::Process(Request);
               }
        }

// We should never get here (but....)
//
   Say.Emsg("Server", "Lost udp connection!");
}

/******************************************************************************/
/* Public:                         S t a r t                                  */
/******************************************************************************/
  
int XrdFrmXfrDaemon::Start()
{

// Start the ponger
//
   Pong();

// Now start nudging
//
   while(1)
        {StgBoss.Wakeup(); GetBoss.Wakeup();
         MigBoss.Wakeup(); PutBoss.Wakeup();
         XrdSysTimer::Snooze(Config.WaitQChk);
        }

// We should never get here
//
   return 0;
}
