/******************************************************************************/
/*                                                                            */
/*                      X r d O l b M a n a g e r . c c                       */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbManagerCVSID = "$Id$";

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
  
#include "XrdOlb/XrdOlbCache.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbManList.hh"
#include "XrdOlb/XrdOlbScheduler.hh"
#include "XrdOlb/XrdOlbServer.hh"
#include "XrdOlb/XrdOlbState.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdNet/XrdNetWork.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

       XrdOlbState     XrdOlbSMon;

extern XrdOlbCache     XrdOlbCache;

extern XrdOlbConfig    XrdOlbConfig;

extern XrdOucTrace     XrdOlbTrace;

extern XrdOucError     XrdOlbSay;

extern XrdNetWork     *XrdOlbNetTCPm;
extern XrdNetWork     *XrdOlbNetTCPs;

extern XrdOlbScheduler XrdOlbSched;

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
class XrdOlbDrop : XrdOlbJob
{
public:

      int DoIt() {extern XrdOlbManager  XrdOlbSM;
                  XrdOlbSM.STMutex.Lock();
                  return XrdOlbSM.Drop_Server(servEnt, servInst, this);
                 }

          XrdOlbDrop(int sid, int inst) : XrdOlbJob("drop server")
                    {servEnt  = sid;
                     servInst = inst;
                     XrdOlbSched.Schedule((XrdOlbJob *)this,
                                            time(0)+XrdOlbConfig.DRPDelay);
                         }
         ~XrdOlbDrop() {}

int  servEnt;
int  servInst;
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOlbManager::XrdOlbManager()
{
     memset((void *)ServTab, 0, sizeof(ServTab));
     memset((void *)ServBat, 0, sizeof(ServBat));
     memset((void *)MastTab, 0, sizeof(MastTab));
     memset((void *)AltMans, (int)' ', sizeof(AltMans));
     AltMend = AltMans;
     ServCnt =  0;
     noData  =  1;
     InstNum =  1;
     MTHi    = -1;
     STHi    = -1;
     XWait   = 0;
     XStage  = 1;
     SelAcnt = 0;
     SelRcnt = 0;
     doReset = 0;
     resetMask = 0;
}
  
/******************************************************************************/
/*                             B r o a d c a s t                              */
/******************************************************************************/
  
void XrdOlbManager::Broadcast(SMask_t smask, char *buff, int blen)
{
   int i;
   XrdOlbServer *sp;

// Obtain a lock on the table
//
   STMutex.Lock();

// Run through the table looking for servers to send messages to
//
   for (i = 0; i <= STHi; i++)
       {if ((sp = ServTab[i]) && sp->isServer(smask) && !sp->isOffline)
           sp->Lock();
           else continue;
        STMutex.UnLock();
        sp->Send(buff, blen);
        sp->UnLock();
        STMutex.Lock();
       }
   STMutex.UnLock();
}

/******************************************************************************/

void XrdOlbManager::Broadcast(SMask_t smask, const struct iovec *iod, int iovcnt)
{
   int i;
   XrdOlbServer *sp;

// Obtain a lock on the table
//
   STMutex.Lock();

// Run through the table looking for servers to send messages to
//
   for (i = 0; i <= STHi; i++)
       {if ((sp = ServTab[i]) && sp->isServer(smask) && !sp->isOffline)
           sp->Lock();
           else continue;
        STMutex.UnLock();
        sp->Send(iod, iovcnt);
        sp->UnLock();
        STMutex.Lock();
       }
   STMutex.UnLock();
}

/******************************************************************************/
/*                                I n f o r m                                 */
/******************************************************************************/
  
void XrdOlbManager::Inform(const char *cmd, int clen, char *arg, int alen)
{
   EPNAME("Inform");
   int i, iocnt, eol;
   struct iovec iod[4];
   XrdOlbServer *sp;

// Set up i/o vector
//
   iod[0].iov_base = XrdOlbConfig.MsgGID; 
   iod[0].iov_len  = XrdOlbConfig.MsgGIDL;
   iod[1].iov_base = (char *)cmd;
   iod[1].iov_len  = (clen ? clen : strlen(cmd));
   if (!arg) {iocnt = 1; eol = (*(cmd+iod[1].iov_len-1) == '\n');}
      else {iod[2].iov_base = arg;
            iod[2].iov_len  = (alen ? alen : strlen(arg));
            eol = (*(arg+iod[2].iov_len-1) == '\n');
            iocnt = 2;
           }
   if (!eol)
      {iocnt++;
       iod[iocnt].iov_base = (char *)"\n";
       iod[iocnt].iov_len  = 1;
      }
   iocnt++;

// Obtain a lock on the table
//
   MTMutex.Lock();

// Run through the table looking for managers to send messages to
//
   for (i = 0; i <= MTHi; i++)
       {if ((sp=MastTab[i]) && !sp->isOffline) 
           {sp->Lock();
            MTMutex.UnLock();
            DEBUG(sp->Name() <<" " <<cmd <<(arg ? arg : (char *)""));
            sp->Send(iod, iocnt);
            sp->UnLock();
            MTMutex.Lock();
           }
       }
   MTMutex.UnLock();
}
  
/******************************************************************************/
  
void XrdOlbManager::Inform(SMask_t mmask, const char *cmd, int clen)
{
   EPNAME("Inform");
   XrdOlbServer *sp;
   int i;

// Obtain a lock on the table
//
   MTMutex.Lock();

// Run through the table looking for managers to send messages to
//
   for (i = 0; i <= MTHi; i++)
       {if ((sp=MastTab[i]) && sp->isServer(mmask) && !sp->isOffline) 
           {sp->Lock();
            MTMutex.UnLock();
            DEBUG(sp->Name() <<" " <<cmd);
            sp->Send(cmd, clen);
            sp->UnLock();
            MTMutex.Lock();
           }
       }
   MTMutex.UnLock();
}

/******************************************************************************/
/*                           L i s t S e r v e r s                            */
/******************************************************************************/
  
XrdOlbSInfo *XrdOlbManager::ListServers(SMask_t mask, int opts)
{
    char *reason;
    int i, iend, nump, delay, lsall = opts & OLB_LS_ALL;
    XrdOlbServer *sp;
    XrdOlbSInfo  *sipp = 0, *sip;

// If only one wanted, the select appropriately
//
   STMutex.Lock();
   iend = (opts & OLB_LS_BEST ? 0 : STHi);
   for (i = 0; i <= iend; i++)
       {if (opts & OLB_LS_BEST)
            sp = (XrdOlbConfig.sched_RR
                 ? SelbyRef( mask, nump, delay, &reason, 0)
                 : SelbyLoad(mask, nump, delay, &reason, 0));
           else if (((sp = ServTab[i]) || (sp = ServBat[i]))
                &&  !lsall && !(sp->ServMask & mask)) sp = 0;
        if (sp)
           {sip = new XrdOlbSInfo(sp->Name(), sipp);
            sip->Id      = sp->ServID;
            sip->Load    = sp->myLoad;
            sip->Free    = sp->DiskTota;
            sip->RefTotA = sp->RefTotA + sp->RefA;
            sip->RefTotR = sp->RefTotR + sp->RefR;
            if (sp->isOffline) sip->Status  = OLB_SERVER_OFFLINE;
               else sip->Status  = 0;
            if (sp->isDisable) sip->Status |= OLB_SERVER_DISABLE;
            if (sp->isNoStage) sip->Status |= OLB_SERVER_NOSTAGE;
            if (sp->isSuspend) sip->Status |= OLB_SERVER_SUSPEND;
            sp->UnLock();
            sipp = sip;
           }
       }
   STMutex.UnLock();

// Return result
//
   return sipp;
}
  
/******************************************************************************/
/*                                 L o g i n                                  */
/******************************************************************************/
  
void *XrdOlbManager::Login(XrdNetLink *lnkp)
{
   EPNAME("Login")
   XrdOlbServer *sp;
   char *tp, *Stype;
   int   Status = 0, fdsk = 0, numfs = 1, addedp = 0, port = 0, Sport = 0;
   int   servID, servInst;
   SMask_t servset = 0, newmask;

// Handle the login for the server stream.
//
   if (!(tp = lnkp->GetLine())) return Login_Failed("missing login",lnkp);

// Check if this is a message request. One such request can be sent ahead of
// the login to be included in the log (typically the manager's log).
//
   if (!strncmp("msg ", tp, 4) && tp[4])
      {XrdOlbSay.Emsg("Manager",lnkp->Name(), tp+4);
       if (!(tp = lnkp->GetLine())) return Login_Failed("missing login",lnkp);
      }
   DEBUG("from " <<lnkp->Name() <<": " <<tp);

// Get the login command and its argument
//
   if (!(tp = lnkp->GetToken()) || strcmp(tp, "login")
   ||  !(tp = lnkp->GetToken()))
      return Login_Failed("first command not login", lnkp);

// Director Command: login director
//
   if (!strcmp(tp, "director"))
      {XrdOlbSay.Emsg("Manager","Director",lnkp->Name(),"logged in.");
       sp = new XrdOlbServer(lnkp);
       sp->Process_Director();
       XrdOlbSay.Emsg("Manager","Director",lnkp->Name(),"logged out.");
       delete sp;
       return (void *)0;
      }

// Server Command: login server [port <dport>] [nostage] [suspend] [+m:<sport>]
// Responses:      <id> ping
//                 <id> try <hostlist>
// Server Command: addpath <mode> <path>
//                 start <maxkb> <numfs> <totkb>
//
   if (strcmp(tp, "server"))
      return Login_Failed("Invalid server type", lnkp);
   Stype = (char *)"server";

// The server may specify a port number
//
   if ((tp = lnkp->GetToken()) && !strcmp("port", tp))
      {if (!(tp = lnkp->GetToken()))
          return Login_Failed("missing start port value", lnkp);
       if (XrdOuca2x::a2i(XrdOlbSay,"start port value",tp,&port,0,65535))
          return Login_Failed("invalid start port value", lnkp);
       tp = lnkp->GetToken();
      }

// The server may specify nostage
//
   if (tp && !strcmp("nostage", tp))
      {Status |= OLB_noStage;
       tp = lnkp->GetToken();
      }

// The server may specify suspend
//
   if (tp && !strcmp("suspend", tp))
      {Status |= OLB_Suspend;
       tp = lnkp->GetToken();
      }

// The server may specify the version (for self-managed servers)
//
   if (tp && *tp == '+')
      {Status |= OLB_Special;
       tp++;
       if (*tp == 'm') 
          {Status |= OLB_isMan; tp++;
           Stype = (char *)"Supervisor";
           if (*tp == ':')
              if (XrdOuca2x::a2i(XrdOlbSay,"subscription port",tp+1,&Sport,0))
                 return Login_Failed("invalid subscription port", lnkp);
           if (!Sport) Sport = XrdOlbConfig.PortTCP;
          }
      }

// Add the server
//
   if (!(sp = AddServer(lnkp, port, Status, Sport)))
      return Login_Failed(0, lnkp);
   servID = sp->ServID; servInst = sp->Instance;

// Immediately send a ping request to validate the login (phase 1)
//
   if (Status & OLB_Special) lnkp->Send("1@0 ping\n", 9);

// At this point, the server will send only addpath commands followed by a start
//
   while((tp = lnkp->GetLine()))
        {DEBUG("Received from " <<lnkp->Name() <<": " <<tp);
         if (!(tp = lnkp->GetToken())) break;
         if (!strcmp(tp, "start")) break;
         if (strcmp(tp, "addpath"))
            return Login_Failed("invalid command sequence", lnkp, sp);
         if (!(newmask = AddPath(sp)))
            return Login_Failed("invalid addpath command", lnkp, sp);
         servset |= newmask;
         addedp= 1;
        }

// At this point if all went well, start the server
//
   if (!tp) return Login_Failed("missing start", lnkp, sp);

// The server may include the max amount of free space, if need be, on the start
//
   if ((tp = lnkp->GetToken())
   && XrdOuca2x::a2i(XrdOlbSay,"start maxkb value",tp,&fdsk,0))
      return Login_Failed("invalid start maxkb value", lnkp, sp);
      else {sp->DiskTota = sp->DiskFree = fdsk; sp->DiskNums = 1;}

// The server may include the number of file systems, on the start
//
   if ((tp = lnkp->GetToken())
   && XrdOuca2x::a2i(XrdOlbSay, "start numfs value",  tp, &numfs, 0))
      return Login_Failed("invalid start numfs value", lnkp, sp);
      else sp->DiskNums = numfs;

// The server may include the total free space in all file systems, on the start
//
   if ((tp = lnkp->GetToken())
   && XrdOuca2x::a2i(XrdOlbSay, "start totkb value",  tp, &fdsk, 0))
      return Login_Failed("invalid start totkb value", lnkp, sp);
      else sp->DiskTota = fdsk;

// Check if we have any special paths. If none, then we must set the cache for
// all entries to indicate that the server bounced.
//
   if (!addedp) 
      {XrdOlbPInfo pinfo;
       pinfo.rovec = sp->ServMask;
       servset = XrdOlbCache.Paths.Insert("/", &pinfo);
       XrdOlbCache.Bounce(sp->ServMask);
       XrdOlbSay.Emsg("Manager",Stype,lnkp->Name(),"defaulted r /");
      }

// Finally set the reference counts for intersecting servers to be the same.
// Additionally, indicate cache refresh will be needed because we have a new
// server that may have files the we already reported on.
//
   ResetRef(servset);
   if (XrdOlbConfig.Manager()) Reset();

// Process responses from the server.
//
   tp = sp->isSuspend ? (char *)"logged in suspended." : (char *)"logged in.";
   XrdOlbSay.Emsg("Manager", Stype, sp->Name(), tp);

   sp->isDisable = 0;
   sp->Process_Responses();

   tp = sp->isOffline ? (char *)"forced out." : (char *)"logged out.";
   XrdOlbSay.Emsg("Manager", Stype, sp->Name(), tp);

// Recycle the server
//
   if (sp->Link) Remove_Server(0, servID, servInst);
   return (void *)0;
}
  
/******************************************************************************/
/*                               M o n P e r f                                */
/******************************************************************************/
  
void *XrdOlbManager::MonPerf()
{
   XrdOlbServer *sp;
   char *reqst;
   int nldval, i;
   int oldval=0, doping = 0;

// Sleep for the indicated amount of time, then maintain load on each server
//
   while(XrdOlbConfig.AskPing)
        {Snooze(XrdOlbConfig.AskPing);
         if (--doping < 0) doping = XrdOlbConfig.AskPerf;
         STMutex.Lock();
         for (i = 0; i <= STHi; i++)
             {if ((sp = ServTab[i]) && sp->isBound) sp->Lock();
                 else continue;
              STMutex.UnLock();
              if (doping || !XrdOlbConfig.AskPerf)
                 {reqst = (char *)"1@0 ping\n"; nldval = 0;
                  if ((oldval = sp->pingpong)) sp->pingpong = 0;
                     else sp->pingpong = -1;
                 } else {
                  reqst = (char *)"1@0 usage\n";oldval = 0;
                  if ((nldval = sp->newload)) sp->newload = 0;
                     else sp->newload = -1;
                 }
              if (oldval < 0 || nldval < 0)
                  Remove_Server("not responding", i, sp->Instance);
                  else sp->Send(reqst);
              sp->UnLock();
              STMutex.Lock();
             }
         STMutex.UnLock();
        }
   return (void *)0;
}
  
/******************************************************************************/
/*                               M o n P i n g                                */
/******************************************************************************/
  
void *XrdOlbManager::MonPing()
{
   XrdOlbServer *sp;
   int i;

// Make sure the manager sends at least one request within twice the ping 
// interval plus a little. If we don't get one, then declare the manager dead 
// and re-initialize the manager connection.
//
   do {Snooze(XrdOlbConfig.AskPing*2+13);
       MTMutex.Lock();
       for (i = 0; i < MTHi; i++)
           if ((sp = MastTab[i]))
              {sp->Lock();
               if (sp->isActive) sp->isActive = 0;
                  else {XrdOlbSay.Emsg("Manager", "Manager", sp->Link->Name(),
                                       "appears to be dead.");
                        sp->isOffline = 1;
                        sp->Link->Close(1);
                       }
               sp->UnLock();
              }
       MTMutex.UnLock();
      } while(1);

// Keep the compiler happy
//
   return (void *)0;
}
  
/******************************************************************************/
/*                               M o n R e f s                                */
/******************************************************************************/
  
void *XrdOlbManager::MonRefs()
{
   XrdOlbServer *sp;
   int  i, snooze_interval = 10*60, loopmax, loopcnt = 0;
   int resetA, resetR, resetAR;

// Compute snooze interval
//
   if ((loopmax = XrdOlbConfig.RefReset / snooze_interval) <= 1)
      if (!XrdOlbConfig.RefReset) loopmax = 0;
         else {loopmax = 1; snooze_interval = XrdOlbConfig.RefReset;}

// Sleep for the snooze interval. If a reset was requested then do a selective
// reset unless we reached our snooze maximum and enough selections have gone
// by; in which case, do a global reset.
//
   do {Snooze(snooze_interval);
       loopcnt++;
       STMutex.Lock();
       resetA  = (SelAcnt >= XrdOlbConfig.RefTurn);
       resetR  = (SelRcnt >= XrdOlbConfig.RefTurn);
       resetAR = (loopmax && loopcnt >= loopmax && (resetA || resetR));
       if (doReset || resetAR)
           {for (i = 0; i <= STHi; i++)
                if ((sp = ServTab[i])
                &&  (resetAR || (doReset && sp->isServer(resetMask))) )
                    {sp->Lock();
                     if (resetA || doReset) {sp->RefTotA += sp->RefA;sp->RefA=0;}
                     if (resetR || doReset) {sp->RefTotR += sp->RefR;sp->RefR=0;}
                     sp->UnLock();
                    }
            if (resetAR)
               {if (resetA) SelAcnt = 0;
                if (resetR) SelRcnt = 0;
                loopcnt = 0;
               }
            if (doReset) {doReset = 0; resetMask = 0;}
           }
       STMutex.UnLock();
      } while(1);
   return (void *)0;
}

/******************************************************************************/
/*                                P a n d e r                                 */
/******************************************************************************/
  
void *XrdOlbManager::Pander(char *manager, int mport)
{
   XrdOlbServer   *sp;
   XrdNetLink     *lp;
   XrdOlbManList   myMans;
   int rc, Status, opts = 0, waits = 6, tries = 6, fails = 0, xport = mport;
   char manbuff[256], *reason, *manp = manager;

// Keep connecting to our manager. If XWait is present, wait for it to
// be turned off first; then try to connect.
//
   do {while(XWait)
            {if (!waits--)
                {XrdOlbSay.Emsg("Manager", "Suspend state still active.");
                 waits = 6;
                }
             Snooze(10);
            }
       if (!(lp = XrdOlbNetTCPs->Connect(manp, xport, opts)))
          {if (tries--) opts = XRDNET_NOEMSG;
              else {tries = 6; opts = 0;}
           if (!XrdOlbServer::myMans.haveAlts()
           ||  !(manp=XrdOlbServer::myMans.Next(xport,manbuff,sizeof(manbuff))))
              {manp = manager; xport = mport; Snooze(10); fails++;}
              else Snooze(3);
           continue;
          }
       opts = 0; tries = waits = 6;

       // Obtain a new server object for this server
       //
       sp = new XrdOlbServer(lp);
       Add_Manager(sp);

       // Login this server. If we are a line manager login suspended
       //
       reason = 0;
       if (XrdOlbConfig.Manager()) Status = OLB_Suspend|OLB_noStage|OLB_isMan;
          else Status = (XWait ? OLB_Suspend : 0)|(XStage ? 0 : OLB_noStage);
       if (fails >= 6 && manp == manager) {Status |= OLB_Lost; fails = 0;}

       if (!(rc=sp->Login(Port, Status))) sp->Process_Requests();
          else if (rc == 1) reason = (char *)"login to manager failed";

       // Just try again
       //
       Remove_Manager(reason, sp);
       delete sp;

       // Cycle on to the next manager if we have one or snooze and try over
       //
       if ((manp=XrdOlbServer::myMans.Next(xport,manbuff,sizeof(manbuff))))
          continue;
       Snooze(15);
       manp = manager; xport = mport; fails++;
      } while(1);
    return (void *)0;
}

/******************************************************************************/
/*                         R e m o v e _ S e r v e r                          */
/******************************************************************************/

void XrdOlbManager::Remove_Server(const char *reason, 
                                  int sent, int sinst, int immed)
{
   EPNAME("Remove_Server")
   XrdOlbServer *sp;

// Obtain a lock on the servtab
//
   STMutex.Lock();

// Make sure this server is the right one
//
   if (!(sp = ServTab[sent]) || sp->Instance != sinst)
      {STMutex.UnLock();
       DEBUG("Remove server " <<sent <<'.' <<sinst <<" failed.");
       return;
      }

// Do a partial drop at this point
//
   if (sp->Link) {sp->Link->Close(1);}
   sp->isOffline = 1;
   if (sp->isBound) {sp->isBound = 0; ServCnt--;}

// Compute new state of all servers if we are a reporting manager
//
   if (XrdOlbConfig.Manager()) XrdOlbSMon.Calc(-1, sp->isNoStage, sp->isSuspend);

// If this is an immediate drop request, do so now
//
   if (immed || !XrdOlbConfig.DRPDelay) {Drop_Server(sent, sinst); return;}

// If a drop job is already scheduled, update the instance field. Otherwise,
// Schedule a server drop at a future time.
//
   sp->DropTime = time(0)+XrdOlbConfig.DRPDelay;
   if (sp->DropJob) sp->DropJob->servInst = sinst;
      else sp->DropJob = new XrdOlbDrop(sent, sinst);

// Document removal
//
   if (reason) 
      XrdOlbSay.Emsg("Manager", sp->Name(), "scheduled for removal;", reason);
      else DEBUG("Will remove " <<sp->Name() <<" server " <<sent <<'.' <<sinst);
   STMutex.UnLock();
}

/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdOlbManager::Reset()
{
   EPNAME("Reset");
   const char *cmd = "rst\n";
   const int cmdln = 4;
   XrdOlbServer *sp;
   int i;

// Obtain a lock on the table
//
   MTMutex.Lock();

// Run through the table looking for managers to send a reset request
//
   for (i = 0; i <= MTHi; i++)
       {if ((sp=MastTab[i]) && !sp->isOffline && sp->isKnown)
           {sp->Lock();
            sp->isKnown = 0;
            MTMutex.UnLock();
            DEBUG("sent to " <<sp->Name());
            sp->Send(cmd, cmdln);
            sp->UnLock();
            MTMutex.Lock();
           }
       }
   MTMutex.UnLock();
}

/******************************************************************************/
/*                                R e s u m e                                 */
/******************************************************************************/

void XrdOlbManager::Resume()
{
     const char *cmd = "resume\n";
     const int   cln = strlen(cmd);

// If the suspend file is still present, ignore this resume request
//
   if (XrdOlbConfig.inSuspend())
      XrdOlbSay.Emsg("Manager","Resume request ignored; suspend file present.");
      else {XWait = 0;
            Inform(cmd, (int)cln);
           }
}
  
/******************************************************************************/
/*                             S e l S e r v e r                              */
/******************************************************************************/
  
int XrdOlbManager::SelServer(int needrw, char *path, 
                            SMask_t pmask, SMask_t amask, char *hbuff,
                            const struct iovec *iodata, int iovcnt)
{
    EPNAME("SelServer")
    char *reason;
    int delay, nump, isalt = 0, pass = 2;
    SMask_t mask;
    XrdOlbServer *sp = 0;

// Check if we have enough servers to do a selection
//
   if (ServCnt < XrdOlbConfig.SUPCount) 
      {TRACE(Defer, "client defered; not enough servers for " <<path);
       return XrdOlbConfig.SUPDelay;
      }

// Scan for a primary and alternate server (alternates do staging)
//
   mask = pmask;
   STMutex.Lock();
   while(pass--)
        {if (mask)
            {sp = (XrdOlbConfig.sched_RR
                   ? SelbyRef( mask, nump, delay, &reason, isalt)
                   : SelbyLoad(mask, nump, delay, &reason, isalt));
             if (sp || (nump && delay)) break;
            }
         mask = amask; isalt = 1;
        }
   STMutex.UnLock();

// Update info
//
   if (sp)
      {strcpy(hbuff, sp->Name());
       sp->RefR++;
       if (isalt)
          {XrdOlbCache.AddFile(path, sp->ServMask, needrw);
           sp->RefA++;
           TRACE(Stage, "Server " <<hbuff <<" bound to " <<path);
           sp->DiskTota -= XrdOlbConfig.DiskAdj;
           if (((sp->DiskFree -= XrdOlbConfig.DiskAdj) < XrdOlbConfig.DiskMin)
           &&   sp->DiskAskdl <= time(0))
              {sp->Link->Send("1@0 space\n");
               sp->DiskAskdl = time(0) + XrdOlbConfig.DiskAsk;
              }
          if (iovcnt && iodata) sp->Link->Send(iodata, iovcnt);
          }
       sp->UnLock();
       return 0;
      }

// Return delay if selection failure is recoverable
//
   if (delay)
      {Record(path, reason);
       return delay;
      }

// Return failure, waits are not allowed (at this point we would bounce the
// client to a proxy server if we had that implemented)
//
   return -1;
}

/******************************************************************************/
/*                              R e s e t R e f                               */
/******************************************************************************/
  
void XrdOlbManager::ResetRef(SMask_t smask)
{

// Obtain a lock on the table
//
   STMutex.Lock();

// Inform the reset thread that we need a reset
//
   doReset = 1;
   resetMask |= smask;

// Unlock table and exit
//
   STMutex.UnLock();
}

/********************************************************************************/
/*                                S n o o z e                                 */
/******************************************************************************/
  
void XrdOlbManager::Snooze(int slpsec)
{
   int retc;
   struct timespec lftp, rqtp = {slpsec, 0};

   while ((retc = nanosleep(&rqtp, &lftp)) < 0 && errno == EINTR)
         {rqtp.tv_sec  = lftp.tv_sec;
          rqtp.tv_nsec = lftp.tv_nsec;
         }

   if (retc < 0) XrdOlbSay.Emsg("Manager", errno, "sleep");
}

/******************************************************************************/
/*                                 S t a g e                                  */
/******************************************************************************/
  
void XrdOlbManager::Stage(int ison, int doinform)
{
     char *cmd = (ison ? (char *)"stage" : (char *)"nostage");

     XStage = ison;
     if (doinform) Inform(cmd, strlen(cmd));
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdOlbManager::Stats(char *bfr, int bln)
{
   static const char statfmt1[] = "<stats id=\"olb\"><name>%s</name>";
   static const char statfmt2[] = "<subscriber><name>%s</name>"
          "<status>%s</status><load>%d</load><diskfree>%d</diskfree>"
          "<refa>%d</refa><refr>%d</refr></subscriber>";
   static const char statfmt3[] = "</stats>\n";
   XrdOlbSInfo *sp;
   int mlen, tlen = sizeof(statfmt3);
   char stat[6], *stp;

   class spmngr {
         public: XrdOlbSInfo *sp;

                 spmngr() {sp = 0;}
                ~spmngr() {XrdOlbSInfo *xsp;
                           while((xsp = sp)) {sp = sp->next; delete xsp;}
                          }
                } mngrsp;

// Check if actual length wanted
//
   if (!bfr) return  sizeof(statfmt1) + 256  +
                    (sizeof(statfmt2) + 20*4 + 256) * STMax +
                     sizeof(statfmt3) + 1;

// Get the statistics
//
   mngrsp.sp = sp = ListServers();

// Format the statistics
//
   mlen = snprintf(bfr, bln, statfmt1, XrdOlbConfig.myName);
   if ((bln -= mlen) <= 0) return 0;
   tlen += mlen;

   while(sp && bln)
        {stp = stat;
         if (sp->Status)
            {if (sp->Status & OLB_SERVER_OFFLINE) *stp++ = 'o';
             if (sp->Status & OLB_SERVER_SUSPEND) *stp++ = 's';
             if (sp->Status & OLB_SERVER_NOSTAGE) *stp++ = 'n';
             if (sp->Status & OLB_SERVER_DISABLE) *stp++ = 'd';
            } else *stp++ = 'a';
         bfr += mlen;
         mlen = snprintf(bfr, bln, statfmt2, sp->Name, stat,
                sp->Load, sp->Free, sp->RefTotA, sp->RefTotR);
         bln  -= mlen;
         tlen += mlen;
         sp = sp->next;
        }

// See if we overflowed. otherwise finish up
//
   if (sp || bln < (int)sizeof(statfmt1)) return 0;
   bfr += mlen;
   strcpy(bfr, statfmt3);
   return tlen;
}
  
/******************************************************************************/
/*                               S u s p e n d                                */
/******************************************************************************/

void XrdOlbManager::Suspend(int doinform)
{
     const char *cmd = "suspend\n";
     const int   cln = strlen(cmd);

     XWait = 1;
     if (doinform) Inform(cmd, (int)cln);
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               A d d P a t h                                */
/******************************************************************************/
  
SMask_t XrdOlbManager::AddPath(XrdOlbServer *sp)
{
    char *tp;
    XrdOlbPInfo pinfo;

// Process: addpath {r | w | rw}[s] path
//
   if (!(tp = sp->Link->GetToken())) return 0;
   while(*tp)
        {     if ('r' == *tp) pinfo.rovec =               sp->ServMask;
         else if ('w' == *tp) pinfo.rovec = pinfo.rwvec = sp->ServMask;
         else if ('s' == *tp) pinfo.rovec = pinfo.ssvec = sp->ServMask;
         else return 0;
         tp++;
        }

// Get the path
//
   if (!(tp = sp->Link->GetToken())) return 0;
   if (pinfo.rwvec || pinfo.ssvec) sp->isRW = 1;

// For everything matching the path indicate in the cache the server bounced
//
   XrdOlbCache.Bounce(sp->ServMask, tp);

// Add the path to the known path list
//
   return XrdOlbCache.Paths.Insert(tp, &pinfo);
}

/******************************************************************************/
/*                           A d d _ M a n a g e r                            */
/******************************************************************************/
  
int XrdOlbManager::Add_Manager(XrdOlbServer *sp)
{
   EPNAME("AddManager")
   const SMask_t smask_1 = 1;
   int i;

// Find available ID for this server
//
   MTMutex.Lock();
   for (i = 0; i < MTMax; i++) if (!MastTab[i]) break;

// Check if we have too many here
//
   if (i > MTMax)
      {MTMutex.UnLock();
       XrdOlbSay.Emsg("Manager", "Login to", sp->Link->Name(),
                     "failed; too many managers");
       return 0;
      }

// Assign new manager
//
   MastTab[i] = sp;
   if (i > MTHi) MTHi = i;
   sp->ServID   = i;
   sp->ServMask = smask_1<<i;
   sp->Instance = InstNum++;
   sp->isOffline  = 0;
   sp->isNoStage  = 0;
   sp->isSuspend  = 0;
   sp->isActive   = 1;
   sp->isMan      = (XrdOlbConfig.Manager() ? 1 : 0);
   MTMutex.UnLock();

// Document login
//
   DEBUG("Added " <<sp->Name() <<" to manager config; id=" <<i);
   return 1;
}

/******************************************************************************/
/*                             A d d S e r v e r                              */
/******************************************************************************/
  
XrdOlbServer *XrdOlbManager::AddServer(XrdNetLink *lp, int port, 
                                           int Status, int sport)
{
   EPNAME("AddServer")
    const SMask_t smask_1 = 1;
    int tmp, i, j = -1, k = -1, os = -1;
    char *act = (char *)"Added ", *hnp = lp->Name();
    unsigned int ipaddr = lp->Addr();
    XrdOlbServer *bump = 0, *sp = 0;

// Find available ID for this server
//
   STMutex.Lock();
   for (i = 0; i < STMax; i++)
       if (ServBat[i])
          {if (ServBat[i]->isServer(ipaddr, port)) break;
              else if (!ServTab[i] && k < 0) k = i;
                   else if (os < 0 && (Status & OLB_isMan)
                        &&  ServTab[i]->isSpecial && !ServTab[i]->isMan) os = i;
          }
          else if (j < 0) j = i;

// Check if server is already logged in or is a relogin
//
   if (i < STMax)
      if (ServTab[i] && ServTab[i]->isBound)
         {STMutex.UnLock();
          XrdOlbSay.Emsg("Manager", "Server", hnp, "already logged in.");
          return 0;
         } else { // Rehook server to previous entry
          sp = ServBat[i];
          if (sp->Link) sp->Link->Recycle();
          sp->Link      = lp;
          sp->isOffline = 0;
          sp->setName(lp->Name(), port);  // Just in case it changed
          ServTab[i] = sp;
          j = i;
          act = (char *)"Re-added ";
         }

// Reuse an old ID if we must
//
   if (!sp)
      {if (j < 0 && k >= 0)
          {DEBUG("ID " <<k <<" reassigned" <<ServBat[k]->Name() <<" to " <<hnp);
           delete ServBat[k]; ServBat[k] = 0;
           j = k;
          } else if (j < 0)
                    {if (os >= 0)
                        {bump = ServTab[os];
                         ServTab[os] = ServBat[os] = 0;
                         j = os;
                        } else {STMutex.UnLock();
                                if (Status & OLB_Special)
                                   {sendAList(lp);
                                    act = (char *)" redirected";
                                   } else act = (char *)" failed";
                                DEBUG("Login " <<hnp <<act 
                                      <<"; too many subscribers");
                                return 0;
                               }
                    }
       ServTab[j]   = ServBat[j] = sp = new XrdOlbServer(lp, port);
       sp->ServID   = j;
       sp->ServMask = smask_1<<j;
      }

// Assign new server
//
   if (Status & OLB_isMan) setAltMan(j, ipaddr, sport);
   if (j > STHi) STHi = j;
   sp->Instance  = InstNum++;
   sp->isBound   = 1;
   sp->isNoStage = (Status & OLB_noStage);
   sp->isSuspend = (Status & OLB_Suspend);
   sp->isMan     = (Status & OLB_isMan);
   sp->isSpecial = (Status & OLB_Special);
   sp->isDisable = 1;
   ServCnt++;
   if (XrdOlbConfig.SUPLevel
   && (tmp = ServCnt*XrdOlbConfig.SUPLevel/100) > XrdOlbConfig.SUPCount)
      XrdOlbConfig.SUPCount=tmp;

// Check if we should bump someone (deferal avoids extra recovery transactions)
//
   if (bump)
      {bump->Lock();
       sendAList(bump->Link);
       DEBUG("ID " <<os <<" bumped " <<bump->Name() <<" for " <<hnp);
       bump->isOffline = 1;
       bump->Link->Close(1);
       bump->UnLock();
      }

// Document login
//
   DEBUG(act <<sp->Name() <<" to server config; id=" <<j <<'.' <<sp->Instance
         <<"; num=" <<ServCnt <<"; min=" <<XrdOlbConfig.SUPCount);

// Compute new state of all servers if we are a reporting manager.
//
   if (XrdOlbConfig.Manager()) XrdOlbSMon.Calc(1, sp->isNoStage, sp->isSuspend);
   STMutex.UnLock();

// All done, all locks released, return the server
//
   return sp;
}

/******************************************************************************/
/*                             c a l c D e l a y                              */
/******************************************************************************/
  
XrdOlbServer *XrdOlbManager::calcDelay(int nump, int numd, int numf, int numo,
                                       int nums, int &delay, char **reason)
{
        if (!nump) {delay = 0;
                    *reason = (char *)"no eligible servers for";
                   }
   else if (numf)  {delay = XrdOlbConfig.DiskWT;
                    *reason = (char *)"no eligible servers have space for";
                   }
   else if (numo)  {delay = XrdOlbConfig.MaxDelay;
                    *reason = (char *)"eligible servers overloaded for";
                   }
   else if (nums)  {delay = XrdOlbConfig.SUSDelay;
                    *reason = (char *)"eligible servers suspended for";
                   }
   else if (numd)  {delay = XrdOlbConfig.SUPDelay;
                    *reason = (char *)"eligible servers offline for";
                   }
   else            {delay = XrdOlbConfig.SUPDelay;
                    *reason = (char *)"server selection error for";
                   }
   return (XrdOlbServer *)0;
}

/******************************************************************************/
/*                           D r o p _ S e r v e r                            */
/******************************************************************************/
  
// Warning: STMutex must be locked upon entry. It will be released upon exit!

int XrdOlbManager::Drop_Server(int sent, int sinst, XrdOlbDrop *djp)
{
   EPNAME("Drop_Server")
   XrdOlbServer *sp;
   char hname[256];

// Make sure this server is the right one
//
   if (!(sp = ServTab[sent]) || sp->Instance != sinst)
      {if (djp == sp->DropJob) {sp->DropJob = 0; sp->DropTime = 0;}
       DEBUG("Drop server " <<sent <<'.' <<sinst <<" cancelled.");
       STMutex.UnLock();
       return 0;
      }

// Check if the drop has been rescheduled
//
   if (djp && time(0) < sp->DropTime)
      {XrdOlbSched.Schedule((XrdOlbJob *)djp, sp->DropTime);
       STMutex.UnLock();
       return 1;
      }

// Save the server name (don't want to hold a lock across a message)
//
   strncpy(hname, sp->Name(), sizeof(hname)-1);
   hname[sizeof(hname)-1] = '\0';

// Remove server from the manager table
//
   ServTab[sent] = 0;
   sp->isOffline = 1;
   sp->DropTime  = 0;
   sp->DropJob   = 0;
   sp->isBound   = 0;

// Remove server entry from the alternate list
//
   if (sp->isMan)
      {memset((void *)&AltMans[sent*AltSize], (int)' ', AltSize);
       sent = sp->ServID-1;
       while(sent >= 0 && ServTab[sent] && !ServTab[sent]->isMan) sent--;
       if (sent < 0) AltMend = AltMans;
          else AltMend = AltMans + ((sent+1)*AltSize);
      }

// Readjust STHi
//
   if (sent == STHi) while(STHi >= 0 && !ServTab[STHi]) STHi--;

// Recycle the link
//
   if (sp->Link) {sp->Link->Recycle(); sp->Link = 0;}

// Document the drop
//
   STMutex.UnLock();
   XrdOlbSay.Emsg("Server", hname, "dropped.");
   return 0;
}

/******************************************************************************/
/*                          L o g i n _ F a i l e d                           */
/******************************************************************************/
  
void *XrdOlbManager::Login_Failed(const char *reason, 
                                 XrdNetLink *lp, XrdOlbServer *sp)
{
     if (sp) Remove_Server(reason, sp->ServID, sp->Instance);
        else {if (reason) XrdOlbSay.Emsg("Manager", lp->Name(),
                                          "login failed;", reason);
              lp->Recycle();
             }
     return (void *)0;
}

/******************************************************************************/
/*                                R e c o r d                                 */
/******************************************************************************/
  
void XrdOlbManager::Record(char *path, const char *reason)
{
   EPNAME("Record")
   static int msgcnt = 256;
   static XrdOucMutex mcMutex;
   int mcnt;

   DEBUG(reason <<path);
   mcMutex.Lock();
   msgcnt++; mcnt = msgcnt;
   mcMutex.UnLock();

   if (mcnt > 255)
      {XrdOlbSay.Emsg("client defered;", reason, path);
       mcnt = 1;
      }
}

/******************************************************************************/
/*                        R e m o v e _ M a n a g e r                         */
/******************************************************************************/

void XrdOlbManager::Remove_Manager(const char *reason, XrdOlbServer *sp)
{
   EPNAME("Remove_Manager")
   int sent  = sp->ServID;
#ifndef NODEBUG
   int sinst = sp->Instance;
#endif

// Obtain a lock on the servtab
//
   MTMutex.Lock();

// Make sure this server is the right one
//
   if (!(sp == MastTab[sent]))
      {MTMutex.UnLock();
       DEBUG("Remove manager " <<sent <<'.' <<sinst <<" failed.");
       return;
      }

// Remove server from the manager table
//
   MastTab[sent] = 0;
   sp->isOffline = 1;
   DEBUG("Removed " <<sp->Name() <<" manager " <<sent <<'.' <<sinst <<" FD=" <<sp->Link->FDnum());

// Readjust MTHi
//
   if (sent == MTHi) while(MTHi >= 0 && !MastTab[MTHi]) MTHi--;
   MTMutex.UnLock();

// Document removal
//
   if (reason) XrdOlbSay.Emsg("Manager", sp->Name(), "removed;", reason);
}

/******************************************************************************/
/*                             S e l b y L o a d                              */
/******************************************************************************/

#define Abs(a) (a < 0 ? -a : a)
  
XrdOlbServer *XrdOlbManager::SelbyLoad(SMask_t mask, int &nump, int &delay,
                                       char **reason, int needspace)
{
    int i, numd, numf, numo, nums;
    XrdOlbServer *np, *sp = 0;

// Scan for a server (preset possible, suspended, overloaded, full, and dead)
//
   nump = nums = numo = numf = numd = 0; 
   for (i = 0; i <= STHi; i++)
       if ((np = ServTab[i]) && (np->ServMask & mask))
          {nump++;
           if (np->isOffline)                     {numd++; continue;}
           if (np->isSuspend || sp->isDisable)    {nums++; continue;}
           if (np->myLoad > XrdOlbConfig.MaxLoad) {numo++; continue;}
           if (needspace && (   np->isNoStage
                             || np->DiskTota < XrdOlbConfig.DiskMin
                             || np->DiskFree < XrdOlbConfig.DiskAdj))
              {numf++; continue;}
           if (!sp) sp = np;
              else if (Abs(sp->myLoad - np->myLoad)
                          <= XrdOlbConfig.P_fuzz)
                      {if (needspace)
                          {if (sp->RefA > (np->RefA+XrdOlbConfig.DiskLinger))
                               sp=np;
                           } 
                           else if (sp->RefR > np->RefR) sp=np;
                       }
                       else if (sp->myLoad > np->myLoad) sp=np;
          }

// Check for overloaded server and return result
//
   if (!sp) return calcDelay(nump, numd, numf, numo, nums, delay, reason);
   if (needspace) SelAcnt++;  // Protected by STMutex
      else        SelRcnt++;
   sp->Lock();
   delay = 0;
   return sp;
}
 
/******************************************************************************/
/*                              S e l b y R e f                               */
/******************************************************************************/

XrdOlbServer *XrdOlbManager::SelbyRef(SMask_t mask, int &nump, int &delay,
                                      char **reason, int needspace)
{
    int i, numd, numf, nums;
    XrdOlbServer *np, *sp = 0;

// Scan for a server
//
   nump = nums = numf = numd = 0; // possible, suspended, full, and dead
   for (i = 0; i <= STHi; i++)
       if ((np = ServTab[i]) && (np->ServMask & mask))
          {nump++;
           if (np->isOffline)                   {numd++; continue;}
           if (np->isSuspend || np->isDisable)  {nums++; continue;}
           if (needspace && (   np->isNoStage
                             || np->DiskTota < XrdOlbConfig.DiskMin
                             || np->DiskFree < XrdOlbConfig.DiskAdj))
              {numf++; continue;}
           if (!sp) sp = np;
              else if (needspace)
                      {if (sp->RefA > (np->RefA+XrdOlbConfig.DiskLinger)) sp=np;}
                      else if (sp->RefR > np->RefR) sp=np;
          }

// Check for overloaded server and return result
//
   if (!sp) return calcDelay(nump, numd, numf, 0, nums, delay, reason);
   if (needspace) SelAcnt++;  // Protected by STMutex
      else        SelRcnt++;
   sp->Lock();
   delay = 0;
   return sp;
}
 
/******************************************************************************/
/*                             s e n d A L i s t                              */
/******************************************************************************/
  
// Single entry at a time, protected by STMutex!

void XrdOlbManager::sendAList(XrdNetLink *lp)
{
   static char *AltNext = AltMans;
   static struct iovec iov[4] = {{(caddr_t)"0 try ", 6}, 
                                 {0, 0},
                                 {AltMans, 0},
                                 {(caddr_t)"\n", 1}};
// Calculate what to send
//
   AltNext = AltNext + AltSize;
   if (AltNext >= AltMend)
      {AltNext = AltMans;
       iov[1].iov_len = 0;
       iov[2].iov_len = AltMend - AltMans;
      } else {
        iov[1].iov_base = (caddr_t)AltNext;
        iov[1].iov_len  = AltMend - AltNext;
        iov[2].iov_len  = AltNext - AltMans;
      }

// Send the list of alternates (rotated once)
//
   lp->Send(iov, 4);
}

/******************************************************************************/
/*                             s e t A l t M a n                              */
/******************************************************************************/
  
// Single entry at a time, protected by STMutex!
  
void XrdOlbManager::setAltMan(int snum, unsigned int ipaddr, int port)
{
   char *ap = &AltMans[snum*AltSize];
   int i;

// Preset the buffer and pre-screen the port number
//
   if (!port || (port > 0x0000ffff)) port = Port;
   memset(ap, int(' '), AltSize);

// Insert the ip address of this server into the list of servers
//
   i = XrdNetDNS::IP2String(ipaddr, port, ap, AltSize);
   ap[i] = ' ';

// Compute new fence
//
   if (ap >= AltMend) AltMend = ap + AltSize;
}
