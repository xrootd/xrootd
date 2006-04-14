/******************************************************************************/
/*                                                                            */
/*                       X r d O l b C o n f i g . c c                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

const char *XrdOlbConfigCVSID = "$Id$";

/*
   The routines in this file handle olb() initialization. They get the
   configuration values either from configuration file or XrdOlbconfig.h (in that
   order of precedence).

   These routines are thread-safe if compiled with:
   AIX: -D_THREAD_SAFE
   SUN: -D_REENTRANT
*/
  
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <iostream.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <dirent.h>

#include "../XrdVersion.hh"
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdOlb/XrdOlbAdmin.hh"
#include "XrdOlb/XrdOlbCache.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbMeter.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbPrepare.hh"
#include "XrdOlb/XrdOlbRRQ.hh"
#include "XrdOlb/XrdOlbState.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOlb/XrdOlbTypes.hh"
#include "XrdNet/XrdNetDNS.hh"
#include "XrdNet/XrdNetLink.hh"
#include "XrdNet/XrdNetWork.hh"
#include "XrdNet/XrdNetSecurity.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucPlugin.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"

using namespace XrdOlb;

/******************************************************************************/
/*                  C o m m a n d   L i n e   O p t i o n s                   */
/******************************************************************************/
/*
   olbd [options] [configfn]

   options: [xopt] [-i] [-m] [-s] [-w]

Where:
    xopt  Are Xrd processed options (some of which we use).

   -i     Immediate start-up (do not wait for a server connection).

   -m     function in manager moede.

   -s     Executes in server  mode.
*/

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

      XrdScheduler    *XrdOlb::Sched = 0;
      XrdOlbConfig     XrdOlb::Config;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOlbStartMonPing(void *carg) { return Manager.MonPing(); }

void *XrdOlbStartMonPerf(void *carg) { return Manager.MonPerf(); }

void *XrdOlbStartMonRefs(void *carg) { return Manager.MonRefs(); }

void *XrdOlbStartMonStat(void *carg) { return OlbState.Monitor(); }

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
       return Manager.Pander(tp->text, tp->val);
      }

void *XrdOlbStartSupervising(void *carg)
      {EPNAME("StartSuper");
       XrdNetWork *NetTCPr = (XrdNetWork *)carg;
       XrdNetLink *newlink;
       while(1) if ((newlink = NetTCPr->Accept(XRDNET_NODNTRIM)))
                   {DEBUG("olbd: FD " <<newlink->FDnum() <<" connected to " <<newlink->Nick());
                    Manager.Login(newlink);
                   }
       return (void *)0;
      }
/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_String(x,m) if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(eDest, CFile);

#define TS_Set(x,v)    if (!strcmp(x,var)) {v=1; return 0;}

#define TS_unSet(x,v)  if (!strcmp(x,var)) {v=0; return 0;}

/******************************************************************************/
/*                            C o n f i g u r e 1                             */
/******************************************************************************/
  
int XrdOlbConfig::Configure1(int argc, char **argv, char *cfn)
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
      {Say.Emsg("Config", "Security reasons prohibit olbd running as "
                  "superuser; olbd is terminating.");
       _exit(8);
      }

// Process the options
//
   opterr = 0; optind = 1;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c=getopt(argc,argv,"imsw")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'i': immed = 1;
                 break;
       case 'm': isManager = 1;
                 break;
       case 's': isServer = 1;
                 break;
       case 'w': immed = -1;   // Backward compatability only
                 break;
       default:  buff[0] = '-'; buff[1] = optopt; buff[2] = '\0';
                 Say.Emsg("Config","Unrecognized option,",buff,", ignored.");
       }
     }

// Bail if no configuration file specified
//
   if (!(ConfigFN = cfn) && !(ConfigFN = getenv("XrdOlbCONFIGFN")) || !*ConfigFN)
      {Say.Emsg("Config", "Required config file not specified.");
       Usage(1);
      }

// Establish my instance name
//
   sprintf(buff, "%s@%s", (myInsName ? myInsName : "anon"), myName);
   myInstance = strdup(buff);

// Print herald
//
   Say.Say(0, myInstance, " phase 1 initialization started.");

// If we don't know our role yet then we must find out before processing the
// config file. This means a double scan, sigh.
//
   if (!(isManager || isServer)) 
      if (!(NoGo |= ConfigProc(1)) && !(isManager || isServer))
         {Say.Emsg("Config", "Role not specified; manager role assumed.");
          isManager = -1;
         }

// Process the configuration file
//
   if (!NoGo) NoGo |= ConfigProc();

// Override the wait/nowait from the command line
//
   if (immed) doWait = (immed > 0 ? 0 : 1);

// Determine the role
//
   if (isManager < 0) isManager = 1;
   if (isServer  < 0) isServer  = 1;

// For managers, make sure that we have a well designated port. If we are a
// server or a supervisor then force an ephemeral port to be used.
//
   if (!NoGo)
      if (isManager && !isServer)
         {if (PortTCP < 0)
             {Say.Emsg("Config","Manager's port not specified."); NoGo = 1;}
         }
         else PortTCP = 0;

// Determine how we ended and return status
//
   sprintf(buff, " phase 1 initialization %s.", (NoGo ? "failed":"ended"));
   Say.Say(0, myInstance, buff);
   return NoGo;
}

/******************************************************************************/
/*                            C o n f i g u r e 2                             */
/******************************************************************************/
  
int XrdOlbConfig::Configure2()
{
/*
  Function: Establish phase 2 configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   int NoGo = 0;
   char *p, buff[512];
   const char *temp, *smtype = 0;

// Print herald
//
   Say.Say(0, myInstance, " phase 2 initialization started.");

// Determine who we are. If we are a manager or supervisor start the file
// location cache scrubber.
//
   if (isManager) 
      {smtype = "manager";
       XrdJob *jp=(XrdJob *)new XrdOlbCache_Scrubber(&Cache, Sched);
       Sched->Schedule(jp, cachelife+time(0));
      }
   if (isServer) smtype = "server";
   if (isServer && isManager) smtype = "supervisor";

// Establish the path to be used for admin functions
//
   p = XrdOucUtils::genPath(AdminPath,(strcmp("anon",myInsName)?myInsName:0),".olb");
   free(AdminPath);
   AdminPath = p;

// Setup the admin path (used in all roles)
//
   if (!NoGo) NoGo = !(AdminSock = ASocket(AdminPath,
                      (isManager ? "olbd.nimda" : "olbd.admin"), AdminMode));

// Develop a stable unique identifier for this olbd independent of the port
//
   if (!NoGo)
      {char sidbuf[1024];
       sprintf(sidbuf, "%s%c", AdminPath, (isManager ? 'm' : 's'));
       mySID = strdup(sidbuf);
      }

// Setup manager or server, as needed
//
  if (!NoGo && isManager) NoGo = setupManager();
  if (!NoGo && isServer)  NoGo = setupServer();

// Set up the generic message id
//
   MsgGIDL = sprintf(buff, "%d@0 ", MsgTTL);
   MsgGID  = strdup(buff);

// Create the pid file
//
   if (!NoGo) NoGo |= PidFile();

// All done, check for success or failure
//
   sprintf(buff, "%s:%d %s ", myInstance, PortTCP, smtype);
   temp = (NoGo ? "phase 2 initialization failed." 
                : "phase 2 initialization completed.");
   Say.Say(0, buff, temp);

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

int XrdOlbConfig::ConfigXeq(char *var, XrdOucStream &CFile, XrdOucError *eDest)
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
   TS_Xeq("space",         xspace);  // Any,        dynamic
   TS_Xeq("trace",         xtrace);  // Any,        dynamic

   if (!dynamic)
   {
   TS_Xeq("cache",         xcache);  // Server,  non-dynamic
   TS_Xeq("adminpath",     xapath);  // Any,     non-dynamic
   TS_Xeq("allow",         xallow);  // Manager, non-dynamic
   TS_Xeq("fsxeq",         xfsxq);   // Server,  non-dynamic
   TS_Xeq("localroot",     xlclrt);  // Server,  non-dynamic
   TS_Xeq("namelib",       xnml);    // Server,  non-dynamic
   TS_Xeq("path",          xpath);   // Server,  non-dynamic
   TS_Xeq("perf",          xperf);   // Server,  non-dynamic
   TS_Xeq("pidpath",       xpidf);   // Any,     non-dynamic
   TS_Xeq("port",          xport);   // Any,     non-dynamic
   TS_Xeq("prep",          xprep);   // Any,     non-dynamic
   TS_Xeq("role",          xrole);   // Server,  non-dynamic
   TS_Xeq("subscribe",     xsubs);   // Server,  non-dynamic
   TS_Set("wait",          doWait);  // Server,  non-dynamic (backward compat)
   TS_unSet("nowait",      doWait);  // Server,  non-dynamic
   }

   // No match found, complain.
   //
   eDest->Emsg("Config", "Warning, unknown directive", var);
   return 0;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdOlbConfig::DoIt()
{
   XrdOucSemaphore SyncUp(0);
   XrdOucTList    *tp;
   pthread_t       tid;
   time_t          eTime = time(0);
   int             wTime;

// Start the notification thread if we need to
//
   if (AnoteSock)
      XrdOucThread::Run(&tid, XrdOlbStartAnote, (void *)AnoteSock,
                        0, "Notification handler");

// Start the admin thread if we need to, we will not continue until told
// to do so by the admin interface.
//
   if (AdminSock)
      {XrdOlbAdmin::setSync(&SyncUp);
       XrdOucThread::Run(&tid, XrdOlbStartAdmin, (void *)AdminSock,
                         0, "Admin traffic");
       SyncUp.Wait();
      }

// Start the supervisor subsystem
//
   if (NetTCPr)
      {if (XrdOucThread::Run(&tid,XrdOlbStartSupervising, 
                             (void *)NetTCPr, 0, "supervisor"))
          {Say.Emsg("olbd", errno, "start supervisor");
          return;
          }
      }

// Start the server subsystem
//
   if (isServer)
      {tp = myManagers;
       while(tp)
            {if (!isManager && !tp->next) Manager.Pander(tp->text, tp->val);
                else {if (XrdOucThread::Run(&tid,XrdOlbStartPandering,(void *)tp,
                                            0, tp->text))
                         {Say.Emsg("olbd", errno, "start server");
                          return;
                         }
                      }
             tp = tp->next;
            }
      }

// If we are a manager then we must do a service enable after a service delay
//
   if (isManager)
      {wTime = SRVDelay - static_cast<int>((time(0) - eTime));
       if (wTime > 0) XrdOucTimer::Wait(wTime*1000);
       Disabled = 0;
       Say.Emsg("Config", "Service enabled.");
      }
}

/******************************************************************************/
/*                          G e n L o c a l P a t h                           */
/******************************************************************************/
  
/* GenLocalPath() generates the path that a file will have in the local file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least XrdOlbMAX_PATH_LEN bytes long.
*/
int XrdOlbConfig::GenLocalPath(const char *oldp, char *newp)
{
    if (lcl_N2N) return -(lcl_N2N->lfn2pfn(oldp, newp, XrdOlbMAX_PATH_LEN));
    if (strlen(oldp) >= XrdOlbMAX_PATH_LEN) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}

/******************************************************************************/
/*                              G e n M s g I D                               */
/******************************************************************************/

int XrdOlbConfig::GenMsgID(char *oldmid, char *buff, int blen)
{
   char *ep;
   int msgnum, midlen;

// Find the id separator, if none, allow the message to be forwarded only 
// one additional time (compatability feature)
//
   msgnum = strtol(oldmid, &ep, 10);
   if (*ep != '@') {msgnum = 1; ep = oldmid;}
      else if (msgnum <= 1) return 0;
              else {msgnum--; ep++;}

// Format new msgid
//
   midlen = snprintf(buff, blen, "%d@%s ", msgnum, ep);
   if (midlen < 0 || midlen >= blen) return 0;
   return midlen;
}
  
/******************************************************************************/
/*                             i n N o S t a g e                              */
/******************************************************************************/

int  XrdOlbConfig::inNoStage()
{
   struct stat buff;

   return (!stat(NoStageFile, &buff));
}

/******************************************************************************/
/*                             i n S u s p e n d                              */
/******************************************************************************/
  
int  XrdOlbConfig::inSuspend()
{
   struct stat buff;

   return (!stat(SuspendFile, &buff));
}
  
/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                               A S o c k e t                                */
/******************************************************************************/
  
XrdNetSocket *XrdOlbConfig::ASocket(char *path, const char *fn, mode_t mode, 
                                    int isudp)
{
   XrdNetSocket *ASock;
   int sflags = (isudp ? XRDNET_UDPSOCKET : 0);
   char *fnp;

// Setup the path
//
   if (!(fnp = ASPath(path, fn, mode))) return (XrdNetSocket *)0;

// Connect to the path
//
   ASock = new XrdNetSocket(&Say);
   if (ASock->Open(fnp, -1, XRDNET_SERVER|sflags) < 0)
      {Say.Emsg("Config",ASock->LastError(),"establish socket",fnp);
       delete ASock;
       return (XrdNetSocket *)0;
      }

// Set the mode and return the socket object
//
   chmod(fnp, mode); // This may fail on some platforms
   return ASock;
}

/******************************************************************************/
/*                                A S P a t h                                 */
/******************************************************************************/

char *XrdOlbConfig::ASPath(char *path, const char *fn, mode_t mode)
{
   int rc, i;
   struct stat buf;
   static char fnbuff[1024];

// Create the directory if it is not already there
//
   if ((rc = XrdOucUtils::makePath(path, mode)))
      {Say.Emsg("Config", errno, "create admin path", path);
       return 0;
      }

// Construct full filename
//
   i = strlen(path);
   strcpy(fnbuff, path);
   if (path[i-1] != '/') fnbuff[i++] = '/';
   strcpy(fnbuff+i, fn);

// Check is we have already created it and whether we can access
//
   if (!stat(fnbuff,&buf))
      {if ((buf.st_mode & S_IFMT) != S_IFSOCK)
          {Say.Emsg("Config","Path",fnbuff,"exists but is not a socket");
           return 0;
          }
       if (access(fnbuff, W_OK))
          {Say.Emsg("Config", errno, "access path", fnbuff);
           return 0;
          }
      }

// All set now
//
   return fnbuff;
}

/******************************************************************************/
/*                        C o n f i g D e f a u l t s                         */
/******************************************************************************/

void XrdOlbConfig::ConfigDefaults(void)
{

// Preset all variables with common defaults
//
   myName   = (char *)"localhost"; // Correctly set in Configure()
   myDomain = 0;
   LUPDelay = 5;
   LUPHold  = 133;
   DRPDelay = 10*60;
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
   P_io     = 0;
   P_load   = 0;
   P_mem    = 0;
   P_pag    = 0;
   AskPerf  = 10;         // Every 10 pings
   AskPing  = 60;         // Every  1 minute
   MaxDelay = -1;
   LogPerf  = 10;         // Every 10 usage requests
   DiskMin  = 10485760LL; // 10737418240/1024 10GB (Min partition space) in KB
   DiskHWM  = 11534336LL; // 11811160064/1024 11GB (High Water Mark SUO) in KB
   DiskAsk  = 12;         // 15 Seconds between space calibrations.
   DiskWT   = 0;          // Do not defer when out of space
   DiskSS   = 0;          // Not a staging server
   ConfigFN = 0;
   sched_RR = 0;
   isManager= 0;
   isServer = 0;
   N2N_Lib  = 0;
   N2N_Parms= 0;
   lcl_N2N  = 0;
   LocalRoot= 0;
   myInsName= 0;
   myManagers=0;
   mySID    = 0;
   perfint  = 3*60;
   perfpgm  = 0;
   AdminPath= strdup("/tmp/");
   AdminMode= 0700;
   AdminSock= 0;
   AnoteSock= 0;
   RedirSock= 0;
   pidPath  = strdup("/tmp");
   Police   = 0;
   monPath  = 0;
   monPathP = 0;
   cachelife= 8*60*60;
   pendplife=   60*60*24*7;
   DiskLinger=0;
   ProgCH   = 0;
   ProgMD   = 0;
   ProgMV   = 0;
   ProgRD   = 0;
   ProgRM   = 0;
   doWait   = 1;
   Disabled = 1;
   RefReset = 60*60;
   RefTurn  = 3*XrdOlbManager::STMax*(DiskLinger+1);
   NoStageFile = 0;
   SuspendFile = 0;
   NetTCPr     = 0;
}
  
  
/******************************************************************************/
/*                             C o n f i g N 2 N                              */
/******************************************************************************/

int XrdOlbConfig::ConfigN2N()
{
   XrdOucPlugin    *myLib;
   XrdOucName2Name *(*ep)(XrdOucgetName2NameArgs);

// If we have no library path then use the default method (this will always
// succeed).
//
   if (!N2N_Lib && LocalRoot)
      {lcl_N2N = XrdOucgetName2Name(&Say, ConfigFN, "", LocalRoot, 0);
       return 0;
      }

// Create a pluin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   if (!(myLib = new XrdOucPlugin(&Say, N2N_Lib))) return 1;

// Now get the entry point of the object creator
//
   ep = (XrdOucName2Name *(*)(XrdOucgetName2NameArgs))(myLib->getPlugin("XrdOucgetName2Name"));
   if (!ep) return 1;

// Get the Object now
//
   lcl_N2N = ep(&Say,ConfigFN,(N2N_Parms ? "" : N2N_Parms),LocalRoot,0);
   return lcl_N2N == 0;
}

/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdOlbConfig::ConfigProc(int getrole)
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucStream CFile(&Say, myInstance);

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Say.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   CFile.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = CFile.GetMyFirstWord()))
        if (getrole)
           {if (!strcmp("olb.role", var)) NoGo |=  xrole(&Say, CFile);}
           else {if (!strncmp(var, "olb.", 4) || !strncmp(var, "all.", 4))
                    {var += 4;
                     NoGo |= ConfigXeq(var, CFile, 0);
                    } else
                 if (!strcmp(var, "oss.cache") 
                 ||  !strcmp(var, "oss.localroot")
                 ||  !strcmp(var, "oss.namelib"))
                    {var += 4;
                     NoGo |= ConfigXeq(var, CFile, 0);
                    }
                 }

// Now check if any errors occured during file i/o
//
   if ((retc = CFile.LastError()))
      NoGo = Say.Emsg("Config", retc, "read config file", ConfigFN);
   CFile.Close();

// Return final return code
//
   return NoGo;
}
 
/******************************************************************************/
/*                                i s E x e c                                 */
/******************************************************************************/
  
int XrdOlbConfig::isExec(XrdOucError *eDest, const char *ptype, char *prog)
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
/*                               P i d F i l e                                */
/******************************************************************************/
  
int XrdOlbConfig::PidFile()
{
    int rc, xfd;
    char buff[1024];
    char pidFN[1200], *ppath=XrdOucUtils::genPath(pidPath,
                                        (strcmp("anon",myInsName)?myInsName:0));
    const char *xop = 0;

    if ((rc = XrdOucUtils::makePath(ppath, XrdOucUtils::pathMode)))
       {Say.Emsg("Config", rc, "create pid file path", ppath);
        free(ppath);
        return 1;
       }

         if (isManager && isServer)
            snprintf(pidFN, sizeof(pidFN), "%s/olbd.super.pid", ppath);
    else if (isServer)
            snprintf(pidFN, sizeof(pidFN), "%s/olbd.pid", ppath);
    else    snprintf(pidFN, sizeof(pidFN), "%s/olbd.mangr.pid", ppath);

    if ((xfd = open(pidFN, O_WRONLY|O_CREAT|O_TRUNC,0644)) < 0) xop = "open";
       else {if ((write(xfd, buff, snprintf(buff,sizeof(buff),"%d",getpid())) < 0)
             || (LocalRoot && (write(xfd,(void *)"\n&pfx=",6)  < 0 ||
                               write(xfd,(void *)LocalRoot,strlen(LocalRoot)) < 0))
             || (AdminPath && (write(xfd,(void *)"\n&ap=", 5)  < 0 ||
                               write(xfd,(void *)AdminPath,strlen(AdminPath)) < 0))
                ) xop = "write";
             close(xfd);
            }

     if (xop) Say.Emsg("Config", errno, xop, pidFN);
     return xop != 0;
}

/******************************************************************************/
/*                          s e t u p M a n a g e r                           */
/******************************************************************************/
  
int XrdOlbConfig::setupManager()
{
   pthread_t tid;
   int rc;

// Setup a local connection when in supervisor role
//
   if (isServer)
      {char *fnp;
       if (!(fnp = ASPath(AdminPath, "olbd.super", AdminMode))) return 1;
       if (!(NetTCPr = new XrdNetWork(&Say)))
          {Say.Emsg("Config","Unable to create supervisor interface.");
           return 1;
          }
       if (myDomain) NetTCPr->setDomain(myDomain);
       if (NetTCPr->Bind(fnp, "tcp")) return 1;
      }

// Compute the scheduling policy
//
   sched_RR = (100 == P_fuzz) || !AskPerf
              || !(P_cpu || P_io || P_load || P_mem || P_pag);
   if (sched_RR)
      Say.Emsg("Config", "Round robin scheduling in effect.");

// Create statistical monitoring thread
//
   if ((rc = XrdOucThread::Run(&tid, XrdOlbStartMonPerf, (void *)0,
                               0, "Performance monitor")))
      {Say.Emsg("Config", rc, "create perf monitor thread");
       return 1;
      }

// Create reference monitoring thread
//
   RefTurn  = 3*XrdOlbManager::STMax*(DiskLinger+1);
   if (RefReset)
      {if ((rc = XrdOucThread::Run(&tid, XrdOlbStartMonRefs, (void *)0,
                                   0, "Refcount monitor")))
          {Say.Emsg("Config", rc, "create refcount monitor thread");
           return 1;
          }
      }

// Create state monitoring thread
//
   if ((rc = XrdOucThread::Run(&tid, XrdOlbStartMonStat, (void *)0,
                               0, "State monitor")))
      {Say.Emsg("Config", rc, "create state monitor thread");
       return 1;
      }

// Initialize the fast redirect queue
//
   RRQ.Init(LUPHold, LUPDelay);

// All done
//
   return 0;
}

/******************************************************************************/
/*                           s e t u p S e r v e r                            */
/******************************************************************************/
  
int XrdOlbConfig::setupServer()
{
   XrdNetWork *Net;
   XrdNetLink *Relay;
   pthread_t tid;
   int rc;

// Make sure we have enough info to be a server
//
   if (!myManagers)
      {Say.Emsg("Config", "Manager node not specified for server role");
       return 1;
      }

// If we need a name library, load it now
//
   if ((N2N_Lib || LocalRoot) && ConfigN2N()) return 1;

// Setup TCP outgoing network connections
//
   if (!(Net = new XrdNetWork(&Say, 0)))
      {Say.Emsg("Config","Unable to create server network interface.");
       return 1;
      }
   if (myDomain) Net->setDomain(myDomain);
   Manager.setNet(Net);

// Setup a UDP relay for the server
//
   if (!(Relay = Net->Relay(0, XRDNET_SENDONLY))) return 1;
   XrdOlbServer::setRelay(Relay);

// If this is a staging server then we better have a disk cache
//
   if (DiskSS && !(monPath || monPathP))
      {Say.Emsg("Config","Staging paths present but no disk cache specified.");
       return 1;
      }

// Calculate overload delay time
//
   if (MaxDelay < 0) MaxDelay = AskPerf*AskPing+30;
   if (DiskWT   < 0) DiskWT   = AskPerf*AskPing+30;

// If no cache has been specified but paths exist get the pfn for each path
// in the list for monitoring purposes
//
   if (!monPath && monPathP && lcl_N2N)
      {XrdOucTList *tlp = monPathP;
       char pbuff[2048];
       while(tlp)
            {if ((rc = lcl_N2N->lfn2pfn(tlp->text, pbuff, sizeof(pbuff))))
                Say.Emsg("Config",rc,"determine pfn for lfn",tlp->text);
                else {free(tlp->text);
                      tlp->text = strdup(pbuff);
                     }
             tlp = tlp->next;
            }
       }

// Setup file system metering
//
   Meter.setParms(monPath ? monPath : monPathP);
   if (perfpgm && Meter.Monitor(perfpgm, perfint))
      Say.Emsg("Config","Load based scheduling disabled.");

// Create manager monitoring thread
//
   if ((rc = XrdOucThread::Run(&tid, XrdOlbStartMonPing, (void *)0,
                               0, "Ping monitor")))
      {Say.Emsg("Config", rc, "create ping monitor thread");
       return 1;
      }

// If this is a staging server then set up the Prepq object
//
   if (DiskSS) 
      {PrepQ.setParms(Sched);
       PrepQ.Reset();
       Sched->Schedule((XrdJob *)&PrepQ,pendplife+time(0));
      }

// Setup notification path
//
   if (!(AnoteSock = ASocket(AdminPath,
                             (isManager ? "olbd.seton" : "olbd.notes"),
                             AdminMode, 1))) return 1;

// Construct the nostage/suspend file path names
//
  {char fnbuff[1048];
   int i;

   i = strlen(AdminPath);
   strcpy(fnbuff, AdminPath);
   if (AdminPath[i-1] != '/') fnbuff[i++] = '/';
   strcpy(fnbuff+i, "NOSTAGE");
   NoStageFile = strdup(fnbuff);
   strcpy(fnbuff+i, "SUSPEND");
   SuspendFile = strdup(fnbuff);
  }

// Determine if we are in nostage and/or suspend state
//
   if (inNoStage())
      {Say.Emsg("Config", "Starting in NOSTAGE state.");
       Manager.Stage(0, 0);
      }
   if (inSuspend())
      {Say.Emsg("Config", "Starting in SUSPEND state.");
       Manager.Suspend(0);
      }

// Determine whether or not we have data
//
   Manager.noData = isManager;
   return 0;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void XrdOlbConfig::Usage(int rc)
{
cerr <<"\nUsage: olbd [xrdopts] [-i] [-m] [-s] -c <cfile>" <<endl;
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

int XrdOlbConfig::xallow(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val;
    int ishost;

    if (!isManager) return 0;

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
/*                                x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path>

             <path>    the path of the named socket to use for admin requests.

   Type: Manager and Server, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xapath(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *pval, *val;
    mode_t mode = S_IRWXU;
    struct sockaddr_un USock;

// Get the path
//
   pval = CFile.GetWord();
   if (!pval || !pval[0])
      {eDest->Emsg("Config", "adminpath not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {eDest->Emsg("Config", "adminpath not absolute"); return 1;}

// Make sure path is not too long (account for "/olbd.admin")
//                                              12345678901
   if (strlen(pval) > sizeof(USock.sun_path) - 11)
      {eDest->Emsg("Config", "admin path", pval, "is too long");
       return 1;
      }
   pval = strdup(pval);

// Get the optional access rights
//
   if ((val = CFile.GetWord()) && val[0])
      if (!strcmp("group", val)) mode |= S_IRWXG;
         else {eDest->Emsg("Config", "invalid admin path modifier -", val);
               free(pval); return 1;
              }

// Record the path
//
   if (AdminPath) free(AdminPath);
   AdminPath = pval;
   AdminMode = mode;
   return 0;
}

/******************************************************************************/
/*                                x c a c h e                                 */
/******************************************************************************/

/* Function: xcache

   Purpose:  To parse the directive: cache <group> <path>[*]

             <group>   the cache group (ignored for olbd)
             <path>    the full path of the filesystem the server will handle.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xcache(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val, *pfxdir, *sfxdir, fn[XrdOlbMAX_PATH_LEN+1];
    int i, k, rc, pfxln, cnum = 0;
    struct dirent *dir;
    DIR *DFD;

    if (!isServer) return 0;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "cache group not specified"); return 1;}

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "cache path not specified"); return 1;}

    k = strlen(val);
    if (k >= (int)(sizeof(fn)-1) || val[0] != '/' || k < 2)
       {eDest->Emsg("Config", "invalid cache path - ", val); return 1;}

    if (val[k-1] != '*') return !Fsysadd(eDest, 0, val);

    for (i = k-1; i; i--) if (val[i] == '/') break;
    i++; strncpy(fn, val, i); fn[i] = '\0';
    sfxdir = &fn[i]; pfxdir = &val[i]; pfxln = strlen(pfxdir)-1;
    if (!(DFD = opendir(fn)))
       {eDest->Emsg("Config", errno, "open cache directory", fn); return 1;}

    errno = 0; rc = 0;
    while((dir = readdir(DFD)))
         {if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")
          || (pfxln && strncmp(dir->d_name, pfxdir, pfxln)))
             continue;
          strcpy(sfxdir, dir->d_name);
          if ((rc = Fsysadd(eDest, 1, fn))  < 0) break;
          cnum += rc;
         }

    if (errno)
       {if (rc >= 0) 
           {rc = -1; eDest->Emsg("Config", errno, "process cache directory", fn);}
       }
       else if (!cnum) eDest->Emsg("config","no cache directories found in ",val);

    closedir(DFD);
    return rc < 0;
}

int XrdOlbConfig::Fsysadd(XrdOucError *eDest, int chk, char *fn)
{
    struct stat buff;

    if (stat(fn, &buff))
       {if (!chk) eDest->Emsg("Config", errno, "process r/w path", fn);
        return -1;
       }

    if ((chk > 0) && !(buff.st_mode & S_IFDIR)) return 0;

    if (chk < 0) monPathP = new XrdOucTList(fn, 0, monPathP);
       else monPath = new XrdOucTList(fn, 0, monPath);
    return 1;
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

   discard   <cnt>     maximum number a message may be forwarded.
   drop      <sec>     seconds to delay a drop of an offline server.
   full      <sec>     seconds to delay client when no servers have space.
   hold      <msec>    millseconds to optimistically hold requests.
   lookup    <sec>     seconds to delay client when finding a file.
   overload  <sec>     seconds to delay client when all servers overloaded.
   servers   <cnt>     minimum number of servers we need.
   service   <sec>     seconds to delay client when waiting for servers.
   startup   <sec>     seconds to delay enabling our service
   suspend   <sec>     seconds to delay client when all servers suspended.

   Type: Manager only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOlbConfig::xdelay(XrdOucError *eDest, XrdOucStream &CFile)
{   char *val;
    const char *etxt = "invalid delay option";
    int  i, ppp, ispercent = 0;
    static struct delayopts {const char *opname; int *oploc; int istime;}
           dyopts[] =
       {
        {"discard",  &MsgTTL,   0},
        {"drop",     &DRPDelay, 1},
        {"full",     &DiskWT,  -1},
        {"hold",     &LUPHold,  0},
        {"lookup",   &LUPDelay, 1},
        {"overload", &MaxDelay,-1},
        {"servers",  &SUPCount, 0},
        {"service",  &SUPDelay, 1},
        {"startup",  &SRVDelay, 1},
        {"suspend",  &SUSDelay, 1}
       };
    int numopts = sizeof(dyopts)/sizeof(struct delayopts);

    if (!isManager) return 0;

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
                              } else {
                               if (*dyopts[i].opname == 's')
                                  {ppp = strlen(val); SUPLevel = 0;
                                   if (val[ppp-1] == '%')
                                      {ispercent = 1; val[ppp-1] = '\0';}
                                  }
                               if (XrdOuca2x::a2i( *eDest,etxt,val,&ppp,1))
                                  return 1;
                              }
                   if (!ispercent) *dyopts[i].oploc = ppp;
                      else {ispercent = 0; SUPCount = 1; SUPLevel = ppp;}
                   break;
                  }
           if (i >= numopts) 
              eDest->Emsg("Config","Warning, invalid delay option",val);
           val = CFile.GetWord();
          }
     return 0;
}

/******************************************************************************/
/*                                 x f s x q                                  */
/******************************************************************************/
  
/* Function: xfsxq

   Purpose:  To parse the directive: fsxeq <types> <prog>

             <types>   what operations the program performs (one or more of):
                       chmod mkdir mv rm rmdir
             <prog>    the program to execute when doing a forwarded fs op.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xfsxq(XrdOucError *eDest, XrdOucStream &CFile)
{
    struct xeqopts {const char *opname; int doset; XrdOucProg **pgm;} xqopts[] =
       {
        {"chmod",    0, &ProgCH},
        {"mkdir",    0, &ProgMD},
        {"mv",       0, &ProgMV},
        {"rm",       0, &ProgRM},
        {"rmdir",    0, &ProgRD}
       };
    int i, xtval = 0, numopts = sizeof(xqopts)/sizeof(struct xeqopts);
    char *val;

// If we are a manager, ignore this option
//
   if (!isServer) return 0;

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
              eDest->Emsg("Config", "invalid fsxeq type option -", val);
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

int XrdOlbConfig::xfxhld(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val;
    int ct;

    if (!isManager) return 0;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "fxhold value not specified."); return 1;}

    if (XrdOuca2x::a2tm(*eDest, "fxhold value", val, &ct, 60)) return 1;

    cachelife = ct;
    Cache.setLifetime(ct);
    return 0;
}

/******************************************************************************/
/*                                x l c l r t                                 */
/******************************************************************************/

/* Function: xpath

   Purpose:  To parse the directive: localroot <path>

             <path>    the path that the server will prefix to all local paths.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xlclrt(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val;
    int i;

// If we are a manager, ignore this option
//
   if (!isServer) return 0;

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
/*                                  x n m l                                   */
/******************************************************************************/

/* Function: xnml

   Purpose:  To parse the directive: namelib <path> [<parms>]

             <path>    the path of the filesystem library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xnml(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val, *parms;

// Get the path
//
   if (!(val = CFile.GetToken(&parms)) || !val[0])
      {eDest->Emsg("Config", "namelib not specified"); return 1;}

// Record the path
//
   if (N2N_Lib) free(N2N_Lib);
   N2N_Lib = strdup(val);

// Record any parms
//
   if (N2N_Parms) free(N2N_Parms);
   if (!parms) N2N_Parms = 0;
      else {while (*parms == ' ') parms++; N2N_Parms = strdup(parms);}
   return 0;
}

/******************************************************************************/
/*                                 x p a t h                                  */
/******************************************************************************/

/* Function: xpath

   Purpose:  To parse the directive: path {r | w | rw}[s] <path>

             <path>    the full path that the server will hanlde. This is

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xpath(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val, *path;
    int i = -1;
    XrdOlbPInfo  pmask;

// If we are a manager, ignore this option
//
   if (!isServer) return 0;

// Get path type
//
   val = CFile.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "path type not specified"); return 1;}

// Translate path type
//
   while(val[++i])
              if ('r' == val[i])  pmask.rovec = 1;
         else if ('w' == val[i])  pmask.rovec = pmask.rwvec = 1;
         else if ('s' == val[i]) {pmask.rovec = pmask.ssvec = 1; DiskSS = 1;}
         else {eDest->Emsg("Config", "invalid path type", val); return 1;}

// Get the path
//
   path = CFile.GetWord();
   if (!path || !path[0])
      {eDest->Emsg("Config", "path not specified"); return 1;}

// Add the path to the list of paths
//
   PathList.Insert(path, &pmask);

// If the path is writable, add it to the potential cache list. This list gets
// used if no cache directives have been specified.
//
   if (pmask.rwvec || pmask.ssvec) Fsysadd(eDest, -1, path);
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
int XrdOlbConfig::xperf(XrdOucError *eDest, XrdOucStream &CFile)
{   int   ival = 3*60;
    char *pgm=0, *val, *rest;

    if (!isServer) return 0;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "perf options not specified"); return 1;}

    do {     if (!strcmp("int", val))
                {if (!(val = CFile.GetWord()))
                    {eDest->Emsg("Config", "perf int value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(*eDest,"perf int",val,&ival,0)) return 1;
                }
        else if (!strcmp("key", val))
                {if (!(val = CFile.GetWord()))
                    {eDest->Emsg("Config", "perf key value not specified");
                     return 1;
                    }
                 eDest->Emsg("Config", "key parameter deprecated; ignored.");
                }
        else if (!strcmp("pgm",  val))
                {CFile.RetToken();
                 CFile.GetToken(&rest);
                 while (*rest == ' ') rest++;
                 if (!*rest)
                    {eDest->Emsg("Config", "perf prog value not specified");
                     return 1;
                    }
                 pgm = rest;
                 break;
                }
        else eDest->Emsg("Config", "Warning, invalid perf option", val);
       } while((val = CFile.GetWord()));

// Make sure that the perf program is here
//
   if (perfpgm) {free(perfpgm); perfpgm = 0;}
   if (pgm)
      if (!isExec(eDest, "perf", pgm)) return 1;
         else perfpgm = strdup(pgm);

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

int XrdOlbConfig::xpidf(XrdOucError *eDest, XrdOucStream &CFile)
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
                       requests (zero, the default, suppresses logging).
             usage     The number of pings between resource usage requests.
                       The default is 10. Zero suppresses usage requests.

   Type: Server for ping value and Manager for all values, dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOlbConfig::xping(XrdOucError *eDest, XrdOucStream &CFile)
{   int pnum = AskPerf, lnum = LogPerf, ping;
    char *val;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "ping value not specified"); return 1;}
    if (XrdOuca2x::a2tm(*eDest, "ping interval",val,&ping,0)) return 1;


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
/*                                 x p o r t                                  */
/******************************************************************************/

/* Function: xport

   Purpose:  To parse the directive: port <tcpnum> [if [<hl> [named <nl>]]

             <tcpnum>   number of the tcp port for incomming requests
             <hl>       apply port directive if this hostname is in <hl>
             <nl>       apply port directive if this netname is in <nl>

   Type: Manager or Server, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOlbConfig::xport(XrdOucError *eDest, XrdOucStream &CFile)
{   int rc, pnum = 0;
    char *val;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "tcp port not specified"); return 1;}
    if (isdigit(*val))
       {if (XrdOuca2x::a2i(*eDest,"tcp port",val,&pnum,1,65535)) return 1;}
       else if (!(pnum = XrdNetDNS::getPort(val, "tcp")))
               {eDest->Emsg("Config", "Unable to find tcp service", val);
                return 1;
               }

    if ((val = CFile.GetWord()) && !strcmp("if", val))
       if ((rc = XrdOucUtils::doIf(eDest,CFile,"role directive",
                              myName,myInsName,myProg)) <= 0) return (rc < 0);

    PortTCP = pnum;

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
                       entries. It must be specified as the last option
                       on the line.

   Type: Any, non-dynamic. Note that the Manager only need the "batch" option
         while slacves need the remaining options.

   Output: 0 upon success or !0 upon failure. Ignored by manager.
*/
int XrdOlbConfig::xprep(XrdOucError *eDest, XrdOucStream &CFile)
{   int   reset=0, scrub=0, echo = 0, doset = 0;
    char  *prepif=0, *val, *rest;

    if (!isServer) return 0;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "prep options not specified"); return 1;}

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
                {CFile.RetToken();
                 CFile.GetToken(&rest);
                 while (*rest == ' ') rest++;
                 if (!*rest)
                    {eDest->Emsg("Config", "prep ifpgm value not specified");
                     return 1;
                    }
                 prepif = rest;
                 break;
                }
        else eDest->Emsg("Config", "Warning, invalid prep option", val);
       } while((val = CFile.GetWord()));



// Set the values
//
   if (scrub) pendplife = scrub;
   if (doset) PrepQ.setParms(reset, scrub, echo);
   if (prepif) 
      if (!isExec(eDest, "prep", prepif)) return 1;
         else return PrepQ.setParms(prepif);
   return 0;
}

/******************************************************************************/
/*                                 x r o l e                                  */
/******************************************************************************/

/* Function: xrole

   Purpose:  To parse the directive: role {manager | server | supervisor}
                                          [if <hostlist> [named <namelist>]]

             manager    act as a manager (incomming no outgoing).
             server     act as a server (no incomming only outgoing).
             supervisor act as a supervisor (incomming and outgoing).
             <hostlist> apply role only when executing on matching host.
             <namelist> apply role only when executing on matching instance.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xrole(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val;
    const char *myrole;
    int rc, xServ = 0, xMan = 0;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "role not specified"); return 1;}

         if (!strcmp("manager",    val)) 
            {xServ =  0; xMan = -1; myrole = "manager";}
    else if (!strcmp("server",     val)) 
            {xServ = -1; xMan =  0; myrole = "server";}
    else if (!strcmp("supervisor", val)) 
            {xServ = -1; xMan = -1; myrole = "supervisor";}
    else {eDest->Emsg("Config", "invalid role -", val); return 1;}

    if ((val = CFile.GetWord()) && !strcmp("if", val))
       if ((rc = XrdOucUtils::doIf(eDest,CFile,"role directive",
                              myName,myInsName,myProg)) <= 0) return (rc < 0);

    if (isServer > 0 || isManager > 0)
       eDest->Emsg("Config",myrole,"role over-ridden by command line options.");
       else {isServer = xServ; isManager = xMan;}

    return 0;
}
/******************************************************************************/
/*                                x s c h e d                                 */
/******************************************************************************/

/* Function: xsched

   Purpose:  To parse directive: sched [cpu <p>] [io <p>] [runq <p>]
                                       [mem <p>] [pag <p>] [fuzz <p>]
                                       [maxload <p>] [refreset <sec>]

             <p>      is the percentage to include in the load as a value
                      between 0 and 100. For fuzz this is the largest
                      difference two load values may have to be treated equal.
                      maxload is the largest load allowed before server is
                      not selected. refreset is the minimum number of seconds
                      between reference counter resets.

   Type: Any, dynamic.

   Output: retc upon success or -EINVAL upon failure.
*/

int XrdOlbConfig::xsched(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val;
    int  i, ppp;
    static struct schedopts {const char *opname; int maxv; int *oploc;}
           scopts[] =
       {
        {"cpu",      100, &P_cpu},
        {"fuzz",     100, &P_fuzz},
        {"io",       100, &P_io},
        {"runq",     100, &P_load}, // Actually load, runq to avoid confusion
        {"mem",      100, &P_mem},
        {"pag",      100, &P_pag},
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
              eDest->Emsg("Config", "Warning, invalid sched option", val);
           val = CFile.GetWord();
          }

    return 0;
}

/******************************************************************************/
/*                                x s p a c e                                 */
/******************************************************************************/

/* Function: xspace

   Purpose:  To parse the directive: space [linger <num>] [[min] <min> [<hwm>]]
                                           [recalc <sec>]

             <num> Maximum number of times a server may be reselected without
                   a break. The default is 0.

             <min> Minimum free space need in bytes (or K, M, G) in a partition.
                   The default is 10G.

             <hwm> Bytes (or K, M,G) of free space needed when bytes falls below
                   <min> to requalify a server for selection.
                   The default is 11G.

             <sec> Number of seconds that must elapse before a disk free space
                   calculation will occur.

   Notes:   This is used by the manager and the server.

   Type: All, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xspace(XrdOucError *eDest, XrdOucStream &CFile)
{
    char *val;
    int alinger = -1, arecalc = -1;
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
      else if (isdigit(*val) || (!strcmp("min", val) && (val = CFile.GetWord())) )
              {if (XrdOuca2x::a2sz(*eDest,"space minfree",val,&minf,0)) return 1;
               if ((val = CFile.GetWord()))
                  {if (isdigit(*val))
                       {if (XrdOuca2x::a2sz(*eDest,"space high watermark",
                                            val,&hwm,0)) return 1;
                       }
                      else CFile.RetToken();
                  } else break;
              }
       else {eDest->Emsg("Config", "invalid space parameters"); return 1;}
       }
    
    if (alinger < 0 && arecalc < 0 && minf < 0)
       {eDest->Emsg("Config", "no space values specified"); return 1;}

    if (alinger >= 0) DiskLinger = alinger;
    if (arecalc >= 0) DiskAsk    = arecalc;

    if (minf >= 0)
       {if (hwm < 0) DiskHWM = minf+1073741824;
           else if (hwm < minf) DiskHWM = minf + hwm;
                   else DiskHWM = hwm;
        DiskMin = minf / 1024;
        DiskHWM /= 1024;
       }
    return 0;
}

/******************************************************************************/
/*                                 x s u b s                                  */
/******************************************************************************/

/* Function: xsubs

   Purpose:  Parse the directive: subscribe <host>[+] [<port>]
                                            [if <hlist> [named <nlist>]]

             <host> The dns name of the host that is the manager of this host.
                    If the host name ends with a plus, all addresses that are
                    associated with the hosts are subscribed to.

             <port> The port number to use for this host. The default comes
                    from the port directive.

            <hlist> Apply directive if this is one of the named hosts.

            <nlist> Apply directive if this is one of the named instances.

   Notes:   Any number of subscribe directives can be given. The server will
            subscribe to all of the managers.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xsubs(XrdOucError *eDest, XrdOucStream &CFile)
{
    struct sockaddr InetAddr[8];
    XrdOucTList *tp = 0;
    char *val, *bval = 0, mbuff[1024];
    int i, port = 0;

    if (!isServer) return 0;

    if (!(val = CFile.GetWord()))
       {eDest->Emsg("Config", "subscribe host not specified"); return 1;}
    strlcpy(mbuff, val, sizeof(mbuff));

    if ((val = CFile.GetWord()) && strcmp(val, "if"))
       {if (isdigit(*val))
           {if (XrdOuca2x::a2i(*eDest,"subscribe port",val,&port,1,65535))
               return 1;
           }
           else if (!(port = XrdNetDNS::getPort(val, "tcp")))
                   {eDest->Emsg("Config", "unable to find tcp service", val);
                    return 1;
                   }
       val = CFile.GetWord();
       }
       else if (!(port = PortTCP))
               {eDest->Emsg("Config","subscribe port not specified for",mbuff);
                return 1;
               }

    if (val)
       {if (strcmp(val, "if"))
           {eDest->Emsg("Config","expecting subscribe 'if' but",val,"found");
            return 1;
           }
        if ((i=XrdOucUtils::doIf(eDest,CFile,"subscribe directive",
                            myName,myInsName,myProg))<=0) return i < 0;
       }

    i = strlen(mbuff);
    if (mbuff[i-1] != '+') i = 0;
        else {bval = strdup(mbuff); mbuff[i-1] = '\0';
              if (!(i = XrdNetDNS::getHostAddr(mbuff, InetAddr, 8)))
                 {eDest->Emsg("Config","Subscribe host", mbuff, "not found");
                  free(bval); return 1;
                 }
             }

    do {if (i)
           {char *mp;
            i--; //Greg had it right
            mp = XrdNetDNS::getHostName(InetAddr[i]);
            strlcpy(mbuff, mp, sizeof(mbuff)); free(mp);
            eDest->Emsg("Config", bval, "-> olb.subscribe", mbuff);
            if (isdigit(*mbuff))
               eDest->Emsg("Config", "Warning! Unable to reverse lookup", mbuff);
           }
        tp = myManagers;
        while(tp) 
             if (strcmp(tp->text, mbuff) || tp->val != port) tp = tp->next;
                else {eDest->Emsg("Config","Duplicate subscription to",mbuff);
                      break;
                     }
        if (tp) break;
        myManagers = new XrdOucTList(mbuff, port, myManagers);
       } while(i);

    if (bval) free(bval);
    return tp != 0;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <options>

   Type: Manager or Server, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xtrace(XrdOucError *eDest, XrdOucStream &CFile)
{
    char  *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"defer",    TRACE_Defer},
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
                      eDest->Emsg("config", "invalid trace option", val);
                  }
          val = CFile.GetWord();
         }

    Trace.What = trval;
    return 0;
}
