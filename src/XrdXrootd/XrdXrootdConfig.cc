/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d C o n f i g . c c                     */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//       $Id$

const char *XrdXrootdConfigCVSID = "$Id$";
 
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <iostream.h>
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdVersion.hh"

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucReqID.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucTrace.hh"

#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdFileLock1.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdPrepare.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"

#include "Xrd/XrdBuffer.hh"

/******************************************************************************/
/*         P r o t o c o l   C o m m a n d   L i n e   O p t i o n s          */
/******************************************************************************/
  
/* This is the XRootd server. The syntax is:

   xrootd [options]

   options: [<xopt>] [-m] [-r] [-s] [-t] [-y] [path]

Where:
   xopt   are xrd specified options that are screened out.

   -m     More than one xrootd will run on this host.

   -r     This is a redirecting server.

   -s     This server should port balance.

   -t     This server is a redirection target.

   -y     This server is a proxy server.

    path  Export path. Any number of paths may be specified.
          By default, only '/tmp' is exported.

*/
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern          XrdOucTrace       *XrdXrootdTrace;

                XrdXrootdPrepare  *XrdXrootdPrepQ;

                XrdOucReqID       *XrdXrootdReqID;

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdXrootdProtocol::Configure(char *parms, XrdProtocol_Config *pi)
{
/*
  Function: Establish configuration at load time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   extern XrdSecProtocol   *XrdXrootdloadSecurity(XrdOucError *, char *, char *);
   extern XrdSfsFileSystem *XrdXrootdloadFileSystem(XrdOucError *, char *, 
                                                    const char *);
   extern XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *, XrdOucLogger *);
   extern int optind, opterr;

   XrdXrootdXPath *xp;
   char *fsver, c, buff[1024], Multxrd = 0;
   int NoGo;

// Copy out the special info we want to use at top level
//
   eDest.logger(pi->eDest->logger());
   XrdXrootdTrace = new XrdOucTrace(&eDest);
   SI           = new XrdXrootdStats(pi->Stats);
   Sched        = pi->Sched;
   BPool        = pi->BPool;
   readWait     = pi->readWait;
   Port         = pi->Port;

// Process any command line options
//
   opterr = 0; optind = 1;
   if (pi->argc > 1 && '-' == *(pi->argv[1]))
      while ((c=getopt(pi->argc,pi->argv,"mrst")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'm': Multxrd = 1;
                 break;
       case 'r': isRedir = 1;
                 putenv((char *)"XRDREDIRECT=R");
                 break;
       case 's': isRedir = 0;
                 putenv((char *)"XRDREDIRECT=L");
                 break;
       case 't': putenv((char *)"XRDRETARGET=1");
                 break;
       case 'y': putenv((char *)"XRDREDPROXY=1");
                 break;
       default:  eDest.Say(0, (char *)"Warning, ignoring invalid option ",
                           pi->argv[optind-1]);
       }
     }

// Pick up exported paths
//
   for ( ; optind < pi->argc; optind++) xexpdo(pi->argv[optind]);

// Initialize remaining async values
//
   as_maxbfsz   = pi->BPool->MaxSize();
   as_aiosize   = as_maxbfsz*2;

// At the moment we don't support multimode
//
   if (Multxrd)
      {eDest.Emsg("Config", "Multiple servers per host unsupported.");
       return 0;
      }

// Now process and configuration parameters
//
   if (parms) NoGo = ConfigIt(parms);
      else if (pi->ConfigFN) NoGo = ConfigFn(pi->ConfigFN);
              else NoGo = 0;
   if (NoGo) return 0;
   if (pi->DebugON) XrdXrootdTrace->What = TRACE_ALL;

// Initialize the security system if this is wanted
//
   if (!SecLib) eDest.Say(0, (char *)"XRootd seclib not specified;"
                          " strong authentication disabled");
      else {TRACE(DEBUG, "Loading security library " <<SecLib);
            if (!(CIA = XrdXrootdloadSecurity(&eDest, SecLib, pi->ConfigFN)))
               {eDest.Emsg("Config", "Unable to load security system.");
                return 0;
               }
           }

// Get the filesystem to be used
//
   if (FSLib)
      {TRACE(DEBUG, "Loading filesystem library " <<FSLib);
       osFS = XrdXrootdloadFileSystem(&eDest, FSLib, (const char *)pi->ConfigFN);
      } else {
       eDest.Say(0, (char *)"XRootd fslib not specified; using native file system");
       osFS = XrdSfsGetFileSystem((XrdSfsFileSystem *)0, eDest.logger());
      }
   if (!osFS)
      {eDest.Emsg("Config", "Unable to load file system.");
       return 0;
      }

// Check if the file system version matches our version
//
   if (chkfsV)
      {fsver = (char *)osFS->getVersion();
       if (strcmp(XrdVERSION, fsver))
          eDest.Emsg("Config", "Warning! xrootd build version " XrdVERSION
                               "differs from file system version ", fsver);
      }

// Create the file lock manager
//
   Locker = (XrdXrootdFileLock *)new XrdXrootdFileLock1();
   XrdXrootdFile::Init(Locker);

// Schedule protocol object cleanup
//
   ProtStack.Set(pi->Sched, XrdXrootdTrace, TRACE_MEM);
   ProtStack.Set(pi->ConnOptn, pi->ConnLife);

// Initialize the request ID generation object
//
   XrdXrootdReqID = new XrdOucReqID((int)Port);

// Initialize for prepare processing
//
   XrdXrootdPrepQ = new XrdXrootdPrepare(&eDest, pi->Sched);
   sprintf(buff, "udp://%s:%d/&L=%%d&U=%%s", pi->myName, pi->Port);
   Notify = strdup(buff);

// Check if we are exporting anything
//
   if (!(xp = XPList.First()))
      {XPList.Insert((char *)"/tmp");
       eDest.Say(0, (char *)"Warning, only '/tmp' will be exported.");
      } else while(xp)
                  {eDest.Say(0, (char *)"Exporting ", xp->Path());
                   xp = xp->Next();
                  }

// Check if monitoring should be enabled
//
   if (monDest && !isRedir)
      if (!XrdXrootdMonitor::Init(Sched,&eDest,monDest,monMBval,monWWval))
         return 0;
         else XrdXrootdMonitor::setMode(monMode);

// Indicate we configured successfully
//
   eDest.Say(0, (char *)"XRootd protocol version " XROOTD_VERSION 
                        " build " XrdVERSION " successfully loaded.");

// Return success
//
   return 1;
}

/******************************************************************************/
/*                              C o n f i g F n                               */
/******************************************************************************/

int XrdXrootdProtocol::ConfigFn(char *fn)
{
    struct stat buf;
    int fd, rsz, NoGo;
    char *fbuff;

    if ((fd = open(fn, O_RDONLY)) < 0)
       {eDest.Emsg("Config", errno, "open", fn); return 1;}
    if (fstat(fd, &buf) < 0)
       {eDest.Emsg("Config", errno, "get size of", fn); return 1;}
    if (!(fbuff = (char *)malloc(buf.st_size+1)))
       {eDest.Emsg("Config", errno, "get buffer for", fn); return 1;}
    if ((rsz = read(fd, (void *)fbuff, buf.st_size)) < 0)
       {eDest.Emsg("Config", errno, "read", fn); return 1;}
    close(fd); fbuff[rsz] = '\0';
    NoGo = (rsz ? ConfigIt(fbuff) : 0);
    free(fbuff);
    return NoGo;
}

/******************************************************************************/
/*                              C o n f i g I t                               */
/******************************************************************************/
  
#define TS_Xeq(x,m) (!strcmp(x,var)) NoGo |=  m(Config)

int XrdXrootdProtocol::ConfigIt(char *parms)
{
   XrdOucTokenizer Config(parms);
   char *var;
   int NoGo = 0, ignore;

   // Process items
   //
   while(Config.GetLine())
        {if ((var = Config.GetToken()))
            {if (!(ignore = strncmp("xrootd.", var, 7)) && var[7]) var += 7;
                  if TS_Xeq("async",         xasync);
             else if TS_Xeq("chksum",        xcksum);
             else if TS_Xeq("export",        xexp);
             else if TS_Xeq("fslib",         xfsl);
             else if TS_Xeq("monitor",       xmon);
             else if TS_Xeq("prep",          xprep);
             else if TS_Xeq("seclib",        xsecl);
             else if TS_Xeq("trace",         xtrace);
             else if (!ignore) eDest.Say(0,
                     (char *)"Warning, unknown xrootd directive ",var);
            }
        }
   return NoGo;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                                x a s y n c                                 */
/******************************************************************************/

/* Function: xasync

   Purpose:  To parse directive: async [maxpl <maxpl>] [maxps <maxps>]
                                       [iosz <iosz>]

             <maxpl>  maximum number of async ops per link. Default 8.
             <maxps>  maximum number of async ops per server. Default 64.
             <iosz>   the minimum number of bytes that must be read or written
                      to allow async processing to occur (default is 2*maxbsz,
                      typically 2M).

   Output: 0 upon success or 1 upon failure.
*/

int XrdXrootdProtocol::xasync(XrdOucTokenizer &Config)
{
    char *val;
    int  i, ppp;
    int  V_mapl = -1, V_maps = -1, V_iosz = -1;
    long long llp;
    static struct asyncopts {const char *opname; int minv; int *oploc;
                             const char *opmsg;} asopts[] =
       {
        {"maxpl",      0, &V_mapl, "async maxpl"},
        {"maxps",      0, &V_maps, "async maxps"},
        {"iosz",    4096, &V_iosz, "async iosz"}};
    int numopts = sizeof(asopts)/sizeof(struct asyncopts);

    if (!(val = Config.GetToken()))
       {eDest.Emsg("Config", "async option not specified"); return 1;}

    while (val)
         {for (i = 0; i < numopts; i++)
              if (!strcmp(val, asopts[i].opname))
                 {if (!(val = Config.GetToken()))
                     {eDest.Emsg("Config","async",(char *)asopts[i].opname,
                                (char *)"value not specified");
                      return 1;
                     }
                  if (*asopts[i].opname == 'i')
                     if (XrdOuca2x::a2sz(eDest,asopts[i].opmsg, val, &llp,
                                       (long long)asopts[i].minv)) return 1;
                        else *asopts[i].oploc = (int)llp;
                     else if (XrdOuca2x::a2i(eDest,asopts[i].opmsg, val, &ppp,
                                            asopts[i].minv)) return 1;
                             else *asopts[i].oploc = ppp;
                  break;
                 }
          if (i >= numopts)
             eDest.Emsg("Config", "Warning, invalid async option", val);
          val = Config.GetToken();
         }

// Make sure max values are consistent
//
   if (V_mapl > 0 && V_maps > 0 && V_mapl > V_maps)
           {eDest.Emsg("Config", "async maxpl may not be greater than maxps");
            return 1;
           }

// Establish async options
//
   if (V_mapl > 0) as_maxaspl = V_mapl;
   if (V_maps > 0) as_maxasps = V_maps;
   if (as_maxaspl > as_maxasps) as_maxaspl = as_maxasps;
   if (V_iosz > 0) as_aiosize = V_iosz;

   return 0;
}

/******************************************************************************/
/*                                x c k s u m                                 */
/******************************************************************************/

/* Function: xcksum

   Purpose:  To parse the directive: chksum <type> <path>

             <type>    algorithm of checksum (e.g., md5)
             <path>    the path of the program performing the checksum

  Output: 0 upon success or !0 upon failure.
*/

int XrdXrootdProtocol::xcksum(XrdOucTokenizer &Config)
{
   char *palg, *prog;

// Get the algorithm name and the program implementing it
//
   if (!(palg = Config.GetToken(&prog)))
      {eDest.Emsg("Config", "chksum algorithm not specified"); return 1;}
   if (!prog || *prog == '\0')
      {eDest.Emsg("Config", "chksum program not specified"); return 1;}

// Set up the program
//
   if (ProgCKT) free(ProgCKT);
   ProgCKT = strdup(palg);
   if (!ProgCKS) ProgCKS = new XrdOucProg(0);
   return ProgCKS->Setup(prog, &eDest);
}
  
/******************************************************************************/
/*                                  x e x p                                   */
/******************************************************************************/

/* Function: xexp

   Purpose:  To parse the directive: export <path>

             <path>    the path to be exported.

  Output: 0 upon success or !0 upon failure.
*/

int XrdXrootdProtocol::xexp(XrdOucTokenizer &Config)
{
    char *val;

// Get the path
//
   val = Config.GetToken();
   if (!val || !val[0])
      {eDest.Emsg("Config", "export path not specified"); return 1;}

// Add path to configuration
//
   return xexpdo(val);
}

/******************************************************************************/

int XrdXrootdProtocol::xexpdo(char *path)
{
// Make sure path start with a slash
//
   if (rpCheck(path))
      {eDest.Emsg("Config", "non-absolute export path -", path); return 1;}

// Record the path
//
   if (!Squash(path)) XPList.Insert(path);
   return 0;
}
  
/******************************************************************************/
/*                                  x f s l                                   */
/******************************************************************************/

/* Function: xfsl

   Purpose:  To parse the directive: fslib [?] <path>

             ?         check if fslib build version matches our version
             <path>    the path of the filesystem library to be used.

  Output: 0 upon success or !0 upon failure.
*/

int XrdXrootdProtocol::xfsl(XrdOucTokenizer &Config)
{
    char *val;

// Get the path
//
   chkfsV = 0;
   if ((val = Config.GetToken()) && *val == '?' && !val[1])
      {chkfsV = '?'; val = Config.GetToken();}

   if (!val || !val[0])
      {eDest.Emsg("Config", "fslib not specified"); return 1;}

// Record the path
//
   if (FSLib) free(FSLib);
   FSLib = strdup(val);
   return 0;
}

/******************************************************************************/
/*                                  x m o n                                   */
/******************************************************************************/

/* Function: xmon

   Purpose:  Parse directive: monitor [all] [io] [off] [mbuff <sz>] 
                                      [window <sec>] dest <host:port>

         all                enables monitoring for all connections.
         io                 only monitors I/O requests.
         off                disabled monitoring but leaves config info in place.
         mbuff  <sz>        size of message buffer.
         window <sec>       time (seconds, M, H) between timing marks.
         dest   <host:port> where monitor records are to be sent.

   Output: 0 upon success or !0 upon failure. Ignored by master.
*/
int XrdXrootdProtocol::xmon(XrdOucTokenizer &Config)
{   char  *val;
    long long tempval;

    monMode = XROOTD_MON_SOME;
    while((val = Config.GetToken()))

         {     if (!strcmp("all", val))  monMode = XROOTD_MON_ALL;
               if (!strcmp("io",  val)) ;
          else if (!strcmp("off", val))  monMode = XROOTD_MON_NONE;
          else if (!strcmp("mbuff", val))
                  {if (!(val = Config.GetToken()))
                      {eDest.Emsg("Config", "monitor mbuff value not specified");
                       return 1;
                      }
                   if (XrdOuca2x::a2sz(eDest,"monitor mbuff", val,
                                             &tempval, 1024, 65536)) return 1;
                    monMBval = static_cast<int>(tempval);
                  }
          else if (!strcmp("window", val))
                {if (!(val = Config.GetToken()))
                    {eDest.Emsg("Config", "monitor window value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(eDest,"monitor window",val,
                                           &monWWval,1)) return 1;
                }
        else if (!strcmp("dest", val))
                {if (monDest) free(monDest);
                 if (!(monDest = Config.GetToken()))
                   {eDest.Emsg("Config", "monitor dest value not specified");
                    return 1;
                   }
                 if (!(val = index(monDest, (int)':')) || !atoi(val+1))
                   {eDest.Emsg("Config", "monitor dest port missing or invalid");
                    return 1;
                   }
                }
        else eDest.Emsg("Config", "Warning, invalid monitor option", val);
       }

   if (!monDest)
       {eDest.Emsg("Config", "monitor dest not specified"); return 1;}

   return 0;
}

/******************************************************************************/
/*                                 x p r e p                                  */
/******************************************************************************/

/* Function: xprep

   Purpose:  To parse the directive: prep [keep <sec>] [scrub <sec>]
                                          [logdir <path>]
         keep   <sec>  time (seconds, M, H) to keep logdir entries.
         scrub  <sec>  time (seconds, M, H) between logdir scrubs.
         logdir <path> the absolute path to the prepare log directory.

   Output: 0 upon success or !0 upon failure. Ignored by master.
*/
int XrdXrootdProtocol::xprep(XrdOucTokenizer &Config)
{   int   rc, keep = 0, scrub=0;
    char  *ldir=0,*val;

    if (!(val = Config.GetToken()))
       {eDest.Emsg("Config", "prep options not specified"); return 1;}

        do { if (!strcmp("keep", val))
                {if (!(val = Config.GetToken()))
                    {eDest.Emsg("Config", "prep keep value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(eDest,"prep keep int",val,&keep,1)) return 1;
                }
        else if (!strcmp("scrub", val))
                {if (!(val = Config.GetToken()))
                    {eDest.Emsg("Config", "prep scrub value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(eDest,"prep scrub",val,&scrub,0)) return 1;
                }
        else if (!strcmp("logdir", val))
                {if (!(ldir = Config.GetToken()))
                   {eDest.Emsg("Config", "prep logdir value not specified");
                    return 1;
                   }
                }
        else eDest.Emsg("Config", "Warning, invalid prep option", val);
       } while((val = Config.GetToken()));

// Set the values
//
   if (scrub || keep) XrdXrootdPrepare::setParms(scrub, keep);
   if (ldir) 
       if ((rc = XrdXrootdPrepare::setParms(ldir)) < 0)
          {eDest.Emsg("Config", rc, "process logdir", ldir);
           return 1;
          }
   return 0;
}

/******************************************************************************/
/*                                 x s e c l                                  */
/******************************************************************************/

/* Function: xsecl

   Purpose:  To parse the directive: seclib <path>

             <path>    the path of the security library to be used.

  Output: 0 upon success or !0 upon failure.
*/

int XrdXrootdProtocol::xsecl(XrdOucTokenizer &Config)
{
    char *val;

// Get the path
//
   val = Config.GetToken();
   if (!val || !val[0])
      {eDest.Emsg("Config", "XRootd seclib not specified"); return 1;}

// Record the path
//
   if (SecLib) free(SecLib);
   SecLib = strdup(val);
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

int XrdXrootdProtocol::xtrace(XrdOucTokenizer &Config)
{
    char *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"emsg",     TRACE_EMSG},
        {"debug",    TRACE_DEBUG},
        {"fs",       TRACE_FS},
        {"login",    TRACE_LOGIN},
        {"mem",      TRACE_MEM},
        {"stall",    TRACE_STALL},
        {"redirect", TRACE_REDIR},
        {"request",  TRACE_REQ},
        {"response", TRACE_RSP}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetToken()))
       {eDest.Emsg("config", "trace option not specified"); return 1;}
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
                      eDest.Emsg("config", "invalid trace option", val);
                  }
          val = Config.GetToken();
         }
    XrdXrootdTrace->What = trval;
    return 0;
}
