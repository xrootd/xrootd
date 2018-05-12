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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "XrdVersion.hh"

#include "XrdFfs/XrdFfsDent.hh"
#include "XrdFfs/XrdFfsMisc.hh"
#include "XrdFfs/XrdFfsQueue.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdNet/XrdNetSecurity.hh"

#include "XrdPss/XrdPss.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucCache2.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucPsx.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucCache2.hh"

#include "XrdPosix/XrdPosixConfig.hh"
#include "XrdPosix/XrdPosixXrootd.hh"

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define Duplicate(x,y) if (y) free(y); y = strdup(x)

#define TS_String(x,m) if (!strcmp(x,var)) {Duplicate(val,m); return 0;}

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(&eDest, Config);

#define TS_PSX(x,m)    if (!strcmp(x,var)) \
                          return (psxConfig->m(&eDest, Config) ? 0 : 1);

/*******x**********************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

const char  *XrdPssSys::ConfigFN;       // -> Pointer to the config file name
const char  *XrdPssSys::myHost;
const char  *XrdPssSys::myName;
uid_t        XrdPssSys::myUid     =  geteuid();
gid_t        XrdPssSys::myGid     =  getegid();

XrdOucPListAnchor XrdPssSys::XPList;

XrdNetSecurity   *XrdPssSys::Police[XrdPssSys::PolNum] = {0, 0};

XrdOucTList *XrdPssSys::ManList   =  0;
const char  *XrdPssSys::urlPlain  =  0;
int          XrdPssSys::urlPlen   =  0;
int          XrdPssSys::hdrLen    =  0;
const char  *XrdPssSys::hdrData   =  0;
const char  *XrdPssSys::urlRdr    =  0;
int          XrdPssSys::Streams   =512;
int          XrdPssSys::Workers   = 16;

char         XrdPssSys::allChmod  =  0;
char         XrdPssSys::allMkdir  =  0;
char         XrdPssSys::allMv     =  0;
char         XrdPssSys::allRm     =  0;
char         XrdPssSys::allRmdir  =  0;
char         XrdPssSys::allTrunc  =  0;

bool         XrdPssSys::outProxy  = false;
bool         XrdPssSys::pfxProxy  = false;
bool         XrdPssSys::xLfn2Pfn  = false;

char         XrdPssSys::cfgDone   =  0;

namespace XrdProxy
{
static XrdPosixXrootd  *Xroot;
  
extern XrdSysError      eDest;

extern XrdOucSid       *sidP;

static const int maxHLen = 1024;
}

namespace
{
XrdOucPsx *psxConfig;
}

using namespace XrdProxy;
  
/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/

void *XrdPssConfigFfs(void *carg)
{
   XrdPssSys *myPSS = (XrdPssSys *)carg;

// Initialize the Ffs (we don't use xrd_init() as it messes up the settings
// We also do not initialize secsss as we don't know how to effectively use it.
//
// XrdFfsMisc_xrd_secsss_init();
   XrdFfsMisc_refresh_url_cache(myPSS->urlRdr);
   XrdFfsDent_cache_init();
   XrdFfsQueue_create_workers(myPSS->Workers);

// Tell everyone waiting for this initialization to complete. We use the trick
// that in all systems a caharcter is atomically accessed and a single change
// in value will be detected in a consistent way.
//
   myPSS->cfgDone = 1;
   return (void *)0;
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdPssSys::Configure(const char *cfn)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   struct {const char *Typ; char *Loc;} Fwd[] = {{" ch", &allChmod},
                                                 {" mk", &allMkdir},
                                                 {" mv", &allMv   },
                                                 {" rd", &allRmdir},
                                                 {" rm", &allRm   },
                                                 {" tr", &allTrunc},
                                                 {0,     0        }
                                                };
   const char *xP;
   XrdOucPList *fP;
   pthread_t tid;
   char *eP, theRdr[maxHLen+1024];
   int i, hpLen, NoGo = 0;
   bool haveFwd = false;

// Get environmental values
//
   myHost = getenv("XRDHOST");
   myName = XrdOucUtils::InstName(1);
   ConfigFN = cfn;

// Thell xrootd to disable POSC mode as this is meaningless here
//
   XrdOucEnv::Export("XRDXROOTD_NOPOSC", "1");

// Create a configurator
//
   psxConfig = new XrdOucPsx(myVersion, cfn); // We be deleted later

// Set debug level if so wanted
//
   XrdPosixConfig::setLogger(eDest.logger());
   if (getenv("XRDDEBUG")) psxConfig->traceLvl = 4;

// Set the defaault number of worker threads for the client
//
   XrdPosixConfig::SetEnv("WorkerThreads", 64);

// Set client IP mode based on what the server is set to
//
   if (XrdNetAddr::IPV4Set()) psxConfig->useV4 = true;

// Set default number of event loops
//
   XrdPosixConfig::SetEnv("ParallelEvtLoop", 3);

// Process the configuration file
//
   if ((NoGo = ConfigProc(cfn))) return NoGo;

// Make sure we have some kind of origin
//
   if (!ManList && !outProxy)
      {eDest.Emsg("Config", "Origin for proxy service not specified.");
       return 1;
      }

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

// Finalize the configuration
//
   if (!(psxConfig->ConfigSetup(eDest))) return 1;

// Complete initialization (this cannot fail)
//
   XrdPosixConfig::SetConfig(*psxConfig);

// Save the N2N library pointer if we will be using it
//
   if (psxConfig->xLfn2Pfn) xLfn2Pfn = (theN2N = psxConfig->theN2N) != 0;

// All done with the configurator
//
   delete psxConfig;

// Allocate an Xroot proxy object (only one needed here). Tell it to not
// shadow open files with real file descriptors (we will be honest). This can
// be done before we initialize the ffs.
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

// If this is an outgoing proxy then we are done
//
   if (outProxy)
      {if (!ManList) strcpy(theRdr, "=");
          else sprintf(theRdr, "= %s:%d", ManList->text, ManList->val);
       XrdOucEnv::Export("XRDXROOTD_PROXY", theRdr);
       if (ManList)
          {hdrLen = sprintf(theRdr, "root://%%s%s:%d/%%s%%s%%s",
                            ManList->text, ManList->val);
           hdrData = strdup(theRdr);
          }
       return 0;
      }

// Build the URL header
//
   if (!(hpLen = buildHdr())) return 1;

// Create a plain url for future use
//
   urlPlen = sprintf(theRdr, hdrData, "", "", "", "", "", "", "", "");
   urlPlain= strdup(theRdr);

// Export the origin
//
   theRdr[urlPlen-1] = 0;
   XrdOucEnv::Export("XRDXROOTD_PROXY", theRdr+hpLen);
   theRdr[urlPlen-1] = '/';

// Copy out the forwarding that might be happening via the ofs
//
   i = 0;
   if ((eP = getenv("XRDOFS_FWD")))
      while(Fwd[i].Typ)
           {if (!strstr(eP, Fwd[i].Typ)) {*(Fwd[i].Loc) = 1; haveFwd = true;}
            i++;
           }

// We would really like that the Ffs interface use the generic method of
// keeping track of data servers. It does not and it even can't handle more
// than one export (really). But it does mean we need to give it a valid one.
//
   if (!(eP = getenv("XRDEXPORTS")) || *eP != '/') xP = "/tmp";
      else if ((xP = rindex(eP, ' '))) xP++;
              else xP = eP;

// Setup the redirection url
//
   strcpy(&theRdr[urlPlen], xP); urlRdr = strdup(theRdr);

// Check if we have any r/w exports as this will determine whether or not we
// need to initialize the ffs (we'd rather ot if at all possible).
//
   fP = XPList.First();
   while(fP && !(fP->Flag() & XRDEXP_NOTRW)) fP = fP->Next();

// We avoid initializing the ffs if we have no r/w paths or there is no need to
// forward r/w metadata operations (e.g. rm, rmdir, etc).
//
   if (!fP || !haveFwd) return 0;

// Now spwan a thread to complete ffs initialization which may hang for a while
//
   if (XrdSysThread::Run(&tid, XrdPssConfigFfs, (void *)this, 0, "Ffs Config"))
      {eDest.Emsg("Config", errno, "start ffs configurator"); return 1;}

// All done
//
   return 0;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                              b u i l d H d r                               */
/******************************************************************************/
  
int XrdPssSys::buildHdr()
{
   XrdOucTList *tp = ManList;
   char buff[maxHLen], *pb;
   int n, hpLen, bleft = sizeof(buff);

// Fill in start of header
//
   strcpy(buff, "root://"); hpLen = strlen(buff);
   pb = buff+hpLen; bleft -= hpLen;

// The redirector list must fit into 1K bytes (along with header)
//
   while(tp)
        {n = snprintf(pb, bleft, "%%s%s:%d%c", tp->text, tp->val,
                                              (tp->next ? ',':'/'));
         if (n >= bleft) break;
         pb += n; bleft -= n;
         tp = tp->next;
        }

   if (tp)
      {eDest.Emsg("Config", "Too many proxy service managers specified.");
       return 0;
      }

   hdrData = strdup(buff);
   hdrLen  = strlen(buff);
   return hpLen;
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

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, "pss.", 4)
         ||  !strcmp(var, "oss.defaults")
         ||  !strcmp(var, "all.export"))
            if (ConfigXeq(var+4, Config)) {Config.Echo(); NoGo = 1;}
        }

// Now check if any errors occured during file i/o
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
   TS_PSX("ciosync",       ParseCio);
   TS_Xeq("config",        xconf);
   TS_Xeq("defaults",      xdef);
   TS_Xeq("export",        xexp);
   TS_PSX("inetmode",      ParseINet);
   TS_Xeq("origin",        xorig);
   TS_Xeq("permit",        xperm);
   TS_PSX("setopt",        ParseSet);
   TS_PSX("trace",         ParseTrace);

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
/*                             g e t D o m a i n                              */
/******************************************************************************/

const char *XrdPssSys::getDomain(const char *hName)
{
   const char *dot = index(hName, '.');

   if (dot) return dot+1;
   return hName;
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

   Purpose:  Parse: origin {= [<dest>] | <dest>}

   Where:    <dest> <host>[+][:<port>|<port>]

   Output: 0 upon success or !0 upon failure.
*/

int XrdPssSys::xorig(XrdSysError *errp, XrdOucStream &Config)
{
    XrdOucTList *tp = 0;
    char *val, *mval = 0;
    int  i, port = 0;

//  We are looking for regular managers. These are our points of contact
//
    if (!(val = Config.GetWord()))
       {errp->Emsg("Config","origin host name not specified"); return 1;}

// Check for outgoing proxy
//
   if (!strcmp(val, "="))
      {pfxProxy = outProxy = true;
       if (!(val = Config.GetWord())) return 0;
      }
      else pfxProxy = outProxy = false;
   mval = strdup(val);

// Check if there is a port number. This could be as ':port' or ' port'.
//
    if (!(val = index(mval,':'))) val = Config.GetWord();
       else {*val = '\0'; val++;}

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
   if (!index(mval, '.') || !strcmp(getDomain(mval), getDomain(myHost)))
      XrdPosixXrootd::setEnv("DirlistDflt", 1);

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
