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
#include "XrdOlb/XrdOlbCache.hh"
#include "XrdOlb/XrdOlbConfig.hh"
#include "XrdOlb/XrdOlbMeter.hh"
#include "XrdOlb/XrdOlbManager.hh"
#include "XrdOlb/XrdOlbPrepare.hh"
#include "XrdOlb/XrdOlbScheduler.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOlb/XrdOlbTypes.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucSecurity.hh"
#include "XrdOuc/XrdOucSocket.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdOuc/XrdOucTList.hh"

/******************************************************************************/
/*                      C o p y r i g h t   S t r i n g                       */
/******************************************************************************/
  
#define XrdCPR "(c) 2004 SLAC olbd version " XrdVSTRING

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/
  
extern int              XrdOlbSTDERR;

extern XrdOlbCache      XrdOlbCache;

extern XrdOlbConfig     XrdOlbConfig;

extern XrdOlbPrepare    XrdOlbPrepQ;

extern XrdOucTrace      XrdOlbTrace;

extern XrdOucLink      *XrdOlbRelay;

extern XrdOucNetwork   *XrdOlbNetTCP;
extern XrdOucNetwork   *XrdOlbNetUDPm;
extern XrdOucNetwork   *XrdOlbNetUDPs;

extern XrdOlbScheduler *XrdOlbSchedM;
extern XrdOlbScheduler *XrdOlbSchedS;

extern XrdOlbManager    XrdOlbSM;

extern XrdOucLogger     XrdOlbLog;

extern XrdOucError      XrdOlbSay;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
extern "C"
{
void *XrdOlbStartMonPing(void *carg) { return XrdOlbSM.MonPing(); }

void *XrdOlbStartMonPerf(void *carg) { return XrdOlbSM.MonPerf(); }

void *XrdOlbStartMonRefs(void *carg) { return XrdOlbSM.MonRefs(); }
}

/******************************************************************************/
/*                        W o r k e r   C l a s s e s                         */
/******************************************************************************/

class XrdOlbManWorker : XrdOlbWorker
{
public:

void *WorkIt(void *) {return (void *)XrdOlbSM.Process();}

     XrdOlbManWorker() {}
    ~XrdOlbManWorker() {}
};

class XrdOlbSrvWorker : XrdOlbWorker
{
public:

void *WorkIt(void *) {return (void *)XrdOlbSM.Respond();}

     XrdOlbSrvWorker() {}
    ~XrdOlbSrvWorker() {}

};

class XrdOlbLogWorker : XrdOlbJob
{
public:

      int DoIt() {XrdOlbSay.Emsg("Config", XrdCPR, 
                            (char *)" executing as ", smtype);
                  midnite += 86400;
                  XrdOlbSchedM->Schedule((XrdOlbJob *)this, midnite);
                  return 1;
                 }

          XrdOlbLogWorker(char *who) : XrdOlbJob("midnight runner")
                         {smtype = who; 
                          midnite = XrdOucTimer::Midnight() + 86400;
                          XrdOlbSchedM->Schedule((XrdOlbJob *)this, midnite);
                         }
         ~XrdOlbLogWorker() {}
private:
char  *smtype;
time_t midnite;
};

/******************************************************************************/
/*                         S t a r t u p   C l a s s                          */
/******************************************************************************/
  
class XrdOlbStartup : XrdOlbJob
{
public:

      int DoIt() {XrdOlbConfig.Disabled = 0;
                  XrdOlbSay.Emsg("Config", "Service enabled.");
                  return 1;
                 }

          XrdOlbStartup() : XrdOlbJob("service startup"){}
         ~XrdOlbStartup() {}
};

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_String(x,m) if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(eDest, Config);

#define TS_Set(x,v)    if (!strcmp(x,var)) {v=1; return 0;}

#define OLB_Prefix    "olb."
#define OLB_PrefLen   sizeof(OLB_Prefix)-1

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOlbConfig::~XrdOlbConfig()
{     if (Meter) delete Meter;
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOlbConfig::Configure(int argc, char **argv)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   int logsync = 86400, NoGo = 0;
   char c, buff[32], *temp, *logfn = 0, *smtype = 0;
   extern char *optarg;
   extern int opterr, optopt;
   static XrdOlbManWorker MWorker;
   static XrdOlbSrvWorker  SWorker;

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc, argv, "c:dl:L:msw")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'c': ConfigFN = optarg;
                 break;
       case 'd': XrdOlbTrace.What = 1;
                 break;
       case 'l': if (logfn) free(logfn);
                 logfn = strdup(optarg);
                 break;
       case 'L': break; // Here for upward compatability only
       case 'm': isManager = 1;
                 break;
       case 's': isServer = 1;
                 break;
       case 'w': doWait = 1;
                 break;
       default:  buff[0] = '-'; buff[1] = optopt; buff[2] = '\0';
                 XrdOlbSay.Emsg("Config", "Invalid option,", buff);
                 Usage(1);
       }
     }

// Bail if no configuration file specified
//
   if (!ConfigFN && !(ConfigFN = getenv("XrdOlbCONFIGFN")) || !*ConfigFN)
      {XrdOlbSay.Emsg("Config", "Required config file not specified.");
       Usage(1);
      }

// Process unsupported configurations
//
   if (isManager && isServer)
      {smtype = (char *)"Manager/Server";
       XrdOlbSay.Emsg("Config", "Hierarchical manager/server mode not supported");
       Usage(1);
      }
   if (!(isManager || isServer))
      {XrdOlbSay.Emsg("Config", "Mode not specified; manager mode assumed");
       isManager = 1;
      } 
   if (isManager) 
      {if (!smtype) smtype = (char *)"Manager";
       XrdOlbSchedM = new XrdOlbScheduler((XrdOlbWorker*)&MWorker);
       XrdOlbJob *jp=(XrdOlbJob *)new XrdOlbCache_Scrubber(&XrdOlbCache,XrdOlbSchedM);
       XrdOlbSchedM->Schedule(jp, cachelife+time(0));
       if (!isServer)  XrdOlbSchedS = XrdOlbSchedM;
      }
   if (isServer)  
      {if (!smtype) smtype = (char *)"Server";
       XrdOlbSchedS = new XrdOlbScheduler((XrdOlbWorker *)&SWorker);
       if (!isManager) XrdOlbSchedM = XrdOlbSchedS;
      }

// Establish pointers to error message handling
//
   if (!logfn) XrdOlbSTDERR = 0;
      else {XrdOlbSTDERR = dup(2);
            XrdOlbLog.Bind(logfn, logsync);
            new XrdOlbLogWorker(smtype);
           }
   XrdOlbSay.Emsg("Config",XrdCPR,(char *)"initializing as",smtype);

// Establish the FD limit
//
   {struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
       XrdOlbSay.Emsg("Config", errno, "get resource limits");
       else {rlim.rlim_cur = rlim.rlim_max;
            if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
               NoGo = XrdOlbSay.Emsg("Config", errno,"set FD limit");
            }
   }

// Process the configuration file
//
   NoGo |= ConfigProc();

// Make sure that we have some ports to work with
//
   if (!PortTCP) temp = (char *)"TCP";
       else if (!PortUDPm) temp = (char *)"UDP";
               else temp = 0;
   if (temp) 
      {NoGo = 1; XrdOlbSay.Emsg("Config",temp, (char *)"port not specified.");}
      else if (PortUDPm == PortUDPs && isManager && isServer)
              {XrdOlbSay.Emsg("Config","Manager and server UDP ports are the same.");
               NoGo = 1;
              }

// Setup the admin path
//
   if (!NoGo) 
      NoGo = !(AdminSock = ASocket(AdminPath, "olbd.admin", AdminMode));

// Setup manager or server, as needed
//
  if (!NoGo)
     if (isManager) NoGo = setupManager();
        else       NoGo = setupServer();

// Set up the generic message id
//
   MsgGIDL = sprintf(buff, "%d@0 ", MsgTTL);
   MsgGID  = strdup(buff);

// All done, check for success or failure
//
   temp = (NoGo ? (char *)"failed." : (char *)"completed.");
   XrdOlbSay.Emsg("Config",(const char *)smtype,
                 (char *)"initialization",temp);
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdOlbConfig::ConfigXeq(char *var, XrdOucStream &Config, XrdOucError *eDest)
{
   int dynamic;

   // Determine whether is is dynamic or not
   //
   if (eDest) dynamic = 1;
      else   {dynamic = 0; eDest = &XrdOlbSay;}

   // Process items
   //
   TS_Xeq("delay",         xdelay);  // Manager,     dynamic
   TS_Xeq("fxhold",        xfxhld);  // Manager,     dynamic
   TS_Xeq("ping",          xping);   // Manager,     dynamic
   TS_Xeq("sched",         xsched);  // Manager,     dynamic
   TS_Xeq("space",         xspace);  // Any,        dynamic
   TS_Xeq("threads",       xthreads);// Any,        dynamic
   TS_Xeq("trace",         xtrace);  // Any,        dynamic

   if (!dynamic)
   {
   TS_Xeq("cache",         xcache);  // Server,  non-dynamic
   TS_Xeq("adminpath",     xapath);  // Any,     non-dynamic
   TS_Xeq("allow",         xallow);  // Manager, non-dynamic
   TS_Xeq("fsxeq",         xfsxq);   // Server,  non-dynamic
   TS_Xeq("localroot",     xlclrt);  // Server,  non-dynamic
   TS_Xeq("path",          xpath);   // Server,  non-dynamic
   TS_Xeq("perf",          xperf);   // Server,  non-dynamic
   TS_Xeq("pidpath",       xpidf);   // Any,     non-dynamic
   TS_Xeq("port",          xport);   // Any,     non-dynamic
   TS_Xeq("prep",          xprep);   // Any,     non-dynamic
   TS_Xeq("remoteroot",    xrmtrt);  // Server,  non-dynamic
   TS_Xeq("subscribe",     xsubs);   // Server,  non-dynamic
   TS_Set("wait",          doWait);  // Server,  non-dynamic
   }

   // No match found, complain.
   //
   eDest->Emsg("Config", "Warning, unknown directive", var);
   return 0;
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
    if (concat_fn(LocalRoot, LocalRLen, oldp, newp))
       return XrdOlbSay.Emsg("glp", -ENAMETOOLONG, "generate local path ",
                               (char *)oldp);
    return 0;
}

/******************************************************************************/
/*                         G e n R e m o t e P a t h                          */
/******************************************************************************/
  
/* GenRemotePath() generates the path that a file will have in the remote file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least XrdOlbMAX_PATH_LEN bytes long.
*/
int XrdOlbConfig::GenRemotePath(const char *oldp, char *newp)
{
   if (concat_fn(RemotRoot, RemotRLen, oldp, newp))
      return XrdOlbSay.Emsg("grp", -ENAMETOOLONG,"generate remote path ",
                               (char *)oldp);
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
   msgnum = strtol((const char *)oldmid, &ep, 10);
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

   return (!stat((const char *)NoStageFile, &buff));
}

/******************************************************************************/
/*                             i n S u s p e n d                              */
/******************************************************************************/
  
int  XrdOlbConfig::inSuspend()
{
   struct stat buff;

   return (!stat((const char *)SuspendFile, &buff));
}
  
/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                               A S o c k e t                                */
/******************************************************************************/
  
XrdOucSocket *XrdOlbConfig::ASocket(char *path, const char *fn, mode_t mode, 
                                    int isudp)
{
   XrdOucSocket *ASock;
   int i, sflags = (isudp ? XrdOucSOCKET_UDP : 0);
   char *act = 0, fnbuff[1024];
   struct stat buf;

// Create the directory if it is not already there
//
   if (stat((const char *)path, &buf))
      {if (errno != ENOENT) act = (char *)"process directory";
          else if (mkdir(path, mode)) act = (char *)"create path directory";
                  else if (chmod((const char *)path, mode))
                          act = (char *)"set access mode for";
      } else
       if ((buf.st_mode & S_IFMT) != S_IFDIR)
          {errno = ENOTDIR; act = (char *)"process directory";}
          else if ((buf.st_mode & S_IAMB) != mode
               &&  chmod((const char *)path, mode))
                  act = (char *)"set access mode for";

   if (act) {XrdOlbSay.Emsg("Config", errno, act, path);
             return (XrdOucSocket *)0;
            }

// Construct full filename
//
   i = strlen(path);
   strcpy(fnbuff, path);
   if (path[i-1] != '/') fnbuff[i++] = '/';
   strcpy(fnbuff+i, fn);

// Check is we have already created it and whether we can access
//
   if (!stat((const char *)fnbuff,&buf))
      {if ((buf.st_mode & S_IFMT) != S_IFSOCK)
          {XrdOlbSay.Emsg("Config","Path", fnbuff,
                                   (char *)"exists but is not a socket");
           return (XrdOucSocket *)0;
          }
       if (access((const char *)fnbuff, W_OK))
          {XrdOlbSay.Emsg("Config", errno, "access path", fnbuff);
           return (XrdOucSocket *)0;
          }
      }

// Connect to the path
//
   ASock = new XrdOucSocket(&XrdOlbSay);
   if (ASock->Open(fnbuff, -1, XrdOucSOCKET_SERVER|sflags) < 0)
      {XrdOlbSay.Emsg("Config",ASock->LastError(),"establish socket",fnbuff);
       delete ASock;
       return (XrdOucSocket *)0;
      }

// Set the mode and return the socket object
//
   chmod(fnbuff, mode); // This may fail on some platforms
   return ASock;
}

/******************************************************************************/
/*                             c o n c a t _ f n                              */
/******************************************************************************/
  
int XrdOlbConfig::concat_fn(const char *prefix, // String to prefix oldp
                            const int   pfxlen, // Length of prefix string
                            const char *path,   // String to suffix prefix
                            char *buffer)       // Resulting buffer
{
   int add_slash = (*path != '/');
   
/* Verify that filename is not too large.
*/
   if( strlen(path) + add_slash + pfxlen > XrdOlbMAX_PATH_LEN ) return -1;

/* Create the file name
*/
   strcpy(buffer, prefix);
   if( add_slash==1 ) buffer[pfxlen] = '/';
   strcpy(buffer + pfxlen + add_slash, path);

/* All done.
*/
   return 0;
}

/******************************************************************************/
/*                        C o n f i g D e f a u l t s                         */
/******************************************************************************/
  
void XrdOlbConfig::ConfigDefaults(void)
{


// Preset all variables with common defaults
//
   myName   = XrdOucNetwork::FullHostName();
   LUPDelay = 5;
   DRPDelay = 10*60;
   SRVDelay = 90;
   SUPCount = 1;
   SUPLevel = 80;
   SUPDelay = 15;
   SUSDelay = 30;
   MaxLoad  = 0x7fffffff;
   MsgTTL   = 7;
   PortTCP  = 0;
   PortUDPm = 0;
   PortUDPs = 0;
   P_cpu    = 0;
   P_fuzz   = 20;
   P_io     = 0;
   P_load   = 0;
   P_mem    = 0;
   P_pag    = 0;
   AskPerf  = 10;       // Every 10 pings
   AskPing  = 60;       // Every  1 minute
   MaxDelay = -1;
   LogPerf  = 10;       // Every 10 usage requests
   DiskMin  = 10485760; // 10GB / 1024
   DiskAdj  = 1048576;  //  1GB / 1024
   DiskWT   = 0;        // Do not defer when out of space
   DiskAsk  = 60;       // Don't ask more often than 30 seconds
   DiskSS   = 0;        // Not a staging server
   ConfigFN = 0;
   sched_RR = 0;
   isManager = 0;
   isServer  = 0;
   LocalRoot= 0;
   LocalRLen= 0;
   RemotRoot= 0;
   RemotRLen= 0;
   myManagers=0;
   Meter    = 0;
   perfint  = 3*60;
   perfpgm  = 0;
   AdminPath= strdup("/tmp/.olb");
   AdminMode= 0700;
   AdminSock= 0;
   AnoteSock= 0;
   pidPath  = strdup("/tmp");
   Police   = 0;
   monPath  = 0;
   cachelife= 8*60*60;
   pendplife=   60*60*24*7;
   DiskLinger=0;
   ProgCH   = 0;
   ProgMD   = 0;
   ProgMV   = 0;
   ProgRD   = 0;
   ProgRM   = 0;
   doWait   = 0;
   Disabled = 1;
   RefReset = 60*60;
   RefTurn  = 3*XrdOlbSTMAX*(DiskLinger+1);
   NoStageFile = 0;
   SuspendFile = 0;
}
  
/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdOlbConfig::ConfigProc()
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucStream Config(&XrdOlbSay);

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {XrdOlbSay.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = Config.GetFirstWord()))
        {if (!strncmp(var, OLB_Prefix, OLB_PrefLen))
            {var += OLB_PrefLen;
             NoGo |= ConfigXeq(var, Config, 0);
            } else
         if (!strcmp(var, "oss.cache") || !strcmp(var, "oss.localroot"))
            {var += 4;
             NoGo |= ConfigXeq(var, Config, 0);
            }
        }

// Now check if any errors occured during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = XrdOlbSay.Emsg("Config", retc, "read config file", ConfigFN);
   Config.Close();

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
   if (access((const char *)prog, X_OK))
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
    int xfd;
    char buff[1024], *xop = 0;
    char pidFN[1200];

    snprintf(pidFN, sizeof(pidFN), "%s/olbd.pid", pidPath);

    if ((xfd = open(pidFN, O_WRONLY|O_CREAT|O_TRUNC,0644)) < 0)
       xop = (char *)"open";
       else {if ((write(xfd, buff, snprintf(buff,sizeof(buff),"%d",getpid())) < 0)
             || (LocalRoot && (write(xfd,(void *)"\n&pfx=",6)  < 0 ||
                               write(xfd,(void *)LocalRoot, LocalRLen) < 0))
             || (AdminPath && (write(xfd,(void *)"\n&ap=", 5)  < 0 ||
                               write(xfd,(void *)AdminPath,strlen(AdminPath)) < 0))
                ) xop = (char *)"write";
             close(xfd);
            }

     if (xop) XrdOlbSay.Emsg("Config", errno, (const char *)xop, pidFN);
     return xop != 0;
}

/******************************************************************************/
/*                          s e t u p M a n a g e r                           */
/******************************************************************************/
  
int XrdOlbConfig::setupManager()
{
   EPNAME("setupManager")
   static XrdOlbStartup StartService;
   pthread_t tid;
   int rc;

// Setup TCP network connections now
//
   if (!Police)
      XrdOlbSay.Emsg("Config","Warning! All hosts are allowed to connect.");

   XrdOlbNetTCP = new XrdOucNetwork(&XrdOlbSay, Police);
   if (XrdOlbNetTCP->Bind(PortTCP, "tcp")) return 1;


// Setup UDP network connections now
//
   XrdOlbNetUDPm = new XrdOucNetwork(&XrdOlbSay, Police);
   if (XrdOlbNetUDPm->Bind(PortUDPm, "udp")) return 1;

// Compute the scheduling policy
//
   sched_RR = (100 == P_fuzz) || !AskPerf
              || !(P_cpu || P_io || P_load || P_mem || P_pag);
   if (sched_RR)
      XrdOlbSay.Emsg("Config", "Round robbin scheduling in effect.");

// Create statistical monitoring thread
//
   if ((rc = XrdOucThread_Run(&tid, XrdOlbStartMonPerf, (void *)0)))
      {XrdOlbSay.Emsg("Config", rc, "create perf monitor thread");
       return 1;
      }
   DEBUG("Config: thread " <<(unsigned int)tid <<" assigned to perf monitor");

// Create reference monitoring thread
//
   RefTurn  = 3*XrdOlbSTMAX*(DiskLinger+1);
   if (RefReset)
      {if ((rc = XrdOucThread_Run(&tid, XrdOlbStartMonRefs, (void *)0)))
          {XrdOlbSay.Emsg("Config", rc, "create refcount monitor thread");
           return 1;
          }
       DEBUG("Config: thread " <<(unsigned int)tid <<" assigned to refcount monitor");
      }

// We normally come up disabled to allow data service machines to connect.
// Scheduler a job to enabled ourselves after the service delay time.
//
   XrdOlbSchedM->Schedule((XrdOlbJob *)&StartService, time(0)+SRVDelay);

// All done
//
   return 0;
}

/******************************************************************************/
/*                           s e t u p S e r v e r                            */
/******************************************************************************/
  
int XrdOlbConfig::setupServer()
{
   EPNAME("setupServer")
   pthread_t tid;
   int rc;

// Make sure we have enough info to be a server
//
   if (!myManagers)
      {XrdOlbSay.Emsg("Config", "Manager node not specified for server mode");
       return 1;
      }

// Setup the pidfile
//
   if (PidFile()) return 1;

// Setup TCP network
//
   XrdOlbNetTCP = new XrdOucNetwork(&XrdOlbSay, 0);

// Setup UDP network for the server
//
   XrdOlbNetUDPs = new XrdOucNetwork(&XrdOlbSay, Police);
   if (XrdOlbNetUDPs->Bind(PortUDPs, "udp")
   || !(XrdOlbRelay = XrdOlbNetUDPs->Relay(&XrdOlbSay))) return 1;

// If this is a staging server then we better have a disk cache
//
   if (DiskSS && !monPath)
      {XrdOlbSay.Emsg("Config","Staging paths present but no disk cache specified.");
       return 1;
      }

// Calculate overload delay time
//
   if (MaxDelay < 0) MaxDelay = AskPerf*AskPing+30;
   if (DiskWT   < 0) DiskWT   = AskPerf*AskPing+30;

// Setup file system metering
//
   XrdOlbMeter::setParms(monPath, DiskMin, DiskAsk);

// Set up load metering
//
   if (perfpgm)
      {Meter = new XrdOlbMeter(&XrdOlbSay);
       if (Meter->Monitor(perfpgm, perfint))
          {delete Meter; Meter = 0;
           XrdOlbSay.Emsg("Config","Load based scheduling disabled.");
          }
      }

// Create manager monitoring thread
//
   if ((rc = XrdOucThread_Run(&tid, XrdOlbStartMonPing, (void *)0)))
      {XrdOlbSay.Emsg("Config", rc, "create ping monitor thread");
       return 1;
      }
   DEBUG("Config: thread " <<(unsigned int)tid <<" assigned to ping monitor");

// If this is a staging server then set up the Prepq object
//
   if (DiskSS) 
      {XrdOlbPrepQ.setParms(XrdOlbSchedS);
       XrdOlbPrepQ.Reset();
       XrdOlbSchedS->Schedule((XrdOlbJob *)&XrdOlbPrepQ, pendplife+time(0));
      }

// Setup notification path
//
   if (!(AnoteSock = ASocket(AdminPath, "olbd.notes", AdminMode, 1)))
      return 1;

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
      {XrdOlbSay.Emsg("Config", "Starting in NOSTAGE state.");
       XrdOlbSM.Stage(0, 0);
      }
   if (inSuspend())
      {XrdOlbSay.Emsg("Config", "Starting in SUSPEND state.");
       XrdOlbSM.Suspend(0);
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void XrdOlbConfig::Usage(int rc)
{
cerr <<"\nUsage: olbd [-d] [-l <fn>] [-m] [-s] [-w] -c <cfn>" <<endl;
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

int XrdOlbConfig::xallow(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int ishost;

    if (!isManager) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "allow type not specified"); return 1;}

    if (!strcmp(val, "host")) ishost = 1;
       else if (!strcmp(val, "netgroup")) ishost = 0;
               else {eDest->Emsg("Config", "invalid allow type -", val);
                     return 1;
                    }

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "allow target name not specified"); return 1;}

    if (!Police) Police = new XrdOucSecurity();
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

int XrdOlbConfig::xapath(XrdOucError *eDest, XrdOucStream &Config)
{
    char *pval, *val;
    mode_t mode = S_IRWXU;
    struct sockaddr_un USock;

// Get the path
//
   pval = Config.GetWord();
   if (!pval || !pval[0])
      {eDest->Emsg("Config", "adminpath not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {eDest->Emsg("Config", "adminpath not absolute"); return 1;}

// Make sure path is not too long
//
   if (strlen(pval) > sizeof(USock.sun_path))
      {eDest->Emsg("Config", "admin path", (char *)pval, (char *)"is too long");
       return 1;
      }
   pval = strdup(pval);

// Get the optional access rights
//
   if ((val = Config.GetWord()) && val[0])
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

int XrdOlbConfig::xcache(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val, *pfxdir, *sfxdir, fn[XrdOlbMAX_PATH_LEN+1];
    int i, k, rc, pfxln, cnum = 0;
    struct dirent *dir;
    DIR *DFD;

    if (!isServer) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "cache group not specified"); return 1;}

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "cache path not specified"); return 1;}

    k = strlen(val);
    if (k >= (int)(sizeof(fn)-1) || val[0] != '/' || k < 2)
       {eDest->Emsg("Config", "invalid cache path - ", val); return 1;}

    if (val[k-1] != '*') return !Fsysadd(eDest, 0, val);

    for (i = k-1; i; i--) if (val[i] == '/') break;
    i++; strncpy(fn, (const char *)val, i); fn[i] = '\0';
    sfxdir = &fn[i]; pfxdir = &val[i]; pfxln = strlen(pfxdir)-1;
    if (!(DFD = opendir((const char *)fn)))
       {eDest->Emsg("Config", errno, "open cache directory", fn); return 1;}

    errno = 0; rc = 0;
    while((dir = readdir(DFD)))
         {if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")
          || (pfxln && strncmp(dir->d_name, (const char *)pfxdir, pfxln)))
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

    if (stat((const char *)fn, &buff))
       {if (!chk) eDest->Emsg("Config", errno, "process cache", fn);
        return -1;
       }

    if (chk && !(buff.st_mode & S_IFDIR)) return 0;

    monPath = new XrdOucTList(fn, 0, monPath);
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
                                           [service <sec>]

   discard   <cnt>     maximum number a message may be forwarded.
   drop      <sec>     seconds to delay a drop of an offline server.
   full      <sec>     seconds to delay client when no servers have space.
   lookup    <sec>     seconds to delay client when finding a file.
   overload  <sec>     seconds to delay client when all servers overloaded.
   servers   <cnt>     minimum number of servers we need.
   service   <sec>     seconds to delay client when waiting for servers.
   startup   <sec>     seconds to delay enabling our service
   suspend   <sec>     seconds to delay client when all servers suspended.

   Type: Manager only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOlbConfig::xdelay(XrdOucError *eDest, XrdOucStream &Config)
{   char *val;
    const char *etxt = "invalid delay option";
    int  i, ppp, ispercent = 0;
    static struct delayopts {const char *opname; int *oploc; int istime;}
           dyopts[] =
       {
        {"discard",  &MsgTTL,   0},
        {"drop",     &DRPDelay, 1},
        {"full",     &DiskWT,  -1},
        {"lookup",   &LUPDelay, 1},
        {"overload", &MaxDelay,-1},
        {"servers",  &SUPCount, 0},
        {"service",  &SUPDelay, 1},
        {"startup",  &SRVDelay, 1},
        {"suspend",  &SUSDelay, 1}
       };
    int numopts = sizeof(dyopts)/sizeof(struct delayopts);

    if (!isManager) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "delay arguments not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, dyopts[i].opname))
                  {if (!(val = Config.GetWord()))
                      {eDest->Emsg("Config", "delay ", (char *)dyopts[i].opname,
                                  (char *)" argument not specified.");
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
           val = Config.GetWord();
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

int XrdOlbConfig::xfsxq(XrdOucError *eDest, XrdOucStream &Config)
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
    val = Config.GetWord();
    while (val && *val != '/')
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, xqopts[i].opname))
                  {xqopts[i].doset = 1;
                   xtval = 1;
                   break;
                  }
           if (i >= numopts)
              eDest->Emsg("Config", "invalid fsxeq type option -", val);
           val = Config.GetWord();
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
   Config.RetToken();

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

int XrdOlbConfig::xfxhld(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int ct;

    if (!isManager) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "fxhold value not specified."); return 1;}

    if (XrdOuca2x::a2tm(*eDest, "fxhold value", val, &ct, 60)) return 1;

    cachelife = ct;
    XrdOlbCache.setLifetime(ct);
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

int XrdOlbConfig::xlclrt(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int i;

// If we are a manager, ignore this option
//
   if (!isServer) return 0;

// Get path type
//
   val = Config.GetWord();
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
       LocalRLen = strlen(val);
      }
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

int XrdOlbConfig::xpath(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val, *path;
    int i = -1;
    XrdOlbPInfo  pmask;

// If we are a manager, ignore this option
//
   if (!isServer) return 0;

// Get path type
//
   val = Config.GetWord();
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
   path = Config.GetWord();
   if (!path || !path[0])
      {eDest->Emsg("Config", "path not specified"); return 1;}

// Add the path to the list of paths
//
   PathList.Insert(path, &pmask);
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
int XrdOlbConfig::xperf(XrdOucError *eDest, XrdOucStream &Config)
{   int   ival = 3*60;
    char *pgm=0, *val, *rest;

    if (!isServer) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "perf options not specified"); return 1;}

    do {     if (!strcmp("int", val))
                {if (!(val = Config.GetWord()))
                    {eDest->Emsg("Config", "perf int value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(*eDest,"perf int",val,&ival,0)) return 1;
                }
        else if (!strcmp("key", val))
                {if (!(val = Config.GetWord()))
                    {eDest->Emsg("Config", "perf key value not specified");
                     return 1;
                    }
                 eDest->Emsg("Config", "key parameter deprecated; ignored.");
                }
        else if (!strcmp("pgm",  val))
                {Config.RetToken();
                 Config.GetToken(&rest);
                 while (*rest == ' ') rest++;
                 if (!*rest)
                    {eDest->Emsg("Config", "perf prog value not specified");
                     return 1;
                    }
                 pgm = rest;
                 break;
                }
        else eDest->Emsg("Config", "Warning, invalid perf option", val);
       } while((val = Config.GetWord()));

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

int XrdOlbConfig::xpidf(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;

// Get the path
//
   val = Config.GetWord();
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
int XrdOlbConfig::xping(XrdOucError *eDest, XrdOucStream &Config)
{   int pnum = AskPerf, lnum = LogPerf, ping;
    char *val;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "ping value not specified"); return 1;}
    if (XrdOuca2x::a2tm(*eDest, "ping interval",val,&ping,0)) return 1;


    while((val = Config.GetWord()))
        {     if (!strcmp("log", val))
                 {if (!(val = Config.GetWord()))
                     {eDest->Emsg("Config", "ping log value not specified");
                      return 1;
                     }
                  if (XrdOuca2x::a2i(*eDest,"ping log",val,&lnum,0)) return 1;
                 }
         else if (!strcmp("usage", val))
                 {if (!(val = Config.GetWord()))
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

   Purpose:  To parse the directive: port <tcpnum> [<udpnumM> [<updnnumS>]]

             <tcpnum>   number of the tcp port for incomming requests
             <udpnumM>  number of the udp port for incomming manager requests
             <udpnumS>  number of the udp port for incomming server  requests

   Type: Manager or Server, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOlbConfig::xport(XrdOucError *eDest, XrdOucStream &Config)
{   int pnum = 0;
    char *val;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "tcp port not specified"); return 1;}
    if (isdigit(*val))
       {if (XrdOuca2x::a2i(*eDest,"tcp port",val,&pnum,1,65535)) return 1;}
       else if (!(pnum = XrdOucNetwork::findPort(val, "tcp")))
               {eDest->Emsg("Config", "Unable to find tcp service", val);
                return 1;
               }
    PortTCP = PortUDPm = PortUDPs = pnum;

    if ((val = Config.GetWord()))
       {if (isdigit(*val))
           {if (XrdOuca2x::a2i(*eDest,"udp port",val,&pnum,1,65535)) return 1;}
           else if (!(pnum = XrdOucNetwork::findPort(val, "udp")))
                   {eDest->Emsg("Config","Unable to find udp service",val);
                    return 1;
                   }
        PortUDPm = PortUDPs = pnum;
       }

    if (val && (val = Config.GetWord()))
       {if (isdigit(*val))
           {if (XrdOuca2x::a2i(*eDest,"server udp port",val,&pnum,1,65535)) return 1;}
           else if (!(pnum = XrdOucNetwork::findPort(val, "udp")))
                   {eDest->Emsg("Config","Unable to find server udp service",val);
                    return 1;
                   }
        PortUDPs = pnum;
       }
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
int XrdOlbConfig::xprep(XrdOucError *eDest, XrdOucStream &Config)
{   int   reset=0, scrub=0, echo = 0, doset = 0;
    char  *prepif=0, *val, *rest;

    if (!isServer) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "prep options not specified"); return 1;}

    do {     if (!strcmp("echo", val)) doset = echo = 1;
        else if (!strcmp("reset", val))
                {if (!(val = Config.GetWord()))
                    {eDest->Emsg("Config", "prep reset value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2i(*eDest,"prep reset int",val,&reset,1)) return 1;
                 doset = 1;
                }
        else if (!strcmp("scrub", val))
                {if (!(val = Config.GetWord()))
                    {eDest->Emsg("Config", "prep scrub value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(*eDest,"prep scrub",val,&scrub,0)) return 1;
                 doset = 1;
                }
        else if (!strcmp("ifpgm",  val))
                {Config.RetToken();
                 Config.GetToken(&rest);
                 while (*rest == ' ') rest++;
                 if (!*rest)
                    {eDest->Emsg("Config", "prep ifpgm value not specified");
                     return 1;
                    }
                 prepif = rest;
                 break;
                }
        else eDest->Emsg("Config", "Warning, invalid prep option", val);
       } while((val = Config.GetWord()));



// Set the values
//
   if (scrub) pendplife = scrub;
   if (doset) XrdOlbPrepQ.setParms(reset, scrub, echo);
   if (prepif) 
      if (!isExec(eDest, "prep", prepif)) return 1;
         else return XrdOlbPrepQ.setParms(prepif);
   return 0;
}

/******************************************************************************/
/*                                x r m t r t                                 */
/******************************************************************************/

/* Function: xpath

   Purpose:  To parse the directive: remoteroot <path>

             <path>    the path that the server will prefix to all remote paths.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xrmtrt(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int i;

// If we are a manager, ignore this option
//
   if (!isServer) return 0;

// Get path type
//
   val = Config.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "remoteroot path not specified"); return 1;}
   if (*val != '/')
      {eDest->Emsg("Config", "remoteroot path not absolute"); return 1;}

// Cleanup the path
//
   i = strlen(val)-1;
   while (i && val[i] == '/') val[i--] = '\0';

// Assign new path prefix
//
   if (i)
      {if (RemotRoot) free(RemotRoot);
       RemotRoot = strdup(val);
       RemotRLen = strlen(val);
      }
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

   Type: Manager only, dynamic.

   Output: retc upon success or -EINVAL upon failure.
*/

int XrdOlbConfig::xsched(XrdOucError *eDest, XrdOucStream &Config)
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

    if (!isManager) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "sched option not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, scopts[i].opname))
                  {if (!(val = Config.GetWord()))
                      {eDest->Emsg("Config", "sched ", (char *)scopts[i].opname,
                                  (char *)" argument not specified.");
                       return 1;
                      }
                   if (scopts[i].maxv < 0)
                      if (XrdOuca2x::a2tm(*eDest,"sched value", val, &ppp, 0)) return 1;
                         else if (XrdOuca2x::a2i(*eDest,"sched value", val, &ppp,
                                         0, scopts[i].maxv)) return 1;
                   *scopts[i].oploc = ppp;
                   break;
                  }
           if (i >= numopts)
              eDest->Emsg("Config", "Warning, invalid sched option", val);
           val = Config.GetWord();
          }

    return 0;
}

/******************************************************************************/
/*                                x s p a c e                                 */
/******************************************************************************/

/* Function: xspace

   Purpose:  To parse the directive: space [linger <num>] [[min] <min> [<adj>]]

             <num> Maximum number of times a server may be reselected without
                   a break. The default is 0.

             <min> Minimum free space need in bytes (or K, M, G).
                   The default is 10G.

             <adj> Bytes (or K, M,G) to adjust downwards per selection.
                   The default is 1G.

   Notes:   This is used by the manager and the server.

   Type: All, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xspace(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int alinger = -1;
    long long minf = -1, adj = -1;

    while((val = Config.GetWord()))
       {if (!strcmp("linger", val))
           {if (!(val = Config.GetWord()))
               {eDest->Emsg("Config", "linger value not specified"); return 1;}
            if (XrdOuca2x::a2i(*eDest,"linger",val,&alinger,0))
               return 1;
           }

        if (!strcmp("min", val) && !(val = Config.GetWord()))
           {eDest->Emsg("Config", "space minfree not specified"); return 1;}
        if (XrdOuca2x::a2sz(*eDest,"space minfree",val,&minf,0))
           return 1;

        if ((val = Config.GetWord())
        && (XrdOuca2x::a2sz(*eDest,"space adjust",val,&adj,0))) return 1;
           else break;
       }

    if (alinger >= 0) DiskLinger = alinger;

    if (minf < 0 && adj < 0)
       {eDest->Emsg("Config", "no space values specified"); return 1;}

    if (minf >= 0)
      {minf = minf / 1024;
       DiskMin = (minf >> 31 ? 0x7fffffff : minf) / 1024;
       if (adj >= 0)
          {adj = adj / 1204;
           DiskAdj = (adj >> 31 ? 0x7fffffff : adj) / 1024;
          }
      }
    return 0;
}
  
/******************************************************************************/
/*                                 x s u b s                                  */
/******************************************************************************/

/* Function: xsubs

   Purpose:  To parse the directive: subscribe <host>[+] [<port>]

             <host> The dns name of the host that is the manager of this host.
                    If the host name ends with a plus, all addresses that are
                    associated with the hosts are subscribed to.

             <port> The port number to use for this host. The default comes
                    from the port directive.

   Notes:   Any number of subscribe directives can be given. The server will
            subscribe to all of the managers.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xsubs(XrdOucError *eDest, XrdOucStream &Config)
{
    struct sockaddr_in InetAddr[8];
    XrdOucTList *tp = 0;
    char *val, *bval = 0, *mval;
    int i, port = 0;

    if (!isServer) return 0;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "subscribe host not specified"); return 1;}
    mval = strdup(val);

    if ((val = Config.GetWord()))
       {if (isdigit(*val))
           {if (XrdOuca2x::a2i(*eDest,"subscribe port",val,&port,1,65535))
               port = 0;
           }
           else if (!(port = XrdOucNetwork::findPort(val, "tcp")))
                   {eDest->Emsg("Config", "unable to find tcp service", val);
                    port = 0;
       }           }
       else if (!(port = PortTCP))
               eDest->Emsg("Config","subscribe port not specified for",mval);

    if (!port) {free(mval); return 1;}

    i = strlen(mval);
    if (mval[i-1] != '+') i = 0;
        else {bval = strdup(mval); mval[i-1] = '\0';
              if (!(i = XrdOucNetwork::getHostAddr(mval, InetAddr, 8)))
                 {eDest->Emsg("Config","Subscribe host", mval,
                              (char *)"not found"); 
                  free(bval); free(mval); return 1;
                 }
             }

    do {if (i)
           {i--; free(mval);
            mval = XrdOucNetwork::getHostName(InetAddr[i]);
            eDest->Emsg("Config", (const char *)bval, 
                        (char *)"-> olb.subscribe", mval);
           }
        tp = myManagers;
        while(tp) 
             if (strcmp(tp->text, mval) || tp->val != port) tp = tp->next;
                else {eDest->Emsg("Config","Duplicate subscription to",mval);
                      break;
                     }
        if (tp) break;
        myManagers = new XrdOucTList(mval, port, myManagers);
       } while(i);

    if (bval) free(bval);
    free(mval);
    return tp != 0;
}

/******************************************************************************/
/*                              x t h r e a d s                               */
/******************************************************************************/
  
/* Function: xthreads

   Purpose:  To parse the directive: threads [manager| server] <max> <min>

             <max>  max number of threads used for scheduling
             <min>  max number of threads used for scheduling

   Type: Any, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOlbConfig::xthreads(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int maxt, mint;
    XrdOlbScheduler *schedM, *schedS;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "threads not specified"); return 1;}

    while(val)
         {schedM = XrdOlbSchedM;
          schedS = XrdOlbSchedS;
          if (!strcmp("manager", val)) schedS = 0;
             else if (!strcmp("server", val)) schedM = 0;

          if (XrdOuca2x::a2i(*eDest, "max threads value", val, &maxt, 1))
             return 1;
          if (!(val = Config.GetWord())) mint = maxt;
             else if (XrdOuca2x::a2i(*eDest, "min threads value", val, &mint, 1))
                     return 1;
                     else val = Config.GetWord();

          if (schedM) schedM->setWorkers(mint, maxt);
          if (schedS) schedS->setWorkers(mint, maxt);
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

int XrdOlbConfig::xtrace(XrdOucError *eDest, XrdOucStream &Config)
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

    if (!(val = Config.GetWord()))
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
          val = Config.GetWord();
         }

    XrdOlbTrace.What = trval;
    return 0;
}
