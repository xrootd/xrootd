/******************************************************************************/
/*                                                                            */
/*                       X r d O l b S e r v e r . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbServerCVSID = "$Id$";
  
#include <limits.h>
#include <stdio.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdNet/XrdNetLink.hh"
#include "XrdOlb/XrdOlbCache.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbManList.hh"
#include "XrdOlb/XrdOlbMeter.hh"
#include "XrdOlb/XrdOlbPrepare.hh"
#include "XrdOlb/XrdOlbServer.hh"
#include "XrdOlb/XrdOlbState.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdOuc/XrdOuca2x.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdOlbCache    XrdOlbCache;

extern XrdOlbConfig   XrdOlbConfig;

extern XrdOucTrace    XrdOlbTrace;

extern XrdOucError    XrdOlbSay;

extern XrdOlbManager  XrdOlbSM;

extern XrdOlbState    XrdOlbSMon;

extern XrdNetLink    *XrdOlbRelay;

extern XrdOlbPrepare  XrdOlbPrepQ;

       XrdOlbManList  XrdOlbServer::myMans;

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

XrdOucMutex XrdOlbServer::mlMutex;

int         XrdOlbServer::xeq_load = 0;
int         XrdOlbServer::cpu_load = 0;
int         XrdOlbServer::mem_load = 0;
int         XrdOlbServer::pag_load = 0;
int         XrdOlbServer::net_load = 0;
int         XrdOlbServer::dsk_free = 0;
int         XrdOlbServer::dsk_tota = 0;
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOlbServer::XrdOlbServer(XrdNetLink *lnkp, int port)
{
    Link     =  lnkp;
    IPAddr   =  (lnkp ? lnkp->Addr() : 0);
    ServMask =  0;
    ServID   = -1;
    Instance =  0;
    isDisable=  0;
    isNoStage=  0;
    isOffline=  (lnkp == 0);
    isSuspend=  0;
    isActive =  0;
    isBound  =  0;
    isSpecial=  0;
    isMan    =  0;
    isKnown  =  0;
    myLoad   =  0;
    DiskFree =  0;
    DiskNums =  0;
    DiskTota =  0;
    DiskAskdl=  0;
    newload  =  1;
    Next     =  0;
    RefA     =  0;
    RefTotA  =  0;
    RefR     =  0;
    RefTotR  =  0;
    pingpong =  0;
    logload  =  XrdOlbConfig.LogPerf;
    DropTime =  0;
    DropJob  =  0;
    myName   =  0;
    Port     =  0; // setName() will set myName and Port!
    setName(lnkp->Name(), port);
    Stype    = (char *)"Server";
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOlbServer::~XrdOlbServer()
{
// Lock server
//
   Lock(); 

// If we are still an attached server, remove ourselves
//
   if (ServMask)
      {XrdOlbCache.Paths.Remove(ServMask);
       XrdOlbCache.Reset(ServID);
      }
   isOffline = 1;

// Recycle the link
//
   if (Link) Link->Recycle(); 
   Link = 0;

// Delete other appendages
//
   if (myName)    free(myName);

// All done
//
   UnLock();
}

/******************************************************************************/
/*                                 L o g i n                                  */
/******************************************************************************/
  
int XrdOlbServer::Login(int dataPort, int Status)
{
   XrdOlbPList *plp = XrdOlbConfig.PathList.First();
   long totfr, maxfr;
   char pbuff[16], buff[1280];

// Send a message is we are lost
//
   if (Status & OLB_Lost)
      Link->Send("msg cannot find usable manager; still searching.\n");

// Send a login request
//
   if (dataPort) sprintf(pbuff, "port %d", dataPort);
      else *pbuff = '\0';
   sprintf(buff, "login server %s %s %s +%c:%d\n", pbuff,
                 (Status & OLB_noStage ? "nostage" : ""),
                 (Status & OLB_Suspend ? "suspend" : ""),
                 (Status & OLB_isMan   ? 'm'       : 's'), 
                 XrdOlbConfig.PortTCP);
   if (Link->Send(buff) < 0) return -1;

// If this is a new manager, it will send us a ping or try response at this
// point. Try to receive the response but wait no more than 5 seconds.
//
   if (Link->OK2Recv(5000))
      {char *tp, id[16];
       if ((tp = Receive(id, sizeof(id))))
          if (!strcmp(tp, "try")) 
             {unsigned int ipaddr = Link->Addr();
              myMans.Del(ipaddr);
              while((tp = Link->GetToken()))
                   myMans.Add(ipaddr, tp, XrdOlbConfig.PortTCP);
              return 2;
             }
      }

// Send all of the addpath commands we need to
//
   while(plp)
        {if (Link->Send(buff, snprintf(buff, sizeof(buff)-1,
                        "addpath %s %s\n", plp->PType(), plp->Path())) < 0)
            return -1;
         plp = plp->Next();
        }

// Now issue a start. For supervisors use zero-knowledge info. A usage
// report will be automatically sent the moment we get some free space.
//
   if (XrdOlbConfig.Manager()) 
      {if (Link->Send("start 0 1 0\n") < 0) return -1;
      } else {
       maxfr = XrdOlbMeter::FreeSpace(totfr);
       if (Link->Send(buff, snprintf(buff, sizeof(buff)-1,
                      "start %ld %d %ld\n",
                      maxfr, XrdOlbMeter::numFS(), totfr)) < 0) return -1;
      }

// Document the login
//
   XrdOlbSay.Emsg("Server", "Logged into", Link->Name());
   return 0;
}

/******************************************************************************/
/*                      P r o c e s s _ D i r e c t o r                       */
/******************************************************************************/
  
void XrdOlbServer::Process_Director()
{
   char *tp, id[16];
   int retc;

// Simply keep reading commands from the director and process them
// The command sequence is <id> <command> <args>
//
   do {     if (!(tp = Receive(id, sizeof(id)))) retc = -1;
       else if (XrdOlbConfig.Disabled) retc = do_Delay(id);
       else if (!strcmp("select", tp)) retc = do_Select(id);
       else if (!strcmp("selects",tp)) retc = do_Select(id, 1);
       else if (!strcmp("prepadd",tp)) retc = do_PrepAdd(id);
       else if (!strcmp("prepdel",tp)) retc = do_PrepDel(id);
       else if (!strcmp("statsz", tp)) retc = do_Stats(id,0);
       else if (!strcmp("stats",  tp)) retc = do_Stats(id,1);
       else if (!strcmp("chmod",  tp)) retc = do_Chmod(id, 0);
       else if (!strcmp("mkdir",  tp)) retc = do_Mkdir(id, 0);
       else if (!strcmp("mv",     tp)) retc = do_Mv(id, 0);
       else if (!strcmp("rm",     tp)) retc = do_Rm(id, 0);
       else if (!strcmp("rmdir",  tp)) retc = do_Rmdir(id, 0);
       else retc = 1;
       if (retc > 0)
          XrdOlbSay.Emsg("Director", "invalid request from", Link->Name());
      } while(retc >= 0);

// If we got here, then the server connection was closed
//
   if (!isOffline)
     {isOffline = 1;
      if ((retc = Link->LastError()))
         XrdOlbSay.Emsg("Server", retc, "read request from", Link->Name());
     }
}

/******************************************************************************/
/*                      P r o c e s s _ R e q u e s t s                       */
/******************************************************************************/

// Process_Requests handles manager requests on the server's side. These
// requests will be forwarded if we have any subscribers.
//
int XrdOlbServer::Process_Requests()
{
   char *tp, id[16];
   int retc;

// If we are a line manager then we must synchronize our state with our manager
//
   if (XrdOlbConfig.Manager()) XrdOlbSMon.Sync(ServMask,1,1);

// Simply keep reading all requests until the link closes
//
   do {     if (!(tp = Receive(id, sizeof(id)))) retc = -1;
       else if (!strcmp("state",  tp)) retc = do_State(id, 0);
       else if (!strcmp("statf",  tp)) retc = do_State(id, 1);
       else if (!strcmp("ping",   tp)) retc = do_Ping(id);
       else if (!strcmp("prepadd",tp)) retc = do_PrepAdd(id,1);
       else if (!strcmp("prepdel",tp)) retc = do_PrepDel(id,1);
       else if (!strcmp("usage",  tp)) retc = do_Usage(id);
       else if (!strcmp("space",  tp)) retc = do_Space(id);
       else if (!strcmp("chmod",  tp)) retc = do_Chmod(id, 1);
       else if (!strcmp("mkdir",  tp)) retc = do_Mkdir(id, 1);
       else if (!strcmp("mv",     tp)) retc = do_Mv(id, 1);
       else if (!strcmp("rm",     tp)) retc = do_Rm(id, 1);
       else if (!strcmp("rmdir",  tp)) retc = do_Rmdir(id, 1);
       else if (!strcmp("try",    tp)) retc = do_Try(id);
       else retc = 1;
       if (retc > 0)
          XrdOlbSay.Emsg("Server", "invalid request from", Link->Name());
      } while(retc >= 0 && !isOffline);

// Check for permanent errors
//
   if (!isOffline)
      {isOffline = 1;
       if (retc < 0 && Link->LastError())
          XrdOlbSay.Emsg("Server", Link->LastError(), "read response from",
                         Link->Name());
      }

// All done
//
   return retc;
}
  
/******************************************************************************/
/*                     P r o c e s s _ R e s p o n s e s                      */
/******************************************************************************/
  
// Process_Responses handles server responses on the manager's side. This is
// a manager function and responses will be propogated up if we are subscribed.
//
int XrdOlbServer::Process_Responses()
{
   char *tp, id[16];
   int retc;

// For newly logged in servers, we need to sync the free space stats
//
   if (XrdOlbConfig.Manager())
      {mlMutex.Lock();
       if (isRW && DiskFree > dsk_free)
          {retc = dsk_free; dsk_free = DiskFree; dsk_tota = DiskTota;
           if (!retc)
              {char respbuff[128];
               XrdOlbSM.Inform("avkb", 4,
                               respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                               " %d %d\n", dsk_free, dsk_tota));
              }
          }
       mlMutex.UnLock();
      }

// Read all of the server's responses until eof then return
//
   do {     if (!(tp = Receive(id, sizeof(id)))) retc = -1;
       else if (!strcmp("have",    tp)) retc = do_Have(id);
       else if (!strcmp("gone",    tp)) retc = do_Gone(id);
       else if (!strcmp("pong",    tp)) retc = do_Pong(id);
       else if (!strcmp("load",    tp)) retc = do_Load(id);
       else if (!strcmp("avkb",    tp)) retc = do_AvKb(id);
       else if (!strcmp("suspend", tp)) retc = do_SuRes(id, 0);
       else if (!strcmp("resume",  tp)) retc = do_SuRes(id, 1);
       else if (!strcmp("stage",   tp)) retc = do_StNst(id, 1);
       else if (!strcmp("nostage", tp)) retc = do_StNst(id, 0);
       else if (!strcmp("rst",     tp)) retc = do_RST(id);
       else retc = 1;
       if (retc > 0)
          XrdOlbSay.Emsg("Server", "invalid response from", myName);
      } while(retc >= 0 && !isOffline);

// Check for permanent errors
//
   if (!isOffline)
      {isOffline = 1;
       if (retc < 0 && Link->LastError())
          XrdOlbSay.Emsg("Server", Link->LastError(), "read response from",
                        Link->Name());
      }

// All done
//
   return retc;
}

/******************************************************************************/
/*                                R e s u m e                                 */
/******************************************************************************/

int XrdOlbServer::Resume(XrdOlbPrepArgs *pargs)
{
    return do_PrepSel(pargs, *(pargs->reqid) != '*');
}
  
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdOlbServer::Send(const char *buff, int blen)
{
    return (isOffline ? -1 : Link->Send(buff, blen));
}

int  XrdOlbServer::Send(const struct iovec iov[], int iovcnt)
{
    return (isOffline ? -1 : Link->Send(iov, iovcnt));
}

/******************************************************************************/
/*                               s e t N a m e                                */
/******************************************************************************/
  
void XrdOlbServer::setName(const char *hname, int port)
{
   char buff[512];

   if (myName)
      if (port == Port) return;
         else free(myName);

   if (!port) myName = strdup(hname);
      else {sprintf(buff, "%s:%d", hname, port); myName = strdup(buff);}
   Port = port;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/* MANAGER LOCAL:                d o _ A v K b                                */
/******************************************************************************/
  
// Server responses to space usage requests are localized to the cell and need
// not be propopagated in any direction.
//
int XrdOlbServer::do_AvKb(char *rid)
{
    char *tp;
    int fdsk;

// Process: <id> avkb <fsdsk> <totdsk>
//
   if (!(tp = Link->GetToken())
   || XrdOuca2x::a2i(XrdOlbSay, "fs kb value",  tp, &fdsk, 0))
      return 1;
   DiskTota = DiskFree = fdsk;
   if ((tp = Link->GetToken()))
      if (XrdOuca2x::a2i(XrdOlbSay, "tot kb value",  tp, &fdsk, 0))
         return 1;
         else DiskTota = fdsk;
   return 0;
}
  
/******************************************************************************/
/* SERVER:                      d o _ C h m o d                               */
/******************************************************************************/
  
// Manager requests to do a chmod must be forwarded to all subscribers.
//
int XrdOlbServer::do_Chmod(char *rid, int do4real)
{
   EPNAME("do_Chmod")
   char *tp, modearg[16];
   char lclpath[XrdOlbMAX_PATH_LEN+1];
   mode_t mode;
   int rc;

// Process: <id> chmod <mode> <path>
// Respond: n/a
//
   if (!(tp = Link->GetToken())
   || strlcpy(modearg, tp, sizeof(modearg)) >= sizeof(modearg))
      {XrdOlbSay.Emsg("Server", "Mode too long in chmod", tp);
       return 0;
      }

// Get the target name
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "chmod path not specified");
       return 0;
      }

// If are a manager then broadcast the request to every server that might
// might be able to do this operation, then check if we should do it as well.
//
   if (XrdOlbSM.ServCnt) Reissue(rid, "chmod ", modearg, tp);
   if (XrdOlbSM.noData) return 0;

// Convert the mode
//
   if (!(mode = strtol(modearg, 0, 8)))
      {XrdOlbSay.Emsg("Server", "Invalid mode in chmod", modearg, tp);
       return 0;
      }

// Generate the true local path
//
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(tp, lclpath)) return 0;
         else tp = lclpath;

// Attempt to change the mode
//
   if (XrdOlbConfig.ProgCH) rc = XrdOlbConfig.ProgCH->Run(modearg, tp);
      else if (chmod(tp, mode)) rc = errno;
              else rc = 0;
   if (rc && rc != ENOENT)
       XrdOlbSay.Emsg("Server", errno, "change mode for", tp);
       else DEBUG("rc=" <<rc <<" chmod " <<std::oct <<mode <<std::dec <<' ' <<tp);
   return 0;
}

/******************************************************************************/
/*                              d o _ D e l a y                               */
/******************************************************************************/
  
int XrdOlbServer::do_Delay(char *rid)
{   char respbuff[64];

   return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                               "%s !wait %d\n", rid, XrdOlbConfig.SUPDelay));
}
/******************************************************************************/
/* MANAGER:                      d o _ G o n e                                */
/******************************************************************************/

// When a manager receives a gone request it is propogated if we are subscribed
// and we have not sent a gone request in the immediate past.
//
int XrdOlbServer::do_Gone(char *rid)
{
   char *tp;
   int   newgone = 0;

// Process: <id> gone <path>
//
   if (!(tp = Link->GetToken())) return 1;

// Update path information (we are not sure we should delete this from the
// prep queue is we are functioning both manager and server, sugh -- we'll see)
//
   if (Instance) newgone = XrdOlbCache.DelFile(tp, ServMask);
      else XrdOlbSay.Emsg("Server", "gone request ignored from", Name());
   if (XrdOlbConfig.DiskSS) XrdOlbPrepQ.Gone(tp);

// If we have no managers and we still have the file or never had it, return
//
   if (!XrdOlbSM.haveManagers() || !newgone) return 0;

// Back-propogate the gone to all of our managers
//
   XrdOlbSM.Inform("gone ", 5, tp);

// All done
//
   return 0;
}

/******************************************************************************/
/* MANAGER:                      d o _ H a v e                                */
/******************************************************************************/
  
// When a manager receives a have request it is propogated if we are subscribed
// and we have not sent a have request in the immediate past.
//
int XrdOlbServer::do_Have(char *rid)
{
   char *cmd, *tp;
   int isnew, isrw;

// Process: <id> have {r | w | ?} <path>
//
   if (!(tp = Link->GetToken())) return 1;
   if (*tp == 'r') isrw = 0;
      else if (*tp == 'w') isrw = 1;
              else isrw = -1;
   if (!(tp = Link->GetToken())) return 1;

// Update path information
//
   if (Instance) isnew = XrdOlbCache.AddFile(tp, ServMask, isrw);
      else {XrdOlbSay.Emsg("Server", "have request ignored from", Name());
            return 0;
           }

// Return if we have no managers or we already informed the managers
//
   if (!XrdOlbSM.haveManagers() || !isnew) return 0;

// Back-propogate the have to all of our managers
//
   if (!isrw) cmd = (char *)"have r ";
      else if (isrw < 0) cmd = (char *)"have ? ";
              else cmd = (char *)"have w ";
   XrdOlbSM.Inform(cmd, 7, tp);

// All done
//
   return 0;
}
  
/******************************************************************************/
/* MANAGER LOCAL:                d o _ L o a d                                */
/******************************************************************************/
  
// Server responses to usage requests are local to the cell and never propagated.
//
int XrdOlbServer::do_Load(char *rid)
{
    char *tp;
    int temp, pcpu, pio, pload, pmem, ppag, fdsk, tdsk = -1;

// Process: <id> load <cpu> <io> <load> <mem> <pag> <dsk>
//
   if (!(tp = Link->GetToken())) return 1;
   if (XrdOuca2x::a2i(XrdOlbSay, "cpu value",  tp, &pcpu, 0, 100))
      return 1;
   if (!(tp = Link->GetToken())) return 1;
   if (XrdOuca2x::a2i(XrdOlbSay, "io value",   tp, &pio,  0, 100))
      return 1;
   if (!(tp = Link->GetToken())) return 1;
   if (XrdOuca2x::a2i(XrdOlbSay, "load value", tp, &pload,0, 100))
      return 1;
   if (!(tp = Link->GetToken())) return 1;
   if (XrdOuca2x::a2i(XrdOlbSay, "mem value",  tp, &pmem, 0, 100))
      return 1;
   if (!(tp = Link->GetToken())) return 1;
   if (XrdOuca2x::a2i(XrdOlbSay, "pag value",  tp, &ppag, 0, 100))
      return 1;
   if (!(tp = Link->GetToken())) return 1;
   if (XrdOuca2x::a2i(XrdOlbSay, "fs dsk value",  tp, &fdsk, 0))
      return 1;
   if ((tp = Link->GetToken())
   && XrdOuca2x::a2i(XrdOlbSay, "tot dsk value",  tp, &tdsk, 0))
      return 1;

// Compute actual load value
//
   myLoad = (XrdOlbConfig.P_cpu  * pcpu /100)
          + (XrdOlbConfig.P_io   * pio  /100)
          + (XrdOlbConfig.P_load * pload/100)
          + (XrdOlbConfig.P_mem  * pmem /100)
          + (XrdOlbConfig.P_pag  * ppag /100);
   DiskFree = fdsk;
   DiskTota = (tdsk >= 0 ? tdsk : fdsk);
   newload = 1;

// If we are also a manager then use this load figure to come up with
// an overall load to report when asked. If we get free space, then we
// must report that now so that we can be selected for allocation.
//
   if (XrdOlbConfig.Manager())
      {mlMutex.Lock();
       temp = cpu_load + cpu_load/2;
       cpu_load = (cpu_load + (pcpu > temp ? temp : pcpu ))/2;
       temp = net_load + net_load/2;
       net_load = (net_load + (pio  > temp ? temp : pio  ))/2;
       temp = xeq_load + xeq_load/2;
       xeq_load = (xeq_load + (pload> temp ? temp : pload))/2;
       temp = mem_load + mem_load/2;
       mem_load = (mem_load + (pmem > temp ? temp : pmem ))/2;
       temp = pag_load + pag_load/2;
       pag_load = (pag_load + (ppag > temp ? temp : ppag ))/2;
       if (isRW && DiskFree > dsk_free)
          {temp = dsk_free; dsk_free = DiskFree; dsk_tota = DiskTota;
           if (!temp)   
              {char respbuff[128];
               XrdOlbSM.Inform("avkb", 4,
                               respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                               " %d %d\n", dsk_free, dsk_tota));
              }
          }
       mlMutex.UnLock();
      }

// Report new load if need be
//
   if (!XrdOlbConfig.LogPerf || logload--) return 0;

// Log it now
//
   {char buff[1024];
    snprintf(buff, sizeof(buff)-1,
            "load=%d; cpu=%d i/o=%d inq=%d mem=%d pag=%d dsk=%d tot=%d",
            myLoad, pcpu, pio, pload, pmem, ppag, fdsk, tdsk);
    XrdOlbSay.Emsg("Server", Name(), buff);
    if ((logload = XrdOlbConfig.LogPerf)) logload--;
   }

   return 0;
}
  
/******************************************************************************/
/* SERVER:                      d o _ M k d i r                               */
/******************************************************************************/
  
// Manager requests to do a mkdir must be forwarded to all subscribers.
//
int XrdOlbServer::do_Mkdir(char *rid, int do4real)
{
   EPNAME("do_Mkdir";)
   char *tp, modearg[16];
   char lclpath[XrdOlbMAX_PATH_LEN+1];
   mode_t mode;
   int rc;

// Process: <id> mkdir <mode> <path>
// Respond: n/a
//
   if (!(tp = Link->GetToken())
   || strlcpy(modearg, tp, sizeof(modearg)) >= sizeof(modearg))
      {XrdOlbSay.Emsg("Server", "Mode too long in mkdir", tp);
       return 0;
      }

// Get the directory name
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "mkdir directory not specified");
       return 0;
      }

// If we have subsscribers then broadcast the request to every server that
// might be able to do this operation
//
   if (XrdOlbSM.ServCnt) Reissue(rid, "mkdir ", modearg, tp);
   if (XrdOlbSM.noData) return 0;

// Convert the mode
//
   if (!(mode = strtol(modearg, 0, 8)))
      {XrdOlbSay.Emsg("Server", "Invalid mode in mkdir", modearg, tp);
       return 0;
      }

// Generate the true local path
//
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(tp, lclpath)) return 0;
         else tp = lclpath;

// Attempt to create the directory
//
   if (XrdOlbConfig.ProgMD) rc = XrdOlbConfig.ProgMD->Run(modearg, tp);
      else if (mkdir(tp, mode)) rc = errno;
              else rc = 0;
   if (rc) XrdOlbSay.Emsg("Server", rc, "create directory", tp);
      else DEBUG("rc=" <<rc <<" mkdir " <<std::oct <<mode <<std::dec <<' ' <<tp);
   return 0;
}
  
/******************************************************************************/
/* SERVER:                         d o _ M v                                  */
/******************************************************************************/
  
// Manager requests to do an mv must be forwarded to all subscribers.
//
int XrdOlbServer::do_Mv(char *rid, int do4real)
{
   EPNAME("do_Mv")
   char *tp;
   char old_lclpath[XrdOlbMAX_PATH_LEN+1];
   char new_lclpath[XrdOlbMAX_PATH_LEN+1];
   int rc;

// Process: <id> mv <old_name> <new_name>
// Respond: n/a

// Get the old name
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "mv old path not specified");
       return 0;
      }

// Generate proper old path name
//
   if (do4real && XrdOlbConfig.LocalRLen)
      {if (XrdOlbConfig.GenLocalPath(tp, old_lclpath))
          return 0;
      }
      else if (strlcpy(old_lclpath,tp,sizeof(old_lclpath)) > XrdOlbMAX_PATH_LEN)
              {XrdOlbSay.Emsg("Server", "mv old path too long", tp);
               return 0;
              }

// Get the new name
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "mv new path not specified");
       return 0;
      }

// If we have subscribers then broadcast the request to every server that
// might be able to do this operation
//
   if (XrdOlbSM.ServCnt) Reissue(rid, "mv ", 0, old_lclpath, tp);
   if (XrdOlbSM.noData) return 0;

// Generate the true local path for new name
//
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(tp, new_lclpath)) return 0;
         else tp = new_lclpath;

// Attempt to rename the file
//
   if (XrdOlbConfig.ProgMV) rc = XrdOlbConfig.ProgMV->Run(old_lclpath, tp);
      else if (rename(old_lclpath, tp)) rc = errno;
              else rc = 0;
   if (rc) XrdOlbSay.Emsg("Server", rc, "rename", old_lclpath);
      else DEBUG("rc=" <<rc <<" mv " <<old_lclpath <<' ' <<tp);

   return 0;
}
  
/******************************************************************************/
/* SERVER LOCAL:                 d o _ P i n g                                */
/******************************************************************************/
  
// Ping requests from a manager are local to the cell and never propagated.
//
int XrdOlbServer::do_Ping(char *rid)
{
    char respbuff[64];

// Process: <id> ping
// Respond: <id> pong
//
   return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                               "%s pong\n", rid));
}
  
/******************************************************************************/
/*                            d o _ P r e p A d d                             */
/******************************************************************************/
  
int XrdOlbServer::do_PrepAdd(char *rid, int server)
{
    XrdOlbPrepArgs pargs;
    char *tp;

// Process: <id> prepadd <reqid> <usr> <prty> <mode> <path>\n
// Respond: No response.
//

// Get the request id
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "no prep request id from", Name());
       return 1;
      }
   pargs.reqid = strdup(tp);

// Get userid
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "no prep user from", Name());
       return 1;
      }
   pargs.user = strdup(tp);

// Get priority
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "no prep prty from", Name());
       return 1;
      }
   pargs.prty = strdup(tp);

// Get mode
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "no prep mode from", Name());
       return 1;
      }
   pargs.mode = strdup(tp);

// Get path
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "no prep path from", Name());
       return 1;
      }
   pargs.path = strdup(tp);

// Process accroding to whether or not we are the endpoint
//
   if (server) return do_PrepAdd4Real(pargs);

// If this is just a background lookup, we can save a lot of time
//
   if (*pargs.reqid == '*') {do_PrepSel(&pargs,0); return 0;}

// Allocate storage for the iovec structure array
//
   struct iovec *iovp = (struct iovec *)malloc(sizeof(struct iovec)*12);

// Create prepare message to be used when a server is selected:
// <mid> prepadd <reqid> <usr> <prty> <mode> <path>\n
//
   iovp[ 0].iov_base = XrdOlbConfig.MsgGID; 
   iovp[ 0].iov_len  = XrdOlbConfig.MsgGIDL;
   iovp[ 1].iov_base = (char *)"prepadd "; iovp[ 1].iov_len  = 8;
   iovp[ 2].iov_base = pargs.reqid;  iovp[ 2].iov_len  = strlen(pargs.reqid);
   iovp[ 3].iov_base = (char *)" ";  iovp[ 3].iov_len  = 1;
   iovp[ 4].iov_base = pargs.user;   iovp[ 4].iov_len  = strlen(pargs.user);
   iovp[ 5].iov_base = (char *)" ";  iovp[ 5].iov_len  = 1;
   iovp[ 6].iov_base = pargs.prty;   iovp[ 6].iov_len  = strlen(pargs.prty);
   iovp[ 7].iov_base = (char *)" ";  iovp[ 7].iov_len  = 1;
   iovp[ 8].iov_base = pargs.mode;   iovp[ 8].iov_len  = strlen(pargs.mode);
   iovp[ 9].iov_base = (char *)" ";  iovp[ 9].iov_len  = 1;
   iovp[10].iov_base = pargs.path;   iovp[10].iov_len  = strlen(pargs.path);
   iovp[11].iov_base = (char *)"\n"; iovp[11].iov_len  = 1;

// Since the pargs is schedulable, it will be deleted asynchronously
//
   XrdOlbPrepArgs *nargs = new XrdOlbPrepArgs;
   *nargs = pargs;
   nargs->iovp = iovp; nargs->iovn = 12;  // See above!
   pargs.Clear();
   do_PrepSel(nargs,1);
   return 0;
}

/******************************************************************************/
/*                       d o _ P r e p A d d 4 R e a l                        */
/******************************************************************************/
  
int XrdOlbServer::do_PrepAdd4Real(XrdOlbPrepArgs &pargs)
{
   EPNAME("do_PrepAdd4Real")
    char  *oldpath, lclpath[XrdOlbMAX_PATH_LEN+1];

// Generate the true local path
//
   oldpath = pargs.path;
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(pargs.path,lclpath)) return 0;
         else pargs.path = lclpath;

// Check if this file is not online, prepare it
//
   if (!isOnline(pargs.path))
      {DEBUG("Preparing " <<pargs.reqid <<' ' <<pargs.user <<' ' <<pargs.prty
                          <<' ' <<pargs.mode <<' ' <<pargs.path);
       if (!XrdOlbConfig.DiskSS)
          XrdOlbSay.Emsg("Server", "staging disallowed; ignoring prep",
                          pargs.user, pargs.reqid);
          else XrdOlbPrepQ.Add(pargs);
       pargs.path = oldpath;
       return 0;
      }

// File is already online, so we are done
//
   pargs.path = oldpath;
   Inform("avail", &pargs);
   return 0;
}
  
/******************************************************************************/
/*                            d o _ P r e p D e l                             */
/******************************************************************************/
  
int XrdOlbServer::do_PrepDel(char *rid, int server)
{
   EPNAME("do_PrepDel")
   char *tp;
   SMask_t amask = (SMask_t)-1;
   BUFF(2048);

// Process: <id> prepcan <reqid>
// Respond: No response.
//

// Get the request id
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "no prepcan request id from", Name());
       return 1;
      }

// If this is a server call, do it for real
//
   if (server)
      {if (!XrdOlbConfig.DiskSS) {DEBUG("Ignoring cancel prepare " <<tp);}
          else {DEBUG("Canceling prepare " <<tp);
                XrdOlbPrepQ.Del(tp);
               }
       return 0;
      }

// Cancel the request. Since we don't know where this went, inform all
// subscribers about this cancellation.
//
  DEBUG("Canceling prepare " <<tp);
  XrdOlbSM.Broadcast(amask, buff, snprintf(buff, sizeof(buff)-1,
                     "%s prepdel %s\n", XrdOlbConfig.MsgGID, tp));
  return 0;
}
  
/******************************************************************************/
/*                            d o _ P r e p S e l                             */
/******************************************************************************/
  
int XrdOlbServer::do_PrepSel(XrdOlbPrepArgs *pargs, int stage)  // This is static!!
{
   EPNAME("do_PrepSel")
   extern XrdOlbScheduler XrdOlbSched;
   BUFF(2048);
   XrdOlbPInfo pinfo;
   XrdOlbCInfo cinfo;
   char ptc, hbuff[512];
   int retc, needrw;
   SMask_t amask, smask, pmask;

// Determine mode
//
   if (index(pargs->mode, (int)'w'))
           {needrw = 1; ptc = 'w';}
      else {needrw = 0; ptc = 'r';}

// Find out who serves this path
//
   if (!XrdOlbCache.Paths.Find(pargs->path, pinfo)
   || (amask = (needrw ? pinfo.rwvec : pinfo.rovec)) == 0)
      {DEBUG("Path find failed for " <<pargs->reqid <<' ' <<ptc <<' ' <<pargs->path);
       return Inform("unavail", pargs);
      }

// First check if we have seen this file before. If not, broadcast a lookup
// to all relevant servers. Note that even if the caller wants the file in
// r/w mode we will ask both r/o and r/w servers for the file.
//
   if (!(retc = XrdOlbCache.GetFile(pargs->path, cinfo)) 
   ||  cinfo.deadline || (cinfo.sbvec != 0))
      {if (!retc)
          {DEBUG("Searching for " <<pargs->path);
           XrdOlbCache.AddFile(pargs->path, 0, 0, XrdOlbConfig.LUPDelay);
           XrdOlbSM.Broadcast(pinfo.rovec, buff, snprintf(buff, sizeof(buff)-1,
                           "%s state %s\n", XrdOlbConfig.MsgGID, pargs->path));
          } else {
           if (cinfo.sbvec != 0)         // Bouncing server
              {XrdOlbCache.DelFile(pargs->path,cinfo.sbvec,XrdOlbConfig.LUPDelay);
               XrdOlbSM.Broadcast(cinfo.sbvec, buff, snprintf(buff,sizeof(buff)-1,
                                "%s state %s\n",XrdOlbConfig.MsgGID,pargs->path));
              }
          }
       if (!stage) return 0;
       DEBUG("Rescheduling lookup in " <<XrdOlbConfig.LUPDelay <<" seconds");
       XrdOlbSched.Schedule((XrdOlbJob *)pargs,XrdOlbConfig.LUPDelay+time(0));
       return 1;
      }

// Compute the primary and secondary selections:
// Primary:   Servers who already have the file
// Secondary: Servers who don't have the file but can stage it in
//
   pmask = (needrw ? cinfo.rwvec : cinfo.rovec);
   smask = amask & pinfo.ssvec;   // Alternate selection

// Select a server
//
   if (!(pmask | smask)) retc = -1;
      else if (!(retc = XrdOlbSM.SelServer(needrw, pargs->path, pmask, smask, 
                        hbuff, pargs->iovp,pargs->iovn)))
              {DEBUG(hbuff <<" prepared " <<pargs->reqid <<' ' <<pargs->path);
               return 0;
              }

// We failed check if we should reschedule this
//
   if (retc > 0)
      {XrdOlbSched.Schedule((XrdOlbJob *)pargs, retc+time(0));
       DEBUG("Prepare delayed " <<retc <<" seconds");
       return 1;
      }

// We failed, terminate the request
//
   DEBUG("No servers available to prepare "  <<pargs->reqid <<' ' <<pargs->path);
   Inform("unavail", pargs);
   return 0;
}
  
/******************************************************************************/
/* MANAGER LOCAL:                d o _ P o n g                                */
/******************************************************************************/
  
// Server responses to a ping are local to the cell and never propagated.
//
int XrdOlbServer::do_Pong(char *rid)
{
// Process: <id> pong
// Reponds: n/a

// Simply indicate a ping has arrived.
//
   pingpong = 1;
   return 0;
}
  
/******************************************************************************/
/* SERVER:                         d o _ R m                                  */
/******************************************************************************/
  
// Manager requests to do an rm must be forwarded to all subscribers.
//
int XrdOlbServer::do_Rm(char *rid, int do4real)
{
   EPNAME("do_Rm")
   char *tp;
   char lclpath[XrdOlbMAX_PATH_LEN+1];
   int rc;

// Process: <id> rm <path>
// Respond: n/a

// Get the path
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "rm path not specified");
       return 0;
      }

// If we have subscribers then broadcast the request to every server that
// might be able to do this operation
//
   if (XrdOlbSM.ServCnt) Reissue(rid, "rm ", 0, tp);
   if (XrdOlbSM.noData) return 0;

// Generate the true local path for name
//
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(tp, lclpath)) return 0;
         else tp = lclpath;

// Attempt to remove the file
//
   if (XrdOlbConfig.ProgRM) rc = XrdOlbConfig.ProgRM->Run(tp);
      else if (unlink(tp)) rc = errno;
              else rc = 0;
   if (rc && rc != ENOENT) XrdOlbSay.Emsg("Server", rc, "remove", tp);
      else {DEBUG("rc=" <<rc <<" rm " <<tp);}

   return 0;
}
  
/******************************************************************************/
/* SERVER:                      d o _ R m d i r                               */
/******************************************************************************/
  
// Manager requests to do an rmdir must be forwarded to all subscribers.
//
int XrdOlbServer::do_Rmdir(char *rid, int do4real)
{
   EPNAME("do_Rmdir")
   char lclpath[XrdOlbMAX_PATH_LEN+1];
   char *tp;
   int rc;

// Process: <id> rmdir <path>
// Respond: n/a

// Get the path
//
   if (!(tp = Link->GetToken()))
      {XrdOlbSay.Emsg("Server", "rmdir path not specified");
       return 0;
      }

// If we have subscribers then broadcast the request to every server that
// might be able to do this operation
//
   if (XrdOlbSM.ServCnt) Reissue(rid, "rmdir ", 0, tp);
   if (XrdOlbSM.noData) return 0;

// Generate the true local path for name
//
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(tp, lclpath)) return 0;
         else tp = lclpath;

// Attempt to remove the directory
//
   if (XrdOlbConfig.ProgRD) rc = XrdOlbConfig.ProgRD->Run(tp);
      else if (rmdir(tp)) rc = errno;
              else rc = 0;
   if (rc && errno != ENOENT) XrdOlbSay.Emsg("Server", errno, "remove", tp);
      else DEBUG("rc=" <<errno <<" rmdir " <<tp);

   return 0;
}

/******************************************************************************/
/* MANAGER:                       d o _ R S T                                 */
/******************************************************************************/

// An rst response is received when a subscribed supervisor adds a new server.
// This causes all cache lines for the supervisor to be marked suspect. Also,
// the RST request is propagated to all of our managers.
//
int XrdOlbServer::do_RST(char *rid)
{

// First propagate the RST to our managers
//
   XrdOlbSM.Reset();

// Now invalidate our cache lines
//
   XrdOlbCache.Bounce(ServMask);
   return 0;
}
  
/******************************************************************************/
/* MANAGER LOCAL:              d o _ S e l e c t                              */
/******************************************************************************/
  
// A select request comes from a redirector and is handled locally within the
// cell. This may cause "state" requests to be broadcast to subscribers.
//
int XrdOlbServer::do_Select(char *rid, int refresh)
{
   EPNAME("do_Select")
   BUFF(2048);
   XrdOlbPInfo pinfo;
   XrdOlbCInfo cinfo;
   char *tp, *amode, ptc, hbuff[512];
   int dowt = 0, retc, needrw, resonly = 0, newfile = 0;
   SMask_t amask, smask, pmask;

// Process: <id> select[s] {c | r | w | x] <path>
// Reponds: ?err  <msg>
//          !try  <host>
//          !wait <sec>

// Pick up Parameters
//
   if (!(tp = Link->GetToken()) || strlen(tp) != 1) return 1;
   ptc = *tp;
   switch(*tp)
        {case 'r': needrw = 0; amode = (char *)"read";  break;
         case 'w': needrw = 1; amode = (char *)"write"; break;
         case 'c': needrw = 1; amode = (char *)"write"; newfile = 1; break;
         case 'x': needrw = 0; amode = (char *)"read";  resonly = 1; break;
         default:  return 1;
        }
   if (!(tp = Link->GetToken())) return 1;

// Find out who serves this path
//
   if (!XrdOlbCache.Paths.Find(tp, pinfo)
   || (amask = (needrw ? pinfo.rwvec : pinfo.rovec)) == 0)
      {Link->Send(buff, snprintf(buff, sizeof(buff)-1,
             "%s ?err No servers have %s access to the file", rid, amode));
       DEBUG("Path find failed for select " <<ptc <<' ' <<tp);
       return 0;
      }

// First check if we have seen this file before. If so, get primary selections.
//
   if (refresh) {retc = 0; pmask = 0;}
      else if (!(retc = XrdOlbCache.GetFile(tp, cinfo))) pmask = 0;
              else pmask = (needrw ? cinfo.rwvec : cinfo.rovec);

// We didn't find the file or a refresh is wanted (easy case). Client must wait.
//
   if (!retc)
      {XrdOlbCache.AddFile(tp, 0, 0, XrdOlbConfig.LUPDelay);
       XrdOlbSM.Broadcast(pinfo.rovec, buff, snprintf(buff, sizeof(buff)-1,
                          "%s stat%c %s\n", XrdOlbConfig.MsgGID,
                          (refresh ? 'f' : 'e'), tp));
       dowt = 1;
      } else

// File was found but either a query is in progress (client must wait)
// or we have a server bounce (client waits if not alternative is available).
//
      {if (cinfo.sbvec != 0)         // Bouncing server
          {XrdOlbCache.DelFile(tp, cinfo.sbvec, XrdOlbConfig.LUPDelay);
           XrdOlbSM.Broadcast(cinfo.sbvec,buff,snprintf(buff,sizeof(buff)-1,
                              "%s state %s\n", XrdOlbConfig.MsgGID, tp));
           dowt = (pmask == 0);
          }
       if (cinfo.deadline) dowt = 1; // Query in progress
      }

// If the client has to wait now, delay the client and return
//
   if (dowt)
      {Link->Send(buff,sprintf(buff,"%s !wait %d\n",rid,XrdOlbConfig.LUPDelay));
       DEBUG("Lookup delay " <<Name() <<' ' <<XrdOlbConfig.LUPDelay);
       return 0;
      }

// Compute the primary and secondary selections:
// Primary:   Servers who already have the file (computed above)
// Secondary: Servers who don't have the file but can get it
//
   if (resonly) smask = 0;
      else smask = (newfile ? pinfo.rwvec : amask & pinfo.ssvec); // Alt selection

// Select a server
//
   if (!(pmask | smask)) retc = -1;
      else if (!(retc = XrdOlbSM.SelServer(needrw, tp, pmask, smask, hbuff)))
              {Link->Send(buff,sprintf(buff, "%s !try %s\n", rid, hbuff));
               DEBUG("Redirect " <<Name() <<" -> " <<hbuff <<" for " <<tp);
               return 0;
              }

// We failed and must delay or terminate the request
//
   if (retc > 0)
      {Link->Send(buff, sprintf(buff, "%s !wait %d\n", rid, retc));
       DEBUG("Select delay " <<Name() <<' ' <<retc);
      } else {
       Link->Send(buff, snprintf(buff, sizeof(buff)-1,
             "%s ?err No servers are available to %s the file.\n", rid, amode));
       DEBUG("No servers available to " <<ptc <<' ' <<tp);
      }

// All done
//
   return 0;
}
  
/******************************************************************************/
/* SERVER:                      d o _ S p a c e                               */
/******************************************************************************/
  
// Manager space requests are local to the cell and never propagated.
//
int XrdOlbServer::do_Space(char *rid)
{
   char respbuff[128];
   long maxfr, totfr;

// Process: <id> space
// Respond: <id> avkb  <numkb>
//
   if (XrdOlbConfig.Manager())
      return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                        "%s avkb %d %d\n", rid, dsk_free, dsk_tota));

   maxfr = XrdOlbMeter::FreeSpace(totfr);
   return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                "%s avkb %ld %ld\n", rid, maxfr, totfr));
}
  
/******************************************************************************/
/* SERVER:                      d o _ S t a t e                               */
/******************************************************************************/
  
// State requests from a manager are rebroadcast to all relevant subscribers.
//
int XrdOlbServer::do_State(char *rid,int reset)
{
   char *pp, *tp, respbuff[2048];
   char lclpath[XrdOlbMAX_PATH_LEN+1];

// Process: <id> state <path>
//          <id> statf <path>
// Respond: <id> {gone | have {r | w | s | ?}} <path>
//
   if (!(tp = Link->GetToken())) return 1;
   isKnown = 1;

// If we are a manager then check for the file in the local cache
//
   if (isMan && do_StateFWD(tp, reset))
      return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
             "%s have %s %s\n", rid, XrdOlbConfig.PathList.Type(tp), tp));
   if (XrdOlbSM.noData) return 0;

// Generate the true local path
//
   if (XrdOlbConfig.LocalRLen)
      if (XrdOlbConfig.GenLocalPath(tp, lclpath)) return 0;
         else pp = lclpath;
      else pp = tp;

// Do a stat, respond if we have the file
//
   if (isOnline(pp, 0))
      return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
             "%s have %s %s\n", rid, XrdOlbConfig.PathList.Type(tp), tp));
   return 0;
}
  
/******************************************************************************/
/* SUPER:                    d o _ S t a t e F W D                            */
/******************************************************************************/
  
int XrdOlbServer::do_StateFWD(char *tp, int reset)
{
   EPNAME("do_StateFWD");
   BUFF(2048);
   XrdOlbPInfo pinfo;
   XrdOlbCInfo cinfo;
   int docr, dowt, retc;
   SMask_t pmask = 0;

// Find out who serves this path
//
   if (!XrdOlbCache.Paths.Find(tp, pinfo) || pinfo.rovec == 0)
      {DEBUG("Path find failed for state " <<tp);
       return 0;
      }

// First check if we have seen this file before. If so, get primary selections.
//
   if (reset) retc = 0;
      else if ((retc = XrdOlbCache.GetFile(tp, cinfo))) pmask = cinfo.rovec;

// If we didn't find the file, or it's being searched for, then return failure.
// Otherwise, we will ask the relevant servers if they have the file. We return
//
   dowt = (!retc || cinfo.deadline);
   docr = (retc && (cinfo.sbvec != 0));
   if (dowt || docr)
      {if (!retc) XrdOlbCache.AddFile(tp, 0, 0, XrdOlbConfig.LUPDelay);
          else XrdOlbCache.DelFile(tp, cinfo.sbvec,
                                  (dowt ? XrdOlbConfig.LUPDelay : 0));
       XrdOlbSM.Broadcast((retc ? cinfo.sbvec : pinfo.rovec), buff,
                          snprintf(buff, sizeof(buff)-1,
                          "%s stat%c %s\n", XrdOlbConfig.MsgGID, 
                          (reset ? 'f' : 'e'), tp));
      }

// Return true if anyone has the file at this point
//
   return (pmask != 0);
}

/******************************************************************************/
/* ANY:                         d o _ S t a t s                               */
/******************************************************************************/
  
// We punt on stats requests as we have no way to export them anyway.
//
int XrdOlbServer::do_Stats(char *rid, int full)
{
   static XrdOucMutex StatsData;
   static int    statsz = 0;
   static int    statln = 0;
   static char  *statbuff = 0;
   static time_t statlast = 0;
   int rc;
   time_t tNow;

// Allocate buffer if we do not have one
//
   StatsData.Lock();
   if (!statsz || !statbuff)
      {statsz    = XrdOlbSM.Stats(0,0);
       statbuff = (char *)malloc(statsz);
      }

// Check if only the size is wanted
//
   if (!full)
      {char respbuff[32];
       StatsData.UnLock();
       return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                                   "%d\n", statsz));
      }

// Get full statistics if enough time has passed
//
   tNow = time(0);
   if (statlast+9 >= tNow)
      {statln = XrdOlbSM.Stats(statbuff, statsz); statlast = tNow;}

// Send response
//
   if (statln) rc = Link->Send(statbuff, statln);
      else     rc = Link->Send("\n", 1);

// All done
//
   StatsData.UnLock();
   return rc;
}
  
/******************************************************************************/
/* MANAGER:                     d o _ S t N s t                               */
/******************************************************************************/
  
// When a manager receives a stage/nostage request, the result is propagated
// to upper-level managers only if the summary state has changed.
//
int XrdOlbServer::do_StNst(char *rid, int Stage)
{
    char *why;

    if (Stage)
       {if (!isNoStage) return 0;
        isNoStage = 0;
        if (!isOffline && !isDisable) why = (char *)"staging resumed";
           else why = isOffline ? (char *)"offlined" : (char *)"disabled";
       } else if (isNoStage) return 0;
                 else {why = (char *)"staging suspended"; isNoStage = 1;}

    if (isMan) XrdOlbSMon.Calc(Stage ? 1 : -1, 0, 0);

    XrdOlbSay.Emsg("Server", Name(), why);
    return 0;
}
  
/******************************************************************************/
/* MANAGER:                     d o _ S u R e s                               */
/******************************************************************************/
  
// When a manager receives a suspend/resume request, the result is propagated
// to upper-level managers only if the summary state has changed.
//
int XrdOlbServer::do_SuRes(char *rid, int Resume)
{
    char *why;

    if (Resume) 
       {if (!isSuspend) return 0;
        isSuspend = 0;
        if (!isOffline && !isDisable) why = (char *)"service resumed";
           else why = isOffline ? (char *)"offlined" : (char *)"disabled";
       } else if (isSuspend) return 0;
                 else {why = (char *)"service suspended"; isSuspend = 1;}

    if (isMan) XrdOlbSMon.Calc(Resume ? -1 : 1, 1, 1);

    XrdOlbSay.Emsg("Server", Name(), why);
    return 0;
}

/******************************************************************************/
/* SERVER LOCAL:                  d o _ T r y                                 */
/******************************************************************************/

// Try requests from a manager indicate that we are being displaced and should
// hunt for another manager. The request provides hints as to where to try.
//
int XrdOlbServer::do_Try(char *rid)
{
   char *tp;
   unsigned int ipaddr = Link->Addr();

// Delete any additions from this manager
//
   myMans.Del(ipaddr);

// Add all the alternates to our alternate list
//
   while((tp = Link->GetToken()))
         myMans.Add(ipaddr, tp, XrdOlbConfig.PortTCP);

// Close the link and reurn an error
//
   isOffline = 1;
   Link->Close(1);
   return -1;
}
  
/******************************************************************************/
/* SERVER LOCAL:                d o _ U s a g e                               */
/******************************************************************************/
  
// Usage requests from a manager are local to the cell and never propagated.
//
int XrdOlbServer::do_Usage(char *rid)
{
    char respbuff[512];
    long maxfr, totfr;

// Process: <id> usage
// Respond: <id> load <cpu> <io> <load> <mem> <pag> <dskfree> <dsktot>
//
   if (XrdOlbConfig.Manager())
      return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                        "%s load %d %d %d %d %d %d %d\n", rid,
                        cpu_load, net_load, xeq_load, mem_load, pag_load,
                        dsk_free, dsk_tota));

   if (XrdOlbConfig.Meter)
   return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                     "%s load %s\n", rid, XrdOlbConfig.Meter->Report()));

   maxfr = XrdOlbMeter::FreeSpace(totfr);
   return Link->Send(respbuff, snprintf(respbuff, sizeof(respbuff)-1,
                     "%s load 0 0 0 0 0 %ld %ld\n", rid, maxfr, totfr));
}

/******************************************************************************/
/*                                I n f o r m                                 */
/******************************************************************************/
  
int XrdOlbServer::Inform(const char *cmd, XrdOlbPrepArgs *pargs)
{
   EPNAME("Inform")
   char *mdest, *minfo;
   struct iovec iodata[8];

// See if user wants a response
//
   if (!index(pargs->mode, (int)'n')
   ||  strcmp("udp://", pargs->mode)
   ||  !XrdOlbRelay)
      {DEBUG(cmd <<' ' <<pargs->reqid <<" not sent to " <<pargs->user);
       return 0;
      }

// Extract out destination and argument
//
   mdest = pargs->mode+6;
   if ((minfo = index(pargs->mode, (int)'/')))
      {*minfo = '\0'; minfo++;}
   if (!minfo || !*minfo) minfo = (char *)"*";
   DEBUG("Sending " <<mdest <<": " <<cmd <<' '<<pargs->reqid <<' ' <<minfo);

// Create message to be sent
//
   iodata[0].iov_base = (char *)cmd;  iodata[0].iov_len  = strlen(cmd);
   iodata[1].iov_base = (char *)" ";  iodata[1].iov_len  = 1;
   iodata[2].iov_base = pargs->reqid; iodata[2].iov_len  = strlen(pargs->reqid);
   iodata[3].iov_base = (char *)" ";  iodata[3].iov_len  = 1;
   iodata[4].iov_base = minfo;        iodata[4].iov_len  = strlen(minfo);
   iodata[5].iov_base = (char *)" ";  iodata[5].iov_len  = 1;
   iodata[6].iov_base = pargs->path;  iodata[6].iov_len  = strlen(pargs->path);
   iodata[7].iov_base = (char *)"\n"; iodata[7].iov_len  = 1;

// Send the message and return
//
   XrdOlbRelay->Send(mdest, (const struct iovec *)&iodata, 8);
   return 0;
}

/******************************************************************************/
/*                              i s O n l i n e                               */
/******************************************************************************/
  
int XrdOlbServer::isOnline(char *path, int upt)
{
   struct stat buf;
   struct utimbuf times;

// Do a stat
//
   if (stat(path, &buf))
      if (XrdOlbConfig.DiskSS && XrdOlbPrepQ.Exists(path)) return 1;
         else return 0;

// Make sure we are doing a state on a file
//
   if ((buf.st_mode & S_IFMT) == S_IFREG)
      {if (upt)
          {times.actime = time(0);
           times.modtime = buf.st_mtime;
           utime(path, &times);
          }
       return 1;
      }

// Indicate possible problem
//
   XrdOlbSay.Emsg("Server", Link->Name(), "stated a non-file,", path);
   return 0;
}

/******************************************************************************/
/*                               R e c e i v e                                */
/******************************************************************************/
  
char *XrdOlbServer::Receive(char *idbuff, int blen)
{
   EPNAME("Receive")
   char *lp, *tp;
   if ((lp=Link->GetLine()) && *lp)
      {if (XrdOlbTrace.What & TRACE_Debug
       &&  strcmp("1@0 ping", lp) && strcmp("1@0 pong", lp))
           TRACEX("from " <<myName <<": " <<lp);
       isActive = 1;
       if ((tp=Link->GetToken()))
          {strncpy(idbuff, tp, blen-2); idbuff[blen-1] = '\0';
           return Link->GetToken();
          }
      } else DEBUG("Null line from " <<myName);
   return 0;
}
 
/******************************************************************************/
/*                               R e i s s u e                                */
/******************************************************************************/

int XrdOlbServer::Reissue(char *rid, const char *op,   char *arg1,
                                           char *path, char *arg3)
{
   XrdOlbPInfo pinfo;
   SMask_t amask;
   struct iovec iod[12];
   char newmid[32];
   int iocnt;

// Check if we can really reissue the command
//
   if (!(iod[0].iov_len = XrdOlbConfig.GenMsgID(rid, newmid, sizeof(newmid))))
      {XrdOlbSay.Emsg("Server", "msg TTL exceeded for", op, path);
       return 0;
      }
   iod[0].iov_base = newmid;
   iocnt = 1;
  
// Find all the servers that might be able to do somthing on this path
//
   if (!XrdOlbCache.Paths.Find(path, pinfo)
   || (amask = pinfo.rwvec | pinfo.rovec) == 0)
      {XrdOlbSay.Emsg("Server",op,"aborted; no servers handling path",path);
       return 0;
      }

// Construct the message
//
       iod[iocnt].iov_base = (char *)op;   iod[iocnt++].iov_len = strlen(op);
   if (arg1)
      {iod[iocnt].iov_base = arg1;         iod[iocnt++].iov_len = strlen(arg1);
       iod[iocnt].iov_base = (char *)" ";  iod[iocnt++].iov_len = 1;
      }
       iod[iocnt].iov_base = path;         iod[iocnt++].iov_len = strlen(path);
   if (arg3)
      {iod[iocnt].iov_base = (char *)" ";  iod[iocnt++].iov_len = 1;
       iod[iocnt].iov_base = arg3;         iod[iocnt++].iov_len = strlen(arg3);
       iod[iocnt].iov_base = (char *)" ";  iod[iocnt++].iov_len = 1;
      }
       iod[iocnt].iov_base = (char *)"\n"; iod[iocnt++].iov_len = 1;

// Now send off the message
//
   XrdOlbSM.Broadcast(amask, iod, iocnt);
   return 0;
}
