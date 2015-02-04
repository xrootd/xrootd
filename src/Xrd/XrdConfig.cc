/*******************************************************************************/
/*                                                                            */
/*                          X r d C o n f i g . c c                           */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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
   The default port number comes from:
   1) The command line option,
   2) The config file,
   3) The /etc/services file for service corresponding to the program name.
*/
  
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdVersion.hh"

#include "Xrd/XrdConfig.hh"
#include "Xrd/XrdInfo.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdStats.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdNet/XrdNetSecurity.hh"
#include "XrdNet/XrdNetUtils.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucSiteName.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysUtils.hh"

#ifdef __linux__
#include <netinet/tcp.h>
#endif
#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
       const char       *XrdConfig::TraceID = "Config";

namespace XrdNetSocketCFG
{
extern int ka_Idle;
extern int ka_Itvl;
extern int ka_Icnt;
};

namespace
{
XrdOucEnv  theEnv;
};

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(eDest, Config);

#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                         X r d C o n f i g P r o t                          */
/******************************************************************************/

class XrdConfigProt
{
public:

XrdConfigProt  *Next;
char           *proname;
char           *libpath;
char           *parms;
int             port;
int             wanopt;

                XrdConfigProt(char *pn, char *ln, char *pp, int np=-1, int wo=0)
                    {Next = 0; proname = pn; libpath = ln; parms = pp; 
                     port=np; wanopt = wo;
                    }
               ~XrdConfigProt()
                    {free(proname);
                     if (libpath) free(libpath);
                     if (parms)   free(parms);
                    }
};

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdConfig::XrdConfig() : Log(&Logger, "Xrd"), Trace(&Log), Sched(&Log, &Trace),
                         BuffPool(&Log, &Trace)
{

// Preset all variables with common defaults
//
   PortTCP  = -1;
   PortUDP  = -1;
   PortWAN  = 0;
   ConfigFN = 0;
   myInsName= 0;
   mySitName= 0;
   AdminPath= strdup("/tmp");
   AdminMode= 0700;
   Police   = 0;
   Net_Blen = 0;  // Accept OS default (leave Linux autotune in effect)
   Net_Opts = XRDNET_KEEPALIVE;
   Wan_Blen = 1024*1024; // Default window size 1M
   Wan_Opts = XRDNET_KEEPALIVE;
   repDest[0] = 0;
   repDest[1] = 0;
   repInt     = 600;
   repOpts    = 0;
   ppNet      = 0;
   NetTCPlep  = -1;
   NetADM     = 0;
   coreV      = 1;
   memset(NetTCP, 0, sizeof(NetTCP));

   Firstcp = Lastcp = 0;

   ProtInfo.eDest   = &Log;             // Stable -> Error Message/Logging Handler
   ProtInfo.NetTCP  = 0;                // Stable -> Network Object
   ProtInfo.BPool   = &BuffPool;        // Stable -> Buffer Pool Manager
   ProtInfo.Sched   = &Sched;           // Stable -> System Scheduler
   ProtInfo.ConfigFN= 0;                // We will fill this in later
   ProtInfo.Stats   = 0;                // We will fill this in later
   ProtInfo.Trace   = &Trace;           // Stable -> Trace Information
   ProtInfo.AdmPath = AdminPath;        // Stable -> The admin path
   ProtInfo.AdmMode = AdminMode;        // Stable -> The admin path mode
   ProtInfo.theEnv  = &theEnv;          // Additional information

   ProtInfo.Format   = XrdFORMATB;
   ProtInfo.WANPort  = 0;
   ProtInfo.WANWSize = 0;
   ProtInfo.WSize    = 0;
   ProtInfo.ConnMax  = -1;     // Max       connections (fd limit)
   ProtInfo.readWait = 3*1000; // Wait time for data before we reschedule
   ProtInfo.idleWait = 0;      // Seconds connection may remain idle (0=off)
   ProtInfo.hailWait =30*1000; // Wait time for data before we drop connection
   ProtInfo.DebugON  = 0;      // 1 if started with -d
   ProtInfo.argc     = 0;
   ProtInfo.argv     = 0;

   XrdNetAddr::SetCache(3*60*60); // Cache address resolutions for 3 hours
}
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdConfig::Configure(int argc, char **argv)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   const char *xrdInst="XRDINSTANCE=";

   int retc, NoGo = 0, clPort = -1, optbg = 0;
   const char *temp;
   char c, buff[512], *dfltProt, *libProt = 0, *logfn = 0;
   uid_t myUid = 0;
   gid_t myGid = 0;
   extern char *optarg;
   extern int optind, opterr;
   int pipeFD[2] = {-1, -1};
   const char *pidFN = 0;
   static const int myMaxc = 80;
   char **urArgv, *myArgv[myMaxc], argBuff[myMaxc*3+8];
   char *argbP = argBuff, *argbE = argbP+sizeof(argBuff)-4;
   char *ifList = 0;
   int   myArgc = 1, bindArg = 1, urArgc = argc, i;
   bool noV6, ipV4 = false, ipV6 = false, pureLFN = false;

// Obtain the protocol name we will be using
//
    retc = strlen(argv[0]);
    while(retc--) if (argv[0][retc] == '/') break;
    myProg = dfltProt = &argv[0][retc+1];

// Setup the initial required protocol. The program name matches the protocol
// name but may be arbitrarily suffixed. We need to ignore this suffix. So we
// look for it here and it it exists we duplicate argv[0] (yes, loosing some
// bytes - sorry valgrind) without the suffix.
//
  {char *p = dfltProt;
   while(*p && (*p == '.' || *p == '-')) p++;
   if (*p)
      {char *dot = index(p, '.'), *dash = index(p, '-'), sepc;
       if (dot  && (dot < dash || !dash)) p = dot;
          else if (dash) p = dash;
                  else   p = 0;
       if (p) {sepc = *p; *p = '\0'; dfltProt = strdup(dfltProt); *p = sepc;}
      }
  }
   myArgv[0] = argv[0];

// Prescan the argument list to see if there is a passthrough option. In any
// case, we will set the ephemeral argv/arg in the environment.
//
  i = 1;
  while(i < argc)
      {if (*(argv[i]) == '-' && *(argv[i]+1) == '+')
          {int n = strlen(argv[i]+2), j = i+1, k = 1;
           if (urArgc == argc) urArgc = i;
           if (n) strncpy(buff, argv[i]+2, (n > 256 ? 256 : n));
           strcpy(&(buff[n]), ".argv**");
           while(j < argc && (*(argv[j]) != '-' || *(argv[j]+1) != '+')) j++;
           urArgv = new char*[j-i+1];
           urArgv[0] = argv[0];
           i++;
           while(i < j) urArgv[k++] = argv[i++];
           urArgv[k] = 0;
           theEnv.PutPtr(buff, urArgv);
           strcpy(&(buff[n]), ".argc");
           theEnv.PutInt(buff, static_cast<long>(k));
          } else i++;
      }
   theEnv.PutPtr("argv[0]", argv[0]);

// Process the options. Note that we cannot passthrough long options or
// options that take arguments because getopt permutes the arguments.
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(urArgc,argv,":bc:dhHI:k:l:L:n:p:P:R:s:S:vz"))
             && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'b': optbg = 1;
                 break;
       case 'c': if (ConfigFN) free(ConfigFN);
                 ConfigFN = strdup(optarg);
                 break;
       case 'd': Trace.What |= TRACE_ALL;
                 ProtInfo.DebugON = 1;
                 XrdOucEnv::Export("XRDDEBUG",  "1");
                 break;
       case 'h': Usage(0);
                 break;
       case 'H': Usage(-1);
                 break;
       case 'I':      if (!strcmp("v4", optarg)) {ipV4 = true;  ipV6 = false;}
                 else if (!strcmp("v6", optarg)) {ipV4 = false; ipV6 = true;}
                 else {Log.Emsg("Config", "Invalid -I argument -",optarg);
                       Usage(1);
                      }
                 break;
       case 'k': if (!(bindArg = Log.logger()->ParseKeep(optarg)))
                    {Log.Emsg("Config","Invalid -k argument -",optarg);
                     Usage(1);
                    }
                 break;
       case 'l': if ((pureLFN = *optarg == '=')) optarg++;
                 if (!*optarg)
                    {Log.Emsg("Config", "Logfile name not specified.");
                     Usage(1);
                    }
                 if (logfn) free(logfn);
                 logfn = strdup(optarg);
                 break;
       case 'L': if (!*optarg)
                    {Log.Emsg("Config", "Protocol library path not specified.");
                     Usage(1);
                    }
                 if (libProt) free(libProt);
                 libProt = strdup(optarg);
                 break;
       case 'n': myInsName = (!strcmp(optarg,"anon")||!strcmp(optarg,"default")
                           ? 0 : optarg);
                 break;
       case 'p': if ((clPort = yport(&Log, "tcp", optarg)) < 0) Usage(1);
                 break;
       case 'P': dfltProt = optarg;
                 break;
       case 'R': if (!(getUG(optarg, myUid, myGid))) Usage(1);
                 break;
       case 's': pidFN = optarg;
                 break;
       case 'S': mySitName = optarg;
                 break;
       case ':': buff[0] = '-'; buff[1] = optopt; buff[2] = 0;
                 Log.Emsg("Config", buff, "parameter not specified.");
                 Usage(1);
                 break;
       case 'v': cerr <<XrdVSTRING <<endl;
                 _exit(0);
                 break;
       case 'z': Log.logger()->setHiRes();
                 break;

       default: if (optopt == '-' && *(argv[optind]+1) == '-')
                   {Log.Emsg("Config", "Long options are not supported.");
                    Usage(1);
                   }
                if (myArgc >= myMaxc || argbP >= argbE)
                   {Log.Emsg("Config", "Too many command line arguments.");
                    Usage(1);
                   }
                myArgv[myArgc++] = argbP;
                *argbP++ = '-'; *argbP++ = optopt; *argbP++ = 0;
                break;
       }
     }

// The first thing we must do is to set the correct networking mode
//
   noV6 = XrdNetAddr::IPV4Set();
        if (ipV4) XrdNetAddr::SetIPV4();
   else if (ipV6){if (noV6) Log.Say("Config warning: ipV6 appears to be broken;"
                                    " forced ipV6 mode not advised!");
                  XrdNetAddr::SetIPV6();
                 }
   else if (noV6) Log.Say("Config warning: ipV6 is misconfigured or "
                          "unavailable; reverting to ipV4.");

// Set the site name if we have one
//
   if (mySitName) mySitName = XrdOucSiteName::Set(mySitName, 63);

// Drop into non-privileged state if so requested
//
   if (myGid && setegid(myGid))
      {Log.Emsg("Config", errno, "set effective gid"); exit(17);}
   if (myUid && seteuid(myUid))
      {Log.Emsg("Config", errno, "set effective uid"); exit(17);}

// Pass over any parameters
//
   if (urArgc-optind+2 >= myMaxc)
      {Log.Emsg("Config", "Too many command line arguments.");
       Usage(1);
      }
   for ( ; optind < urArgc; optind++) myArgv[myArgc++] = argv[optind];

// Record the actual arguments that we will pass on
//
   myArgv[myArgc] = 0;
   ProtInfo.argc = myArgc;
   ProtInfo.argv = myArgv;

// Resolve background/foreground issues
//
   if (optbg)
   {
#ifdef WIN32
      XrdOucUtils::Undercover(&Log, !logfn);
#else
      if (pipe( pipeFD ) == -1)
         {Log.Emsg("Config", errno, "create a pipe"); exit(17);}
      XrdOucUtils::Undercover(Log, !logfn, pipeFD);
#endif
   }

// Bind the log file if we have one
//
   if (logfn)
      {char *lP;
       if (!pureLFN && !(logfn = XrdOucUtils::subLogfn(Log,myInsName,logfn)))
          _exit(16);
       Log.logger()->AddMsg(XrdBANNER);
       if (Log.logger()->Bind(logfn, bindArg)) exit(19);
       if ((lP = rindex(logfn,'/'))) {*(lP+1) = '\0'; lP = logfn;}
          else lP = (char *)"./";
       XrdOucEnv::Export("XRDLOGDIR", lP);
      }

// Get the full host name. In theory, we should always get some kind of name.
// We must define myIPAddr here because we may need to run in v4 mode and
// that doesn't get set until after the options are scanned.
//
   static XrdNetAddr *myIPAddr = new XrdNetAddr((int)0);
   if (!(myName = myIPAddr->Name(0, &temp)))
      {Log.Emsg("Config", "Unable to determine host name; ",
                           (temp ? temp : "reason unknown"),
                           "; execution terminated.");
       _exit(16);
      }

// Verify that we have a real name. We've had problems with people setting up
// bad /etc/hosts files that can cause connection failures if "allow" is used.
// Otherwise, determine our domain name.
//
   if (!myIPAddr->isRegistered())
      {Log.Emsg("Config",myName,"does not appear to be registered in the DNS.");
       Log.Emsg("Config","Verify that the '/etc/hosts' file is correct and "
                         "this machine is registered in DNS.");
       Log.Emsg("Config", "Execution continues but connection failures may occur.");
       myDomain = 0;
      } else if (!(myDomain = index(myName, '.')))
                Log.Say("Config warning: this hostname, ", myName,
                            ", is registered without a domain qualification.");

// Get our IP address and FQN
//
   ProtInfo.myName = myName;
   ProtInfo.myAddr = myIPAddr->SockAddr();
   ProtInfo.myInst = XrdOucUtils::InstName(myInsName);
   ProtInfo.myProg = myProg;

// Set the Environmental variable to hold the instance name
// XRDINSTANCE=<pgm> <instance name>@<host name>
//                 XrdOucEnv::Export("XRDINSTANCE")
//
   sprintf(buff,"%s%s %s@%s", xrdInst, myProg, ProtInfo.myInst, myName);
   myInstance = strdup(buff);
   putenv(myInstance);   // XrdOucEnv::Export("XRDINSTANCE",...)
   myInstance += strlen(xrdInst);
   XrdOucEnv::Export("XRDHOST", myName);
   XrdOucEnv::Export("XRDNAME", ProtInfo.myInst);
   XrdOucEnv::Export("XRDPROG", myProg);
   XrdNetIF::SetMsgs(&Log);

// Put out the herald
//
   strcpy(buff, "Starting on ");
   retc = strlen(buff);
   XrdSysUtils::FmtUname(buff+retc, sizeof(buff)-retc);
   Log.Say(0, buff);
   Log.Say(XrdBANNER);

// Setup the initial required protocol.
//
   Firstcp = Lastcp = new XrdConfigProt(strdup(dfltProt), libProt, 0);

// Let start it up!
//
   Log.Say("++++++ ", myInstance, " initialization started.");

// Process the configuration file, if one is present
//
   if (ConfigFN && *ConfigFN)
      {Log.Say("Config using configuration file ", ConfigFN);
       ProtInfo.ConfigFN = ConfigFN;
       setCFG();
       NoGo = ConfigProc();
      }
   if (clPort >= 0) PortTCP = clPort;
   if (ProtInfo.DebugON) 
      {Trace.What = TRACE_ALL;
       XrdSysThread::setDebug(&Log);
      }

// Export the network interface list at this point
//
   if (ppNet && XrdNetIF::GetIF(ifList, 0, true))
      XrdOucEnv::Export("XRDIFADDRS",ifList);

// Configure network routing
//
   if (!XrdInet::netIF.SetIF(myIPAddr, ifList))
      {Log.Emsg("Config", "Unable to determine interface addresses!");
       NoGo = 1;
      }

// Now initialize the default protocl
//
   if (!NoGo) NoGo = Setup(dfltProt);

// If we hae a net name change the working directory
//
   if (myInsName) XrdOucUtils::makeHome(Log, myInsName);

   // if we call this it means that the daemon has forked and we are
   // in the child process
#ifndef WIN32
   if (optbg)
   {
      if (pidFN && !XrdOucUtils::PidFile(Log, pidFN ) )
         NoGo = 1;

      int status = NoGo ? 1 : 0;
      if(write( pipeFD[1], &status, sizeof( status ) )) {};
      close( pipeFD[1]);
   }
#endif

// Establish a pid/manifest file for auto-collection
//
   if (!NoGo) Manifest(pidFN);

// All done, close the stream and return the return code.
//
   temp = (NoGo ? " initialization failed." : " initialization completed.");
   sprintf(buff, "%s:%d", myInstance, PortTCP);
   Log.Say("------ ", buff, temp);
   if (logfn)
      {strcat(buff, " running ");
       retc = strlen(buff);
       XrdSysUtils::FmtUname(buff+retc, sizeof(buff)-retc);
       Log.logger()->AddMsg(buff);
       free(logfn);
      }
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdConfig::ConfigXeq(char *var, XrdOucStream &Config, XrdSysError *eDest)
{
   int dynamic;

   // Determine whether is is dynamic or not
   //
   if (eDest) dynamic = 1;
      else   {dynamic = 0; eDest = &Log;}

   // Process common items
   //
   TS_Xeq("buffers",       xbuf);
   TS_Xeq("network",       xnet);
   TS_Xeq("sched",         xsched);
   TS_Xeq("trace",         xtrace);

   // Process items that can only be processed once
   //
   if (!dynamic)
   {
   TS_Xeq("adminpath",     xapath);
   TS_Xeq("allow",         xallow);
   TS_Xeq("port",          xport);
   TS_Xeq("protocol",      xprot);
   TS_Xeq("report",        xrep);
   TS_Xeq("sitename",      xsit);
   TS_Xeq("timeout",       xtmo);
   }

   // No match found, complain.
   //
   eDest->Say("Config warning: ignoring unknown xrd directive '",var,"'.");
   Config.Echo();
   return 0;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                               A S o c k e t                                */
/******************************************************************************/
  
int XrdConfig::ASocket(const char *path, const char *fname, mode_t mode)
{
   char xpath[MAXPATHLEN+8], sokpath[108];
   int  plen = strlen(path), flen = strlen(fname);
   int rc;

// Make sure we can fit everything in our buffer
//
   if ((plen + flen + 3) > (int)sizeof(sokpath))
      {Log.Emsg("Config", "admin path", path, "too long");
       return 1;
      }

// Create the directory path
//
   strcpy(xpath, path);
   if ((rc = XrdOucUtils::makePath(xpath, mode)))
       {Log.Emsg("Config", rc, "create admin path", xpath);
        return 1;
       }

// *!*!* At this point we do not yet support the admin path for xrd.
// sp we comment out all of the following code.

/*
// Construct the actual socket name
//
  if (sokpath[plen-1] != '/') sokpath[plen++] = '/';
  strcpy(&sokpath[plen], fname);

// Create an admin network
//
   NetADM = new XrdInet(&Log);
   if (myDomain) NetADM->setDomain(myDomain);

// Bind the netwok to the named socket
//
   if (!NetADM->Bind(sokpath)) return 1;

// Set the mode and return
//
   chmod(sokpath, mode); // This may fail on some platforms
*/
   return 0;
}

/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdConfig::ConfigProc()
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucEnv myEnv;
  XrdOucStream Config(&Log, myInstance, &myEnv, "=====> ");

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Log.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        if (!strncmp(var, "xrd.", 4)
        ||  !strcmp (var, "all.adminpath")
        ||  !strcmp (var, "all.sitename" ))
           if (ConfigXeq(var+4, Config)) {Config.Echo(); NoGo = 1;}

// Now check if any errors occured during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = Log.Emsg("Config", retc, "read config file", ConfigFN);
   Config.Close();

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                                 g e t U G                                  */
/******************************************************************************/
  
int XrdConfig::getUG(char *parm, uid_t &newUid, gid_t &newGid)
{
   struct passwd *pp;

// Get the userid entry
//
   if (!(*parm))
      {Log.Emsg("Config", "-R user not specified."); return 0;}

   if (isdigit(*parm))
      {if (!(newUid = atol(parm)))
          {Log.Emsg("Config", "-R", parm, "is invalid"); return 0;}
       pp = getpwuid(newUid);
      }
      else pp = getpwnam(parm);

// Make sure it is valid and acceptable
//
   if (!pp) 
      {Log.Emsg("Config", errno, "retrieve -R user password entry");
       return 0;
      }
   if (!(newUid = pp->pw_uid))
      {Log.Emsg("Config", "-R", parm, "is still unacceptably a superuser!");
       return 0;
      }
   newGid = pp->pw_gid;
   return 1;
}

/******************************************************************************/
/*                              M a n i f e s t                               */
/******************************************************************************/
  
void XrdConfig::Manifest(const char *pidfn)
{
   const char *Slash;
   char envBuff[8192], pwdBuff[1024], manBuff[1024], *pidP, *sP, *xP;
   int envFD, envLen;

// Get the current working directory
//
   if (!getcwd(pwdBuff, sizeof(pwdBuff)))
      {Log.Emsg("Config", "Unable to get current working directory!");
       return;
      }

// Prepare for symlinks
//
   strcpy(envBuff, ProtInfo.AdmPath);
   envLen = strlen(envBuff);
   if (envBuff[envLen-1] != '/') {envBuff[envLen] = '/'; envLen++;}
   strcpy(envBuff+envLen, ".xrd/");
   xP = envBuff+envLen+5;

// Create a symlink to the configuration file
//
   if ((sP = getenv("XRDCONFIGFN")))
      {sprintf(xP, "=/conf/%s.cf", myProg);
       XrdOucUtils::ReLink(envBuff, sP);
      }

// Create a symlink to where core files will be found
//
   sprintf(xP, "=/core/%s", myProg);
   XrdOucUtils::ReLink(envBuff, pwdBuff);

// Create a symlink to where log files will be found
//
   if ((sP = getenv("XRDLOGDIR")))
      {sprintf(xP, "=/logs/%s", myProg);
       XrdOucUtils::ReLink(envBuff, sP);
      }

// Create a symlink to out proc information (Linux only)
//
#ifdef __linux__
   sprintf(xP, "=/proc/%s", myProg);
   sprintf(manBuff, "/proc/%d", getpid());
   XrdOucUtils::ReLink(envBuff, manBuff);
#endif

// Create environment string
//
   envLen = snprintf(envBuff, sizeof(envBuff), "pid=%d&host=%s&inst=%s&ver=%s"
                     "&cfgfn=%s&cwd=%s&apath=%s&logfn=%s\n",
                     static_cast<int>(getpid()), ProtInfo.myName,
                     ProtInfo.myInst, XrdVSTRING,
                     (getenv("XRDCONFIGFN") ? getenv("XRDCONFIGFN") : ""),
                     pwdBuff, ProtInfo.AdmPath, Log.logger()->xlogFN());

// Find out where we should write this
//
   if (pidfn && (Slash = rindex(pidfn, '/')))
      {strncpy(manBuff, pidfn, Slash-pidfn); pidP = manBuff+(Slash-pidfn);}
      else {strcpy(manBuff, "/tmp");         pidP = manBuff+4;}

// Construct the pid file name for ourselves
//
   snprintf(pidP, sizeof(manBuff)-(pidP-manBuff), "/%s.%s.env",
                     ProtInfo.myProg, ProtInfo.myInst);

// Open the file
//
   if ((envFD = open(manBuff, O_WRONLY|O_CREAT|O_TRUNC, 0664)) < 0)
      {Log.Emsg("Config", errno, "create envfile", manBuff);
       return;
      }

// Write out environmental information
//
   if (write(envFD, envBuff, envLen) < 0)
      Log.Emsg("Config", errno, "write to envfile", manBuff);

// All done
//
   close(envFD);
}

/******************************************************************************/
/*                                s e t C F G                                 */
/******************************************************************************/
  
void XrdConfig::setCFG()
{
   char cwdBuff[1024], *cfnP = cwdBuff;
   int n;

// If the config file is absolute, export it as is
//
   if (*ConfigFN == '/')
      {XrdOucEnv::Export("XRDCONFIGFN", ConfigFN);
       return;
      }

// Prefix current working directory to the config file
//
   if (!getcwd(cwdBuff, sizeof(cwdBuff)-strlen(ConfigFN)-2)) cfnP = ConfigFN;
      else {n = strlen(cwdBuff);
            if (cwdBuff[n-1] != '/') cwdBuff[n++] = '/';
            strcpy(cwdBuff+n, ConfigFN);
           }

// Export result
//
   XrdOucEnv::Export("XRDCONFIGFN", cfnP);
}

/******************************************************************************/
/*                                s e t F D L                                 */
/******************************************************************************/
  
int XrdConfig::setFDL()
{
   static const int maxFD = 1048576;
   struct rlimit rlim;
   char buff[100];

// Get the resource limit
//
   if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
      return Log.Emsg("Config", errno, "get FD limit");

// Set the limit to the maximum allowed
//
   if (rlim.rlim_max == RLIM_INFINITY) rlim.rlim_max = maxFD;
   rlim.rlim_cur = rlim.rlim_max;
#if (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_5))
   if (rlim.rlim_cur > OPEN_MAX) rlim.rlim_max = rlim.rlim_cur = OPEN_MAX;
#endif
   if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
      return Log.Emsg("Config", errno,"set FD limit");

// Obtain the actual limit now
//
   if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
      return Log.Emsg("Config", errno, "get FD limit");

// Establish operating limit
//
   ProtInfo.ConnMax = rlim.rlim_cur;
   sprintf(buff, "%d", ProtInfo.ConnMax);
   Log.Say("Config maximum number of connections restricted to ", buff);

// Set core limit and but Solaris
//
#if !defined( __solaris__ ) && defined(RLIMIT_CORE)
   if (coreV >= 0)
      {if (getrlimit(RLIMIT_CORE, &rlim) < 0)
          Log.Emsg("Config", errno, "get core limit");
          else {rlim.rlim_cur = (coreV ? rlim.rlim_max : 0);
                if (setrlimit(RLIMIT_CORE, &rlim) < 0)
                   Log.Emsg("Config", errno,"set core limit");
               }
      }
#endif

// We try to set the thread limit here but only if we can
//
#if defined(__linux__) && defined(RLIMIT_NPROC)

// Obtain the actual limit now (Scheduler construction sets this to rlim_max)
//
   if (getrlimit(RLIMIT_NPROC, &rlim) < 0)
      return Log.Emsg("Config", errno, "get thread limit");

// Establish operating limit
//
   int nthr = static_cast<int>(rlim.rlim_cur);
   if (nthr < 8192 || ProtInfo.DebugON)
      {sprintf(buff, "%d", static_cast<int>(rlim.rlim_cur));
       Log.Say("Config maximum number of threads restricted to ", buff);
      }
#endif

   return 0;
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
int XrdConfig::Setup(char *dfltp)
{
   XrdInet *NetWAN;
   XrdConfigProt *cp;
   int i, wsz, arbNet;

// Establish the FD limit
//
   if (setFDL()) return 1;

// Special handling for Linux sendfile()
//
#if defined(__linux__) && defined(TCP_CORK)
{  int sokFD, setON = 1;
   if ((sokFD = socket(PF_INET, SOCK_STREAM, 0)) >= 0)
      {setsockopt(sokFD, XrdNetUtils::ProtoID("tcp"), TCP_NODELAY,
                  &setON, sizeof(setON));
       if (setsockopt(sokFD, SOL_TCP, TCP_CORK, &setON, sizeof(setON)) < 0)
          XrdLink::sfOK = 0;
       close(sokFD);
      }
}
#endif

// Indicate how sendfile is being handled
//
   TRACE(NET,"sendfile " <<(XrdLink::sfOK ? "enabled." : "disabled!"));

// Initialize the buffer manager
//
   BuffPool.Init();

// Start the scheduler
//
   Sched.Start();

// Setup the link and socket polling infrastructure
//
   XrdLink::Init(&Log, &Trace, &Sched);
   XrdPoll::Init(&Log, &Trace, &Sched);
   if (!XrdLink::Setup(ProtInfo.ConnMax, ProtInfo.idleWait)
   ||  !XrdPoll::Setup(ProtInfo.ConnMax)) return 1;

// Modify the AdminPath to account for any instance name. Note that there is
// a negligible memory leak under ceratin path combinations. Not enough to
// warrant a lot of logic to get around.
//
   if (myInsName) ProtInfo.AdmPath = XrdOucUtils::genPath(AdminPath,myInsName);
      else ProtInfo.AdmPath = AdminPath;
   XrdOucEnv::Export("XRDADMINPATH", ProtInfo.AdmPath);
   AdminPath = XrdOucUtils::genPath(AdminPath, myInsName, ".xrd");

// Setup admin connection now
//
   if (ASocket(AdminPath, "admin", (mode_t)AdminMode)) return 1;

// Determine the default port number (only for xrootd) if not specified.
//
   if (PortTCP < 0)  
      {if ((PortTCP = XrdNetUtils::ServPort(dfltp))) PortUDP = PortTCP;
          else PortTCP = -1;
      }

// We now go through all of the protocols and get each respective port number.
//
   XrdProtLoad::Init(&Log, &Trace); cp = Firstcp;
   while(cp)
        {ProtInfo.Port = (cp->port < 0 ? PortTCP : cp->port);
         XrdOucEnv::Export("XRDPORT", ProtInfo.Port);
         if ((cp->port = XrdProtLoad::Port(cp->libpath, cp->proname,
                                           cp->parms, &ProtInfo)) < 0) return 1;
         cp = cp->Next;
        }

// Allocate the statistics object. This is akward since we only know part
// of the current configuration. The object will figure this out later.
//
   ProtInfo.Stats = new XrdStats(&Log, &Sched, &BuffPool,
                                 ProtInfo.myName, Firstcp->port,
                                 ProtInfo.myInst, ProtInfo.myProg, mySitName);

// Allocate a WAN port number of we need to
//
   if (PortWAN &&  (NetWAN = new XrdInet(&Log, &Trace, Police)))
      {if (Wan_Opts || Wan_Blen) NetWAN->setDefaults(Wan_Opts, Wan_Blen);
       if (myDomain) NetWAN->setDomain(myDomain);
       if (NetWAN->Bind((PortWAN > 0 ? PortWAN : 0), "tcp")) return 1;
       PortWAN  = NetWAN->Port();
       wsz      = NetWAN->WSize();
       Wan_Blen = (wsz < Wan_Blen || !Wan_Blen ? wsz : Wan_Blen);
       TRACE(NET,"WAN port " <<PortWAN <<" wsz=" <<Wan_Blen <<" (" <<wsz <<')');
       NetTCP[XrdProtLoad::ProtoMax] = NetWAN;
      } else {PortWAN = 0; Wan_Blen = 0;}

// Load the protocols. For each new protocol port number, create a new
// network object to handle the port dependent communications part. All
// port issues will have been resolved at this point.
//
   arbNet = XrdProtLoad::ProtoMax;
   while((cp = Firstcp))
        {if (!(cp->port)) i = arbNet;
            else for (i = 0; i < XrdProtLoad::ProtoMax && NetTCP[i]; i++)
                     {if (cp->port == NetTCP[i]->Port()) break;}
         if (i >= XrdProtLoad::ProtoMax || !NetTCP[i])
            {NetTCP[++NetTCPlep] = new XrdInet(&Log, &Trace, Police);
             if (Net_Opts || Net_Blen)
                NetTCP[NetTCPlep]->setDefaults(Net_Opts, Net_Blen);
             if (myDomain) NetTCP[NetTCPlep]->setDomain(myDomain);
             if (NetTCP[NetTCPlep]->Bind(cp->port, "tcp")) return 1;
             ProtInfo.Port   = NetTCP[NetTCPlep]->Port();
             ProtInfo.NetTCP = NetTCP[NetTCPlep];
             wsz             = NetTCP[NetTCPlep]->WSize();
             ProtInfo.WSize  = (wsz < Net_Blen || !Net_Blen ? wsz : Net_Blen);
             TRACE(NET,"LCL port " <<ProtInfo.Port <<" wsz=" <<ProtInfo.WSize
                       <<" (" <<wsz <<')');
             if (cp->wanopt)
                {ProtInfo.WANPort = PortWAN;
                 ProtInfo.WANWSize= Wan_Blen;
                } else ProtInfo.WANPort = ProtInfo.WANWSize = 0;
             if (!(cp->port)) arbNet = NetTCPlep;
             if (!NetTCPlep) XrdLink::Init(NetTCP[0]);
             XrdOucEnv::Export("XRDPORT", ProtInfo.Port);
            }
         if (!XrdProtLoad::Load(cp->libpath,cp->proname,cp->parms,&ProtInfo))
            return 1;
         Firstcp = cp->Next; delete cp;
        }

// Leave the env port number to be the first used port number. This may
// or may not be the same as the default port number.
//
   ProtInfo.Port = NetTCP[0]->Port();
   PortTCP = ProtInfo.Port;
   XrdOucEnv::Export("XRDPORT", PortTCP);

// Now check if we have to setup automatic reporting
//
   if (repDest[0] != 0 && repOpts) 
      ProtInfo.Stats->Report(repDest, repInt, repOpts);

// All done
//
   return 0;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void XrdConfig::Usage(int rc)
{
  extern const char *XrdLicense;

  if (rc < 0) cerr <<XrdLicense;
     else
     cerr <<"\nUsage: " <<myProg <<" [-b] [-c <cfn>] [-d] [-h] [-H] [-I {v4|v6}]\n"
            "[-k {n|sz|sig}] [-l [=]<fn>] [-n name] [-p <port>] [-P <prot>] [-L <libprot>]\n"
            "[-R] [-s pidfile] [-S site] [-v] [-z] [<prot_options>]" <<endl;
     _exit(rc > 0 ? rc : 0);
}

/******************************************************************************/
/*                                x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path> [group]

             <path>    the path of the FIFO to use for admin requests.

             group     allows group access to the admin path

   Note: A named socket is created <path>/<name>/.xrd/admin

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xapath(XrdSysError *eDest, XrdOucStream &Config)
{
    char *pval, *val;
    mode_t mode = S_IRWXU;

// Get the path
//
   pval = Config.GetWord();
   if (!pval || !pval[0])
      {eDest->Emsg("Config", "adminpath not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {eDest->Emsg("Config", "adminpath not absolute"); return 1;}

// Record the path
//
   if (AdminPath) free(AdminPath);
   AdminPath = strdup(pval);

// Get the optional access rights
//
   if ((val = Config.GetWord()) && val[0])
      {if (!strcmp("group", val)) mode |= S_IRWXG;
          else {eDest->Emsg("Config", "invalid admin path modifier -", val);
                return 1;
               }
      }
   AdminMode = ProtInfo.AdmMode = mode;
   return 0;
}
  
/******************************************************************************/
/*                                x a l l o w                                 */
/******************************************************************************/

/* Function: xallow

   Purpose:  To parse the directive: allow {host | netgroup} <name>

             <name> The dns name of the host that is allowed to connect or the
                    netgroup name the host must be a member of. For DNS names,
                    a single asterisk may be specified anywhere in the name.

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xallow(XrdSysError *eDest, XrdOucStream &Config)
{
    char *val;
    int ishost;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "allow type not specified"); return 1;}

    if (!strcmp(val, "host")) ishost = 1;
       else if (!strcmp(val, "netgroup")) ishost = 0;
               else {eDest->Emsg("Config", "invalid allow type -", val);
                     return 1;
                    }

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "allow target name not specified"); return 1;}

    if (!Police) Police = new XrdNetSecurity();
    if (ishost)  Police->AddHost(val);
       else      Police->AddNetGroup(val);

    return 0;
}

/******************************************************************************/
/*                                  x b u f                                   */
/******************************************************************************/

/* Function: xbuf

   Purpose:  To parse the directive: buffers <memsz> [<rint>]

             <memsz>    maximum amount of memory devoted to buffers
             <rint>     minimum buffer reshape interval in seconds

   Output: 0 upon success or !0 upon failure.
*/
int XrdConfig::xbuf(XrdSysError *eDest, XrdOucStream &Config)
{
    int bint = -1;
    long long blim;
    char *val;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "buffer memory limit not specified"); return 1;}
    if (XrdOuca2x::a2sz(*eDest,"buffer limit value",val,&blim,
                       (long long)1024*1024)) return 1;

    if ((val = Config.GetWord()))
       if (XrdOuca2x::a2tm(*eDest,"reshape interval", val, &bint, 300))
          return 1;

    BuffPool.Set((int)blim, bint);
    return 0;
}

/******************************************************************************/
/*                                  x n e t                                   */
/******************************************************************************/

/* Function: xnet

   Purpose:  To parse directive: network [wan] [[no]keepalive] [buffsz <blen>]
                                         [kaparms parms] [cache <ct>] [[no]dnr]
                                         [routes <rtype> [use <ifn1>,<ifn2>]]

             <rtype>: split | common | local

             wan       parameters apply only to the wan port
             keepalive do [not] set the socket keepalive option.
             kaparms   keepalive paramters as specfied by parms.
             <blen>    is the socket's send/rcv buffer size.
             <ct>      Seconds to cache address to name resolutions.
             [no]dnr   do [not] perform a reverse DNS lookup if not needed.
             routes    specifies the network configuration (see reference)

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xnet(XrdSysError *eDest, XrdOucStream &Config)
{
    char *val;
    int  i, n, V_keep = -1, V_nodnr = 0, V_iswan = 0, V_blen = -1, V_ct = -1;
    long long llp;
    struct netopts {const char *opname; int hasarg; int opval;
                           int *oploc;  const char *etxt;}
           ntopts[] =
       {
        {"keepalive",  0, 1, &V_keep,   "option"},
        {"nokeepalive",0, 0, &V_keep,   "option"},
        {"kaparms",    4, 0, &V_keep,   "option"},
        {"buffsz",     1, 0, &V_blen,   "network buffsz"},
        {"cache",      2, 0, &V_ct,     "cache time"},
        {"dnr",        0, 0, &V_nodnr,  "option"},
        {"nodnr",      0, 1, &V_nodnr,  "option"},
        {"routes",     3, 1, 0,         "routes"},
        {"wan",        0, 1, &V_iswan,  "option"}
       };
    int numopts = sizeof(ntopts)/sizeof(struct netopts);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "net option not specified"); return 1;}

    while (val)
    {for (i = 0; i < numopts; i++)
         if (!strcmp(val, ntopts[i].opname))
            {if (!ntopts[i].hasarg) *ntopts[i].oploc = ntopts[i].opval;
                else {if (!(val = Config.GetWord()))
                         {eDest->Emsg("Config", "network",
                              ntopts[i].opname, "argument missing");
                          return 1;
                         }
                      if (ntopts[i].hasarg == 4)
                         {if (xnkap(eDest, val)) return 1;
                          break;
                         }
                      if (ntopts[i].hasarg == 3)
                         {     if (!strcmp(val, "split"))
                                  XrdNetIF::Routing(XrdNetIF::netSplit);
                          else if (!strcmp(val, "common"))
                                  XrdNetIF::Routing(XrdNetIF::netCommon);
                          else if (!strcmp(val, "local"))
                                  XrdNetIF::Routing(XrdNetIF::netLocal);
                          else {eDest->Emsg("Config","Invalid routes argument -",val);
                                return 1;
                               }
                          if (!(val =  Config.GetWord())|| !(*val)) break;
                          if (strcmp(val, "use")) continue;
                          if (!(val =  Config.GetWord())|| !(*val))
                             {eDest->Emsg("Config", "network routes i/f names "
                                                    "not specified.");
                              return 1;
                             }
                          if (!XrdNetIF::SetIFNames(val)) return 1;
                          ppNet = 1;
                          break;
                         }
                      if (ntopts[i].hasarg == 2)
                         {if (XrdOuca2x::a2tm(*eDest,ntopts[i].etxt,val,&n,0))
                             return 1;
                          *ntopts[i].oploc = n;
                         } else {
                          if (XrdOuca2x::a2sz(*eDest,ntopts[i].etxt,val,&llp,0))
                             return 1;
                          *ntopts[i].oploc = (int)llp;
                         }
                     }
              break;
            }
      if (i >= numopts)
         eDest->Say("Config warning: ignoring invalid net option '",val,"'.");
         else if (!val) break;
      val = Config.GetWord();
     }

     if (V_iswan)
        {if (V_blen >= 0) Wan_Blen = V_blen;
         if (V_keep >= 0) Wan_Opts = (V_keep  ? XRDNET_KEEPALIVE : 0);
         Wan_Opts |= (V_nodnr ? XRDNET_NORLKUP   : 0);
         if (!PortWAN) PortWAN = -1;
        } else {
         if (V_blen >= 0) Net_Blen = V_blen;
         if (V_keep >= 0) Net_Opts = (V_keep  ? XRDNET_KEEPALIVE : 0);
         Net_Opts |= (V_nodnr ? XRDNET_NORLKUP   : 0);
        }

     if (V_ct >= 0) XrdNetAddr::SetCache(V_ct);
     return 0;
}

/******************************************************************************/
/*                                 x n k a p                                  */
/******************************************************************************/

/* Function: xnkap

   Purpose:  To parse the directive: kaparms idle[,itvl[,cnt]]

             idle       Seconds the connection needs to remain idle before TCP
                        should start sending keepalive probes.
             itvl       Seconds between individual keepalive probes.
             icnt       Maximum number of keepalive probes TCP should send
                        before dropping the connection,
*/

int XrdConfig::xnkap(XrdSysError *eDest, char *val)
{
   char *karg, *comma;
   int knum;

// Get the first parameter, idle seconds
//
   karg = val;
   if ((comma = index(val, ','))) {val = comma+1; *comma = 0;}
      else val = 0;
   if (XrdOuca2x::a2tm(*eDest,"kaparms idle", karg, &knum, 0)) return 1;
   XrdNetSocketCFG::ka_Idle = knum;

// Get the second parameter, interval seconds
//
   if (!(karg = val)) return 0;
   if ((comma = index(val, ','))) {val = comma+1; *comma = 0;}
      else val = 0;
   if (XrdOuca2x::a2tm(*eDest,"kaparms interval", karg, &knum, 0)) return 1;
   XrdNetSocketCFG::ka_Itvl = knum;

// Get the third parameter, count
//
   if (!val) return 0;
   if (XrdOuca2x::a2i(*eDest,"kaparms count", val, &knum, 0)) return 1;
   XrdNetSocketCFG::ka_Icnt = knum;

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                 x p o r t                                  */
/******************************************************************************/

/* Function: xport

   Purpose:  To parse the directive: port [wan] <tcpnum>
                                               [if [<hlst>] [named <nlst>]]

             wan        apply this to the wan port
             <tcpnum>   number of the tcp port for incomming requests
             <hlst>     list of applicable host patterns
             <nlst>     list of applicable instance names.

   Output: 0 upon success or !0 upon failure.
*/
int XrdConfig::xport(XrdSysError *eDest, XrdOucStream &Config)
{   int rc, iswan = 0, pnum = 0;
    char *val, cport[32];

    do {if (!(val = Config.GetWord()))
           {eDest->Emsg("Config", "tcp port not specified"); return 1;}
        if (strcmp("wan", val) || iswan) break;
        iswan = 1;
       } while(1);

    strncpy(cport, val, sizeof(cport)-1); cport[sizeof(cport)-1] = '\0';

    if ((val = Config.GetWord()) && !strcmp("if", val))
       if ((rc = XrdOucUtils::doIf(eDest,Config, "port directive", myName,
                              ProtInfo.myInst, myProg)) <= 0)
          {if (!rc) Config.noEcho(); return (rc < 0);}

    if ((pnum = yport(eDest, "tcp", cport)) < 0) return 1;
    if (iswan) PortWAN = pnum;
       else PortTCP = PortUDP = pnum;

    return 0;
}

/******************************************************************************/

int XrdConfig::yport(XrdSysError *eDest, const char *ptype, const char *val)
{
    int pnum;
    if (!strcmp("any", val)) return 0;

    const char *invp = (*ptype == 't' ? "tcp port" : "udp port" );
    const char *invs = (*ptype == 't' ? "Unable to find tcp service" :
                                        "Unable to find udp service" );

    if (isdigit(*val))
       {if (XrdOuca2x::a2i(*eDest,invp,val,&pnum,1,65535)) return 0;}
       else if (!(pnum = XrdNetUtils::ServPort(val, (*ptype != 't'))))
               {eDest->Emsg("Config", invs, val);
                return -1;
               }
    return pnum;
}
  
/******************************************************************************/
/*                                 x p r o t                                  */
/******************************************************************************/

/* Function: xprot

   Purpose:  To parse the directive: protocol [wan] <name>[:<port>] <loc> [<parm>]

             wan    The protocol is WAN optimized
             <name> The name of the protocol (e.g., rootd)
             <port> Port binding for the protocol, if not the default.
             <loc>  The shared library in which it is located.
             <parm> A one line parameter to be passed to the protocol.

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xprot(XrdSysError *eDest, XrdOucStream &Config)
{
    XrdConfigProt *cpp;
    char *val, *parms, *lib, proname[64], buff[1024];
    int vlen, bleft = sizeof(buff), portnum = -1, wanopt = 0;

    do {if (!(val = Config.GetWord()))
           {eDest->Emsg("Config", "protocol name not specified"); return 1;}
        if (wanopt || strcmp("wan", val)) break;
        wanopt = 1;
       } while(1);

    if (strlen(val) > sizeof(proname)-1)
       {eDest->Emsg("Config", "protocol name is too long"); return 1;}
    strcpy(proname, val);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "protocol library not specified"); return 1;}
    if (strcmp("*", val)) lib = strdup(val);
       else lib = 0;

    parms = buff;
    while((val = Config.GetWord()))
         {vlen = strlen(val); bleft -= (vlen+1);
          if (bleft <= 0)
             {eDest->Emsg("Config", "Too many parms for protocol", proname);
              return 1;
             }
          *parms = ' '; parms++; strcpy(parms, val); parms += vlen;
         }
    if (parms != buff) parms = strdup(buff+1);
       else parms = 0;

    if ((val = index(proname, ':')))
       {if ((portnum = yport(&Log, "tcp", val+1)) < 0) return 1;
           else *val = '\0';
       }

    if (wanopt && !PortWAN) PortWAN = 1;

    if ((cpp = Firstcp))
       do {if (!strcmp(proname, cpp->proname))
              {if (cpp->libpath) free(cpp->libpath);
               if (cpp->parms)   free(cpp->parms);
               cpp->libpath = lib;
               cpp->parms   = parms;
               cpp->wanopt  = wanopt;
               return 0;
              }
          } while((cpp = cpp->Next));

    if (lib)
       {cpp = new XrdConfigProt(strdup(proname), lib, parms, portnum, wanopt);
        if (Lastcp) Lastcp->Next = cpp;
           else    Firstcp = cpp;
        Lastcp = cpp;
       }

    return 0;
}

/******************************************************************************/
/*                                  x r e p                                   */
/******************************************************************************/
  
/* Function: xrep

   Purpose:  To parse the directive: report <dest1>[,<dest2>]
                                            [every <sec>] <opts>

             <dest1>   where a UDP based report is to be sent. It may be a
                       <host:port> or a local named UDP pipe (i.e., "/...").

             <dest2>   A secondary destination.

             <sec>     the reporting interval. The default is 10 minutes.

             <opts>    What to report. "all" is the default.

  Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xrep(XrdSysError *eDest, XrdOucStream &Config)
{
   static struct repopts {const char *opname; int opval;} rpopts[] =
       {
        {"all",      XRD_STATS_ALL},
        {"buff",     XRD_STATS_BUFF},
        {"info",     XRD_STATS_INFO},
        {"link",     XRD_STATS_LINK},
        {"poll",     XRD_STATS_POLL},
        {"process",  XRD_STATS_PROC},
        {"protocols",XRD_STATS_PROT},
        {"prot",     XRD_STATS_PROT},
        {"sched",    XRD_STATS_SCHD},
        {"sgen",     XRD_STATS_SGEN},
        {"sync",     XRD_STATS_SYNC},
        {"syncwp",   XRD_STATS_SYNCA}
       };
   int i, neg, numopts = sizeof(rpopts)/sizeof(struct repopts);
   char  *val, *cp;

   if (!(val = Config.GetWord()))
      {eDest->Emsg("Config", "report parameters not specified"); return 1;}

// Cleanup to start anew
//
   if (repDest[0]) {free(repDest[0]); repDest[0] = 0;}
   if (repDest[1]) {free(repDest[1]); repDest[1] = 0;}
   repOpts = 0;
   repInt  = 600;

// Decode the destination
//
   if ((cp = (char *)index(val, ',')))
      {if (!*(cp+1))
          {eDest->Emsg("Config","malformed report destination -",val); return 1;}
          else { repDest[1] = cp+1; *cp = '\0';}
      }
   repDest[0] = val;
   for (i = 0; i < 2; i++)
       {if (!(val = repDest[i])) break;
        if (*val != '/' && (!(cp = index(val, (int)':')) || !atoi(cp+1)))
           {eDest->Emsg("Config","report dest port missing or invalid in",val);
            return 1;
           }
        repDest[i] = strdup(val);
       }

// Make sure dests differ
//
   if (repDest[0] && repDest[1] && !strcmp(repDest[0], repDest[1]))
      {eDest->Emsg("Config", "Warning, report dests are identical.");
       free(repDest[1]); repDest[1] = 0;
      }

// Get optional "every"
//
   if (!(val = Config.GetWord())) {repOpts = XRD_STATS_ALL; return 0;}
   if (!strcmp("every", val))
      {if (!(val = Config.GetWord()))
          {eDest->Emsg("Config", "report every value not specified"); return 1;}
       if (XrdOuca2x::a2tm(*eDest,"report every",val,&repInt,1)) return 1;
       val = Config.GetWord();
      }

// Get reporting options
//
   while(val)
        {if (!strcmp(val, "off")) repOpts = 0;
            else {if ((neg = (val[0] == '-' && val[1]))) val++;
                  for (i = 0; i < numopts; i++)
                      {if (!strcmp(val, rpopts[i].opname))
                          {if (neg) repOpts &= ~rpopts[i].opval;
                              else  repOpts |=  rpopts[i].opval;
                           break;
                          }
                      }
                  if (i >= numopts)
                     eDest->Say("Config warning: ignoring invalid report option '",val,"'.");
                 }
         val = Config.GetWord();
        }

// All done
//
   if (!(repOpts & XRD_STATS_ALL)) repOpts = XRD_STATS_ALL & ~XRD_STATS_INFO;
   return 0;
}

/******************************************************************************/
/*                                x s c h e d                                 */
/******************************************************************************/

/* Function: xsched

   Purpose:  To parse directive: sched [mint <mint>] [maxt <maxt>] [avlt <at>]
                                       [idle <idle>] [stksz <qnt>] [core <cv>]

             <mint>   is the minimum number of threads that we need. Once
                      this number of threads is created, it does not decrease.
             <maxt>   maximum number of threads that may be created. The
                      actual number of threads will vary between <mint> and
                      <maxt>.
             <avlt>   Are the number of threads that must be available for
                      immediate dispatch. These threads are never bound to a
                      connection (i.e., made stickied). Any available threads
                      above <ft> will be allowed to stick to a connection.
             <cv>     asis - leave current value alone.
                      max  - set value to maximum allowed (hard limit).
                      off  - turn off core files.
             <idle>   The time (in time spec) between checks for underused
                      threads. Those found will be terminated. Default is 780.
             <qnt>    The thread stack size in bytes or K, M, or G.

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xsched(XrdSysError *eDest, XrdOucStream &Config)
{
    char *val;
    long long lpp;
    int  i, ppp;
    int  V_mint = -1, V_maxt = -1, V_idle = -1, V_avlt = -1;
    struct schedopts {const char *opname; int minv; int *oploc;
                      const char *opmsg;} scopts[] =
       {
        {"stksz",      0,       0, "sched stksz"},
        {"mint",       1, &V_mint, "sched mint"},
        {"maxt",       1, &V_maxt, "sched maxt"},
        {"avlt",       1, &V_avlt, "sched avlt"},
        {"core",       1,       0, "sched core"},
        {"idle",       0, &V_idle, "sched idle"}
       };
    int numopts = sizeof(scopts)/sizeof(struct schedopts);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "sched option not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, scopts[i].opname))
                  {if (!(val = Config.GetWord()))
                      {eDest->Emsg("Config", "sched", scopts[i].opname,
                                  "value not specified");
                       return 1;
                      }
                        if (*scopts[i].opname == 'i')
                           {if (XrdOuca2x::a2tm(*eDest, scopts[i].opmsg, val,
                                                &ppp, scopts[i].minv)) return 1;
                           }
                   else if (*scopts[i].opname == 'c')
                           {     if (!strcmp("asis", val)) coreV = -1;
                            else if (!strcmp("max",  val)) coreV =  1;
                            else if (!strcmp("off",  val)) coreV =  0;
                            else {eDest->Emsg("Config","invalid sched core value -",val);
                                  return 1;
                                 }
                           }
                   else if (*scopts[i].opname == 's')
                           {if (XrdOuca2x::a2sz(*eDest, scopts[i].opmsg, val,
                                                &lpp, scopts[i].minv)) return 1;
                            XrdSysThread::setStackSize((size_t)lpp);
                            break;
                           }
                   else if (XrdOuca2x::a2i(*eDest, scopts[i].opmsg, val,
                                     &ppp,scopts[i].minv)) return 1;
                   *scopts[i].oploc = ppp;
                   break;
                  }
           if (i >= numopts)
              eDest->Say("Config warning: ignoring invalid sched option '",val,"'.");
           val = Config.GetWord();
          }

// Make sure specified quantities are consistent
//
  if (V_maxt > 0)
     {if (V_mint > 0 && V_mint > V_maxt)
         {eDest->Emsg("Config", "sched mint must be less than maxt");
          return 1;
         }
      if (V_avlt > 0 && V_avlt > V_maxt)
         {eDest->Emsg("Config", "sched avlt must be less than maxt");
          return 1;
         }
     }

// Establish scheduler options
//
   Sched.setParms(V_mint, V_maxt, V_avlt, V_idle);
   return 0;
}

/******************************************************************************/
/*                                  x s i t                                   */
/******************************************************************************/

/* Function: xsit

   Purpose:  To parse directive: sitename <name>

             <name>   is the 1- to 15-character site name to be included in
                      monitoring information. This can also come from the
                      command line -N option. The first such name is used.

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xsit(XrdSysError *eDest, XrdOucStream &Config)
{
    char *val;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "sitename value not specified"); return 1;}

    if (mySitName) eDest->Emsg("Config", "sitename already specified, using '",
                               mySitName, "'.");
       else mySitName = XrdOucSiteName::Set(val, 63);
    return 0;
}

/******************************************************************************/
/*                                  x t m o                                   */
/******************************************************************************/

/* Function: xtmo

   Purpose:  To parse directive: timeout [read <msd>] [hail <msh>]
                                         [idle <msi>] [kill <msk>]

             <msd>    is the maximum number of seconds to wait for pending
                      data to arrive before we reschedule the link
                      (default is 5 seconds).
             <msh>    is the maximum number of seconds to wait for the initial
                      data after a connection  (default is 30 seconds)
             <msi>    is the minimum number of seconds a connection may remain
                      idle before it is closed (default is 5400 = 90 minutes)
             <msk>    is the minimum number of seconds to wait after killing a
                      connection for it to end (default is 3 seconds)

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xtmo(XrdSysError *eDest, XrdOucStream &Config)
{
    char *val;
    int  i, ppp, rc;
    int  V_read = -1, V_idle = -1, V_hail = -1, V_kill = -1;
    struct tmoopts { const char *opname; int istime; int minv;
                            int *oploc;  const char *etxt;}
           tmopts[] =
       {
        {"read",       1, 1, &V_read, "timeout read"},
        {"hail",       1, 1, &V_hail, "timeout hail"},
        {"idle",       1, 0, &V_idle, "timeout idle"},
        {"kill",       1, 0, &V_kill, "timeout kill"}
       };
    int numopts = sizeof(tmopts)/sizeof(struct tmoopts);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "timeout option not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, tmopts[i].opname))
                   {if (!(val = Config.GetWord()))
                       {eDest->Emsg("Config","timeout", tmopts[i].opname,
                                   "value not specified");
                        return 1;
                       }
                    rc = (tmopts[i].istime ?
                          XrdOuca2x::a2tm(*eDest,tmopts[i].etxt,val,&ppp,
                                                 tmopts[i].minv) :
                          XrdOuca2x::a2i (*eDest,tmopts[i].etxt,val,&ppp,
                                                 tmopts[i].minv));
                    if (rc) return 1;
                    *tmopts[i].oploc = ppp;
                    break;
                   }
           if (i >= numopts)
              eDest->Say("Config warning: ignoring invalid timeout option '",val,"'.");
           val = Config.GetWord();
          }

// Set values and return
//
   if (V_read >  0) ProtInfo.readWait = V_read*1000;
   if (V_hail >= 0) ProtInfo.hailWait = V_hail*1000;
   if (V_idle >= 0) ProtInfo.idleWait = V_idle;
   XrdLink::setKWT(V_read, V_kill);
   return 0;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xtrace(XrdSysError *eDest, XrdOucStream &Config)
{
    char *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"off",      TRACE_NONE},
        {"none",     TRACE_NONE},
        {"conn",     TRACE_CONN},
        {"debug",    TRACE_DEBUG},
        {"mem",      TRACE_MEM},
        {"net",      TRACE_NET},
        {"poll",     TRACE_POLL},
        {"protocol", TRACE_PROT},
        {"sched",    TRACE_SCHED}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "trace option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) trval = 0;
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, tropts[i].opname))
                           {if (neg)
                               if (tropts[i].opval) trval &= ~tropts[i].opval;
                                  else trval = TRACE_ALL;
                               else if (tropts[i].opval) trval |= tropts[i].opval;
                                       else trval = TRACE_NONE;
                            break;
                           }
                       }
                   if (i >= numopts)
                      eDest->Say("Config warning: ignoring invalid trace option '",val,"'.");
                  }
          val = Config.GetWord();
         }
    Trace.What = trval;
    return 0;
}
