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

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

       XrdBuffManager    XrdBuffPool;

extern XrdConfig         XrdConfig;

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

     void DoIt() {XrdLog.Say(0, (char *)XrdBANNER);
                  midnite += 86400;
                  XrdSched.Schedule((XrdJob *)this, midnite);
                 }

          XrdLogWorker() : XrdJob("midnight runner")
                         {midnite = XrdOucTimer::Midnight() + 86400;
                          XrdSched.Schedule((XrdJob *)this, midnite);
                         }
         ~XrdLogWorker() {}
private:
time_t midnite;
};

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdConfig::XrdConfig(void)
{
   static sockaddr myIPAddr;
   char *dnp;

// Preset all variables with common defaults
//
   myName   = XrdNetDNS::getHostName();
              XrdNetDNS::getHostAddr(myName, &myIPAddr);
   PortTCP  = 0;
   PortUDP  = 0;
   ConfigFN = 0;
   PidPath  = strdup("/tmp");
   AdminPath= 0;
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

   ProtInfo.Format   = XrdFORMATB;
   ProtInfo.myName   = myName;
   ProtInfo.myAddr   = &myIPAddr;
   ProtInfo.ConnOptn = -4;     // Num of connections to optimize for (1/4*max)
   ProtInfo.ConnLife = 60*60;  // Time   of connections to optimize for.
   ProtInfo.ConnMax  = -1;     // Max       connections (fd limit)
   ProtInfo.readWait = 5*1000; // Wait time for data before we reschedule
   ProtInfo.idleWait = 0;      // Seconds connection may remain idle (0=off)
   ProtInfo.DebugON  = 0;      // 1 if started with -d
   ProtInfo.argc     = 0;
   ProtInfo.argv     = 0;

// Create Domain name
//
   dnp = myName;
   while(*dnp && *dnp != '.') dnp++;
   myDomain = (*dnp == '.' ? dnp : 0);
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
   int retc, dotrim = 1, NoGo = 0, aP = 1;
   char c, *myProg, buff[128], *temp, *dfltProt, *logfn = 0;
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
      while ((c = getopt(argc,argv,"c:dhl:p:P:")) && ((unsigned char)c != 0xff))
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
       case 'p': if (!(PortTCP = yport(&XrdLog, "tcp", optarg))) Usage(myProg,1);
                 break;
       case 'P': dfltProt = optarg; dotrim = 0;
                 break;
       default:  if (index("clpP", (int)(*(argv[optind-1]+1))))
                    {XrdLog.Emsg("Config", (char *)argv[optind-1],
                                 (char *)"parameter not specified.");
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
   if (logfn) {XrdLogger.Bind(logfn, 24*60*60);
               new XrdLogWorker();
              }

// Put out the herald
//
   XrdLog.Say(0, (char *)XrdBANNER);
   sprintf(buff, "xrd@%s", myName);
   XrdLog.Say(0, buff,(char *)" initialization started.");

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
      {XrdLog.Say(0,(char *)"Using configuration file ", ConfigFN);
       ProtInfo.ConfigFN = ConfigFN;
       NoGo = ConfigProc();
      }
   if (!NoGo) NoGo = Setup(dfltProt);
   if (ProtInfo.DebugON) 
      {XrdTrace.What = TRACE_ALL;
       XrdOucThread::setDebug(&XrdLog);
      }
   ProtInfo.Threads = XrdThread;

// All done, close the stream and return the return code.
//
   temp = (NoGo ? (char *)"failed." : (char *)"completed.");
   sprintf(buff, "xrd@%s:%d initialization ", myName, ProtInfo.Port);
   XrdLog.Say(0, buff, temp);
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
   TS_Xeq("pidpath",       xpidf);
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
  
int XrdConfig::ASocket(const char *path, const char *dname, const char *fname,
                        mode_t mode)
{
   char sokpath[108];
   int  plen = strlen(path), dlen = strlen(dname), flen = strlen(fname);
   int NoGo = 0;
   mode_t dmode;
   struct stat buf;

// Make sure we can fit everything in our buffer
//
   if ((plen + dlen + flen + 3) > (int)sizeof(sokpath))
      {XrdLog.Emsg("Config", "admin path", (char *)path, (char *)"too long"); 
       return 1;
      }

// Construct the directory name we will need to create
//
   strcpy(sokpath, path);
   if (sokpath[plen-1] != '/') sokpath[plen++] = '/';
   strcpy(&sokpath[plen], dname);
   plen += dlen;

// Establish directory mode for this socket
//
   dmode = mode;
   if (mode & S_IRWXU) dmode |= S_IXUSR;
   if (mode & S_IRWXG) dmode |= S_IXGRP;

// Create the directory if it is not present
//
   if (stat(sokpath, &buf))
      {if (errno != ENOENT)
          NoGo=XrdLog.Emsg("Config",errno,"process admin path",(char *)path);
          else if (mkdir(sokpath, dmode))
                  NoGo=XrdLog.Emsg("Config",errno,"create admin path",(char *)path);
      } else {
       if ((buf.st_mode & S_IFMT) != S_IFDIR)
          {XrdLog.Emsg("Config", "Admin path", (char *)path,
                       (char *)"exists but is not a directory"); NoGo = 1;}
          else if ((buf.st_mode & S_IAMB) != dmode
               &&  chmod((const char *)sokpath, dmode))
                  {XrdLog.Emsg("Config",errno,"set access mode for",
                                (char *)path); NoGo = 1;}
      }
   if (NoGo) return 1;

// Construct the actual socket name
//
  sokpath[plen++] = '/';
  strcpy(&sokpath[plen], fname);

// Create an admin network
//
   XrdNetADM = new XrdInet(&XrdLog);
   if (myDomain) XrdNetADM->setDomain((const char *)myDomain);

// Bind the netwok to the named socket
//
   if (!XrdNetADM->Bind(sokpath)) return 1;

// Set the mode and return
//
   chmod(sokpath, mode); // This may fail on some platforms
   return 0;
}

/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdConfig::ConfigProc()
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucStream Config(&XrdLog);

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {XrdLog.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = Config.GetFirstWord()))
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
/*                               P i d F i l e                                */
/******************************************************************************/
  
int XrdConfig::PidFile(char *dfltp)
{
    int xfd;
    char buff[32], *xop = 0;
    char newpidFN[1024];

    snprintf(newpidFN, sizeof(newpidFN)-1,"%s/%s:%d.pid",PidPath,dfltp,PortTCP);
    newpidFN[sizeof(newpidFN)-1] = '\0';

    if ((xfd = open(newpidFN, O_WRONLY|O_CREAT|O_TRUNC,0644)) < 0)
       xop = (char *)"open";
       else {snprintf(buff, sizeof(buff), "%d", getpid());
             if (write(xfd, (void *)buff, strlen(buff)) < 0)
                xop = (char *)"write";
             close(xfd);
            }

     if (xop) XrdLog.Emsg("Config", errno, (const char *)xop, newpidFN);
     return xop != 0;
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
   XrdLog.Say(0,(char *)"Optimizing for ", buff);

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
   XrdConfigProt *cp;

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

// Setup admin connection now
//
   if (AdminPath)
      {char fname[32];
       snprintf(fname, sizeof(fname)-1, ".admin.%d", PortTCP);
       fname[sizeof(fname)-1] = '\0';
       if (ASocket((const char *)AdminPath, ".xrd",
                  (const char *)fname, (mode_t)AdminMode)) return 1;
      }

// Setup network connections now
//
   if (!PortTCP && !(PortTCP = XrdNetDNS::getPort(dfltp, "tcp")))
      PortTCP = XrdDEFAULTPORT;
   ProtInfo.NetTCP = XrdNetTCP = new XrdInet(&XrdLog, Police);
   if (XrdNetTCP->Bind(PortTCP, "tcp")) return 1;
   if (Net_Opts | Net_Blen) XrdNetTCP->setDefaults(Net_Opts, Net_Blen);
   if (myDomain) XrdNetTCP->setDomain((const char *)myDomain);
   ProtInfo.Port = PortTCP;

// Allocate the statistics object. This is akward since we only know part
// of the current configuration. The object will figure this out later.
//
   ProtInfo.Stats = new XrdStats(ProtInfo.myName, ProtInfo.Port);

// Load the protocols
//
   while((cp= Firstcp))
        {if(!XrdProtocol_Select::Load((const char *)(cp->libpath),
                                       (const char *)(cp->proname),
                                        cp->parms,    &ProtInfo)) return 1;
         Firstcp = cp->Next;
         delete cp;
        }

// Create the pid file, if need be
//
   PidFile(dfltp);

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
            "[-p <port>] [-P <prot>] [<prot_options>]" <<endl;
     _exit(rc);
}

/******************************************************************************/
/*                                x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path> [group]

             <path>    the path of the FIFO to use for admin requests.

             group     allows group access to the admin path

   Note: A named socket is created <path>/.xrd/.admin.<port>

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
   AdminMode = mode;
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
       if (!strcmp(val, "netgroup")) ishost = 0;
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
                                   (char *)cnopts[i].opname,
                                   (char *)"value not specified");
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
                              (char *)ntopts[i].opname,(char *)ntopts[i].etxt);
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
/*                                 x p i d f                                  */
/******************************************************************************/

/* Function: xpidf

   Purpose:  To parse the directive: pidpath <path>

             <path>    the path where the pid file is to be created.

  Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xpidf(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;

// Get the path
//
   val = Config.GetWord();
   if (!val || !val[0])
      {eDest->Emsg("Config", "pidpath not specified"); return 1;}

// Record the path
//
   if (PidPath) free(PidPath);
   PidPath = strdup(val);
   return 0;
}
  
/******************************************************************************/
/*                                 x p o r t                                  */
/******************************************************************************/

/* Function: xport

   Purpose:  To parse the directive: port <tcpnum> [<udpnum>]

             <tcpnum>   number of the tcp port for incomming requests
             <udpnum>   number of the udp port for incomming requests

   Output: 0 upon success or !0 upon failure.
*/
int XrdConfig::xport(XrdOucError *eDest, XrdOucStream &Config)
{   int pnum = 0;
    char *val;

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "tcp port not specified"); return 1;}
    if (!(pnum = yport(eDest, "tcp", val))) return 1;
    PortTCP = PortUDP = pnum;

    if ((val = Config.GetWord()))
       {if (!(pnum = yport(eDest, "udp", val))) return 1;
        PortUDP = pnum;
       }

    return 0;
}

/******************************************************************************/

int XrdConfig::yport(XrdOucError *eDest, const char *ptype, char *val)
{
    int pnum;
    char *invp = (*ptype == 't' ? (char *)"tcp port" :
                                  (char *)"udp port" );
    char *invs = (*ptype == 't' ? (char *)"Unable to find tcp service" :
                                  (char *)"Unable to find udp service" );

    if (isdigit(*val))
       {if (XrdOuca2x::a2i(*eDest,(const char *)invp,val,&pnum,1,65535)) return 0;}
       else if (!(pnum = XrdNetDNS::getPort(val, "tcp")))
               {eDest->Emsg("Config", (const char *)invs, val);
                return 0;
               }
    return pnum;
}
  
/******************************************************************************/
/*                                 x p r o t                                  */
/******************************************************************************/

/* Function: xprot

   Purpose:  To parse the directive: protocol <name> <loc> [<parms>]

             <name> The name of the protocol (e.g., rootd)
             <loc>  The shared library in which it is located.
             <parm> The parameters to pass to the protocol when calling
                    it's Configure() method.

   Output: 0 upon success or !0 upon failure.
*/

int XrdConfig::xprot(XrdOucError *eDest, XrdOucStream &Config)
{
    XrdConfigProt *cpp;
    char *val, *parms, *lib, proname[17];

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "protocol name not specified"); return 1;}
    if (strlen(val) > sizeof(proname)-1)
       {eDest->Emsg("Config", "protocol name is too long"); return 1;}
    strcpy(proname, (const char *)val);

    if (!(val = Config.GetWord()))
       {eDest->Emsg("Config", "protocol library not specified"); return 1;}
    if (strcmp("*", val)) lib = strdup(val);
       else lib = 0;

    if (!(parms = xprotparms(eDest, Config)))
       {if (lib) free(lib);
        return 1;
       }
    if (*parms == '\0') parms = 0;

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

char *XrdConfig::xprotparms(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val, pbuff[4096], *bp = pbuff;
    int tlen, eoc = 1, braces = 0, bleft = sizeof(pbuff)-2;

    *pbuff = '\0';
    if ((val = Config.GetWord()))
       {if (*val == '{')
           {val++; eoc = 0; braces = 1;
            if (*val == '\0') val = Config.GetWord();
           }
        eDest->Emsg("Config","Warning! Protocol directive brace notation is "
                    "deprecated and will not be supported in the next release.");
        do {while(val)
                 {tlen = strlen(val);
                  if (braces && val[tlen-1] == '}')
                     {eoc = 1; tlen--;
                      val[tlen] = '\0';
                      if (!tlen) break;
                     }
                  if (tlen >= bleft)
                     {eDest->Emsg("Config","excessive protocol parameters");
                      return 0;
                     }
                  *bp++ = ' '; strcpy(bp, val); bp += tlen; bleft -= tlen;
                  val = Config.GetWord();
                 }
            *bp = (eoc ? '\0' : '\n'); bp++; bleft--;
           } while(!eoc && (val = Config.GetFirstWord()));
       }

    if (!eoc)
       {eDest->Emsg("Config","protocol parameters not terminated with '}'");
        return 0;
       }

    return (*pbuff ? strdup(&pbuff[1]) : (char *)"");
}

/******************************************************************************/
/*                                x s c h e d                                 */
/******************************************************************************/

/* Function: xsched

   Purpose:  To parse directive: sched [mint <mint>] [maxt <maxt>] [avlt <at>]
                                       [idle <idle>]

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

   Output: 0 upon success or 1 upon failure.
*/

int XrdConfig::xsched(XrdOucError *eDest, XrdOucStream &Config)
{
    char *val;
    int  i, ppp;
    int  V_mint = -1, V_maxt = -1, V_idle = -1, V_avlt = -1;
    static struct schedopts {const char *opname; int minv; int *oploc;
                             const char *opmsg;} scopts[] =
       {
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
                      {eDest->Emsg("Config", "sched", (char *)scopts[i].opname,
                                  (char *)"value not specified");
                       return 1;
                      }
                   if (*scopts[i].opname == 'i')
                      {if (XrdOuca2x::a2tm(*eDest, scopts[i].opmsg, val, &ppp,
                                          scopts[i].minv)) return 1;
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
                       {eDest->Emsg("Config","timeout",(char *)tmopts[i].opname,
                                   (char *)"value not specified");
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
