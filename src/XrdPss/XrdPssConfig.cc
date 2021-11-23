/******************************************************************************/
/*                                                                            */
/*                       X r d P s s C o n f i g . c c                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <cctype>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdNet/XrdNetSecurity.hh"

#include "XrdPss/XrdPss.hh"
#include "XrdPss/XrdPssTrace.hh"
#include "XrdPss/XrdPssUrlInfo.hh"
#include "XrdPss/XrdPssUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucPsx.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdPosix/XrdPosixConfig.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"

#include "XrdSecsss/XrdSecsssID.hh"

#include "XrdXrootd/XrdXrootdGStream.hh"

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define Duplicate(x,y) if (y) free(y); y = strdup(x)

#define TS_String(x,m) if (!strcmp(x,var)) {Duplicate(val,m); return 0;}

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(&eDest, Config);

#define TS_PSX(x,m)    if (!strcmp(x,var)) \
                          return (psxConfig->m(&eDest, Config) ? 0 : 1);

#define TS_DBG(x,m)    if (!strcmp(x,var)) {SysTrace.What |= m; return 0;}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

const char  *XrdPssSys::ConfigFN;       // -> Pointer to the config file name
const char  *XrdPssSys::myHost;
const char  *XrdPssSys::myName;

XrdOucPListAnchor XrdPssSys::XPList;

XrdNetSecurity   *XrdPssSys::Police[XrdPssSys::PolNum] = {0, 0};

XrdOucTList *XrdPssSys::ManList   =  0;
const char  *XrdPssSys::protName  =  "root:";
const char  *XrdPssSys::hdrData   =  "";
int          XrdPssSys::hdrLen    =  0;
int          XrdPssSys::Streams   =512;
int          XrdPssSys::Workers   = 16;
int          XrdPssSys::Trace     =  0;
int          XrdPssSys::dcaCTime  =  0;

bool         XrdPssSys::xLfn2Pfn  = false;
bool         XrdPssSys::dcaCheck  = false;
bool         XrdPssSys::dcaWorld  = false;
bool         XrdPssSys::deferID   = false;
bool         XrdPssSys::reProxy   = false;

namespace XrdProxy
{
static XrdPosixXrootd  *Xroot;
  
extern XrdSysError      eDest;

extern XrdOucSid       *sidP;

extern XrdOucEnv       *envP;

extern XrdSecsssID     *idMapper; // -> Auth ID mapper

extern int              rpFD;

extern bool             idMapAll;

extern bool             outProxy; // True means outgoing proxy

extern bool             xrdProxy; // True means dest using xroot protocol

extern XrdSysTrace      SysTrace;

static const int maxHLen = 1024;
}

namespace
{
XrdOucPsx *psxConfig;

XrdSecsssID::authType sssMap;      // persona setting

std::vector<const char *> protVec;    // Additional wanted protocols
}

using namespace XrdProxy;

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdPssSys::Configure(const char *cfn, XrdOucEnv *envP)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   char theRdr[maxHLen];
   int NoGo = 0;

// Get environmental values
//
   myHost = getenv("XRDHOST");
   myName = XrdOucUtils::InstName(1);
   ConfigFN = cfn;

// Thell xrootd to disable POSC mode as this is meaningless here
//
   XrdOucEnv::Export("XRDXROOTD_NOPOSC", "1");

// Create a configurator. It will be deleted when we are done.
//
   psxConfig = new XrdOucPsx(myVersion, cfn, eDest.logger(), envP);

// Set debug level if so wanted
//
   if (getenv("XRDDEBUG"))
      {psxConfig->traceLvl = 4;
       SysTrace.What |=  TRACEPSS_Debug;
      }

// Set the defaault number of worker threads for the client
//
   XrdPosixConfig::SetEnv("WorkerThreads", 64);

// Set client IP mode based on what the server is set to
//
   if (XrdNetAddr::IPV4Set()) psxConfig->useV4 = true;

// Set default number of event loops
//
   XrdPosixConfig::SetEnv("ParallelEvtLoop", 3);

// Turn off the fork handler as we always exec after forking.
//
   XrdPosixConfig::SetEnv("RunForkHandler", 0);

// Process the configuration file
//
   if ((NoGo = ConfigProc(cfn))) return NoGo;

// Make sure we have some kind of origin
//
   if (!ManList && !outProxy)
      {eDest.Emsg("Config", "Origin for proxy service not specified.");
       return 1;
      }

// Check if we should configure authentication security mapping
//
   if (sssMap && !ConfigMapID()) return 1;

// Handle the local root here
//
   if (LocalRoot) psxConfig->SetRoot(LocalRoot);

// Pre-screen any n2n library parameters
//
   if (outProxy && psxConfig->xLfn2Pfn)
      {const char *txt;
       if (!(psxConfig->xNameLib)) txt = "localroot directive";
          else if (psxConfig->xPfn2Lfn) txt = "namelib -lfn2pfn option";
                  else txt = "namelib directive";
       eDest.Say("Config warning: ignoring ",txt,"; this is forwarding proxy!");
       psxConfig->xLfn2Pfn = false;
      }

// If we have a cache, indicate so in the feature set
//
   if(psxConfig->hasCache()) myFeatures |= XRDOSS_HASCACH;

// If we need to reproxy, then open the directory where the reproxy information
// will ne placed. The path is in the Env.
//
   if (reProxy)
      {char *rPath;
       if (!envP || !(rPath = envP->Get("tpc.rpdir")))
          {eDest.Say("Config warning: ignoring 'pss.reproxy'; TPC is not enabled!");
           reProxy = false;
           myFeatures &= ~XRDOSS_HASRPXY;
          } else {
           rpFD = XrdSysFD_Open(rPath, O_DIRECTORY);
           if (rpFD < 0)
              {eDest.Emsg("Config", "to open reproxy directory", rPath);
               return 1;
              }
          }
      }

// Finalize the configuration
//
   if (!(psxConfig->ConfigSetup(eDest))) return 1;

// Complete initialization (we would set the env pointer here)
//
   if (!XrdPosixConfig::SetConfig(*psxConfig)) return 1;

// Save the N2N library pointer if we will be using it
//
   if (psxConfig->xLfn2Pfn) xLfn2Pfn = (theN2N = psxConfig->theN2N) != 0;

// If we have a cache then save it and check if we need to tell
// xrootd we allow a redirect on a read (this is complicated).
// ??? Why are we doing this
// if (psxConfig->theCache2 && dcaCTime)
//    {char buff[32];
//     sprintf(buff, "%d", dcaCTime);
//     XrdOucEnv::Export("XRDXROOTD_CACHERDRDR", buff);
//    }

// All done with the configurator
//
   delete psxConfig;

// Allocate an Xroot proxy object (only one needed here). Tell it to not
// shadow open files with real file descriptors (we will be honest).
//
   Xroot = new XrdPosixXrootd(-32768, 16384);

// Allocate an streaim ID object if need be
//
   if (Streams) sidP = new XrdOucSid((Streams > 8192 ? 8192 : Streams));

// Tell any security manager we are a proxy as this will force it to use our
// credentials. We don't support credential forwarding, yet. If we did we would
// also set XrdSecPROXYCREDS to accomplish that feat.
//
    XrdOucEnv::Export("XrdSecPROXY", "1");

// Add the origin protocl to the recognized list of protocol names
//
   if (!XrdPosixXrootPath::AddProto(protName))
      {eDest.Emsg("Config", "Unable to add origin protocol to protocol list.");
       return 1;
      }

// Add any other protocols to the recognized list of protocol names
//
   if (protVec.size())
      {for (int i = 0; i < (int)protVec.size(); i++)
           {if (!XrdPosixXrootPath::AddProto(protVec[i]))
               {eDest.Emsg("Config", "Unable to add", protVec[i],
                                     "protocol to protocol list.");
                return 1;
               }
           }
       protVec.clear();
      }

// Construct the redirector name:port (we might not have one) export it
//
   const char *outeq = (outProxy ? "= " : "");
   if (ManList) sprintf(theRdr, "%s%s:%d", outeq, ManList->text, ManList->val);
      else strcpy(theRdr, outeq);
   XrdOucEnv::Export("XRDXROOTD_PROXY",  theRdr);
   XrdOucEnv::Export("XRDXROOTD_ORIGIN", theRdr); // Backward compatibility

// Construct the contact URL header
//
   if (ManList)               //<prot><id>@<host>:<port>/<path>
      {hdrLen = sprintf(theRdr, "%s%%s%s:%d/%%s",
                        protName, ManList->text, ManList->val);
       hdrData = strdup(theRdr);
      }

// Check if we have any r/w exports as this will determine whether or not we
// need to initialize any r/w cache. Currently, we don't support this so we
// have no particular initialization to do.
//
// XrdOucPList *fP = XPList.First();
// while(fP && !(fP->Flag() & XRDEXP_NOTRW)) fP = fP->Next();
// if (!fP) . . .

// All done
//
   return 0;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                           C o n f i g M a p I D                            */
/******************************************************************************/
  
bool XrdPssSys::ConfigMapID()
{
   XrdSecsssCon *conTracker;
   bool isOK, Debug = (SysTrace.What & TRACEPSS_Debug) != 0;

// If this is a generic static ID mapping, we are done
//
   if (sssMap == XrdSecsssID::idStatic) return true;

// For optimzation we also note if we have a cache in he way of the map
//
   deferID = psxConfig->hasCache();

// Now that we did the cache thing, currently we don't support client personas
// with a cache because aren't able to tell which client will be used.
//
   if (deferID)
      {eDest.Emsg("Config", "Client personas are not supported for "
                            "caching proxy servers.");
       return false;
      }

// If this server is only a forwarding proxy server, we can't support client
// personas either because we don't control the URL. However, if we have an
// origin then simply warn that the client persona applies to the origin.
//
   if (outProxy)
      {if (!ManList)
          {eDest.Emsg("Config", "Client personas are not supported for "
                                "strictly forwarding proxy servers.");
           return false;
          }
       eDest.Say("Config warning: client personas only apply to "
                 "the origin server!");
      }

// We need to get a connection tracker object from the posix interface.
// However, we only need it if we are actually mapping id's.
//
   if (sssMap == XrdSecsssID::idStaticM) conTracker = 0;
      else conTracker = XrdPosixConfig::conTracker(Debug);

// Get an mapper object
//
   idMapper = new XrdSecsssID(sssMap, 0, conTracker, &isOK);
   if (!isOK)
      {eDest.Emsg("Config", "Unable to render persona; persona mapper failed!");
       return false;
      }

// If ths is a server persona then we don't need the mapper; abandon it.
//
   if (sssMap == XrdSecsssID::idStaticM) idMapper = 0;
      else XrdPssUrlInfo::setMapID(true);

// We are all done
//
   return true;
}

/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdPssSys::ConfigProc(const char *Cfn)
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucEnv myEnv;
  XrdOucStream Config(&eDest, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Make sure we have a config file
//
   if (!Cfn || !*Cfn)
      {eDest.Emsg("Config", "pss configuration file not specified.");
       return 1;
      }

// Try to open the configuration file.
//
   if ( (cfgFD = open(Cfn, O_RDONLY, 0)) < 0)
      {eDest.Emsg("Config", errno, "open config file", Cfn);
       return 1;
      }
   Config.Attach(cfgFD);
   static const char *cvec[] = { "*** pss (oss) plugin config:", 0 };
   Config.Capture(cvec);

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, "pss.", 4)
         ||  !strcmp(var, "oss.defaults")
         ||  !strcmp(var, "all.export"))
            if (ConfigXeq(var+4, Config)) {Config.Echo(); NoGo = 1;}
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = eDest.Emsg("Config", retc, "read config file", Cfn);
   Config.Close();

// Set the defaults for the export list
//
   XPList.Set(DirFlags);

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdPssSys::ConfigXeq(char *var, XrdOucStream &Config)
{
   char myVar[80], *val;

   // Process items. for either a local or a remote configuration
   //
   TS_PSX("namelib",       ParseNLib);
   TS_PSX("memcache",      ParseCache);  // Backward compatibility
   TS_PSX("cache",         ParseCache);
   TS_PSX("cachelib",      ParseCLib);
   TS_PSX("ccmlib",        ParseMLib);
   TS_PSX("ciosync",       ParseCio);
   TS_Xeq("config",        xconf);
   TS_Xeq("dca",           xdca);
   TS_Xeq("defaults",      xdef);
   TS_DBG("debug",         TRACEPSS_Debug);
   TS_Xeq("export",        xexp);
   TS_PSX("inetmode",      ParseINet);
   TS_Xeq("origin",        xorig);
   TS_Xeq("permit",        xperm);
   TS_Xeq("persona",       xpers);
   TS_PSX("setopt",        ParseSet);
   TS_PSX("trace",         ParseTrace);

   if (!strcmp("reproxy", var))
      {myFeatures |= XRDOSS_HASRPXY;
       reProxy = true;
       Config.GetWord(); // Force echo
       return 0;
      }

   // Copy the variable name as this may change because it points to an
   // internal buffer in Config. The vagaries of effeciency. Then get value.
   //
   strlcpy(myVar, var, sizeof(myVar)); var = myVar;
   if (!(val = Config.GetWord()))
      {eDest.Emsg("Config", "no value for directive", var);
       return 1;
      }

   // Match directives that take a single argument
   //
   TS_String("localroot",  LocalRoot);

   // No match found, complain.
   //
   eDest.Say("Config warning: ignoring unknown directive '",var,"'.");
   Config.Echo();
   return 0;
}
  
/******************************************************************************/
/*                                 x c o n f                                  */
/******************************************************************************/

/* Function: xconf

   Purpose:  To parse the directive: config <keyword> <value>

             <keyword> is one of the following:
             streams   number of i/o   streams
             workers   number of queue workers

   Output: 0 upon success or 1 upon failure.
*/

int XrdPssSys::xconf(XrdSysError *Eroute, XrdOucStream &Config)
{
   char  *val, *kvp;
   int    kval;
   struct Xtab {const char *Key; int *Val;} Xopts[] =
               {{"streams", &Streams},
                {"workers", &Workers}};
   int i, numopts = sizeof(Xopts)/sizeof(struct Xtab);

   if (!(val = Config.GetWord()))
      {Eroute->Emsg("Config", "options argument not specified."); return 1;}

do{for (i = 0; i < numopts; i++) if (!strcmp(Xopts[i].Key, val)) break;

   if (i >= numopts)
      Eroute->Say("Config warning: ignoring unknown config option '",val,"'.");
      else {if (!(val = Config.GetWord()))
               {Eroute->Emsg("Config","config",Xopts[i].Key,"value not specified.");
                return 1;
               }

            kval = strtol(val, &kvp, 10);
            if (*kvp || !kval)
               {Eroute->Emsg("Config", Xopts[i].Key, 
                             "config value is invalid -", val);
                return 1;
               }
            *(Xopts[i].Val) = kval;
           }
   val = Config.GetWord();
  } while(val && *val);

   return 0;
}

/******************************************************************************/
/*                                  x d c a                                   */
/******************************************************************************/

/* Function: xdca

   Purpose:  To parse the directive: dca [group|world] [recheck {<tm> | off}]

             <tm>      recheck for applicability every <tm> interval
             world     When specified, files are made world deadable.
                       Otherwise, they are only made group readable.

   Output: 0 upon success or 1 upon failure.
*/

int XrdPssSys::xdca(XrdSysError *errp, XrdOucStream &Config)
{
    static const int maxsz = 0x7fffffff;
    char *val;

// Preset the defaults
//
   dcaCheck = true;
   dcaCTime = 0;
   dcaWorld = false;

// If no options then we are done
//
   while((val = Config.GetWord()))
        {     if (!strcmp(val, "world")) dcaWorld = true;
         else if (!strcmp(val, "group")) dcaWorld = false;
         else if (!strcmp(val, "recheck"))
                 {if (!strcmp(val, "off")) dcaCTime = 0;
                     else {if (!(val = Config.GetWord()))
                              {errp->Emsg("Config",
                                          "dca recheck value not specified");
                               return 1;
                              }
                           if (XrdOuca2x::a2tm(*errp,"dca recheck",val,
                                               &dcaCTime,10,maxsz)) return 1;
                          }
                 }
         else {errp->Emsg("Config","invalid dca option -", val); return 1;}
        }

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                  x d e f                                   */
/******************************************************************************/

/* Function: xdef

   Purpose:  Parse: defaults <default options>
                              
   Notes: See the oss configuration manual for the meaning of each option.
          The actual implementation is defined in XrdOucExport.

   Output: 0 upon success or !0 upon failure.
*/

int XrdPssSys::xdef(XrdSysError *Eroute, XrdOucStream &Config)
{
   DirFlags = XrdOucExport::ParseDefs(Config, *Eroute, DirFlags);
   return 0;
}
  
/******************************************************************************/
/*                                  x e x p                                   */
/******************************************************************************/

/* Function: xrcp

   Purpose:  To parse the directive: {export | path} <path> [<options>]

             <path>    the full path that resides in a remote system.
             <options> a blank separated list of options (see XrdOucExport)

   Output: 0 upon success or !0 upon failure.
*/

int XrdPssSys::xexp(XrdSysError *Eroute, XrdOucStream &Config)
{
   XrdOucPList *pP;

// Parse the arguments
//
   if (!(pP = XrdOucExport::ParsePath(Config, *Eroute, XPList, DirFlags)))
      return 1;

// Check if we are allowing object id's
//
   if (*(pP->Path()) == '*') XrdPosixConfig::setOids(true);
   return 0;
}

/******************************************************************************/
/*                                 x o r i g                                  */
/******************************************************************************/

/* Function: xorig

   Purpose:  Parse: origin {=[<prot>,<prot>,...] [<dest>] | <dest>}

                                                                                 d
   where:    <dest> <host>[+][:<port>|<port>] or a URL of the form
                    <prot>://<dest>[:<port>] where <prot> is one
                    http, https, root, xroot

   Output: 0 upon success or !0 upon failure.
*/

int XrdPssSys::xorig(XrdSysError *errp, XrdOucStream &Config)
{
    XrdOucTList *tp = 0;
    char *val, *colon, *slash, *mval = 0;
    int  i, port = 0;
    bool isURL;

//  We are looking for regular managers. These are our points of contact
//
    if (!(val = Config.GetWord()))
       {errp->Emsg("Config","origin host name not specified"); return 1;}

// Check for outgoing proxy
//
   if (*val == '=')
      {outProxy = true;
       if (*(val+1))
          {std::vector<char *> pVec;
           char *pData = strdup(val+1);
           const char *pName;
           protVec.clear();
           if (!XrdPssUtils::Vectorize(pData, pVec, ','))
              {errp->Emsg("Config", "Malformed forwarding specification");
               free(pData);
               return 1;
              }
           protVec.reserve(pVec.size());
           for (int i = 0; i < (int)pVec.size(); i++)
               {int n = strlen(pVec[i]);
                if (!(pName = XrdPssUtils::valProt(pVec[i], n, 3)))
                   {errp->Emsg("Config","Unsupported forwarding protocol -",pVec[i]);
                    free(pData);
                    return 1;
                   }
                protVec.push_back(pName);
               }
           free(pData);
          }
       if (!(val = Config.GetWord())) return 0;
      }
      else outProxy = false;

// Check if the <dest> is a url, if so, the protocol, must be supported
//
   if ((colon = index(val, ':')) && *(colon+1) == '/' && *(colon+2) == '/')
      {int pnlen;
       protName = XrdPssUtils::valProt(val, pnlen);
       if (!protName)
          {errp->Emsg("Config", "Unsupported origin protocol -", val);
           return 1;
          }
       if (*val == 'x') protName++;
       xrdProxy = (*val == 'r');
       val += pnlen;
       if ((slash = index(val, '/')))
          {if (*(slash+1))
              {errp->Emsg("Config","badly formed origin URL"); return 1;}
           *slash = 0;
          }
       mval = strdup(val);
       isURL = true;
      } else {
       protName = "root://";
       mval = strdup(val);
       isURL = false;
       xrdProxy = true;
      }

// Check if there is a port number. This could be as ':port' or ' port'.
//
    if (!(val = index(mval,':')) && !isURL) val = Config.GetWord();
       else {*val = '\0'; val++;}

// At this point, make sure we actually have a host name
//
   if (!(*mval))
      {errp->Emsg("Config","origin host name not specified"); return 1;}

// Validate the port number
//
    if (val)
       {if (isdigit(*val))
            {if (XrdOuca2x::a2i(*errp,"origin port",val,&port,1,65535))
                port = 0;
            }
            else if (!(port = XrdNetUtils::ServPort(val)))
                    {errp->Emsg("Config", "unable to find tcp service", val);
                     port = 0;
                    }
       } else errp->Emsg("Config","origin port not specified for",mval);

// If port is invalid or missing, fail this
//
    if (!port) {free(mval); return 1;}

// For proxies we need not expand 'host+' spec but need to supress the plus
//
    if ((i = strlen(mval)) > 1 && mval[i-1] == '+') mval[i-1] = 0;

// We used to support multiple destinations in the URL but the new client
// does not support this. So, we only provide a single destination here. The
// original code is left commented out just in case we actually revert to this.
//
// tp = ManList;
// while(tp && (strcmp(tp->text, mval) || tp->val != port)) tp = tp->next;
// if (tp) errp->Emsg("Config","Duplicate origin",mval);
//    else ManList = new XrdOucTList(mval, port, ManList);

   if (ManList) delete ManList;
   ManList = new XrdOucTList(mval, port);

// We now set the default dirlist flag based on whether the origin is in or out
// of domain. Composite listings are normally disabled for out of domain nodes.
//
   if (!index(mval, '.')
   || !strcmp(XrdPssUtils::getDomain(mval), XrdPssUtils::getDomain(myHost)))
      XrdPosixConfig::SetEnv("DirlistDflt", 1);

// All done
//
   free(mval);
   return tp != 0;
}
  
/******************************************************************************/
/*                                 x p e r m                                  */
/******************************************************************************/

/* Function: xperm

   Purpose:  To parse the directive: permit [/] [*] <name>

                    netgroup name the host must be a member of. For DNS names,
                    A single asterisk may be specified anywhere in the name.

   Output: 0 upon success or !0 upon failure.
*/

int XrdPssSys::xperm(XrdSysError *Eroute, XrdOucStream &Config)
{   char *val;
    bool pType[PolNum] = {false, false};
    int i;

do {if (!(val = Config.GetWord()))
       {Eroute->Emsg("Config", "permit target not specified"); return 1;}
         if (!strcmp(val, "/")) pType[PolPath] = true;
    else if (!strcmp(val, "*")) pType[PolObj ] = true;
    else break;
   } while(1);

    if (!pType[PolPath] && !pType[PolObj])
       pType[PolPath] = pType[PolObj] = true;

    for (i = 0; i < PolNum; i++)
        {if (pType[i])
            {if (!Police[i]){Police[i] = new XrdNetSecurity();}
                             Police[i]->AddHost(val);
            }
        }

    return 0;
}
  
/******************************************************************************/
/*                                 x p e r s                                  */
/******************************************************************************/

/* Function: xpers

   Purpose:  To parse the directive: persona {client | server} [options]

   options:  [[non]strict] [[no]verify]

                    client    proxy client's identity via sss authentication
                    server    use server's identity at end point
                    strict    all requests must use the client persona
                    nonstrict certain requests can use a server persona
                    noverify  do not verify endpoint
                    verify    verify endpoint

   Output: 0 upon success or !0 upon failure.
*/

int XrdPssSys::xpers(XrdSysError *Eroute, XrdOucStream &Config)
{   char *val;
    bool isClient = false, strict = false;
    int doVer = -1;

// Make sure a parameter was specified
//
   if (!(val = Config.GetWord()))
      {Eroute->Emsg("Config", "persona not specified"); return 1;}

// Check for persona
//
        if (!strcmp(val, "client")) isClient = true;
   else if (!strcmp(val, "server")) isClient = false;
   else {Eroute->Emsg("Config", "Invalid persona - ", val); return 1;}

// Process the subsequent options
//
   while ((val = Config.GetWord()))
         {     if (!strcmp(val, "strict"     )) strict = true;
          else if (!strcmp(val, "nonstrict"  )) strict = false;
          else if (!strcmp(val, "verify"     )) doVer  = 1;
          else if (!strcmp(val, "noverify"   )) doVer  = 0;
          else {Eroute->Emsg("Config", "Invalid persona option - ", val);
                return 1;
               }
      }

// Resolve options vs persona
//
   if (isClient)
      {idMapAll = (strict ? true : false);
       if (doVer < 0) doVer = 1;
      }

// Now record the information for future processin
//
   if (isClient) sssMap = (doVer ? XrdSecsssID::idMappedM
                                 : XrdSecsssID::idMapped);
      else       sssMap = (doVer ? XrdSecsssID::idStaticM
                                 : XrdSecsssID::idStatic);

// All done
//
    return 0;
}
