/******************************************************************************/
/*                                                                            */
/*                       X r d C m s C o n f i g . c c                        */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/*
   The methods in this file handle cmsd() initialization.
*/
  
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "XrdVersion.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdCms/XrdCmsAdmin.hh"
#include "XrdCms/XrdCmsBaseFS.hh"
#include "XrdCms/XrdCmsBlackList.hh"
#include "XrdCms/XrdCmsCache.hh"
#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsConfig.hh"
#include "XrdCms/XrdCmsManager.hh"
#include "XrdCms/XrdCmsManTree.hh"
#include "XrdCms/XrdCmsMeter.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsPrepare.hh"
#include "XrdCms/XrdCmsPrepArgs.hh"
#include "XrdCms/XrdCmsProtocol.hh"
#include "XrdCms/XrdCmsRole.hh"
#include "XrdCms/XrdCmsRRQ.hh"
#include "XrdCms/XrdCmsSecurity.hh"
#include "XrdCms/XrdCmsState.hh"
#include "XrdCms/XrdCmsSupervisor.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdCms/XrdCmsUtils.hh"
#include "XrdCms/XrdCmsXmi.hh"
#include "XrdCms/XrdCmsXmiReq.hh"

#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdNet/XrdNetSecurity.hh"
#include "XrdNet/XrdNetSocket.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdCms;

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

       XrdCmsAdmin      XrdCms::Admin;

       XrdCmsBaseFS     XrdCms::baseFS(&XrdCmsNode::do_StateDFS);

       XrdCmsConfig     XrdCms::Config;

       XrdSysError      XrdCms::Say(0, "");

       XrdOucTrace      XrdCms::Trace(&Say);

       XrdScheduler    *XrdCms::Sched = 0;

       XrdCmsXmi       *XrdCms::Xmi_Chmod  = 0;
       XrdCmsXmi       *XrdCms::Xmi_Load   = 0;
       XrdCmsXmi       *XrdCms::Xmi_Mkdir  = 0;
       XrdCmsXmi       *XrdCms::Xmi_Mkpath = 0;
       XrdCmsXmi       *XrdCms::Xmi_Prep   = 0;
       XrdCmsXmi       *XrdCms::Xmi_Rename = 0;
       XrdCmsXmi       *XrdCms::Xmi_Remdir = 0;
       XrdCmsXmi       *XrdCms::Xmi_Remove = 0;
       XrdCmsXmi       *XrdCms::Xmi_Select = 0;
       XrdCmsXmi       *XrdCms::Xmi_Space  = 0;
       XrdCmsXmi       *XrdCms::Xmi_Stat   = 0;

/******************************************************************************/
/*                S e c u r i t y   S y m b o l   T i e - I n                 */
/******************************************************************************/
  
// The following is a bit of a kludge. The client side will use the xrootd
// security infrastructure if it exists. This is tipped off by the presence
// of the following symbol being non-zero. On the server side, we have no
// such symbol and need to provide one initialized to zero.
//
       XrdSecProtocol *(*XrdXrootdSecGetProtocol)
                                          (const char             *hostname,
                                           const struct sockaddr  &netaddr,
                                           const XrdSecParameters &parms,
                                                 XrdOucErrInfo    *einfo)=0;
  
/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/

void *XrdCmsStartMonPerf(void *carg) { return Cluster.MonPerf(); }

void *XrdCmsStartMonRefs(void *carg) { return Cluster.MonRefs(); }

void *XrdCmsStartMonStat(void *carg) { return CmsState.Monitor(); }

void *XrdCmsStartAdmin(void *carg)
      {return XrdCms::Admin.Start((XrdNetSocket *)carg);
      }

void *XrdCmsStartAnote(void *carg)
      {XrdCmsAdmin Anote;
       return Anote.Notes((XrdNetSocket *)carg);
      }

void *XrdCmsStartPreparing(void *carg)
      {XrdCmsPrepArgs::Process();
       return (void *)0;
      }

void *XrdCmsStartSupervising(void *carg)
      {XrdCmsSupervisor::Start();
       return (void *)0;
      }

/******************************************************************************/
/*                    P i n g   C l o c k   H a n d l e r                     */
/******************************************************************************/
  
namespace XrdCms
{
  
class PingClock : XrdJob
{
public:

       void DoIt() {Config.PingTick++;
                    Sched->Schedule((XrdJob *)this,time(0)+Config.AskPing);
                   }

static void Start() {static PingClock selfie;}

          PingClock() : XrdJob("ping clock") {DoIt();}
         ~PingClock() {}
private:
};
};

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_String(x,m) if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(eDest, CFile);

#define TS_Set(x,v)    if (!strcmp(x,var)) {v=1; CFile.Echo(); return 0;}

#define TS_unSet(x,v)  if (!strcmp(x,var)) {v=0; CFile.Echo(); return 0;}

/******************************************************************************/
/*                            C o n f i g u r e 1                             */
/******************************************************************************/
  
int XrdCmsConfig::Configure1(int argc, char **argv, char *cfn)
{
/*
  Function: Establish phase 1 configuration at start up time.

  Input:    argc - argument count
            argv - argument vector
            cfn  - optional configuration file name

  Output:   0 upon success or !0 otherwise.
*/
   int NoGo = 0, immed = 0;
   char c, buff[512];
   extern int opterr, optopt;

// Prohibit this program from executing as superuser
//
   if (geteuid() == 0)
      {Say.Emsg("Config", "Security reasons prohibit cmsd running as "
                  "superuser; cmsd is terminating.");
       _exit(8);
      }

// Process the options
//
   opterr = 0; optind = 1;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c=getopt(argc,argv,"iw")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'i': immed = 1;
                 break;
       case 'w': immed = -1;   // Backward compatability only
                 break;
       default:  buff[0] = '-'; buff[1] = optopt; buff[2] = '\0';
                 Say.Say("Config warning: unrecognized option, ",buff,", ignored.");
       }
     }

// Accept a single parameter defining the overiding major role
//
   if (optind < argc)
      {     if (!strcmp(argv[optind], "manager")) isManager = 1;
       else if (!strcmp(argv[optind], "server" )) isServer  = 1;
       else if (!strcmp(argv[optind], "super"  )) isServer  = isManager = 1;
       else Say.Say("Config warning: unrecognized parameter, ",
                    argv[optind],", ignored.");
      }

// Bail if no configuration file specified
//
   inArgv = argv; inArgc = argc;
   if ((!(ConfigFN = cfn) && !(ConfigFN = getenv("XrdCmsCONFIGFN")))
   ||  !*ConfigFN)
      {Say.Emsg("Config", "Required config file not specified.");
       Usage(1);
      }

// Establish my instance name
//
   sprintf(buff, "%s@%s", XrdOucUtils::InstName(myInsName), myName);
   myInstance = strdup(buff);

// Print herald
//
   Say.Say("++++++ ", myInstance, " phase 1 initialization started.");

// If we don't know our role yet then we must find out before processing the
// config file. This means a double scan, sigh.
//
   if (!(isManager || isServer)) 
      if (!(NoGo |= ConfigProc(1)) && !(isManager || isServer))
         {Say.Say("Config warning: role not specified; manager role assumed.");
          isManager = -1;
         }

// Process the configuration file
//
   if (!NoGo) NoGo |= ConfigProc();

// Override the trace option
//
   if (getenv("XRDDEBUG")) Trace.What = TRACE_ALL;

// Override the wait/nowait from the command line
//
   if (immed) doWait = (immed > 0 ? 0 : 1);

// Determine the role
//
   if (isManager < 0) isManager = 1;
   if (isPeer    < 0) isPeer    = 1;
   if (isProxy   < 0) isProxy   = 1;
   if (isServer  < 0) isServer  = 1;

// Create a text description of our role for use in messages
//
   if (!myRole)
      {XrdCmsRole::RoleID rid;
             if (isMeta)        rid = XrdCmsRole::MetaManager;
        else if (isPeer)        rid = XrdCmsRole::Peer;
        else if (isProxy)
                {if (isManager) rid = (isServer ? XrdCmsRole::ProxySuper
                                                : XrdCmsRole::ProxyManager);
                    else        rid = XrdCmsRole::ProxyServer;
                }
        else if (isManager)
                {if (isManager) rid = (isServer ? XrdCmsRole::Supervisor
                                                : XrdCmsRole::Manager);
                }
        else                    rid = XrdCmsRole::Server;
        strcpy(myRType, XrdCmsRole::Type(rid));
        myRole   = strdup(XrdCmsRole::Name(rid));
        myRoleID = static_cast<int>(rid);
      }

// Export the role IN basic form and expanded form
//
   XrdOucEnv::Export("XRDROLE", myRole);
   XrdOucEnv::Export("XRDROLETYPE", myRType);

// For managers, make sure that we have a well designated port.
// For servers or supervisors, force an ephemeral port to be used.
//
   if (!NoGo)
      {if ((isManager && !isServer) || isPeer)
          {if (PortTCP <= 0)
              {Say.Emsg("Config","port for this", myRole, "not specified.");
               NoGo = 1;
              }
          }
          else PortTCP = 0;
      }

// If we are configured in proxy mode then we are running a shared filesystem
//
   if (isProxy) baseFS.Init(XrdCmsBaseFS::DFSys | XrdCmsBaseFS::Immed |
               (baseFS.Local() ? XrdCmsBaseFS::Cntrl : 0), 0, 0);

// Determine how we ended and return status
//
   sprintf(buff, " phase 1 %s initialization %s.", myRole,
                (NoGo ? "failed" : "completed"));
   Say.Say("------ ", myInstance, buff);
   return NoGo;
}

/******************************************************************************/
/*                            C o n f i g u r e 2                             */
/******************************************************************************/
  
int XrdCmsConfig::Configure2()
{
/*
  Function: Establish phase 2 configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   EPNAME("Configure2");
   int Who, NoGo = 0;
   char *p, buff[512];

// Print herald
//
   sprintf(buff, " phase 2 %s initialization started.", myRole);
   Say.Say("++++++ ", myInstance, buff);

// Fix up the QryMinum (we hard code 64 as the max) and P_gshr values.
// The QryMinum only applies to a metamanager and is set as 1 minus the min.
//
        if (!isMeta)       QryMinum =  0;
   else if (QryMinum <  2) QryMinum =  0;
   else if (QryMinum > 64) QryMinum = 64;
   if (P_gshr < 0) P_gshr = 0;
      else if (P_gshr > 100) P_gshr = 100;

// Determine who we are. If we are a manager or supervisor start the file
// location cache scrubber.
//
   if (QryDelay < 0) QryDelay = LUPDelay;
   if (isManager) 
      NoGo = !Cache.Init(cachelife, LUPDelay, QryDelay, baseFS.isDFS());

// Issue warning if the adminpath resides in /tmp
//
   if (!strncmp(AdminPath, "/tmp/", 5))
      Say.Say("Config warning: adminpath resides in /tmp and may be unstable!");


// Establish the path to be used for admin functions
//
   p = XrdOucUtils::genPath(AdminPath,XrdOucUtils::InstName(myInsName,0),".olb");
   free(AdminPath);
   AdminPath = p;

// Setup the admin path (used in all roles)
//
   if (!NoGo) NoGo = !(AdminSock = XrdNetSocket::Create(&Say, AdminPath,
                     (isManager|isPeer ? "olbd.nimda":"olbd.admin"),AdminMode));

// Develop a stable unique identifier for this cmsd independent of the port
//
   if (!NoGo && !(mySID = setupSid()))
      {Say.Emsg("cmsd", "Unable to generate system ID; too many managers.");
       NoGo = 1;
      } else {DEBUG("Global System Identification: " <<mySID);}

// If we need a name library, load it now
//
   if ((LocalRoot || RemotRoot) && ConfigN2N()) NoGo = 1;

// Configure the OSS, the base filesystem, and initialize the prep queue
//
   if (!NoGo) NoGo = ConfigOSS();
   if (!NoGo) baseFS.Start();
   if (!NoGo) PrepQ.Init();

// Setup manager or server, as needed
//
  if (!NoGo && isManager)              NoGo = setupManager();
  if (!NoGo && (isServer || ManList))  NoGo = setupServer();

// If we are a solo peer then we have no servers and a lot of space and
// connections don't matter. Only one connection matters for a meta-manager.
// Servers, supervisors, and managers who have a meta manager must wait for
// for the local data server to connect so port mapping occurs. Otherwise,
// we indicate that it doesn't matter as the local server won't connect.
//
   if (isPeer && isSolo) 
      {SUPCount = SUPLevel = 0; Meter.setVirtual(XrdCmsMeter::peerFS);}
      else if (isManager)
              {Meter.setVirtual(XrdCmsMeter::manFS);
               if (isMeta) {SUPCount = 1; SUPLevel = 0;}
               if (!ManList) CmsState.Update(XrdCmsState::FrontEnd, 1);
              }
    if (isManager) Who = (isServer ? -1 : 1);
       else        Who = 0;
    CmsState.Set(SUPCount, Who, AdminPath);

// Create the pid file
//
   if (!NoGo) NoGo |= PidFile();

// Load the XMI plugin
//
   if (!NoGo && XmiPath) NoGo = setupXmi();

// All done, check for success or failure
//
   sprintf(buff, " phase 2 %s initialization %s.", myRole,
                 (NoGo ? "failed" : "completed"));
   Say.Say("------ ", myInstance, buff);

// The remainder of the configuration needs to be run in a separate thread
//
   if (!NoGo) Sched->Schedule((XrdJob *)this);

// All done
//
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdCmsConfig::ConfigXeq(char *var, XrdOucStream &CFile, XrdSysError *eDest)
{
   int dynamic;

   // Determine whether is is dynamic or not
   //
   if (eDest) dynamic = 1;
      else   {dynamic = 0; eDest = &Say;}

   // Process items
   //
   TS_Xeq("delay",         xdelay);  // Manager,     dynamic
   TS_Xeq("fxhold",        xfxhld);  // Manager,     dynamic
   TS_Xeq("ping",          xping);   // Manager,     dynamic
   TS_Xeq("sched",         xsched);  // Any,         dynamic
   TS_Xeq("space",         xspace);  // Any,         dynamic
   TS_Xeq("trace",         xtrace);  // Any,         dynamic

   if (!dynamic)
   {
   TS_Xeq("adminpath",     xapath);  // Any,     non-dynamic
   TS_Xeq("allow",         xallow);  // Manager, non-dynamic
   TS_Xeq("altds",         xaltds);  // Server,  non-dynamic
   TS_Xeq("blacklist",     xblk);    // Manager, non-dynamic
   TS_Xeq("defaults",      xdefs);   // Server,  non-dynamic
   TS_Xeq("dfs",           xdfs);    // Any,     non-dynamic
   TS_Xeq("export",        xexpo);   // Any,     non-dynamic
   TS_Xeq("fsxeq",         xfsxq);   // Server,  non-dynamic
   TS_Xeq("localroot",     xlclrt);  // Any,     non-dynamic
   TS_Xeq("manager",       xmang);   // Server,  non-dynamic
   TS_Xeq("namelib",       xnml);    // Server,  non-dynamic
   TS_Xeq("osslib",        xolib);   // Any,     non-dynamic
   TS_Xeq("perf",          xperf);   // Server,  non-dynamic
   TS_Xeq("pidpath",       xpidf);   // Any,     non-dynamic
   TS_Xeq("prep",          xprep);   // Any,     non-dynamic
   TS_Xeq("prepmsg",       xprepm);  // Any,     non-dynamic
   TS_Xeq("remoteroot",    xrmtrt);  // Any,     non-dynamic
   TS_Xeq("repstats",      xreps);   // Any,     non-dynamic
   TS_Xeq("role",          xrole);   // Server,  non-dynamic
   TS_Xeq("seclib",        xsecl);   // Server,  non-dynamic
   TS_Set("wait",          doWait);  // Server,  non-dynamic (backward compat)
   TS_unSet("nowait",      doWait);  // Server,  non-dynamic
   TS_Xeq("xmilib",        xxmi);    // Any,     non-dynamic
   }

   // The following are client directives that we will ignore
   //
   if (!strcmp(var, "conwait")
   ||  !strcmp(var, "request")) return 0;

   // No match found, complain.
   //
   eDest->Say("Config warning: ignoring unknown directive '", var, "'.");
   CFile.Echo();
   return 0;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdCmsConfig::DoIt()
{
   XrdSysSemaphore SyncUp(0);
   XrdCmsProtocol *pP;
   XrdOucTList    *tp;
   pthread_t       tid;
   time_t          eTime = time(0);
   int             wTime;

// Set doWait correctly. We only wait if we have to provide a data path. This
// include server, supervisors, and managers who have a meta-manager, only.
// Why? Because we never get a primary login if we are a mere manager.
//
   if (isManager && !isServer && !ManList) doWait = 0;
      else if (isServer && adsMon)         doWait = 1;

// Start the notification thread if we need to
//
   if (AnoteSock)
      if (XrdSysThread::Run(&tid, XrdCmsStartAnote, (void *)AnoteSock,
                            0, "Notification handler"))
         Say.Emsg("cmsd", errno, "start notification handler");

// Start the prepare handler
//
   if (XrdSysThread::Run(&tid,XrdCmsStartPreparing,
                             (void *)0, 0, "Prep handler"))
      Say.Emsg("cmsd", errno, "start prep handler");

// Start the supervisor subsystem
//
   if (XrdCmsSupervisor::superOK)
      {if (XrdSysThread::Run(&tid,XrdCmsStartSupervising, 
                             (void *)0, 0, "supervisor"))
          {Say.Emsg("cmsd", errno, "start", myRole);
          return;
          }
      }

// Start the ping clock if we are a manager of any kind
//
   if (isManager) PingClock::Start();

// Start the admin thread if we need to, we will not continue until told
// to do so by the admin interface.
//
   if (AdminSock)
      {XrdCmsAdmin::setSync(&SyncUp);
       if (XrdSysThread::Run(&tid, XrdCmsStartAdmin, (void *)AdminSock,
                             0, "Admin traffic"))
          Say.Emsg("cmsd", errno, "start admin handler");
       SyncUp.Wait();
      }

// Start the server subsystem. We check here to make sure we will not be
// tying to connect to ourselves. This is possible if the manager and meta-
// manager were defined to be the same and we are a manager. We would have
// liked to screen this out earlier but port discovery prevents it.
//
   if (isManager || isServer || isPeer)
      {tp = ManList;
       while(tp)
            {if (strcmp(tp->text, myName) || tp->val != PortTCP)
                {pP = XrdCmsProtocol::Alloc(myRole, tp->text, tp->val);
                 Sched->Schedule((XrdJob *)pP);
                } else {
                 char buff[512];
                 sprintf(buff, "%s:%d", tp->text, tp->val);
                 Say.Emsg("Config", "Circular connection to", buff, "ignored.");
                }
             tp = tp->next;
            }
      }

// Start state monitoring thread
//
   if (XrdSysThread::Run(&tid, XrdCmsStartMonStat, (void *)0,
                               0, "State monitor"))
      {Say.Emsg("Config", errno, "create state monitor thread");
       return;
      }

// If we are a manager then we must do a service enable after a service delay
//
   if ((isManager || isPeer) && SRVDelay)
      {wTime = SRVDelay - static_cast<int>((time(0) - eTime));
       if (wTime > 0) XrdSysTimer::Wait(wTime*1000);
      }

// All done
//
   if (!SUPCount) CmsState.Update(XrdCmsState::Counts, 0, 0);
   CmsState.Enable();
   Say.Emsg("Config", myRole, "service enabled.");
}

/******************************************************************************/
/*                          G e n L o c a l P a t h                           */
/******************************************************************************/
  
/* GenLocalPath() generates the path that a file will have in the local file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least XrdCmsMAX_PATH_LEN bytes long.
*/
int XrdCmsConfig::GenLocalPath(const char *oldp, char *newp)
{
    if (lcl_N2N) return -(lcl_N2N->lfn2pfn(oldp, newp, XrdCmsMAX_PATH_LEN));
    if (strlen(oldp) >= XrdCmsMAX_PATH_LEN) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}
  
/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                        C o n f i g D e f a u l t s                         */
/******************************************************************************/

void XrdCmsConfig::ConfigDefaults(void)
{
   static XrdVERSIONINFODEF(myVer, cmsd, XrdVNUMBER, XrdVERSION);
   int myTZ, isEast = 0;

// Preset all variables with common defaults
//
   myName   = (char *)"localhost"; // Correctly set in Configure()
   myDomain = 0;
   LUPDelay = 5;
   QryDelay =-1;
   QryMinum = 0;
   LUPHold  = 178;
   DRPDelay = 10*60;
   PSDelay  = 0;
   RWDelay  = 2;
   SRVDelay = 90;
   SUPCount = 1;
   SUPLevel = 80;
   SUPDelay = 15;
   SUSDelay = 30;
   MaxLoad  = 0x7fffffff;
   MsgTTL   = 7;
   PortTCP  = 0;
   P_cpu    = 0;
   P_fuzz   = 20;
   P_gsdf   = 0;
   P_gshr   = 0;
   P_io     = 0;
   P_load   = 0;
   P_mem    = 0;
   P_pag    = 0;
   AskPerf  = 10;         // Every 10 pings
   AskPing  = 60;         // Every  1 minute
   PingTick = 0;
   DoMWChk  = 1;
   MaxDelay = -1;
   LogPerf  = 10;         // Every 10 usage requests
   DiskMin  = 10240;      // 10GB*1024 (Min partition space) in MB
   DiskHWM  = 11264;      // 11GB*1024 (High Water Mark SUO) in MB
   DiskMinP = 2;
   DiskHWMP = 5;
   DiskAsk  = 12;         // 15 Seconds between space calibrations.
   DiskWT   = 0;          // Do not defer when out of space
   DiskSS   = 0;          // Not a staging server
   DiskOK   = 0;          // Does not have any disk
   myPaths  = (char *)""; // Default is 'r /'
   ConfigFN = 0;
   sched_RR = 0;
   isManager= 0;
   isMeta   = 0;
   isPeer   = 0;
   isSolo   = 0;
   isProxy  = 0;
   isServer = 0;
   N2N_Lib  = 0;
   N2N_Parms= 0;
   lcl_N2N  = 0;
   xeq_N2N  = 0;
   LocalRoot= 0;
   RemotRoot= 0;
   myInsName= 0;
   RepStats = 0;
   myRole    =0;
   myRType[0]=0;
   myRoleID  = XrdCmsRole::noRole;
   ManList   =0;
   NanList   =0;
   mySID    = 0;
   ifList    =0;
   perfint  = 3*60;
   perfpgm  = 0;
   AdminPath= strdup("/tmp/");
   AdminMode= 0700;
   AdminSock= 0;
   AnoteSock= 0;
   RedirSock= 0;
   pidPath  = strdup("/tmp");
   Police   = 0;
   cachelife= 8*60*60;
   pendplife=   60*60*24*7;
   DiskLinger=0;
   ProgCH   = 0;
   ProgMD   = 0;
   ProgMV   = 0;
   ProgRD   = 0;
   ProgRM   = 0;
   doWait   = 1;
   RefReset = 60*60;
   RefTurn  = 3*STMax*(DiskLinger+1);
   XmiPath     = 0;
   XmiParms    = 0;
   DirFlags    = 0;
   blkList     = 0;
   blkChk      = 600;
   SecLib      = 0;
   ossLib      = 0;
   ossParms    = 0;
   ossFS       = 0;
   myVInfo     = &myVer;
   adsPort     = 0;
   adsMon      = 0;
   adsProt     = 0;

// Compute the time zone we are in
//
   myTZ = XrdSysTimer::TimeZone();
   if (myTZ <= 0) {isEast = 0x10; myTZ = -myTZ;}
   if (myTZ > 12) myTZ = 12;
   TimeZone = (myTZ | isEast);
}
  
/******************************************************************************/
/*                             C o n f i g N 2 N                              */
/******************************************************************************/

int XrdCmsConfig::ConfigN2N()
{
   XrdOucN2NLoader n2nLoader(&Say, ConfigFN, N2N_Parms, LocalRoot, RemotRoot);

// Get the plugin
//
   if (!(xeq_N2N = n2nLoader.Load(N2N_Lib, *myVInfo))) return 1;

// Optimize the local case
//
   if (N2N_Lib || LocalRoot || (RemotRoot && XmiPath)) lcl_N2N = xeq_N2N;

// All done
//
   PrepQ.setParms(lcl_N2N);
   return 0;
}
  
/******************************************************************************/
/*                             C o n f i g O S S                              */
/******************************************************************************/

int XrdCmsConfig::ConfigOSS()
{
   extern XrdOss *XrdOssGetSS(XrdSysLogger *, const char *, const char *,
                              const char   *, XrdOucEnv  *, XrdVersionInfo &);

// Set up environment for the OSS to keep it relevant for cmsd
//
   XrdOucEnv::Export("XRDREDIRECT", "Q");
   XrdOucEnv::Export("XRDOSSTYPE",  "cms");
   XrdOucEnv::Export("XRDOSSCSCAN", "off");

// If no osslib was specified but we are a proxy, then we must load the
// the proxy osslib.
//
   if (!ossLib && isProxy) ossLib = strdup("libXrdPss.so");

// Load and return result
//
   return !(ossFS=XrdOssGetSS(Say.logger(),ConfigFN,ossLib,ossParms,0,*myVInfo));
}

/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdCmsConfig::ConfigProc(int getrole)
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucEnv myEnv;
  XrdOucStream CFile(&Say, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Say.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   CFile.Attach(cfgFD);

// Turn off echoing if we are doing a pre-scan
//
   if (getrole) CFile.SetEroute(0);

// Now start reading records until eof.
//
   while((var = CFile.GetMyFirstWord()))
        if (getrole)
           {if (!strcmp("all.role", var) || !strcmp("olb.role", var))
               if (xrole(&Say, CFile))
                  {CFile.SetEroute(&Say); CFile.Echo(); NoGo = 1;
                   CFile.SetEroute(0);
                  }
           }
           else if (!strncmp(var, "cms.", 4)
                ||  !strncmp(var, "olb.", 4)      // Backward compatability
                ||  !strcmp(var, "ofs.osslib")
                ||  !strcmp(var, "oss.defaults")
                ||  !strcmp(var, "oss.localroot")
                ||  !strcmp(var, "oss.remoteroot")
                ||  !strcmp(var, "oss.namelib")
                ||  !strcmp(var, "all.adminpath")
                ||  !strcmp(var, "all.export")
                ||  !strcmp(var, "all.manager")
                ||  !strcmp(var, "all.pidpath")
                ||  !strcmp(var, "all.role")
                ||  !strcmp(var, "all.seclib"))
                   {if (ConfigXeq(var+4, CFile, 0)) {CFile.Echo(); NoGo = 1;}}
                   else if (!strcmp(var, "oss.stagecmd")) DiskSS = 1;

// Now check if any errors occured during file i/o
//
   if ((retc = CFile.LastError()))
      NoGo = Say.Emsg("Config", retc, "read config file", ConfigFN);
   CFile.Close();

// Merge Paths as needed
//
   if (!getrole && ManList) NoGo |= MergeP();

// Return final return code
//
   return NoGo;
}
 
/******************************************************************************/
/*                                i s E x e c                                 */
/******************************************************************************/
  
int XrdCmsConfig::isExec(XrdSysError *eDest, const char *ptype, char *prog)
{
  char buff[512], pp, *mp = prog;

// Isolate the program name
//
   while(*mp && *mp != ' ') mp++;
   pp = *mp; *mp ='\0';

// Make sure the program is executable by us
//
   if (access(prog, X_OK))
      {sprintf(buff, "find %s execuatble", ptype);
       eDest->Emsg("Config", errno, buff, prog);
       *mp = pp;
       return 0;
      }

// All is well
//
   *mp = pp;
   return 1;
}

/******************************************************************************/
/*                                M e r g e P                                 */
/******************************************************************************/
  
int XrdCmsConfig::MergeP()
{
   static const unsigned long long stage4MM = XRDEXP_STAGEMM & ~XRDEXP_STAGE;
   XrdOucPList *plp = PexpList.First();
   XrdCmsPList *pp;
   XrdCmsPInfo opinfo, npinfo;
   const char *ptype;
   char *pbP;
   unsigned long long Opts;
   int pbLen = 0, NoGo = 0, export2MM = isManager && !isServer;
   npinfo.rovec = 1;

// For each path in the export list merge it into the path list
//
   while(plp)
        {Opts = plp->Flag();
         if (!(Opts & XRDEXP_LOCAL))
            {npinfo.rwvec = (Opts & (XRDEXP_GLBLRO | XRDEXP_NOTRW) ? 0 : 1);
             if (export2MM) npinfo.ssvec = (Opts &  stage4MM       ? 1 : 0);
                else        npinfo.ssvec = (Opts &  XRDEXP_STAGE   ? 1 : 0);
             if (!PathList.Add(plp->Path(), &npinfo))
                Say.Emsg("Config","Ignoring duplicate export path",plp->Path());
                else if (npinfo.ssvec) DiskSS = 1;
            }
          plp = plp->Next();
         }

// Document what we will be declaring as available
//
   if (!NoGo)
      {const char *Who = (isManager ? (isServer ? "manager:" : "meta-manager:")
                                    : "redirector:");
       Say.Say("The following paths are available to the ", Who);
       if (!(pp = PathList.First())) Say.Say("r  /");
        else while(pp)
             {ptype = pp->PType();
              Say.Say(ptype, (strlen(ptype) > 1 ? " " : "  "), pp->Path());
              pbLen += strlen(pp->Path())+8; pp = pp->Next();
             }
       Say.Say(" ");
      }

// Now allocate a buffer and place all of the paths into that buffer to be
// sent during the login phase.
//
   if (pbLen != 0 && (pp = PathList.First()))
      {pbP = myPaths = (char *)malloc(pbLen);
       while(pp)
            {pbP += sprintf(pbP, "\n%s %s", pp->PType(), pp->Path());
             pp = pp->Next();
            }
       myPaths++;
      }

// All done update the staging status (it's nostage by default)
//
   if (DiskSS) CmsState.Update(XrdCmsState::Counts, 0, 1);
   return NoGo;
}

/******************************************************************************/
/*                               P i d F i l e                                */
/******************************************************************************/
  
int XrdCmsConfig::PidFile()
{
    int rc, xfd;
    const char *clID;
    char buff[1024];
    char pidFN[1200], *ppath=XrdOucUtils::genPath(pidPath,
                             XrdOucUtils::InstName(myInsName,0));
    const char *xop = 0;

    if ((rc = XrdOucUtils::makePath(ppath, XrdOucUtils::pathMode)))
       {Say.Emsg("Config", rc, "create pid file path", ppath);
        free(ppath);
        return 1;
       }

    if ((clID = index(mySID, ' '))) clID++;
       else clID = mySID;

         if (isManager && isServer)
            snprintf(pidFN, sizeof(pidFN), "%s/cmsd.super.pid", ppath);
    else if (isServer)
            snprintf(pidFN, sizeof(pidFN), "%s/cmsd.pid", ppath);
    else    snprintf(pidFN, sizeof(pidFN), "%s/cmsd.mangr.pid", ppath);

    if ((xfd = open(pidFN, O_WRONLY|O_CREAT|O_TRUNC,0644)) < 0) xop = "open";
       else {if ((write(xfd,buff,snprintf(buff,sizeof(buff),"%d",
                            static_cast<int>(getpid()))) < 0)
             || (LocalRoot && 
                           (write(xfd,(void *)"\n&pfx=",6)  < 0 ||
                            write(xfd,(void *)LocalRoot,strlen(LocalRoot)) < 0
                )          )
             || (AdminPath && 
                           (write(xfd,(void *)"\n&ap=", 5)  < 0 ||
                            write(xfd,(void *)AdminPath,strlen(AdminPath)) < 0
                )          )
             ||             write(xfd,(void *)"\n&cn=", 5)  < 0
             ||             write(xfd,(void *)clID,     strlen(clID))      < 0
                ) xop = "write";
             close(xfd);
            }

     if (xop) Say.Emsg("Config", errno, xop, pidFN);
        else XrdOucEnv::Export("XRDCMSPIDFN", pidFN);

     free(ppath);
     return xop != 0;
}

/******************************************************************************/
/*                          s e t u p M a n a g e r                           */
/******************************************************************************/
  
int XrdCmsConfig::setupManager()
{
   pthread_t tid;
   int rc;

// Setup supervisor mode if we are also a server
//
   if (isServer && !XrdCmsSupervisor::Init(AdminPath, AdminMode)) return 1;

// Compute the scheduling policy
//
   sched_RR = (100 == P_fuzz) || !AskPerf
              || !(P_cpu || P_io || P_load || P_mem || P_pag);
   if (sched_RR)
      Say.Say("Config round robin scheduling in effect.");

// Create statistical monitoring thread
//
   if ((rc = XrdSysThread::Run(&tid, XrdCmsStartMonPerf, (void *)0,
                               0, "Performance monitor")))
      {Say.Emsg("Config", rc, "create perf monitor thread");
       return 1;
      }

// Create reference monitoring thread
//
   RefTurn  = 3*STMax*(DiskLinger+1);
   if (RefReset)
      {if ((rc = XrdSysThread::Run(&tid, XrdCmsStartMonRefs, (void *)0,
                                   0, "Refcount monitor")))
          {Say.Emsg("Config", rc, "create refcount monitor thread");
           return 1;
          }
      }

// Initialize the fast redirect queue
//
   RRQ.Init(LUPHold, LUPDelay);

// Initialize the security interface
//
   if (SecLib && !XrdCmsSecurity::Configure(SecLib, ConfigFN)) return 1;

// Initialize the black list
//
   if (!isServer) XrdCmsBlackList::Init(Sched, &Cluster, blkList, blkChk);

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                           s e t u p S e r v e r                            */
/******************************************************************************/
  
int XrdCmsConfig::setupServer()
{
   XrdOucTList *tp;
   int n = 0;

// Make sure we have enough info to be a server
//
   if (!ManList)
      {Say.Emsg("Config", "Manager node not specified for", myRole, "role");
       return 1;
      }

// Count the number of managers we have and tell ManTree about it
//
   tp = ManList;
   while(tp) {n++; tp = tp->next;}
   if (n > XrdCmsManager::MTMax)
      {Say.Emsg("Config", "Too many managers have been specified"); return 1;}
   ManTree.setMaxCon(n);

// Calculate overload delay time
//
   if (MaxDelay < 0) MaxDelay = AskPerf*AskPing+30;
   if (DiskWT   < 0) DiskWT   = AskPerf*AskPing+30;

// Setup notification path
//
   if (!(AnoteSock = XrdNetSocket::Create(&Say, AdminPath,
                             (isManager|isPeer ? "olbd.seton":"olbd.notes"),
                             AdminMode, XRDNET_UDPSOCKET))) return 1;

// We have data only if we are a pure data server (the default is noData)
// If we have no data, then we are done (the rest is for pure servers)
//
   if (isManager || isPeer) return 0;
   SUPCount = 0; SUPLevel = 0;
   if (isProxy) return 0;
   DiskOK = 1; 

// If this is a staging server then set up the Prepq object
//
   if (DiskSS) PrepQ.Reset(myInsName, AdminPath, AdminMode);

// Setup file system metering (skip it for peers)
//
   Meter.Init();
   if (perfpgm && Meter.Monitor(perfpgm, perfint))
      Say.Say("Config warning: load based scheduling disabled.");

// All done
//
   return 0;
}

/******************************************************************************/
/*                              s e t u p S i d                               */
/******************************************************************************/
  
char *XrdCmsConfig::setupSid()
{
   XrdOucTList *tp = (NanList ? NanList : ManList);
   char sfx;

// Grab the interfaces. This is normally set as an envar. If present then
// we will copy it because we must use it permanently.
//
   if (getenv("XRDIFADDRS")) ifList = strdup(getenv("XRDIFADDRS"));

// Determine what type of role we are playing
//
   if (isManager && isServer) sfx = 'u';
      else sfx = (isManager ? 'm' : 's');

// Generate the system ID and set the cluster ID
//
   return XrdCmsSecurity::setSystemID(tp, myInsName, myName, sfx);
}

/******************************************************************************/
/*                              s e t u p X m i                               */
/******************************************************************************/
  
int XrdCmsConfig::setupXmi()
{
   EPNAME("setupXmi");
   static XrdCmsXmiEnv XmiEnv;
   XrdSysPlugin       *xmiLib;
   XrdCmsXmi          *(*ep)(int, char **, XrdCmsXmiEnv *);
   unsigned int isNormal, isDirect;
   XrdCmsXmi          *XMI, *myXMI;
   const char         *theMode;
   int i;

   struct {unsigned int   theMask;
           XrdCmsXmi    **theAddr;
           const char    *theName;} XmiTab[] =
          {{XMI_CHMOD,  &Xmi_Chmod,  "chmod"},
           {XMI_LOAD,   &Xmi_Load,   "load"},
           {XMI_MKDIR,  &Xmi_Mkdir,  "mkdir"},
           {XMI_MKPATH, &Xmi_Mkpath, "mkpath"},
           {XMI_PREP,   &Xmi_Prep,   "prep"},
           {XMI_RENAME, &Xmi_Rename, "rename"},
           {XMI_REMDIR, &Xmi_Remdir, "remdir"},
           {XMI_REMOVE, &Xmi_Remove, "remove"},
           {XMI_SELECT, &Xmi_Select, "select"},
           {XMI_SPACE,  &Xmi_Space,  "space"},
           {XMI_STAT,   &Xmi_Stat,   "stat"}};
   int numintab = sizeof(XmiTab)/sizeof(XmiTab[0]);

// Fill out the rest of the XmiEnv structure
//
   XmiEnv.Role     = myRole;
   XmiEnv.ConfigFN = ConfigFN;
   XmiEnv.Parms    = XmiParms;
   XmiEnv.eDest    = &Say;
   XmiEnv.iNet     = NetTCP;
   XmiEnv.Sched    = Sched;
   XmiEnv.Trace    = &Trace;
   XmiEnv.Name2Name= xeq_N2N;

// Create a pluin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   if (!(xmiLib = new XrdSysPlugin(&Say, XmiPath, "xmilib", myVInfo))) return 1;

// Now get the entry point of the object creator
//
   ep = (XrdCmsXmi *(*)(int, char **, XrdCmsXmiEnv *))(xmiLib->getPlugin("XrdCmsgetXmi"));
   if (!ep) return 1;

// Get the Object now
//
   if (!(XMI = ep(inArgc, inArgv, &XmiEnv))) return 1;
   DEBUG("xmi library loaded; path=" <<XmiPath);

// Obtain the execution mode
//
   XMI->XeqMode(isNormal, isDirect);

// Check if we need to create an indirect XMI interface
//
   if ((isDirect & XMI_ALL) == XMI_ALL) myXMI = 0;
      else myXMI = (XrdCmsXmi *)new XrdCmsXmiReq(XMI);

// Now run throw all of the possibilities setting the execution mode as needed
//
   for (i = 0; i < numintab; i++)
       {if (!(isNormal & XmiTab[i].theMask))
           if (isDirect & XmiTab[i].theMask)
                   {*XmiTab[i].theAddr =   XMI; theMode = "direct";}
              else {*XmiTab[i].theAddr = myXMI; theMode = "queued";}
           else theMode = "normal";
        DEBUG(XmiTab[i].theName <<" is " <<theMode);
       }
   return 0;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void XrdCmsConfig::Usage(int rc)
{
cerr <<"\nUsage: cmsd [xrdopts] [-i] [-m] [-s] -c <cfile>" <<endl;
exit(rc);
}
  
/******************************************************************************/
/*                                x a l l o w                                 */
/******************************************************************************/

/* Function: xallow

   Purpose:  To parse the directive: allow {host | netgroup} <name>

             <name> The dns name of the host that is allowed to connect or the
                    netgroup name the host must be a member of. For DNS names,
                    a single asterisk may be specified anywhere in the name.

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xallow(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;
    int ishost;

    if (!isManager) return CFile.noEcho();

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "allow type not specified"); return 1;}

    if (!strcmp(val, "host")) ishost = 1;
       else if (!strcmp(val, "netgroup")) ishost = 0;
               else {eDest->Emsg("Config", "invalid allow type -", val);
                     return 1;
                    }

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "allow target name not specified"); return 1;}

    if (!Police) Police = new XrdNetSecurity();
    if (ishost)  Police->AddHost(val);
       else      Police->AddNetGroup(val);

    return 0;
}
  
/******************************************************************************/
/*                                x a l t d s                                 */
/******************************************************************************/

/* Function: xaltds

   Purpose:  To parse the directive: altds xroot <port> [[no]monitor]

             xroot  The protocol used by the alternate data server.
             <port> The port being used by the alternate data server.
               mon  Actively monitor alternate data server by connecting to it.
                    This is the default.
             nomon  Do not monitor the alternate data server.
                    it and if <sec> is greater than zero, send "ping" requests
                    every <sec> seconds. Zero merely connects.

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xaltds(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;

    if (isManager) return CFile.noEcho();

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "protocol not specified"); return 1;}

    if (strcmp(val, "xroot"))
       {eDest->Emsg("Config", "unsupported protocol, '", val, "'."); return 1;}
    if (adsProt) free(adsProt);
    adsProt = strdup(val);

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "data server port not specified"); return 1;}

    if (isdigit(*val))
       {if (XrdOuca2x::a2i(*eDest,"data server port",val,&adsPort,1,65535))
           return 1;
       }
       else if (!(adsPort = XrdNetUtils::ServPort(val, "tcp")))
               {eDest->Emsg("Config", "Unable to find tcp service '",val,"'.");
                return 1;
               }

         if (!(val = CFile.GetWord()) || !strcmp(val, "monitor")) adsMon  = 1;
    else if (!strcmp(val, "nomonitor")) adsMon  = 0;
    else    {eDest->Emsg("Config", "invalid option, '", val, "'.");
             return 1;
            }

    return 0;
}

/******************************************************************************/
/*                                x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path>

             <path>    the path of the named socket to use for admin requests.

   Type: Manager and Server, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xapath(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *pval, *val;
    mode_t mode = S_IRWXU;

// Get the path
//
   pval = CFile.GetWord();
   if (!pval || !pval[0])
      {eDest->Emsg("Config", "adminpath not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {eDest->Emsg("Config", "adminpath not absolute"); return 1;}
   pval = strdup(pval);

// Get the optional access rights
//
   if ((val = CFile.GetWord()) && val[0])
      {if (!strcmp("group", val)) mode |= S_IRWXG;
          else {eDest->Emsg("Config", "invalid admin path modifier -", val);
                free(pval); return 1;
               }
      }

// Record the path
//
   if (AdminPath) free(AdminPath);
   AdminPath = pval;
   AdminMode = mode;
   return 0;
}

/******************************************************************************/
/*                                  x b l k                                   */
/******************************************************************************/

/* Function: xblk

   Purpose:  To parse the directive: blacklist [check <time> [path]] | <path>

             <time>    how often to check for black list changes.
             <path>    the path to the blacklist file

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xblk(XrdSysError *eDest, XrdOucStream &CFile)
{
   char *val = CFile.GetWord();

// We only support this for managers
//
   if (!isManager || isServer) return CFile.noEcho();

// See if a time was specified
//
   if (val && !strcmp(val, "check"))
      {if (!(val = CFile.GetWord()) || !val[0])
          {eDest->Emsg("Config", "blacklist check interval not specified");
           return 1;
          }
       if (XrdOuca2x::a2tm(*eDest, "check value", val, &blkChk, 60)) return 1;
       if (!(val = CFile.GetWord()))
          {if (blkList) {free(blkList); blkList = 0;}
           return 0;
          }
      }

// Verify the path it must be absolute
//
   if (!val || !val[0])
      {eDest->Emsg("Config", "blacklist path not specified"); return 1;}
   if (*val != '/')
      {eDest->Emsg("Config", "blacklist path not absolute"); return 1;}

// Record the path
//
   if (blkList) free(blkList);
   blkList = strdup(val);
   return 0;
}
  
/******************************************************************************/
/*                                x d e l a y                                 */
/******************************************************************************/

/* Function: xdelay

   Purpose:  To parse the directive: delay [lookup <sec>] [overload <sec>]
                                           [startup <sec>] [servers <cnt>[%]]
                                           [full <sec>] [discard <cnt>]
                                           [suspend <sec>] [drop <sec>]
                                           [service <sec>] [hold <msec>]
                                           [peer <sec>] [rw <lvl>] [qdl <sec>]
                                           [qdn <cnt>]

   discard   <cnt>     maximum number a message may be forwarded.
   drop      <sec>     seconds to delay a drop of an offline server.
   full      <sec>     seconds to delay client when no servers have space.
   hold      <msec>    millseconds to optimistically hold requests.
   lookup    <sec>     seconds to delay client when finding a file.
   overload  <sec>     seconds to delay client when all servers overloaded.
   peer      <sec>     maximum seconds client may be delayed before peer
                       selection is triggered.
   qdl       <sec>     the query response deadline.
   qdn       <cnt>     Min number of servers that must respond to satisfy qdl.
   rw        <lvl>     how to delay r/w lookups (one of three levels):
                       0 - always use fast redirect when possible
                       1 - delay update requests only
                       2 - delay all rw requests (the default)
   servers   <cnt>     minimum number of servers we need.
   service   <sec>     seconds to delay client when waiting for servers.
   startup   <sec>     seconds to delay enabling our service
   suspend   <sec>     seconds to delay client when all servers suspended.

   Type: Manager only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdCmsConfig::xdelay(XrdSysError *eDest, XrdOucStream &CFile)
{   char *val;
    const char *etxt = "invalid delay option";
    int  i, ppp, minV = 1, ispercent = 0;
    static struct delayopts {const char *opname; int *oploc; int istime;}
           dyopts[] =
       {
        {"discard",  &MsgTTL,   0},
        {"drop",     &DRPDelay, 1},
        {"full",     &DiskWT,  -1},
        {"hold",     &LUPHold,  0},
        {"lookup",   &LUPDelay, 1},
        {"overload", &MaxDelay,-1},
        {"peer",     &PSDelay,  1},
        {"qdl",      &QryDelay, 1},
        {"qdn",      &QryMinum, 0},
        {"rw",       &RWDelay,  0},
        {"servers",  &SUPCount, 0},
        {"service",  &SUPDelay, 1},
        {"startup",  &SRVDelay, 1},
        {"suspend",  &SUSDelay, 1}
       };
    int numopts = sizeof(dyopts)/sizeof(struct delayopts);

    if (!isManager && !isPeer) return CFile.noEcho();

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "delay arguments not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, dyopts[i].opname))
                  {if (!(val = CFile.GetWord()))
                      {eDest->Emsg("Config", "delay ", dyopts[i].opname,
                                   " argument not specified.");
                       return 1;
                      }
                   if (dyopts[i].istime < 0 && !strcmp(val, "*")) ppp = -1;
                      else if (dyopts[i].istime)
                              {if (XrdOuca2x::a2tm(*eDest,etxt,val,&ppp,1))
                                  return 1;
                              } else
                               if (*dyopts[i].opname == 'r')
                              {if (XrdOuca2x::a2i( *eDest,etxt,val,&ppp,0,2))
                                  return 1;
                              } else {
                               if (*dyopts[i].opname == 's')
                                  {ppp = strlen(val); SUPLevel = 0; minV = 0;
                                   if (val[ppp-1] == '%')
                                      {ispercent = 1; val[ppp-1] = '\0';}
                                  } else minV = 1;
                               if (XrdOuca2x::a2i( *eDest,etxt,val,&ppp,minV))
                                  return 1;
                              }
                   if (!ispercent) *dyopts[i].oploc = ppp;
                      else {ispercent = 0; SUPCount = 1; SUPLevel = ppp;}
                   break;
                  }
           if (i >= numopts) 
              eDest->Say("Config warning: ignoring invalid delay option '",val,"'.");
           val = CFile.GetWord();
          }
     return 0;
}

/******************************************************************************/
/*                                 x d e f s                                  */
/******************************************************************************/

/* Function: xdefs

   Purpose:  Parse: oss.defaults <default options>
                              
   Notes: See the oss configuration manual for the meaning of each option.
          The actual implementation is defined in XrdOucExport.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xdefs(XrdSysError *eDest, XrdOucStream &CFile)
{
   DirFlags = XrdOucExport::ParseDefs(CFile, *eDest, DirFlags);
   return 0;
}
  
/******************************************************************************/
/*                                  x d f s                                   */
/******************************************************************************/
  
/* Function: xdfs

   Purpose:  To parse the directive: dfs <opts>

   <opts>:   limit [central] [=]<n>
                       central - apply limit on manager node. Otherwise, limit
                                 is applied where lookups occur.
                       [=]<n>  - the limit value as transactions per second. If
                                 an equals is given before the limit, then
                                 requests are paced at the specified rate.
                                 Otherwise, a predictive algorithm is used.
                                 Zero (default) turns limit off.

             lookup   {central | distrib}
                       central - perform file lookups on the manager.
                       distrib - distribute file lookups to servers (default).

             mdhold <n>        - remember missing directories for n seconds
                                 Zero (default) turns this off.

             qmax <n>          - maximum number of requests that may be queued.
                                 One is the minimum. The default qmax is 2.5
                                 the limit value.

             redirect {immed | verify}
                       immed   - do not verify file existence prior to
                                 redirecting a client. This is the
                                 default for proxy configurations.
                       verify  - verify file existence prior to
                                 redirecting a client. This is the
                                 default for non-proxy configurations.    top

   Type: Any, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xdfs(XrdSysError *eDest, XrdOucStream &CFile)
{
    int Opts = XrdCmsBaseFS::DFSys | (isProxy ? XrdCmsBaseFS::Immed : 0)
             | (!isManager && isServer ? XrdCmsBaseFS::Servr: 0);
    int Hold = 0, limCent = 0, limFix = 0, limV = 0, qMax = 0;
    char *val;

// If we are a meta-manager or a peer, ignore this option
//
   if (isMeta || isPeer) return CFile.noEcho();

// Get first option. We need one but they can come in any order
//
   if (!(val = CFile.GetWord()))
      {eDest->Emsg("Config", "dfs option not specified"); return 1;}

// Now parse each option
//
do{     if (!strcmp("mdhold",  val))
           {if (!(val = CFile.GetWord()))
               {eDest->Emsg("Config","mdhold value not specified.");  return 1;}
            if (XrdOuca2x::a2tm(*eDest, "hold value", val, &Hold, 1)) return 1;
           }
   else if (!strcmp("limit",   val))
           {if (!(val = CFile.GetWord()))
               {eDest->Emsg("Config","limit value not specified.");   return 1;}
            if ((limCent = !strcmp("central",val)) && !(val = CFile.GetWord()))
               {eDest->Emsg("Config","limit value not specified.");   return 1;}
            if ((limFix = (*val == '=')) && *(val+1)) val++;
            if (XrdOuca2x::a2i(*eDest, "limit value", val, &limV, 0)) return 1;
           }
   else if (!strcmp("lookup",  val))
           {if (!(val = CFile.GetWord()))
               {eDest->Emsg("Config","lookup value not specified.");  return 1;}
                 if (!strcmp("central",  val)) Opts |=  XrdCmsBaseFS::Cntrl;
            else if (!strcmp("distrib", val))  Opts &= ~XrdCmsBaseFS::Cntrl;
            else {eDest->Emsg("Config","invalid lookup value '", val, "'.");
                  return 1;
                 }
           }
   else if (!strcmp("qmax",    val))
           {if (!(val = CFile.GetWord()))
               {eDest->Emsg("Config","qmax value not specified.");    return 1;}
            if (XrdOuca2x::a2i(*eDest, "qmax value", val, &qMax, 1))  return 1;
           }
   else if (!strcmp("redirect",val))
           {if (!(val = CFile.GetWord()))
               {eDest->Emsg("Config","redirect value not specified.");return 1;}
                 if (!strcmp("immed",  val)) Opts |=  XrdCmsBaseFS::Immed;
            else if (!strcmp("verify", val)) Opts &= ~XrdCmsBaseFS::Immed;
            else {eDest->Emsg("Config","invalid redirect value -", val);
                  return 1;
                 }
           }
   else {eDest->Emsg("Config", "invalid dfs option '",val,"'."); return 1;}
  } while((val = CFile.GetWord()));

// Supervisors are special beasts so we need to make transparent. One of these
// days we'll allow lookups to go down to the supervisor level.
//
   if (isManager && isServer)
      {limV = 0;
       Opts &= ~XrdCmsBaseFS::Cntrl;
      }

// Adjust the limit value and option as needed
//
   if (limV)
      {if (limFix) limV  = -limV;
       if (limCent || Opts & XrdCmsBaseFS::Cntrl) {if (isServer) limV = 0;}
          else if (isManager) limV = 0;
      }

// If we are a manager but not doing local lookups, then hold does not apply
//
   if (isManager && !(Opts & XrdCmsBaseFS::Cntrl)) Hold = 0;

// All done, simply set the values
//
   baseFS.Limit(limV, qMax);
   baseFS.Init(Opts, Hold, Hold*10);
   return 0;
}
  
/******************************************************************************/
/*                                 x e x p o                                  */
/******************************************************************************/

/* Function: xexpo

   Purpose:  To parse the directive: all.export <path> [<options>]

             <path>    the full path that resides in a remote system.
             <options> a blank separated list of options (see XrdOucExport)

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xexpo(XrdSysError *eDest, XrdOucStream &CFile)
{

// Parse the arguments
//
   return (XrdOucExport::ParsePath(CFile, *eDest, PexpList, DirFlags) ? 0 : 1);
}
  
/******************************************************************************/
/*                                 x f s x q                                  */
/******************************************************************************/
  
/* Function: xfsxq

   Purpose:  To parse the directive: fsxeq <types> <prog>

             <types>   what operations the program performs (one or more of):
                       chmod mkdir mkpath mv rm rmdir
             <prog>    the program to execute when doing a forwarded fs op.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xfsxq(XrdSysError *eDest, XrdOucStream &CFile)
{
    struct xeqopts {const char *opname; int doset; XrdOucProg **pgm;} xqopts[] =
       {
        {"chmod",    0, &ProgCH},
        {"mkdir",    0, &ProgMD},
        {"mkpath",   0, &ProgMP},
        {"mv",       0, &ProgMV},
        {"rm",       0, &ProgRM},
        {"rmdir",    0, &ProgRD},
        {"trunc",    0, &ProgTR}
       };
    int i, xtval = 0, numopts = sizeof(xqopts)/sizeof(struct xeqopts);
    char *val;

// If we are a manager, ignore this option
//
   if (!isServer) return CFile.noEcho();

// Get the operation types
//
    val = CFile.GetWord();
    while (val && *val != '/')
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, xqopts[i].opname))
                  {xqopts[i].doset = 1;
                   xtval = 1;
                   break;
                  }
           if (i >= numopts)
              eDest->Say("Config warning: ignoring invalid fsxeq type option '",val,"'.");
           val = CFile.GetWord();
          }

// Make sure some type was specified
//
   if (!xtval)
      {eDest->Emsg("Config", "fsxeq type option not specified"); return 1;}

// Make sure a program was specified
//
   if (!val)
      {eDest->Emsg("Config", "fsxeq program not specified"); return 1;}

// Get the program
//
   CFile.RetToken();

// Set the program for each type
//
   for (i = 0; i < numopts; i++)
       if (xqopts[i].doset)
          {if (!*xqopts[i].pgm) *(xqopts[i].pgm) = new XrdOucProg(0);
           if ((*(xqopts[i].pgm))->Setup(val, eDest)) return 1;
          }

// All done
//
   return 0;
}

/******************************************************************************/
/*                                x f x h l d                                 */
/******************************************************************************/

/* Function: xfxhld

   Purpose:  To parse the directive: fxhold <sec>

             <sec>  number of seconds (or M, H, etc) to cache file existence

   Type: Manager only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xfxhld(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;
    int ct;

    if (!isManager) return CFile.noEcho();

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "fxhold value not specified."); return 1;}

    if (XrdOuca2x::a2tm(*eDest, "fxhold value", val, &ct, 60)) return 1;

    cachelife = ct;
    return 0;
}

/******************************************************************************/
/*                                x l c l r t                                 */
/******************************************************************************/

/* Function: xlclrt

   Purpose:  To parse the directive: localroot <path>

             <path>    the path that the server will prefix to all local paths.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xlclrt(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;
    int i;

// If we are a manager, ignore this option
//
   if (!isServer) return CFile.noEcho();

// Get path type
//
   val = CFile.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "localroot path not specified"); return 1;}
   if (*val != '/')
      {eDest->Emsg("Config", "localroot path not absolute"); return 1;}

// Cleanup the path
//
   i = strlen(val)-1;
   while (i && val[i] == '/') val[i--] = '\0';

// Assign new path prefix
//
   if (i)
      {if (LocalRoot) free(LocalRoot);
       LocalRoot = strdup(val);
      }
   return 0;
}
  
/******************************************************************************/
/*                                 x m a n g                                  */
/******************************************************************************/

/* Function: xmang

   Purpose:  Parse: manager [meta | peer | proxy] [all|any]
                            <host>[+][:<port>|<port>] [if ...]

             meta   For cmsd:   Specified the manager when running as a manager
                    For xrootd: The directive is ignored.
             peer   For cmsd:   Specified the manager when running as a peer
                    For xrootd: The directive is ignored.
             proxy  For cmsd:   This directive is ignored.
                    For xrootd: Specifies the cmsd-proxy service manager
             all    Ignored (useful only to the cmsd client)
             any    Ignored (useful only to the cmsd client)
             <host> The dns name of the host that is the cache manager.
                    If the host name ends with a plus, all addresses that are
                    associated with the host are treated as managers.
             <port> The port number to use for this host.
             if     Apply the manager directive if "if" is true. See
                    XrdOucUtils:doIf() for "if" syntax.

   Notes:   Any number of manager directives can be given. 

   Type: Remote server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xmang(XrdSysError *eDest, XrdOucStream &CFile)
{
    class StorageHelper
    {public:
          StorageHelper(char **v1, char **v2) : val1(v1), val2(v2) {}
         ~StorageHelper() {if (*val1) free(*val1);
                           if (*val2) free(*val2);
                          }
    char **val1, **val2;
    };

    XrdOucTList **theList = &ManList;
    char *val, *hSpec = 0, *hPort = 0;
    StorageHelper SHelp(&hSpec, &hPort);
    int rc, xMeta = 0, xPeer = 0, xProxy = 0, *myPort = 0;

// Ignore this call if we are a meta-manager and know our port number
//
   if (isMeta && PortTCP > 0) return CFile.noEcho();

//  Process the optional "meta", "peer" or "proxy"
//
    if ((val = CFile.GetWord()))
       {if ((xMeta  = !strcmp("meta", val))
        ||  (xPeer  = !strcmp("peer", val))
        ||  (xProxy = !strcmp("proxy", val)))
            {if ((xMeta  && (isServer || isPeer))
             ||  (xPeer  && !isPeer)
             ||  (xProxy && !isProxy)) return CFile.noEcho();
            val = CFile.GetWord();
           } else if (isPeer) return CFile.noEcho();
       }

//  We can accept this manager. Skip the optional "all" or "any"
//
    if (val)
       if (!strcmp("any", val) || !strcmp("all", val)) val = CFile.GetWord();

//  Get the actual host name and copy it
//
    if (!val)
       {eDest->Emsg("Config","manager host name not specified"); return 1;}
    hSpec = strdup(val);

//  Grab the port number (either in hostname or following token)
//
    if (!(hPort = XrdCmsUtils::ParseManPort(eDest, CFile, hSpec))) return 1;

// Check if this statement is gaurded by and "if" and process it
//
   if ((val = CFile.GetWord()))
      {if (strcmp(val, "if"))
          {eDest->Emsg("Config","expecting manager 'if' but",val,"found");
           return 1;
          }
       if ((rc = XrdOucUtils::doIf(eDest,CFile,"manager directive",
                                   myName,myInsName,myProg))<=0)
          {if (!rc) CFile.noEcho(); return rc < 0;}
      }

// Calculate the correct queue and port number to update
//
   if (isManager && !isServer)
//    {if (((xMeta && isMeta) || (!xMeta && !isMeta)) && PortTCP < 1)
      {if (((xMeta && isMeta) || (!xMeta && !isMeta)))
          myPort = &PortTCP;
        if (isMeta) theList = 0;
           else theList = (xMeta ? &ManList : &NanList);
       }

// Parse the specification and return
//
   return (XrdCmsUtils::ParseMan(eDest, theList, hSpec, hPort, myPort) ? 0 : 1);
}
  
/******************************************************************************/
/*                                  x n m l                                   */
/******************************************************************************/

/* Function: xnml

   Purpose:  To parse the directive: namelib <path> [<parms>]

             <path>    the path of the filesystem library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xnml(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val, parms[1024];

// Get the path
//
   if (!(val = CFile.GetWord()) || !val[0])
      {eDest->Emsg("Config", "namelib not specified"); return 1;}

// Record the path
//
   if (N2N_Lib) free(N2N_Lib);
   N2N_Lib = strdup(val);

// Record any parms
//
   if (!CFile.GetRest(parms, sizeof(parms)))
      {eDest->Emsg("Config", "namelib parameters too long"); return 1;}
   if (N2N_Parms) free(N2N_Parms);
   N2N_Parms = (*parms ? strdup(parms) : 0);
   return 0;
}
  
/******************************************************************************/
/*                                 x o l i b                                  */
/******************************************************************************/

/* Function: xolib

   Purpose:  To parse the directive: osslib <path>

             <path>    the path of the filesystem library to be used.

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xolib(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val, parms[1024];

// Get the path
//
   if (!(val = CFile.GetWord()) || !val[0])
      {eDest->Emsg("Config", "osslib not specified"); return 1;}
   if (ossLib) free(ossLib);
   ossLib = strdup(val);

// Record any parms
//
   if (!CFile.GetRest(parms, sizeof(parms)))
      {eDest->Emsg("Config", "osslib parameters too long"); return 1;}
   if (ossParms) free(ossParms);
   ossParms = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/*                                 x p e r f                                  */
/******************************************************************************/

/* Function: xperf

   Purpose:  To parse the directive: perf [key <num>] [int <sec>] [pgm <pgm>]

         int <time>    estimated time (seconds, M, H) between reports by <pgm>
         key <num>     This is no longer documented but kept for compatability.
         pgm <pgm>     program to start that will write perf values to standard
                       out. It must be the last option.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure. Ignored by manager.
*/
int XrdCmsConfig::xperf(XrdSysError *eDest, XrdOucStream &CFile)
{   int   ival = 3*60;
    char *pgm=0, *val, rest[2048];

    if (!isServer) return CFile.noEcho();

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "perf options not specified"); return 1;}

    do {     if (!strcmp("int", val))
                {if (!(val = CFile.GetWord()))
                    {eDest->Emsg("Config", "perf int value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(*eDest,"perf int",val,&ival,0)) return 1;
                }
        else if (!strcmp("pgm",  val))
                {if (!CFile.GetRest(rest, sizeof(rest)))
                    {eDest->Emsg("Config", "perf pgm parameters too long"); return 1;}
                 if (!*rest)
                    {eDest->Emsg("Config", "perf prog value not specified");
                     return 1;
                    }
                 pgm = rest;
                 break;
                }
        else eDest->Say("Config warning: ignoring invalid perf option '",val,"'.");
       } while((val = CFile.GetWord()));

// Make sure that the perf program is here
//
   if (perfpgm) {free(perfpgm); perfpgm = 0;}
   if (pgm) {if (!isExec(eDest, "perf", pgm)) return 1;
                else perfpgm = strdup(pgm);
            }

// Set remaining values
//
    perfint = ival;
    return 0;
}

  
/******************************************************************************/
/*                                 x p i d f                                  */
/******************************************************************************/

/* Function: xpidf

   Purpose:  To parse the directive: pidpath <path>

             <path>    the path where the pid file is to be created.

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xpidf(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;

// Get the path
//
   val = CFile.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "pidpath not specified"); return 1;}

// Record the path
//
   if (pidPath) free(pidPath);
   pidPath = strdup(val);
   return 0;
}
  
/******************************************************************************/
/*                                 x p i n g                                  */
/******************************************************************************/

/* Function: xping

   Purpose:  To parse the directive: ping <ptm> [log <num>] [usage <cnt>]

             <ptm>     Time (seconds, M, H. etc) between keepalive pings.
                       The default is 60 seconds.
             log       values are logged to the log every <num> usage
                       requests (default 10). Zero, suppresses logging.
             usage     The number of pings between resource usage requests.
                       The default is 10. Zero suppresses usage requests.

   Note: The defaults will log usage 100 minutes (little less than 2 hours).

   Type: Server for ping value and Manager for all values, dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdCmsConfig::xping(XrdSysError *eDest, XrdOucStream &CFile)
{   int pnum = AskPerf, lnum = LogPerf, ping;
    char *val;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "ping value not specified"); return 1;}
    if (XrdOuca2x::a2tm(*eDest, "ping interval",val,&ping,0)) return 1;
    if (ping < 3) ping = 3;

    while((val = CFile.GetWord()))
        {     if (!strcmp("log", val))
                 {if (!(val = CFile.GetWord()))
                     {eDest->Emsg("Config", "ping log value not specified");
                      return 1;
                     }
                  if (XrdOuca2x::a2i(*eDest,"ping log",val,&lnum,0)) return 1;
                 }
         else if (!strcmp("usage", val))
                 {if (!(val = CFile.GetWord()))
                    {eDest->Emsg("Config", "ping usage value not specified");
                     return 1;
                    }
                  if (XrdOuca2x::a2i(*eDest,"ping usage",val,&pnum,1)) return 1;
                 }
        }
    AskPerf = pnum;
    AskPing = ping;
    LogPerf = lnum;
    return 0;
}
  
/******************************************************************************/
/*                                 x p r e p                                  */
/******************************************************************************/

/* Function: xprep

   Purpose:  To parse the directive: prep  [echo]
                                           [reset <cnt>] [scrub <sec>] 
                                           [ifpgm <pgm>]

         echo          display list of pending prepares during resets.
         reset <cnt>   number of scrubs after which a full reset is done.
         scrub <sec>   time (seconds, M, H) between pendq scrubs.
         ifpgm <pgm>   program that adds, deletes, and lists prepare queue
                       entries. If specified, t must be specified as the last
                       option on the line. If not specified, then the built-in
                       frm_xfragent program is used.

   Type: Any, non-dynamic. Note that the Manager only need the "batch" option
         while slacves need the remaining options.

   Output: 0 upon success or !0 upon failure. Ignored by manager.
*/
int XrdCmsConfig::xprep(XrdSysError *eDest, XrdOucStream &CFile)
{   int   reset=0, scrub=0, echo = 0, doset = 0;
    char  *prepif=0, *val, rest[2048];

    if (!isServer) return CFile.noEcho();

    if (!(val = CFile.GetWord())) {PrepQ.setParms(""); return 0;}

    do {     if (!strcmp("echo", val)) doset = echo = 1;
        else if (!strcmp("reset", val))
                {if (!(val = CFile.GetWord()))
                    {eDest->Emsg("Config", "prep reset value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2i(*eDest,"prep reset int",val,&reset,1)) return 1;
                 doset = 1;
                }
        else if (!strcmp("scrub", val))
                {if (!(val = CFile.GetWord()))
                    {eDest->Emsg("Config", "prep scrub value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(*eDest,"prep scrub",val,&scrub,0)) return 1;
                 doset = 1;
                }
        else if (!strcmp("ifpgm",  val))
                {if (!CFile.GetRest(rest, sizeof(rest)))
                    {eDest->Emsg("Config", "prep ifpgm parameters too long"); return 1;}
                 if (!*rest)
                    {eDest->Emsg("Config", "prep ifpgm value not specified");
                     return 1;
                    }
                 prepif = rest;
                 break;
                }
        else eDest->Say("Config warning: ignoring invalid prep option '",val,"'.");
       } while((val = CFile.GetWord()));



// Set the values
//
   if (scrub) pendplife = scrub;
   if (doset) PrepQ.setParms(reset, scrub, echo);
   if (prepif) {if (!isExec(eDest, "prep", prepif)) return 1;
                   else return PrepQ.setParms(prepif);
               } else PrepQ.setParms("");
   return 0;
}

/******************************************************************************/
/*                                x p r e p m                                 */
/******************************************************************************/

/* Function: xprepm

   Purpose:  To parse the directive: prepmsg <msg>

             <msg>     the message to be sent to the prep ifpgm (see prep).

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xprepm(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val, buff[2048];
    XrdOucEnv *myEnv = CFile.SetEnv(0);

   // At this point, make sure we have a value
   //
   if (!(val = CFile.GetWord()))
      {eDest->Emsg("Config", "no value for prepmsg directive");
       CFile.SetEnv(myEnv);
       return 1;
      }

   // We need to suck all the tokens to the end of the line for remaining
   // options. Do so, until we run out of space in the buffer.
   //
   CFile.RetToken();
   if (!CFile.GetRest(buff, sizeof(buff)))
      {eDest->Emsg("Config", "prepmsg arguments too long");
       CFile.SetEnv(myEnv);
       return 1;
      }

   // Restore substitutions and parse the message
   //
   CFile.SetEnv(myEnv);
   return PrepQ.setParms(0, buff);
}
  
/******************************************************************************/
/*                                 x r e p s                                  */
/******************************************************************************/

/* Function: xreps

   Purpose:  To parse the directive: repstats <options>

   Type: Manager or Server, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xreps(XrdSysError *eDest, XrdOucStream &CFile)
{
    char  *val;
    static struct repsopts {const char *opname; int opval;} rsopts[] =
       {
        {"all",      RepStat_All},
        {"frq",      RepStat_frq},
        {"shr",      RepStat_shr}
       };
    int i, neg, rsval = 0, numopts = sizeof(rsopts)/sizeof(struct repsopts);

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("config", "repstats option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) rsval = 0;
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, rsopts[i].opname))
                           {if (neg) rsval &= ~rsopts[i].opval;
                               else  rsval |=  rsopts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      eDest->Say("Config warning: ignoring invalid repstats option '",val,"'.");
                  }
          val = CFile.GetWord();
         }

    RepStats = rsval;
    return 0;
}

/******************************************************************************/
/*                                x r m t r t                                 */
/******************************************************************************/

/* Function: xrmtrt

   Purpose:  To parse the directive: remoteroot <path>

             <path>    the path that the server will prefix to all remote paths.

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xrmtrt(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val, *colon, *slash;
    int i;

// If we are a manager, ignore this option
//
   if (isManager) return CFile.noEcho();

// Get path type
//
   val = CFile.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "remoteroot path not specified"); return 1;}

// For remote roots we allow a url-type specification o/w path must be absolute
//
   if (*val != '/')
      {colon = index(val, ':'); slash = index(val, '/');
       if ((colon+1) != slash)
          {eDest->Emsg("Config", "remoteroot path not absolute"); return 1;}
      }

// Cleanup the path
//
   i = strlen(val)-1;
   while (i && val[i] == '/') val[i--] = '\0';

// Assign new path prefix
//
   if (i)
      {if (RemotRoot) free(RemotRoot);
       RemotRoot = strdup(val);
      }
   return 0;
}

/******************************************************************************/
/*                                 x r o l e                                  */
/******************************************************************************/

/* Function: xrole
   Purpose:  Parse: role { {[meta] | [peer] [proxy]} manager
                           | peer | proxy | [proxy]  server
                           |                [proxy]  supervisor
                         } [if ...]

             manager    xrootd: act as a manager (redirecting server). Prefixes:
                                meta  - connect only to manager meta's
                                peer  - ignored
                                proxy - ignored
                        cmsd:   accept server subscribes and redirectors. Prefix
                                modifiers do the following:
                                meta  - No other managers apply
                                peer  - subscribe to other managers as a peer
                                proxy - manage a cluster of proxy servers

             peer       xrootd: same as "peer manager"
                        cmsd:   same as "peer manager" but no server subscribers
                                are required to function (i.e., run stand-alone).

             proxy      xrootd: act as a server but supply data from another 
                                server. No local cmsd is present or required.
                        cmsd:   Generates an error as this makes no sense.

             server     xrootd: act as a server (supply local data). Prefix
                                modifications do the following:
                                proxy - server is part of a cluster. A local
                                        cmsd is required.
                        cmsd:   subscribe to a manager, possibly as a proxy.

             supervisor xrootd: equivalent to manager.
                        cmsd:   equivalent to manager but also subscribe to a
                                manager. When proxy is specified, subscribe as
                                a proxy and only accept proxy servers.


             if         Apply the manager directive if "if" is true. See
                        XrdOucUtils:doIf() for "if" syntax.


   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xrole(XrdSysError *eDest, XrdOucStream &CFile)
{
    XrdCmsRole::RoleID roleID;
    char *val, *Tok1, *Tok2;
    int rc, xMeta=0, xPeer=0, xProxy=0, xServ=0, xMan=0, xSolo=0;

// Get the first token
//
   if (!(val = CFile.GetWord()) || !strcmp(val, "if"))
      {eDest->Emsg("Config", "role not specified"); return 1;}
   Tok1 = strdup(val);

// Get second token which might be an "if"
//
   if ((val = CFile.GetWord()) && strcmp(val, "if"))
      {Tok2 = strdup(val);
       val = CFile.GetWord();
      } else Tok2 = 0;

// Process the if at this point
//
   if (val && !strcmp("if", val))
      if ((rc = XrdOucUtils::doIf(eDest,CFile,"role directive",
                             myName,myInsName,myProg)) <= 0)
         {free(Tok1); if (Tok2) free(Tok2);
          if (!rc) CFile.noEcho();
          return (rc < 0);
         }

// Convert the role names to a role ID, if possible
//
   roleID = XrdCmsRole::Convert(Tok1, Tok2);

// Set markers based on the role we have
//
   rc = 0;
   switch(roleID)
         {case XrdCmsRole::MetaManager:  xMeta  = xMan  =         -1; break;
          case XrdCmsRole::Manager:               xMan  =         -1; break;
          case XrdCmsRole::Supervisor:            xMan  = xServ = -1; break;
          case XrdCmsRole::Server:                        xServ = -1; break;
          case XrdCmsRole::ProxyManager: xProxy = xMan  =         -1; break;
          case XrdCmsRole::ProxySuper:   xProxy = xMan  = xServ = -1; break;
          case XrdCmsRole::ProxyServer:  xProxy =         xServ = -1; break;
          case XrdCmsRole::PeerManager:  xPeer  = xMan  =         -1; break;
          case XrdCmsRole::Peer:         xPeer  = xSolo = xServ   -1; break;
          default: eDest->Emsg("Config", "invalid role -", Tok1, Tok2); rc = 1;
         }

// Release storage and return if an error occured
//
   free(Tok1);
   if (Tok2) free(Tok2);
   if (rc) return rc;

// If the role was specified on the command line, issue warning and ignore this
//
   if (isServer > 0 || isManager > 0 || isProxy > 0 || isPeer > 0)
      {eDest->Say("Config warning: role directive over-ridden by command line.");
       return 0;
      }

// Fill out information
//
   isServer = xServ; isManager = xMan;  isProxy = xProxy;
   isPeer   = xPeer; isSolo    = xSolo; isMeta  = xMeta;
   if (myRole) free(myRole);
   myRole   = strdup(XrdCmsRole::Name(roleID));
   myRoleID = static_cast<int>(roleID);
   strcpy(myRType, XrdCmsRole::Type(roleID));
   return 0;
}

/******************************************************************************/
/*                                x s c h e d                                 */
/******************************************************************************/

/* Function: xsched

   Purpose:  To parse directive: sched [cpu <p>] [gsdflt <p>] [gshr <p>]
                                       [io <p>] [runq <p>]
                                       [mem <p>] [pag <p>] [space <p>]
                                       [fuzz <p>] [maxload <p>] [refreset <sec>]

             <p>      is the percentage to include in the load as a value
                      between 0 and 100. For fuzz this is the largest
                      difference two load values may have to be treated equal.
                      maxload is the largest load allowed before server is
                      not selected. refreset is the minimum number of seconds
                      between reference counter resets. gshr is the percentage
                      share of requests that should be redirected here via the 
                      metamanager (i.e. global share). The gsdflt is the
                      default to be used by the metamanager.

   Type: Any, dynamic.

   Output: retc upon success or -EINVAL upon failure.
*/

int XrdCmsConfig::xsched(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;
    int  i, ppp;
    static struct schedopts {const char *opname; int maxv; int *oploc;}
           scopts[] =
       {
        {"cpu",      100, &P_cpu},
        {"fuzz",     100, &P_fuzz},
        {"gsdflt",   100, &P_gsdf},
        {"gshr",     100, &P_gshr},
        {"io",       100, &P_io},
        {"runq",     100, &P_load}, // Actually load, runq to avoid confusion
        {"mem",      100, &P_mem},
        {"pag",      100, &P_pag},
        {"space",    100, &P_dsk},
        {"maxload",  100, &MaxLoad},
        {"refreset", -1,  &RefReset}
       };
    int numopts = sizeof(scopts)/sizeof(struct schedopts);

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "sched option not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, scopts[i].opname))
                  {if (!(val = CFile.GetWord()))
                      {eDest->Emsg("Config", "sched ", scopts[i].opname,
                                   "argument not specified.");
                       return 1;
                      }
                   if (scopts[i].maxv < 0)
                      {if (XrdOuca2x::a2tm(*eDest,"sched value", val, &ppp, 0)) 
                          return 1;
                      }
                      else if (XrdOuca2x::a2i(*eDest,"sched value", val, &ppp,
                                              0, scopts[i].maxv)) return 1;
                   *scopts[i].oploc = ppp;
                   break;
                  }
           if (i >= numopts)
              eDest->Say("Config warning: ignoring invalid sched option '",val,"'.");
           val = CFile.GetWord();
          }

    return 0;
}

/******************************************************************************/
/*                                 x s e c l                                  */
/******************************************************************************/

/* Function: xsecl

   Purpose:  To parse the directive: seclib <path>

             <path>    the location of the security library.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xsecl(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;

// If we are a server, ignore this option
//
   if (!isManager) return CFile.noEcho();

// Get path
//
   val = CFile.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "seclib path not specified"); return 1;}

// Assign new path
//
   if (SecLib) free(SecLib);
   SecLib = strdup(val);
   return 0;
}
  
/******************************************************************************/
/*                                x s p a c e                                 */
/******************************************************************************/

/* Function: xspace

   Purpose:  To parse the directive: space [linger <num>] [recalc <sec>]

                          [[min] {<mnp> [<min>] | <min>}  [[<hwp>] <hwm>]]

                          [mwfiles]

             <num> Maximum number of times a server may be reselected without
                   a break. The default is 0.

             <mnp> Min free space needed as percentage of the largest partition.

             <min> Min free space needed in bytes (or K, M, G) in a partition.
                   The default is 10G.

             <hwp> Percentage of free space needed to requalify.

             <hwm> Bytes (or K, M,G) of free space needed when bytes falls below
                   <min> to requalify a server for selection.
                   The default is 11G.

             <sec> Number of seconds that must elapse before a disk free space
                   calculation will occur.

             mwfiles
                   space supports multiple writable file copies. This suppresses
                   multiple file check when open a file in write mode.

   Notes:   This is used by the manager and the server.

   Type: All, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xspace(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val;
    int i, alinger = -1, arecalc = -1, minfP = -1, hwmP = -1;
    long long minf = -1, hwm = -1;

    while((val = CFile.GetWord()))
      {    if (!strcmp("linger", val))
              {if (!(val = CFile.GetWord()))
                  {eDest->Emsg("Config", "linger value not specified"); return 1;}
               if (XrdOuca2x::a2i(*eDest,"linger",val,&alinger,0)) return 1;
              }
      else if (!strcmp("recalc", val))
              {if (!(val = CFile.GetWord()))
                  {eDest->Emsg("Config", "recalc value not specified"); return 1;}
               if (XrdOuca2x::a2i(*eDest,"recalc",val,&arecalc,1)) return 1;
              }
      else if (!strcmp("min", val))
              {if (!(val = CFile.GetWord()) || !isdigit(*val))
                  {eDest->Emsg("Config", "space min value not specified"); return 1;}
               break;
              }
      else if (!strcmp("mwfiles", val)) DoMWChk = 0;
      else if (isdigit(*val)) break;
      else {eDest->Emsg("Config", "invalid space parameters"); return 1;}
      }

    if (val && isdigit(*val))
       {i = strlen(val);
        if (val[i-1] == '%')
           {val[i-1] = '\0';
            if (XrdOuca2x::a2i(*eDest,"space % minfree",val,&minfP,1,99)) return 1;
            val = CFile.GetWord();
           }
       }

    if (val && isdigit(*val))
       {i = strlen(val);
        if (val[i-1] != '%')
           {if (XrdOuca2x::a2sz(*eDest,"space minfree",val,&minf,0)) return 1;
            val = CFile.GetWord();
           }
       }

    if (minfP >= 0 && minf < 0)
       {eDest->Emsg("Config", "absolute min value not specified"); return 1;}

    if (val && isdigit(*val))
       {i = strlen(val);
        if (val[i-1] == '%')
           {val[i-1] = '\0';
            if (XrdOuca2x::a2i(*eDest,"space % high watermark",val,&hwmP,1,99)) return 1;
            val = CFile.GetWord();
           }
       }

    if (val && isdigit(*val))
       {i = strlen(val);
        if (val[i-1] != '%')
           {if (XrdOuca2x::a2sz(*eDest,"space high watermark",val,&hwm,0)) return 1;
            val = CFile.GetWord();
           }
       }

    if (hwmP >= 0 && hwm < 0)
       {eDest->Emsg("Config", "absolute high watermark value not specified"); return 1;}

    if (val) {eDest->Emsg("Config", "invalid space parameter -", val); return 1;}
    
    if (alinger < 0 && arecalc < 0 && minf < 0)
       {eDest->Emsg("Config", "no space values specified"); return 1;}

    if (alinger >= 0) DiskLinger = alinger;
    if (arecalc >= 0) DiskAsk    = arecalc;

    if (minfP > 0)
       {if (hwmP < minfP) hwmP = minfP + 1;
        DiskMinP = minfP; DiskHWMP = hwmP;
       } else DiskMinP = DiskHWMP = 0;

    if (minf >= 0)
       {if (hwm < minf) hwm = minf+1073741824;     // Minimum + 1GB
        minf = minf >> 20LL; hwm = hwm >> 20LL;    // Now Megabytes
        if (minf >> 31LL) {minf = 0x7fefffff; hwm = 0x7fffffff;}
           else if (hwm >> 31LL) minf = 0x7fffffff;
        DiskMin = static_cast<int>(minf);
        DiskHWM = static_cast<int>(hwm);
       }
    return 0;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <options>

   Type: Manager or Server, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xtrace(XrdSysError *eDest, XrdOucStream &CFile)
{
    char  *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"defer",    TRACE_Defer},
        {"files",    TRACE_Files},
        {"forward",  TRACE_Forward},
        {"redirect", TRACE_Redirect},
        {"stage",    TRACE_Stage}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("config", "trace option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) trval = 0;
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, tropts[i].opname))
                           {if (neg) trval &= ~tropts[i].opval;
                               else  trval |=  tropts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      eDest->Say("Config warning: ignoring invalid trace option '",val,"'.");
                  }
          val = CFile.GetWord();
         }

    Trace.What = trval;
    return 0;
}
  
/******************************************************************************/
/*                                  x x m i                                   */
/******************************************************************************/

/* Function: xxmi

   Purpose:  To parse the directive: xmilib <path> [<parms>]

             <path>    the SO path for the XrdCmsXmi plugin.
             <parms>   optional parms to be passed to the Xmi object

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsConfig::xxmi(XrdSysError *eDest, XrdOucStream &CFile)
{
    char *val, parms[1024];

// Get the path
//
   if (!(val = CFile.GetWord()) || !val[0])
      {eDest->Emsg("Config", "xmilib path not specified"); return 1;}

// Record the path
//
   if (XmiPath) free(XmiPath);
   XmiPath = strdup(val);

// Record any parms
//
   if (!CFile.GetRest(parms, sizeof(parms)))
      {eDest->Emsg("Config", "xmilib parameters too long"); return 1;}
   if (XmiParms) free(XmiParms);
   XmiParms = (*parms ? strdup(parms) : 0);

   return 0;
}
