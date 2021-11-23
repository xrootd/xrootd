/******************************************************************************/
/*                                                                            */
/*                 X r d C m s C l i e n t C o n f i g . c c                  */
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

#include <unistd.h>
#include <strings.h>
#include <cstdio>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "XrdCms/XrdCmsClientConfig.hh"
#include "XrdCms/XrdCmsClientMsg.hh"
#include "XrdCms/XrdCmsPerfMon.hh"
#include "XrdCms/XrdCmsSecurity.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdCms/XrdCmsUtils.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace XrdCms
{
extern XrdVersionInfo myVersion;
}

using namespace XrdCms;

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(Config);
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdCmsClientConfig::~XrdCmsClientConfig()
{
  XrdOucTList *tp, *tpp;

  tpp = ManList;
  while((tp = tpp)) {tpp = tp->next; delete tp;}
  tpp = PanList;
  while((tp = tpp)) {tpp = tp->next; delete tp;}

  if (VNID_Lib)   free(VNID_Lib);
  if (VNID_Parms) free(VNID_Parms);
  if (prfLib)     free(prfLib);
  if (prfParms)   free(prfParms);
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdCmsClientConfig::Configure(const char *cfn, configWhat What,
                                                   configHow  How)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   XrdOucTList *tpe, *tpl;
   int i, NoGo = 0;
   const char *eText = 0;
   char buff[256], *slash, *temp, *bP, sfx;

// Preset some values
//
   myHost = getenv("XRDHOST");
   if (!myHost) myHost = "localhost";
   myName = XrdOucUtils::InstName(1);
   CMSPath = getenv("XRDADMINPATH");
   if (CMSPath && *CMSPath) CMSPath = strdup(CMSPath);
      else CMSPath = XrdOucUtils::genPath("/tmp/",XrdOucUtils::InstName(myName,0));
   isMeta = (How & configMeta) != 0;
   isMan  = (What& configMan)  != 0;
   isServer = What == configServer;

// Process the configuration file
//
   if (!(NoGo = ConfigProc(cfn)) && isMan)
      {if (How & configProxy) eText = (PanList ? 0 : "Proxy manager");
          else if (!ManList)
                  eText = (How & configMeta ? "Meta manager" : "Manager");
       if (eText) {Say.Emsg("Config", eText, "not specified."); NoGo=1;}
      }

// Reset tracing options
//
   if (getenv("XRDDEBUG")) Trace.What = TRACE_ALL;

// Set proper local socket path
//
   temp = XrdOucUtils::genPath(CMSPath, (const char *)0, ".olb");
   free(CMSPath); CMSPath = temp;
   XrdOucEnv::Export("XRDCMSPATH", temp);
   XrdOucEnv::Export("XRDOLBPATH", temp); //Compatibility

// Determine what type of role we are playing
//
        if (What & configServer) sfx = 's';
   else if (What & configSuper)  sfx = 'u';
   else                          sfx = 'm';

// Determine which manager list we will be using
//
   if (How & configProxy)
      {sfx = toupper(sfx);
       tpl = PanList;
      } else tpl = ManList;

// Generate the system ID for this configuration.
//
   if (!ConfigSID(cfn, tpl, sfx)) NoGo = 1;

// Export the manager list
//
   if (tpl)
      {i = 0; tpe = tpl;
       while(tpe) {i += strlen(tpe->text) + 9; tpe = tpe->next;}
       bP = temp = (char *)malloc(i);
       while(tpl)
            {bP += sprintf(bP, "%s:%d ", tpl->text, tpl->val);
             tpl = tpl->next;
            }
       *(bP-1) = '\0';
       XrdOucEnv::Export("XRDCMSMAN", temp); free(temp);
      }

// Construct proper communications path for a supervisor node
//
   i = strlen(CMSPath);
   if (What & configSuper)
      {while((tpl = ManList)) {ManList = tpl->next; delete tpl;}
       slash = (CMSPath[i-1] == '/' ? (char *)"" : (char *)"/");
       sprintf(buff, "%s%solbd.super", CMSPath, slash);
       ManList = new XrdOucTList(buff, -1, 0);
       SMode = SModeP = FailOver;
      }

// Construct proper old communication path for a target node
//
   temp = (What & (configMan|configSuper) ? (char *)"nimda" : (char *)"admin");
   slash = (CMSPath[i-1] == '/' ? (char *)"" : (char *)"/");
   sprintf(buff, "%s%solbd.%s", CMSPath, slash, temp);
   free(CMSPath); CMSPath = strdup(buff);

   RepWaitMS = RepWait * 1000;

// Initialize the msg queue
//
   if (XrdCmsClientMsg::Init())
      {Say.Emsg("Config", ENOMEM, "allocate initial msg objects");
       NoGo = 1;
      }

// Load the performance monitor (server pre-screened) if specified.
//
   if (prfLib && cmsMon)
      {perfMon = XrdCmsUtils::loadPerfMon(&Say, prfLib, XrdCms::myVersion);
       if (!perfMon || !perfMon->Configure(cfn, prfParms, *Say.logger(),
                                           *cmsMon, 0, false))
          {Say.Emsg("Config","Unable to configure performance monitor plugin.");
           NoGo = 1;
          }
      }

   return NoGo;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdCmsClientConfig::ConfigProc(const char *ConfigFN)
{
  static int DoneOnce = 0;
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucEnv myEnv;
  XrdOucStream Config((DoneOnce ? 0 : &Say), getenv("XRDINSTANCE"),
                      &myEnv, "=====> ");

// Make sure we have a config file
//
   if (!ConfigFN || !*ConfigFN)
      {Say.Emsg("Config", "cms configuration file not specified.");
       return 1;
      }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Say.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);
   static const char *cvec[] = { "*** cms plugin config:", 0 };
   Config.Capture(cvec);

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, "cms.", 4)
         ||  !strncmp(var, "odc.", 4)      // Compatibility
         ||  !strcmp(var, "all.manager")
         ||  !strcmp(var, "all.adminpath")
         ||  !strcmp(var, "olb.adminpath"))
            if (ConfigXeq(var+4, Config)) {Config.Echo(); NoGo = 1;}
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = Say.Emsg("Config", retc, "read config file", ConfigFN);
   Config.Close();

// Return final return code
//
   DoneOnce = 1;
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g S i d                              */
/******************************************************************************/
  
bool XrdCmsClientConfig::ConfigSID(const char *cFN, XrdOucTList *tpl, char sfx)
{
   char *sidVal;

// Get the node ID if we need to
//
   if (VNID_Lib)
      {myVNID = XrdCmsSecurity::getVnId(Say, cFN, VNID_Lib, VNID_Parms, sfx);
       if (!myVNID) return false;
      }

// Generate the system ID and set the cluster ID
//
   sidVal = XrdCmsSecurity::setSystemID(tpl, myVNID, cidTag, sfx);
   if (!sidVal || *sidVal == '!')
      {const char *msg;
       if (!sidVal) msg = "too many managers.";
          else msg = sidVal+1;
       Say.Emsg("Config ","Unable to generate system ID; ", msg);
       return false;
      }
      else if (QTRACE(Debug))
              Say.Say("Config ", "Global System Identification: ", sidVal);
   return true;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdCmsClientConfig::ConfigXeq(char *var, XrdOucStream &Config)
{

   // Process items. for either a local or a remote configuration
   //
   TS_Xeq("adminpath",     xapath);
   TS_Xeq("cidtag",        xcidt);
   TS_Xeq("conwait",       xconw);
   TS_Xeq("manager",       xmang);
   TS_Xeq("perf",          xperf);
   TS_Xeq("request",       xreqs);
   TS_Xeq("trace",         xtrac);
   TS_Xeq("vnid",          xvnid);
   return 0;
}

/******************************************************************************/
/*                                x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path> [ group ]

             <path>    the path of the named socket to use for admin requests.
                       Only the path may be specified, not the filename.
             group     allow group access to the path.

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/
  
int XrdCmsClientConfig::xapath(XrdOucStream &Config)
{
    char *pval;

// Get the path
//
   pval = Config.GetWord();
   if (!pval || !pval[0])
      {Say.Emsg("Config", "cms admin path not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {Say.Emsg("Config", "cms admin path not absolute"); return 1;}

// Record the path
//
   if (CMSPath) free(CMSPath);
   CMSPath = XrdOucUtils::genPath(pval,XrdOucUtils::InstName(myName,0));
   return 0;
}

/******************************************************************************/
/*                                 x c i d t                                  */
/******************************************************************************/

/* Function: xcidt

   Purpose:  To parse the directive: cidtag <tag>

             <tag>     a 1- to 16-character cluster ID tag.

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsClientConfig::xcidt(XrdOucStream &Config)
{
    char *val;

// Get the path
//
   if (!(val = Config.GetWord()) || !val[0])
      {Say.Emsg("Config", "tag not specified"); return 1;}

// Make sure it is not too long
//
   if ((int)strlen(val) > 16)
      {Say.Emsg("Config", "tag is > 16 characters"); return 1;}

// Record the tag
//
   if (cidTag) free(cidTag);
   cidTag = strdup(val);
   return 0;
}

/******************************************************************************/
/*                                 x c o n w                                  */
/******************************************************************************/

/* Function: xconw

   Purpose:  To parse the directive: conwait <sec>

             <sec>   number of seconds to wait for a manager connection

   Type: Remote server only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsClientConfig::xconw(XrdOucStream &Config)
{
    char *val;
    int cw;

    if (!(val = Config.GetWord()))
       {Say.Emsg("Config", "conwait value not specified."); return 1;}

    if (XrdOuca2x::a2tm(Say,"conwait value",val,&cw,1)) return 1;

    ConWait = cw;
    return 0;
}
  
/******************************************************************************/
/*                                 x m a n g                                  */
/******************************************************************************/

/* Function: xmang

   Purpose:  Parse: manager [meta | peer | proxy] [all|any]
                            <host>[+][:<port>|<port>] [if ...]

             meta   For cmsd:   Specifies the manager when running as a manager
                    For xrootd: Specifies the manager when running as a meta
             peer   For cmsd:   Specifies the manager when running as a peer
                    For xrootd: The directive is ignored.
             proxy  For cmsd:   This directive is ignored.
                    For xrootd: Specifies the cms-proxy service manager
             all    Distribute requests across all managers.
             any    Choose different manager only when necessary (default).
             <host> The dns name of the host that is the cache manager.
                    If the host name ends with a plus, all addresses that are
                    associated with the host are treated as managers.
             <port> The port number to use for this host.
             if     Apply the manager directive if "if" is true. See
                    XrdOucUtils:doIf() for "if" syntax.

   Notes:   Any number of manager directives can be given. When niether peer nor
            proxy is specified, then regardless of role the following occurs:
            cmsd:   Subscribes to each manager whens role is not peer.
            xrootd: Logins in as a redirector to each manager when role is not 
                    proxy or server.

   Type: Remote server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsClientConfig::xmang(XrdOucStream &Config)
{
    class StorageHelper
    {public:
          StorageHelper(char **v1, char **v2) : val1(v1), val2(v2) {}
         ~StorageHelper() {if (*val1) free(*val1);
                           if (*val2) free(*val2);
                          }
    char **val1, **val2;
    };

    XrdOucTList **theList;
    char *val, *hSpec = 0, *hPort = 0;
    StorageHelper SHelp(&hSpec, &hPort);
    int rc, xMeta = 0, xProxy = 0, smode = FailOver;

//  Process the optional "peer" or "proxy"
//
    if ((val = Config.GetWord()))
       {if (!strcmp("peer", val)) return Config.noEcho();
        if ((xProxy = !strcmp("proxy", val))) val = Config.GetWord();
           else if ((xMeta = !strcmp("meta", val)))
                   if (isMeta || isMan) val = Config.GetWord();
                      else return Config.noEcho();
                   else if (isMeta) return Config.noEcho();
       }

//  We can accept this manager. Skip the optional "all" or "any"
//
    if (val)
       {     if (!strcmp("any", val)) smode = FailOver;
        else if (!strcmp("all", val)) smode = RoundRob;
        else                          smode = 0;
        if (smode)
           {if (xProxy) SModeP = smode;
               else     SMode  = smode;
            val = Config.GetWord();
           }
       }

//  Get the actual manager
//
    if (!val)
       {Say.Emsg("Config","manager host name not specified"); return 1;}
       else hSpec = strdup(val);

//  Grab the port number (either in hostname or following token)
//
    if (!(hPort = XrdCmsUtils::ParseManPort(&Say, Config, hSpec))) return 1;

// Process any "if" clause now
//
   if ((val = Config.GetWord()) && !strcmp("if", val))
      if ((rc = XrdOucUtils::doIf(&Say,Config,"manager directive",
                                  myHost, myName, getenv("XRDPROG"))) <= 0)
          {if (!rc) Config.noEcho(); return (rc < 0);}

// If we are a manager and found a meta-manager indidicate it and bail.
//
    if (xMeta && !isMeta) {haveMeta = 1; return 0;}
    theList = (xProxy ? &PanList : &ManList);

// Parse the manager list and return the result
//
   return (XrdCmsUtils::ParseMan(&Say, theList, hSpec, hPort, 0) ? 0 : 1);
}
  

/******************************************************************************/
/*                                 x p e r f                                  */
/******************************************************************************/

/* Function: xperf

   Purpose:  To parse the directive: perf [xrootd] [int <sec>]
                                          [lib <lib> [<parms>] | pgm <pgm>]

         int <time>    estimated time (seconds, M, H) between reports by <pgm>
         lib <lib>     the shared library holding the XrdCmsPerf object that
                       reports perf values. It must be the last option.
         pgm <pgm>     program to start that will write perf values to standard
                       out. It must be the last option. This is not supported
                       when xrootd is specified.
         xrootd        This directive only applies to the cms xrootd plugin.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure. Ignored by manager.
*/

int XrdCmsClientConfig::xperf(XrdOucStream &Config)
{   char *val;

    if (!isServer) return Config.noEcho();

    if (!(val = Config.GetWord()))
       {Say.Emsg("Config", "perf options not specified"); return 1;}

    if (strcmp("xrootd", val)) return Config.noEcho();
    perfInt = 3*60;

    do {     if (!strcmp("int", val))
                {if (!(val = Config.GetWord()))
                    {Say.Emsg("Config", "perf int value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(Say,"perf int",val,&perfInt,0)) return 1;
                }
        else if (!strcmp("lib",  val))
                {return (XrdOucUtils::parseLib(Say, Config, "perf lib",
                                               prfLib, &prfParms) ? 0 : 1);
                 break;
                }
        else if (!strcmp("pgm",  val))
                {Say.Emsg("Config", "perf pgm is not supported for xrootd.");
                 return 1;
                }
        else Say.Say("Config warning: ignoring invalid perf option '",val,"'.");
       } while((val = Config.GetWord()));

// All done.
//
    return 0;
}

/******************************************************************************/
/*                                 x r e q s                                  */
/******************************************************************************/

/* Function: xreqs

   Purpose:  To parse the directive: request [repwait <sec1>] [delay <sec2>]
                                             [noresp <cnt>] [prep <ms>]
                                             [fwd <ms>]

             <sec1>  max number of seconds to wait for a cmsd reply
             <sec2>  number of seconds to delay a retry upon failure
             <cnt>   number of no-responses before cms fault declared.
             <ms>    milliseconds between prepare/forward requests

   Type: Remote server only, dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdCmsClientConfig::xreqs(XrdOucStream &Config)
{
    char *val;
    static struct reqsopts {const char *opname; int istime; int *oploc;}
           rqopts[] =
       {
        {"delay",    1, &RepDelay},
        {"fwd",      0, &FwdWait},
        {"noresp",   0, &RepNone},
        {"prep",     0, &PrepWait},
        {"repwait",  1, &RepWait}
       };
    int i, ppp, numopts = sizeof(rqopts)/sizeof(struct reqsopts);

    if (!(val = Config.GetWord()))
       {Say.Emsg("Config", "request arguments not specified"); return 1;}

    while (val)
    do {for (i = 0; i < numopts; i++)
            if (!strcmp(val, rqopts[i].opname))
               { if (!(val = Config.GetWord()))
                  {Say.Emsg("Config","request argument value not specified");
                   return 1;}
                   if (rqopts[i].istime ?
                       XrdOuca2x::a2tm(Say,"request value",val,&ppp,1) :
                       XrdOuca2x::a2i( Say,"request value",val,&ppp,1))
                      return 1;
                      else *rqopts[i].oploc = ppp;
                break;
               }
        if (i >= numopts) Say.Say("Config warning: ignoring invalid request option '",val,"'.");
       } while((val = Config.GetWord()));
     return 0;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: retc upon success or -EINVAL upon failure.
*/

int XrdCmsClientConfig::xtrac(XrdOucStream &Config)
{
    char  *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"files",    TRACE_Files},
        {"forward",  TRACE_Forward},
        {"redirect", TRACE_Redirect},
        {"defer",    TRACE_Defer},
        {"stage",    TRACE_Stage}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Say.Emsg("config", "trace option not specified"); return 1;}
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
                      Say.Say("Config warning: ignoring invalid trace option '",val,"'.");
                  }
          val = Config.GetWord();
         }
    Trace.What = trval;
    return 0;
}
  
/******************************************************************************/
/*                                 x v n i d                                  */
/******************************************************************************/

/* Function: xvnid

   Purpose:  To parse the directive: vnid {=|<|@}<vnarg> [<parms>]

             <vnarg>   = - the actual vnid value
                       < - the path of the file to be read for the vnid.
                       @ - the path of the plugin library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdCmsClientConfig::xvnid(XrdOucStream &Config)
{
    char *val, parms[1024];

// Get the argument
//
   if (!(val = Config.GetWord()) || !val[0])
      {Say.Emsg("Config", "vnid not specified"); return 1;}

// Record the path
//
   if (VNID_Lib) free(VNID_Lib);
   VNID_Lib = strdup(val);

// Record any parms (only if it starts with an @)
//
   if (VNID_Parms) {free(VNID_Parms); VNID_Parms = 0;}
   if (*VNID_Lib == '@')
      {if (!Config.GetRest(parms, sizeof(parms)))
          {Say.Emsg("Config", "vnid plug-in parameters too long"); return 1;}
       if (*parms) VNID_Parms = strdup(parms);
      }
   return 0;
}
