/******************************************************************************/
/*                                                                            */
/*                          X r d C o n f i g . c c                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

const char *XrdConfigCVSID = "$Id$";

/*
   The default port number comes from:
   1) The command line option,
   2) The config file,
   3) The /etc/services file for service corresponding to the program name.
*/
  
#include <unistd.h>
#include <ctype.h>
#include <iostream.h>
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdConfig.hh"
#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdProtocol.hh"
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdStats.hh"
#include "Xrd/XrdTrace.hh"
#include "Xrd/XrdInfo.hh"

#include "XrdNet/XrdNetDNS.hh"
#include "XrdNet/XrdNetSecurity.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdOuc/XrdOucUtils.hh"

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

       XrdBuffManager    XrdBuffPool;

extern XrdInet          *XrdNetTCP;
extern XrdInet          *XrdNetADM;

extern XrdScheduler      XrdSched;

extern XrdOucError       XrdLog;

extern XrdOucLogger      XrdLogger;

extern XrdOucThread     *XrdThread;

extern XrdOucTrace       XrdTrace;

       const char       *XrdConfig::TraceID = "Config";

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(eDest, Config);

#define XRD_Prefix     "xrd."
#define XRD_PrefLen    sizeof(XRD_Prefix)-1

#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdConfigProt
{
public:

XrdConfigProt *Next;
char           *proname;
char           *libpath;
char           *parms;

                XrdConfigProt(char *pn, char *ln, char *pp)
                    {Next = 0; proname = pn; libpath = ln; parms = pp;
                    }
               ~XrdConfigProt()
                    {free(proname);
                     if (libpath) free(libpath);
                     if (parms)   free(parms);
                    }
};

class XrdLogWorker : XrdJob
{
public:

     void DoIt() {XrdLog.Say(0, XrdBANNER);
                  XrdLog.Say(0, mememe, "running.");
                  midnite += 86400;
                  XrdSched.Schedule((XrdJob *)this, midnite);
                 }

          XrdLogWorker(char *who) : XrdJob("midnight runner")
                         {midnite = XrdOucTimer::Midnight() + 86400;
                          mememe = strdup(who);
                          XrdSched.Schedule((XrdJob *)this, midnite);
                         }
         ~XrdLogWorker() {}
private:
time_t midnite;
const char *mememe;
};

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdConfig::XrdConfig(void)
{

// Preset all variables with common defaults
//
   PortTCP  = 0;
   PortUDP  = 0;
   ConfigFN = 0;
   myInsName= 0;
   AdminPath= strdup("/tmp");
   AdminMode= 0700;
   Police   = 0;
   Net_Blen = 0;
   Net_Opts = 0;
   setSched = 1;

   Firstcp = Lastcp = 0;

   ProtInfo.eDest   = &XrdLog;          // Stable -> Error Message/Logging Handler
   ProtInfo.NetTCP  = 0;                // Stable -> Network Object
   ProtInfo.BPool   = &XrdBuffPool;     // Stable -> Buffer Pool Manager
   ProtInfo.Sched   = &XrdSched;        // Stable -> System Scheduler
   ProtInfo.ConfigFN= 0;                // We will fill this in later
   ProtInfo.Stats   = 0;                // We will fill this in later
   ProtInfo.Trace   = &XrdTrace;        // Stable -> Trace Information
   ProtInfo.Threads = 0;                // Stable -> The thread manager (later)
   ProtInfo.AdmPath = AdminPath;        // Stable -> The admin path
   ProtInfo.AdmMode = AdminMode;        // Stable -> The admin path mode

   ProtInfo.Format   = XrdFORMATB;
   ProtInfo.ConnOptn = -4;     // Num of connections to optimize for (1/4*max)
   ProtInfo.ConnLife = 60*60;  // Time   of connections to optimize for.
   ProtInfo.ConnMax  = -1;     // Max       connections (fd limit)
   ProtInfo.readWait = 3*1000; // Wait time for data before we reschedule
   ProtInfo.idleWait = 0;      // Seconds connection may remain idle (0=off)
   ProtInfo.DebugON  = 0;      // 1 if started with -d
   ProtInfo.argc     = 0;
   ProtInfo.argv     = 0;
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
   const char *xrdName="XRDNAME=";
   const char *xrdHost="XRDHOST=";

   static sockaddr myIPAddr;
   int retc, dotrim = 1, NoGo = 0, aP = 1, clPort = 0;
   const char *temp;
   char c, *Penv, *myProg, buff[512], *dfltProt, *logfn = 0;
   extern char *optarg;
   extern int optind, opterr;

// Obtain the protocol name we will be using
//
    retc = strlen(argv[0]);
    while(retc--) if (argv[0][retc] == '/') break;
    myProg = dfltProt = &argv[0][retc+1];

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,"c:dhl:n:p:P:")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'c': if (ConfigFN) free(ConfigFN);
                 ConfigFN = strdup(optarg);
                 break;
       case 'd': XrdTrace.What |= TRACE_ALL;
                 ProtInfo.DebugON = 1;
                 putenv((char *)"XRDDEBUG=1");
                 break;
       case 'h': Usage(myProg, 0);
       case 'l': if (logfn) free(logfn);
                 logfn = strdup(optarg);
                 break;
       case 'n': myInsName = optarg;
                 break;
       case 'p': if (!(clPort = yport(&XrdLog, "tcp", optarg))) Usage(myProg,1);
                 break;
       case 'P': dfltProt = optarg; dotrim = 0;
                 break;
       default:  if (index("clpP", (int)(*(argv[optind-1]+1))))
                    {XrdLog.Emsg("Config", argv[optind-1],
                                 "parameter not specified.");
                     Usage(myProg, 1);
                    }
                 argv[aP++] = argv[optind-1];
                 if (argv[optind] && *argv[optind] != '-') 
                    argv[aP++] = argv[optind++];
       }
     }

// Pass over any parameters
//
   if (aP != optind)
      {for ( ; optind < argc; optind++) argv[aP++] = argv[optind];
       argv[aP] = 0;
       ProtInfo.argc = aP;
      } else ProtInfo.argc = argc;
   ProtInfo.argv = argv;

// Bind the log file if we have one
//
   if (logfn)
      {if (!(logfn = XrdOucUtils::subLogfn(XrdLog, myInsName, logfn))) _exit(16);
       XrdLogger.Bind(logfn, 24*60*60);
       free(logfn);
      }

// Get the full host name. In theory, we should always get some kind of name.
//
   if (!(myName = XrdNetDNS::getHostName()))
      {XrdLog.Emsg("Config", "Unable to determine host name; "
                             "execution terminated.");
       _exit(16);
      }

// Verify that we have a real name. We've had problems with people setting up
// bad /etc/hosts files that can cause connection failures if "allow" is used.
// Otherwise, determine our domain name.
//
   if (isdigit(*myName) && (isdigit(*(myName+1)) || *(myName+1) == '.'))
      {XrdLog.Emsg("Config", myName, "is not the true host name of this machine.");
       XrdLog.Emsg("Config", "Verify that the '/etc/hosts' file is correct and "
                             "this machine is registered in DNS.");
       XrdLog.Emsg("Config", "Execution continues but connection failures may occur.");
       myDomain = 0;
      } else if (!(myDomain = index(myName, '.')))
                XrdLog.Emsg("Config", "Warning! This hostname,", myName,
                            ", is registered without a domain qualification.");

// Get our IP address
//
   XrdNetDNS::getHostAddr(myName, &myIPAddr);
   ProtInfo.myName = myName;
   ProtInfo.myAddr = &myIPAddr;
   ProtInfo.myInst = (myInsName && *myInsName ? myInsName : "anon");

// Set the Environmental variable to hold the instance name
// XRDINSTANCE=<instance name>@<host name>
//
   sprintf(buff,"%s%s@%s", xrdInst, ProtInfo.myInst, myName);
   myInstance = strdup(buff);
   putenv(myInstance);
   myInstance += strlen(xrdInst);
   sprintf(buff, "%s%s", xrdHost, myName);
   Penv = strdup(buff);
   putenv(Penv);
   if (myInsName)
      {sprintf(buff, "%s%s", xrdName, myInsName);
       Penv = strdup(buff);
       putenv(Penv);
      }

// Put out the herald
//
   XrdLog.Say(0, XrdBANNER);
   XrdLog.Say(0, myInstance, " initialization started.");

// Setup the initial required protocol
//
   if (dotrim && *dfltProt != '.' )
      {char *p = dfltProt;
       while (*p && *p != '.') p++;
       if (*p == '.') *p = '\0';
      }
   Firstcp = Lastcp = new XrdConfigProt(strdup(dfltProt), 0, 0);

// Process the configuration file, if one is present
//
   if (ConfigFN && *ConfigFN)
      {XrdLog.Say(0, "Using configuration file ", ConfigFN);
       ProtInfo.ConfigFN = ConfigFN;
       NoGo = ConfigProc();
      }
   if (clPort) PortTCP = clPort;
   if (!NoGo) NoGo = Setup(dfltProt);
   if (ProtInfo.DebugON) 
      {XrdTrace.What = TRACE_ALL;
       XrdOucThread::setDebug(&XrdLog);
      }
   ProtInfo.Threads = XrdThread;

// If we hae a net name change the working directory
//
   if (myInsName) XrdOucUtils::makeHome(XrdLog, myInsName);

// All done, close the stream and return the return code.
//
   temp = (NoGo ? " initialization failed." : " initialization completed.");
   sprintf(buff, "%s:%d", myInstance, PortTCP);
   XrdLog.Say(0, buff, temp);
   if (logfn) new XrdLogWorker(buff);
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdConfig::ConfigXeq(char *var, XrdOucStream &Config, XrdOucError *eDest)
{
   int dynamic;

   // Determine whether is is dynamic or not
   //
   if (eDest) dynamic = 1;
      else   {dynamic = 0; eDest = &XrdLog;}

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
   TS_Xeq("connections",   xcon);
   TS_Xeq("port",          xport);
   TS_Xeq("protocol",      xprot);
   TS_Xeq("timeout",       xtmo);
   }

   // No match found, complain.
   //
   eDest->Emsg("Config", "Warning, unknown xrd directive", var);
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
   char xpath[1024], sokpath[108];
   int  plen = strlen(path), flen = strlen(fname);
   int rc;

// Make sure we can fit everything in our buffer
//
   if ((plen + flen + 3) > (int)sizeof(sokpath))
      {XrdLog.Emsg("Config", "admin path", path, "too long");
       return 1;
      }

// Create the directory path
//
   strcpy(xpath, path);
   if ((rc = XrdOucUtils::makePath(xpath, mode)))
       {XrdLog.Emsg("Config", rc, "create admin path", xpath);
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
   XrdNetADM = new XrdInet(&XrdLog);
   if (myDomain) XrdNetADM->setDomain(myDomain);

// Bind the netwok to the named socket
//
   if (!XrdNetADM->Bind(sokpath)) return 1;

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
  XrdOucStream Config(&XrdLog, myInstance);

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {XrdLog.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, XRD_Prefix, XRD_PrefLen))
            {var += XRD_PrefLen;
             NoGo |= ConfigXeq(var, Config);
            }
        }

// Now check if any errors occured during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = XrdLog.Emsg("Config", retc, "read config file", ConfigFN);
   Config.Close();

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                                s e t F D L                                 */
/******************************************************************************/
  
int XrdConfig::setFDL()
{
   struct rlimit rlim;
   char buff[100];

// Get the resource limit
//
   if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
      return XrdLog.Emsg("Config", errno, "get FD limit");

// Set the limit to the maximum allowed
//
   if (ProtInfo.ConnMax > 0 && ProtInfo.ConnMax < (int)rlim.rlim_max)
           rlim.rlim_cur = ProtInfo.ConnMax;
      else rlim.rlim_cur = rlim.rlim_max;
   if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
      return XrdLog.Emsg("Config", errno,"set FD limit");

// Obtain the actual limit now
//
   if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
      return XrdLog.Emsg("Config", errno, "get FD limit");

// Establish operating limit
//
   if (ProtInfo.ConnMax < 0) ProtInfo.ConnMax = rlim.rlim_cur;
      else if (ProtInfo.ConnMax > (int)rlim.rlim_cur)
              {ProtInfo.ConnMax = rlim.rlim_cur;
               sprintf(buff,"%d > system FD limit of %d",
                       ProtInfo.ConnMax, ProtInfo.ConnMax);
               XrdLog.Emsg("Config", "Warning: connection mfd", buff);
              }

// Establish optimization point
//
   if (ProtInfo.ConnOptn < 0)
      {ProtInfo.ConnOptn = -ProtInfo.ConnOptn;
       if (!(ProtInfo.ConnOptn = ProtInfo.ConnMax/ProtInfo.ConnOptn))
          ProtInfo.ConnOptn = (2 > ProtInfo.ConnMax ? 1 : 2);
      }
      else if (ProtInfo.ConnOptn > ProtInfo.ConnMax)
              {sprintf(buff,"%d > system FD limit of %d",
                            ProtInfo.ConnOptn, ProtInfo.ConnMax);
               XrdLog.Emsg("Config", "Warning: connection avg", buff);
               ProtInfo.ConnOptn = ProtInfo.ConnMax;
              }

// Indicate what we optimized for
//
   sprintf(buff, "%d connections; maximum is %d",
                 ProtInfo.ConnOptn, ProtInfo.ConnMax);
   XrdLog.Say(0, "Optimizing for ", buff);

// Establish scheduler limits now if they have not been already set
//
   if (setSched)
      {int V_mint, V_maxt, V_avlt, ncb2 = 0, numcon = ProtInfo.ConnOptn;
       while((numcon = numcon >> 1)) ncb2++;
       if (ncb2 == 0) ncb2 = 1;
       if ((V_maxt = ProtInfo.ConnOptn / ncb2) > 1024) V_maxt = 1024;
       if ((V_mint = V_maxt / ncb2) <= 0)              V_mint = 1;
       if ((V_avlt = V_maxt /  5) <= 0)                V_avlt = 1;
       XrdSched.setParms(V_mint, V_maxt, V_avlt, -1);
      }
   return 0;
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
int XrdConfig::Setup(char *dfltp)
{
   static char portbuff[32];
   XrdConfigProt *cp;
   char *p;

// Establish the FD limit
//
   if (setFDL()) return 1;

// Initialize the buffer manager
//
   XrdBuffPool.Init();

// Start the required number of workers
//
   XrdSched.Start(ProtInfo.ConnOptn/256);

// Setup the link and socket polling infrastructure
//
   if (!XrdLink::Setup(ProtInfo.ConnMax, ProtInfo.idleWait)
   ||  !XrdPoll::Setup(ProtInfo.ConnMax)) return 1;

// Determine correct port number
//
   if (PortTCP < 0) PortTCP = 0;
      else if (!PortTCP && !(PortTCP = XrdNetDNS::getPort(dfltp, "tcp")))
               PortTCP = XrdDEFAULTPORT;

// Setup network connections now
//
   ProtInfo.NetTCP = XrdNetTCP = new XrdInet(&XrdLog, Police);
   if (XrdNetTCP->Bind(PortTCP, "tcp")) return 1;
   if (Net_Opts | Net_Blen) XrdNetTCP->setDefaults(Net_Opts, Net_Blen);
   if (myDomain) XrdNetTCP->setDomain(myDomain);

// Do final port resolution
//
   if (!PortTCP) PortTCP = PortUDP = XrdNetTCP->Port();
   ProtInfo.Port = PortTCP;
   sprintf(portbuff, "XRDPORT=%d", PortTCP);
   putenv(portbuff);

// Modify the AdminPath to account for any instance name. Note that there is
// a negligible memory leak under ceratin path combinations. Not enough to
// warrant a lot of logic to get around.
//
   if (myInsName) ProtInfo.AdmPath = XrdOucUtils::genPath(AdminPath,myInsName);
   p = XrdOucUtils::genPath(AdminPath, myInsName, ".xrd");
   AdminPath = p;

// Setup admin connection now
//
   if (ASocket(AdminPath, "admin", (mode_t)AdminMode)) return 1;

// Allocate the statistics object. This is akward since we only know part
// of the current configuration. The object will figure this out later.
//
   ProtInfo.Stats = new XrdStats(ProtInfo.myName, ProtInfo.Port);

// Load the protocols
//
   while((cp= Firstcp))
        {if (!XrdProtocol_Select::Load((cp->libpath), (cp->proname), 
                                       (cp->parms), &ProtInfo))
            return 1;
         Firstcp = cp->Next;
         delete cp;
        }

// All done
//
   return 0;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void XrdConfig::Usage(char *myProg, int rc)
{

     cerr <<"\nUsage: " <<myProg <<" [-c <cfn>] [-d] [-l <fn>] "
            "[-n name] [-p <port>] [-P <prot>] [<prot_options>]" <<endl;
     _exit(rc);
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

int XrdConfig::xapath(XrdOucError *eDest, XrdOucStream &Config)
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

// Get the optional access rights
//
   if ((val = Config.GetWord()) && val[0])
      if (!strcmp("group", val)) mode |= S_IRWXG;
         else {eDest->Emsg("Config", "invalid admin path modifier -", val);
               return 1;
              }

// Record the path
//
   if (AdminPath) free(AdminPath);
   AdminPath = strdup(pval);
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

int XrdConfig::xallow(XrdOucError *eDest, XrdOucStream &Config)
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
int XrdConfig::xbuf(XrdOucError *eDest, XrdOucStream &Config)
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

    XrdBuffPool.Set((int)blim, bint);
    return 0;
}
  
/******************************************************************************/
/*                                  x c o n                                   */
/******************************************************************************/

/* Function: xcon

   Purpose:  To parse the directive: connections [avg <avg>] [dur <dur>]
                                                 [mfd <mfd>]

             <avg>      number of connections expected, on average, as an
                        absolute number or /nn as a fraction of max possible.
             <dur>      average second duration for a connection.
             <mfd>      maximum number of FDs to allow

   Output: 0 upon success or !0 upon failure.
*/
int XrdConfig::xcon(XrdOucError *eDest, XrdOucStream &Config)
{   int i, n, rc, sgn, aval = -1, dval=60*60, fval = -1;
    char *val;
    static struct conopts {const char *opname; int istime; 
                           int *oploc; const char *etxt;} cnopts[] =
       {
        {"avg",-1, &aval, "conections avg"},
        {"dur", 1, &dval, "conections dur"},
        {"mfd", 0, &fval, "conections mfd"}
       };
    int numopts = sizeof(cnopts)/sizeof(struct conopts);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "connections option not specified"); return 1;}

    while (val)
          {for (i = 0; i < numopts; i++)
               if (!strcmp(val, cnopts[i].opname))
                  {if (!(val = Config.GetWord()))
                      {eDest->Emsg("Config", "connections", 
                                   cnopts[i].opname, "value not specified");
                       return 1;
                      }
                   sgn = 1;
                   if (cnopts[i].istime < 0)
                      if (val[0] == '1' && val[1] == '/') {val+=2; sgn = -1;}
                         else if (*val == '/') {val++; sgn = -1;}
                   rc = (cnopts[i].istime > 0 ?
                         XrdOuca2x::a2tm(*eDest,cnopts[i].etxt,val,&n,1):
                         XrdOuca2x::a2i (*eDest,cnopts[i].etxt,val,&n,1));
                   if (rc) return 1;
                   *cnopts[i].oploc = n * sgn;
                   break;
                  }
           if (i >= numopts)
              eDest->Emsg("Config", "Warning, invalid connections option", val);
           val = Config.GetWord();
          }

// Make sure values are consistent
//
   if (ProtInfo.ConnOptn > 0 && ProtInfo.ConnMax > 0
   &&  ProtInfo.ConnOptn > ProtInfo.ConnMax) 
      {eDest->Emsg("Config", "connection avg may not be greater than mfd");
       return 1;
      }

// Set values and return
//
    ProtInfo.ConnOptn = aval;
    ProtInfo.ConnLife = dval;
    ProtInfo.ConnMax  = fval;

    return 0;
}

/******************************************************************************/
/*                                  x n e t                                   */
/******************************************************************************/

/* Function: xnet

   Purpose:  To parse directive: network [keepalive] [buffsz <blen>]

             keepalive sets the socket keepalive option.
             <blen>    is the socket's send/rcv buffer size.

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xnet(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int  i, V_keep = 0;
    long long llp;
    static struct netopts {const char *opname; int hasarg;
                           int  *oploc;  const char *etxt;}
           ntopts[] =
       {
        {"keepalive",  0, &V_keep,   "option"},
        {"buffsz",     1, &Net_Blen, "network buffsz"}
       };
    int numopts = sizeof(ntopts)/sizeof(struct netopts);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "net option not specified"); return 1;}

    while (val)
    {for (i = 0; i < numopts; i++)
         if (!strcmp(val, ntopts[i].opname))
            {if (!ntopts[i].hasarg) llp = 1;
                else {if (!(val = Config.GetWord()))
                         {eDest->Emsg("Config", "network",
                              ntopts[i].opname, ntopts[i].etxt);
                          return 1;
                         }
                      if (XrdOuca2x::a2sz(*eDest,ntopts[i].etxt,val,&llp,0))
                         return 1;
                      *ntopts[i].oploc = (int)llp;
                     }
              break;
             }
      if (i >= numopts)
         eDest->Emsg("Config", "Warning, invalid net option", val);
      val = Config.GetWord();
     }

     Net_Opts = (V_keep ? XRDNET_KEEPALIVE : 0);
     return 0;
}
  
/******************************************************************************/
/*                                 x p o r t                                  */
/******************************************************************************/

/* Function: xport

   Purpose:  To parse the directive: port <tcpnum> [if [<hlst>] [named <nlst>]]

             <tcpnum>   number of the tcp port for incomming requests
             <hlst>     list of applicable host patterns
             <nlst>     list of applicable instance names.

   Output: 0 upon success or !0 upon failure.
*/
int XrdConfig::xport(XrdOucError *eDest, XrdOucStream &Config)
{   int rc, pnum = 0;
    char *val, cport[32];

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "tcp port not specified"); return 1;}
    strncpy(cport, val, sizeof(cport)-1); cport[sizeof(cport)-1] = '\0';

    if ((val = Config.GetWord()) && !strcmp("if", val))
       if ((rc = XrdOucUtils::doIf(eDest,Config, "role directive",
                              myName, ProtInfo.myInst)) <= 0) return (rc < 0);

    if (!(pnum = yport(eDest, "tcp", cport))) return 1;
    PortTCP = PortUDP = pnum;

    return 0;
}

/******************************************************************************/

int XrdConfig::yport(XrdOucError *eDest, const char *ptype, const char *val)
{
    int pnum;
    if (!strcmp("any", val)) return -1;

    const char *invp = (*ptype == 't' ? "tcp port" : "udp port" );
    const char *invs = (*ptype == 't' ? "Unable to find tcp service" :
                                        "Unable to find udp service" );

    if (isdigit(*val))
       {if (XrdOuca2x::a2i(*eDest,invp,val,&pnum,1,65535)) return 0;}
       else if (!(pnum = XrdNetDNS::getPort(val, "tcp")))
               {eDest->Emsg("Config", invs, val);
                return 0;
               }
    return pnum;
}
  
/******************************************************************************/
/*                                 x p r o t                                  */
/******************************************************************************/

/* Function: xprot

   Purpose:  To parse the directive: protocol <name> <loc> [<parm>]

             <name> The name of the protocol (e.g., rootd)
             <loc>  The shared library in which it is located.
             <parm> A one line parameter to be passed to the protocol.

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xprot(XrdOucError *eDest, XrdOucStream &Config)
{
    XrdConfigProt *cpp;
    char *val, *parms, *lib, proname[17], buff[1024];
    int vlen, bleft = sizeof(buff);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "protocol name not specified"); return 1;}
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

    if ((cpp = Firstcp))
       do {if (!strcmp(proname, cpp->proname))
              {if (cpp->libpath) free(cpp->libpath);
               if (cpp->parms)   free(cpp->parms);
               cpp->libpath = lib;
               cpp->parms   = parms;
               return 0;
              }
          } while((cpp = cpp->Next));

    cpp = new XrdConfigProt(strdup(proname), lib, parms);
    if (Lastcp) Lastcp->Next = cpp;
       else    Firstcp = cpp;
    Lastcp = cpp;

    return 0;
}

/******************************************************************************/
/*                                x s c h e d                                 */
/******************************************************************************/

/* Function: xsched

   Purpose:  To parse directive: sched [mint <mint>] [maxt <maxt>] [avlt <at>]
                                       [idle <idle>] [stksz <qnt>]

             <mint>   is the minimum number of threads that we need. Once
                      this number of threads is created, it does not decrease.
             <maxt>   maximum number of threads that may be created. The
                      actual number of threads will vary between <mint> and
                      <maxt>.
             <avlt>   Are the number of threads that must be available for
                      immediate dispatch. These threads are never bound to a
                      connection (i.e., made stickied). Any available threads
                      above <ft> will be allowed to stick to a connection.
             <idle>   The time (in time spec) between checks for underused
                      threads. Those found will be terminated. Default is 780.
             <qnt>    The thread stack size in bytes or K, M, or G.

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xsched(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    long long lpp;
    int  i, ppp;
    int  V_mint = -1, V_maxt = -1, V_idle = -1, V_avlt = -1;
    static struct schedopts {const char *opname; int minv; int *oploc;
                             const char *opmsg;} scopts[] =
       {
        {"stksz",      0,       0, "sched stksz"},
        {"mint",       1, &V_mint, "sched mint"},
        {"maxt",       1, &V_maxt, "sched maxt"},
        {"avlt",       1, &V_avlt, "sched avlt"},
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
                   else if (*scopts[i].opname == 's')
                           {if (XrdOuca2x::a2sz(*eDest, scopts[i].opmsg, val,
                                                &lpp, scopts[i].minv)) return 1;
                            XrdOucThread::setStackSize((size_t)lpp);
                            break;
                           }
                   else if (XrdOuca2x::a2i(*eDest, scopts[i].opmsg, val,
                                     &ppp,scopts[i].minv)) return 1;
                   *scopts[i].oploc = ppp;
                   break;
                  }
           if (i >= numopts)
              eDest->Emsg("Config", "Warning, invalid sched option", val);
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

// Calculate consistent values for any values that are missing
//
        if (V_maxt > 0)
           {if (V_mint < 0) V_mint = (V_maxt/10 ? V_maxt/10 : 1);
            if (V_avlt < 0) V_avlt = (V_maxt/5  ? V_maxt/5  : 1);
           }
   else if (V_mint > 0)
           {if (V_maxt < 0) V_maxt =  V_mint*5;
            if (V_avlt < 0) V_avlt = (V_maxt/5 ? V_maxt/5 : 1);
           }
   else if (V_avlt > 0)
           {if (V_maxt < 0) V_maxt =  V_avlt*5;
            if (V_mint < 0) V_mint = (V_maxt/10 ? V_maxt/10 : 1);
           }

// Establish scheduler options
//
   if (V_mint > 0 || V_maxt > 0 || V_avlt > 0) setSched = 0;
   XrdSched.setParms(V_mint, V_maxt, V_avlt, V_idle);
   return 0;
}

/******************************************************************************/
/*                                  x t m o                                   */
/******************************************************************************/

/* Function: xtmo

   Purpose:  To parse directive: timeout [read <msd>] [idle <msi>]

             <msd>    is the maximum number of seconds to wait for pending
                      data to arrive before we reschedule the link
                      (default is 5 seconds).
             <msi>    is the minimum number of seconds a connection may remain
                      idle before it is closed (default is 5400 = 90 minutes)

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xtmo(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int  i, ppp, rc;
    int  V_read = -1, V_idle = -1;
    static struct tmoopts { const char *opname; int istime; int minv;
                            int  *oploc;  const char *etxt;}
           tmopts[] =
       {
        {"read",       1, 1, &V_read, "timeout read"},
        {"idle",       1, 0, &V_idle, "timeout idle"}
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
              eDest->Emsg("Config", "Warning, invalid timeout option", val);
           val = Config.GetWord();
          }

// Set values and return
//
   if (V_read >  0) ProtInfo.readWait = V_read*1000;
   if (V_idle >= 0) ProtInfo.idleWait = V_idle;
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

int XrdConfig::xtrace(XrdOucError *eDest, XrdOucStream &Config)
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
       {eDest->Emsg("config", "trace option not specified"); return 1;}
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
                      eDest->Emsg("config", "invalid trace option", val);
                  }
          val = Config.GetWord();
         }
    XrdTrace.What = trval;
    return 0;
}
