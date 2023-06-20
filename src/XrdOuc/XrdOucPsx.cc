/******************************************************************************/
/*                                                                            */
/*                          X r d O u c P s x . c c                           */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucPsx.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define Duplicate(x,y) if (y) free(y); y = strdup(x)

#define TS_String(x,m) if (!strcmp(x,var)) {Duplicate(val,m); return 0;}

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(&eDest, Config);

/*******x**********************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace
{
XrdSysLogger *logP = 0;
bool          warn = false;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucPsx::~XrdOucPsx()
{
   XrdOucTList *tP;

   if (mCache)    free(mCache);
   if (LocalRoot) free(LocalRoot);
   if (RemotRoot) free(RemotRoot);
   if (N2NLib)    free(N2NLib);
   if (N2NParms)  free(N2NParms);
   if (cPath)     free(cPath);
   if (cParm)     free(cParm);
   if (mPath)     free(mPath);
   if (mParm)     free(mParm);
   if (configFN)  free(configFN);

   while((tP = setFirst)) {setFirst = tP->next; delete tP;}
}

/******************************************************************************/
/*                          C l i e n t C o n f i g                           */
/******************************************************************************/
  
bool XrdOucPsx::ClientConfig(const char *pfx, bool hush)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   XrdOucEnv myEnv;
   const char *theIname = "*client anon@localhost";
   XrdOucTListFIFO tFifo;
   char *var;
   int  cfgFD, retc, pfxlen = strlen(pfx);
   bool aOK = true;

// Export the instance name as this is used in all sort of places
//
   XrdOucEnv::Export("XRDINSTANCE", theIname);

// We must establish a stable logger for client-side configs. The error and
// stream objects can be temporary.
//
   logP = new XrdSysLogger;
   XrdSysError  eDest(logP, "psx");
   XrdOucStream Config(&eDest, theIname, &myEnv, "=====> ");

// Try to open the configuration file.
//
   if ((cfgFD = open(configFN, O_RDONLY, 0)) < 0)
      {eDest.Emsg("Config", errno, "open config file", configFN);
       return false;
      }
   Config.Attach(cfgFD);

// Capture all lines going to stderr if so wanted
//
   if (hush) logP->Capture(&tFifo);

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, pfx, pfxlen) && !Parse(var+pfxlen, Config, eDest))
            {Config.Echo(); aOK = false;}
        }

// Check if we should blither about any warnings or errors
//
   if (hush)
      {logP->Capture(0);
       if ((!aOK || warn) && tFifo.first) WarnConfig(eDest, tFifo.first, !aOK);
       tFifo.Clear();
      }

// Now check if any errors occurred during file i/o
//
   if ((retc = Config.LastError()))
      {eDest.Emsg("Config", retc, "read config file", configFN); aOK = false;}
   Config.Close();


// If all went well, materialize the configuration and return result
//
   if (aOK) return ConfigSetup(eDest, hush);
   return false;
}
  
/******************************************************************************/
/* Private:                  C o n f i g C a c h e                            */
/******************************************************************************/

bool XrdOucPsx::ConfigCache(XrdSysError &eDest)
{
   XrdOucPinLoader  myLib(&eDest,myVersion,"cachelib",cPath);

// Get the cache Object now
//
   XrdOucCache_t ep = (XrdOucCache_t)myLib.Resolve("XrdOucGetCache");
   if (!ep) return false;
   theCache = (XrdOucCache*)ep(eDest.logger(), configFN, cParm, theEnv);
   return theCache != 0;
}
  
/******************************************************************************/
/*                             C o n f i g N 2 N                              */
/******************************************************************************/

bool XrdOucPsx::ConfigN2N(XrdSysError &eDest)
{  
   XrdOucN2NLoader n2nLoader(&eDest, configFN, N2NParms, LocalRoot, RemotRoot);

// Skip all of this we are not doing name mapping.
//
  if (!N2NLib && !LocalRoot)
     {xLfn2Pfn = false;
      xPfn2Lfn = xP2Loff;
      return true;
     }

// Check if the n2n is applicable
//
   if (xPfn2Lfn && !(mCache || cPath) && N2NLib)
      {const char *txt = (xLfn2Pfn ? "-lfncache option" : "directive");
       eDest.Say("Config warning: ignoring namelib ", txt,
                 "; caching not in effect!");
       if (!xLfn2Pfn) return true;
      }

// Get the plugin
//
   return (theN2N = n2nLoader.Load(N2NLib, *myVersion)) != 0;
}

/******************************************************************************/
/*                           C o n f i g S e t u p                            */
/******************************************************************************/

bool XrdOucPsx::ConfigSetup(XrdSysError &eDest, bool hush)
{
   XrdOucTListFIFO tFifo;
   bool aOK = true;

// Handle hush option for client-side configs
//
   if (hush) eDest.logger()->Capture(&tFifo);

// Initialize an alternate cache if one is present and load a CCM if need be
//
   if (cPath && !ConfigCache(eDest))
      {aOK = false;
       if (hush)
          {eDest.logger()->Capture(0);
           WarnPlugin(eDest, tFifo.first, "cachelib", cPath);
           tFifo.Clear();
           eDest.logger()->Capture(&tFifo);
          }
      } else {
       if (mPath && theCache && !LoadCCM(eDest))
          {aOK = false;
           if (hush)
              {eDest.logger()->Capture(0);
               WarnPlugin(eDest, tFifo.first, "ccmlib", mPath);
               tFifo.Clear();
               eDest.logger()->Capture(&tFifo);
              }
          }
      }

// Configure the N2N library:
//
   if (!ConfigN2N(eDest))
      {aOK = false;
       if (hush)
          {eDest.logger()->Capture(0);
           if (N2NLib) WarnPlugin(eDest,tFifo.first,"namelib",N2NLib);
              else     WarnPlugin(eDest,tFifo.first,"name2name for",LocalRoot);
           tFifo.Clear();
          }
      }

// All done
//
   if (hush) eDest.logger()->Capture(0);
   return aOK;
}

/******************************************************************************/
/* Private:                      L o a d C C M                                */
/******************************************************************************/

bool XrdOucPsx::LoadCCM(XrdSysError &eDest)
{
   XrdOucPinLoader  myLib(&eDest,myVersion,"ccmlib",mPath);

// Resolve the context manager entry point
//
   initCCM = (XrdOucCacheCMInit_t)myLib.Resolve("XrdOucCacheCMInit");
   return initCCM != 0;
}
  
/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/

bool XrdOucPsx::Parse(char *var, XrdOucStream &Config, XrdSysError &eDest)
{

   // Process items. for either a local or a remote configuration
   //
   TS_Xeq("memcache",      ParseCache);  // Backward compatibility
   TS_Xeq("cache",         ParseCache);
   TS_Xeq("cachelib",      ParseCLib);
   TS_Xeq("ccmlib",        ParseMLib);
   TS_Xeq("ciosync",       ParseCio);
   TS_Xeq("inetmode",      ParseINet);
   TS_Xeq("namelib",       ParseNLib);
   TS_Xeq("setopt",        ParseSet);
   TS_Xeq("trace",         ParseTrace);

   // No match found, complain.
   //
   eDest.Say("Config warning: ignoring unknown directive '",var,"'.");
   warn = true;
   Config.Echo();
   return true;
}
  
/******************************************************************************/
/*                            P a r s e C a c h e                             */
/******************************************************************************/

/* Function: ParseCache

   Purpose:  To parse the directive: cache <keyword> <value> [...]

             <keyword> is one of the following:
             debug     {0 | 1 | 2}
             logstats  enables stats logging
             max2cache largest read to cache   (can be suffixed with k, m, g).
             minpages  smallest number of pages allowed (default 256)
             mode      {r | w}
             pagesize  size of each cache page (can be suffixed with k, m, g).
             preread   [minpages [minrdsz]] [perf nn [recalc]]
             r/w       enables caching for files opened read/write.
             sfiles    {on | off | .<sfx>}
             size      size of cache in bytes  (can be suffixed with k, m, g).

   Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseCache(XrdSysError *Eroute, XrdOucStream &Config)
{
   long long llVal, cSize=-1, m2Cache=-1, pSize=-1, minPg = -1;
   const char *ivN = 0;
   char  *val, *sfSfx = 0, sfVal = '0', lgVal = '0', dbVal = '0', rwVal = '0';
   char eBuff[2048], pBuff[1024], *eP;
   struct sztab {const char *Key; long long *Val;} szopts[] =
               {{"max2cache", &m2Cache},
                {"minpages",  &minPg},
                {"pagesize",  &pSize},
                {"size",      &cSize}
               };
   int i, numopts = sizeof(szopts)/sizeof(struct sztab);

// Delete any cache parameters we may have
//
   if (mCache) {free(mCache); mCache = 0;}

// If we have no parameters, then we just use the defaults
//
   if (!(val = Config.GetWord()))
      {mCache = strdup("mode=s&optwr=0"); return true;}
   *pBuff = 0;

do{for (i = 0; i < numopts; i++) if (!strcmp(szopts[i].Key, val)) break;

   if (i < numopts)
      {if (!(val = Config.GetWord())) ivN = szopts[i].Key;
          else if (XrdOuca2x::a2sz(*Eroute,szopts[i].Key,val,&llVal,0))
                  return false;
                  else *(szopts[i].Val) = llVal;
      } else {
            if (!strcmp("debug", val))
               {if (!(val = Config.GetWord())
                || ((*val < '0' || *val > '3') && !*(val+1))) ivN = "debug";
                   else dbVal = *val;
               }
       else if (!strcmp("logstats", val)) lgVal = '1';
       else if (!strcmp("preread", val))
               {if ((val = ParseCache(Eroute, Config, pBuff))) continue;
                if (*pBuff == '?') return false;
                break;
               }
       else if (!strcmp("r/w", val)) rwVal = '1';
       else if (!strcmp("sfiles", val))
               {if (sfSfx) {free(sfSfx); sfSfx = 0;}
                     if (!(val = Config.GetWord())) ivN = "sfiles";
                else if (!strcmp("on",  val)) sfVal = '1';
                else if (!strcmp("off", val)) sfVal = '0';
                else if (*val == '.' && strlen(val) < 16) sfSfx = strdup(val);
                else ivN = "sfiles";
               }
       else {Eroute->Emsg("Config","invalid cache keyword -", val);
             return false;
            }
     }

   if (ivN)
      {if (!val) Eroute->Emsg("Config","cache", ivN,"value not specified.");
          else   Eroute->Emsg("Config", val, "is invalid for cache", ivN);
       return false;
      }
  } while ((val = Config.GetWord()));

// Construct the envar string
//
   strcpy(eBuff, "mode=s&maxfiles=16384"); eP = eBuff + strlen(eBuff);
   if (cSize > 0)    eP += sprintf(eP, "&cachesz=%lld", cSize);
   if (dbVal != '0') eP += sprintf(eP, "&debug=%c", dbVal);
   if (m2Cache > 0)  eP += sprintf(eP, "&max2cache=%lld", m2Cache);
   if (minPg > 0)
      {if (minPg > 32767) minPg = 32767;
       eP += sprintf(eP, "&minpages=%lld", minPg);
      }
   if (pSize > 0)    eP += sprintf(eP, "&pagesz=%lld", pSize);
   if (lgVal != '0') strcat(eP, "&optlg=1");
   if (sfVal != '0' || sfSfx)
      {if (!sfSfx)   strcat(eP, "&optsf=1");
          else {strcat(eP, "&optsf="); strcat(eBuff, sfSfx); free(sfSfx);}
      }
   if (rwVal != '0') strcat(eP, "&optwr=1");
   if (*pBuff)       strcat(eP, pBuff);

   mCache = strdup(eBuff);
   return true;
}

/******************************************************************************/

/* Parse the directive: preread [pages [minrd]] [perf pct [calc]]

             pages     minimum number of pages to preread.
             minrd     minimum size   of read  (can be suffixed with k, m, g).
             perf      preread performance (0 to 100).
             calc      calc perf every n bytes (can be suffixed with k, m, g).
*/

char *XrdOucPsx::ParseCache(XrdSysError *Eroute, XrdOucStream &Config, char *pBuff)
{
   long long minr = 0, maxv = 0x7fffffff, recb = 50*1024*1024;
   int minp = 1, perf = 90, Spec = 0;
   char *val;

// Check for our options
//
   *pBuff = '?';
   if ((val = Config.GetWord()) && isdigit(*val))
      {if (XrdOuca2x::a2i(*Eroute,"preread pages",val,&minp,0,32767)) return 0;
       if ((val = Config.GetWord()) && isdigit(*val))
          {if (XrdOuca2x::a2sz(*Eroute,"preread rdsz",val,&minr,0,maxv))
              return 0;
           val = Config.GetWord();
          }
       Spec = 1;
      }
   if (val && !strcmp("perf", val))
      {if (!(val = Config.GetWord()))
          {Eroute->Emsg("Config","cache", "preread perf value not specified.");
           return 0;
          }
       if (XrdOuca2x::a2i(*Eroute,"perf",val,&perf,0,100)) return 0;
       if ((val = Config.GetWord()) && isdigit(*val))
          {if (XrdOuca2x::a2sz(*Eroute,"perf recalc",val,&recb,0,maxv))
              return 0;
           val = Config.GetWord();
          }
       Spec = 1;
      }

// Construct new string
//
   if (!Spec) strcpy(pBuff,"&optpr=1&aprminp=1");
      else sprintf(pBuff,  "&optpr=1&aprtrig=%lld&aprminp=%d&aprcalc=%lld"
                           "&aprperf=%d",minr,minp,recb,perf);
   return val;
}
  
/******************************************************************************/
/*                              P a r s e C i o                               */
/******************************************************************************/

/* Function: ParseCio

   Purpose:  To parse the directive: ciosync <tsec> <tries>

             <tsec>    the number of seconds between each sync attempt.
             <tries>   the maximum number of tries before giving up.

  Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseCio(XrdSysError *Eroute, XrdOucStream &Config)
{
   char *val;
   int tsec, mtry;

// Get the try seconds
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute->Emsg("Config", "ciosync parameter not specified"); return false;}

// Convert to seconds
//
   if (XrdOuca2x::a2i(*Eroute,"ciosync interval",val,&tsec,10)) return false;

// Get the max seconds
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute->Emsg("Config", "max time not specified"); return false;}

// Convert to seconds
//
   if (XrdOuca2x::a2i(*Eroute,"ciosync max time",val,&mtry,2)) return false;

// Set values and return success
//
   cioWait  = tsec;
   cioTries = mtry;
   return true;
}
  
/******************************************************************************/
/*                             P a r s e C L i b                              */
/******************************************************************************/

/* Function: ParseCLib

   Purpose:  To parse the directive: cachelib {<path>|default} [<parms>]

             <path>    the path of the cache library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseCLib(XrdSysError *Eroute, XrdOucStream &Config)
{
    char *val, parms[2048];

// Get the path and parms
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute->Emsg("Config", "cachelib not specified"); return false;}

// Save the path
//
   if (cPath) free(cPath);
   if (!strcmp(val,"libXrdFileCache.so") || !strcmp(val,"libXrdFileCache-4.so"))
      {Eroute->Say("Config warning: 'libXrdFileCache' has been replaced by "
            "'libXrdPfc'; for future compatibility specify 'default' instead!");
       cPath = strdup("libXrdPfc.so");
      } else {
       cPath = (strcmp(val,"default") ? strdup(val) : strdup("libXrdPfc.so"));
      }

// Get the parameters
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute->Emsg("Config", "cachelib parameters too long"); return false;}
   if (cParm) free(cParm);
   cParm = (*parms ? strdup(parms) : 0);

// All done
//
   return true;
}
  
/******************************************************************************/
/*                             P a r s e M L i b                              */
/******************************************************************************/

/* Function: ParseCLib

   Purpose:  To parse the directive: ccmlib <path> [<parms>]

             <path>    the path of the cache context mgmt library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseMLib(XrdSysError *Eroute, XrdOucStream &Config)
{
    char *val, parms[2048];

// Get the path and parms
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute->Emsg("Config", "ccmlib not specified"); return false;}

// Save the path
//
   if (mPath) free(mPath);
   mPath = strdup(val);

// Get the parameters
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute->Emsg("Config", "ccmlib parameters too long"); return false;}
   if (mParm) free(mParm);
   mParm = (*parms ? strdup(parms) : 0);

// All done
//
   return true;
}
  
/******************************************************************************/
/*                             P a r s e I N e t                              */
/******************************************************************************/

/* Function: ParseINet

   Purpose:  To parse the directive: inetmode v4 | v6

             v4        use only IPV4 addresses to connect to servers.
             v6        use IPV4 mapped addresses or IPV6 addresses, as needed.

  Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseINet(XrdSysError *Eroute, XrdOucStream &Config)
{
   char *val;

// Get the mode
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute->Emsg("Config", "inetmode value not specified"); return false;}

// Validate the value
//
        if (!strcmp(val, "v4")) useV4 = true;
   else if (!strcmp(val, "v6")) useV4 = false;
   else {Eroute->Emsg("Config", "invalid inetmode value -", val); return false;}

// All done
//
   return true;
}
  
/******************************************************************************/
/*                             P a r s e N L i b                              */
/******************************************************************************/

/* Function: ParseNLib

   Purpose:  To parse the directive: namelib [<opts>] pfn<path> [<parms>]

             <opts>    one or more: [-lfn2pfn] [-lfncache[src[+]]]
             <path>    the path of the filesystem library to be used.
             <parms>   optional parms to be passed

  Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseNLib(XrdSysError *Eroute, XrdOucStream &Config)
{
    char *val, parms[1024];
    bool l2p = false, p2l = false, p2lsrc = false, p2lsgi = false;

// Parse options, if any
//
   while((val = Config.GetWord()) && val[0])
        {     if (!strcmp(val, "-lfn2pfn"))      l2p = true;
         else if (!strcmp(val, "-lfncache"))     p2l = true;
         else if (!strcmp(val, "-lfncachesrc"))  p2l = p2lsrc = true;
         else if (!strcmp(val, "-lfncachesrc+")) p2l = p2lsgi = true;
         else break;
        }

   if (!l2p && !p2l) l2p = true;
   xLfn2Pfn = l2p;
         if (!p2l)   xPfn2Lfn = xP2Loff;
    else if (p2lsrc) xPfn2Lfn = xP2Lsrc;
    else if (p2lsgi) xPfn2Lfn = xP2Lsgi;
    else             xPfn2Lfn = xP2Lon;

// Get the path
//
   if (!val || !val[0])
      {Eroute->Emsg("Config", "namelib not specified"); return false;}
   xNameLib = true;

// Record the path
//
   if (N2NLib) free(N2NLib);
   N2NLib = strdup(val);

// Record any parms
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute->Emsg("Config", "namelib parameters too long"); return false;}
   if (N2NParms) free(N2NParms);
   N2NParms = (*parms ? strdup(parms) : 0);
   return true;
}

/******************************************************************************/
/*                              P a r s e S e t                               */
/******************************************************************************/

/* Function: ParseSet

   Purpose:  To parse the directive: setopt <keyword> <value>

             <keyword> is an XrdClient option keyword.
             <value>   is the value the option is to have.

   Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseSet(XrdSysError *Eroute, XrdOucStream &Config)
{
    char  kword[256], *val;
    int   kval, noGo;
    static struct sopts {const char *Sopt; const char *Copt; int isT;} Sopts[] =
       {
         {"ConnectTimeout",        "ConnectionWindow",1},    // Default  120
         {"ConnectionRetry",       "ConnectionRetry",1},     // Default    5
         {"DataServerTTL",         "DataServerTTL",1},       // Default  300
         {"DataServerConn_ttl",    "DataServerTTL",1},       // Default  300
         {"DebugLevel",            "*",0},                   // Default   -1
         {"DebugMask",             "*",0},                   // Default   -1
         {"DirlistAll",            "DirlistAll",0},
         {"DataServerTTL",         "DataServerTTL",1},       // Default  300
         {"LBServerConn_ttl",      "LoadBalancerTTL",1},     // Default 1200
         {"LoadBalancerTTL",       "LoadBalancerTTL",1},     // Default 1200
         {"ParallelEvtLoop",       "ParallelEvtLoop",0},     // Default   10
         {"ParStreamsPerPhyConn",  "SubStreamsPerChannel",0},// Default    1
         {"ReadAheadSize",         0,0},
         {"ReadAheadStrategy",     0,0},
         {"ReadCacheBlkRemPolicy", 0,0},
         {"ReadCacheSize",         0,0},
         {"ReadTrimBlockSize",     0,0},
         {"ReconnectWait",         "StreamErrorWindow",1},   // Default 1800
         {"RedirCntTimeout",       "!use RedirectLimit instead.",0},
         {"RedirectLimit",         "RedirectLimit",0},       // Default   16
         {"RedirectorConn_ttl",    "LoadBalancerTTL",1},     // Default 1200
         {"RemoveUsedCacheBlocks", 0,0},
         {"RequestTimeout",        "RequestTimeout",1},      // Default 1800
         {"StreamTimeout",         "StreamTimeout",1},
         {"TransactionTimeout",    "",1},
         {"WorkerThreads",         "WorkerThreads",0}        // Set To    64
       };
    int i, numopts = sizeof(Sopts)/sizeof(struct sopts);

    if (!(val = Config.GetWord()))
       {Eroute->Emsg("Config", "setopt keyword not specified"); return false;}
    strlcpy(kword, val, sizeof(kword));
    if (!(val = Config.GetWord()))
       {Eroute->Emsg("Config", "setopt", kword, "value not specified");
        return false;
       }

    for (i = 0; i < numopts; i++)
        if (!strcmp(Sopts[i].Sopt, kword))
           {if (!Sopts[i].Copt || *(Sopts[i].Copt) == '!')
               {Eroute->Emsg("Config", kword, "no longer supported;",
                             (Sopts[i].Copt ? Sopts[i].Copt+1 : "ignored"));
               } else if (*(Sopts[i].Copt))
                         {noGo = (Sopts[i].isT
                               ?  XrdOuca2x::a2tm(*Eroute,kword,val,&kval)
                               :  XrdOuca2x::a2i (*Eroute,kword,val,&kval));
                          if (noGo) return false;
                          if (*(Sopts[i].Copt) == '*') debugLvl = kval;
                             else ParseSet(Sopts[i].Copt, kval);
                         }
            return true;
           }

    Eroute->Say("Config warning: ignoring unknown setopt '",kword,"'.");
    warn = true;
    return true;
}
  
/******************************************************************************/

void XrdOucPsx::ParseSet(const char *kword, int kval)
{
   XrdOucTList *item = new XrdOucTList(kword, kval);

   if (setLast) item->next = setLast;
      else      setFirst   = item;
   setLast = item;
}

/******************************************************************************/
/*                            P a r s e T r a c e                             */
/******************************************************************************/

/* Function: ParseTrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: true upon success or false upon failure.
*/

bool XrdOucPsx::ParseTrace(XrdSysError *Eroute, XrdOucStream &Config)
{
    char  *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      3},
        {"debug",    2},
        {"on",       1}
       };
    int i, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Eroute->Emsg("Config", "trace option not specified"); return false;}
    while (val)
         {if (!strcmp(val, "off")) trval = 0;
             else {for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, tropts[i].opname))
                           {trval |=  tropts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      {Eroute->Say("Config warning: ignoring invalid trace option '",val,"'.");
                       warn = true;
                      }
                  }
          val = Config.GetWord();
         }
    traceLvl = trval;
    return true;
}

/******************************************************************************/
/*                               S e t R o o t                                */
/******************************************************************************/
  
void XrdOucPsx::SetRoot(const char *lroot, const char *rroot)
{
// Handle the local root (posix dependent)
//
   if (LocalRoot) free(LocalRoot);
   if (!lroot) LocalRoot = 0;
      {LocalRoot = strdup(lroot);
       xLfn2Pfn = true;
      }

// Handle the oss local root
//
   if (RemotRoot) free(RemotRoot);
   RemotRoot = (rroot ? strdup(rroot) : 0);
}
  
/******************************************************************************/
/* Private:                   W a r n C o n f i g                             */
/******************************************************************************/
  
void XrdOucPsx::WarnConfig(XrdSysError &eDest, XrdOucTList *tList, bool fatal)
{

// Indicate we have a problem
//
   eDest.Say("\n--------------");

   eDest.Say("Config problem: ",(fatal ? "fatal ":0),"errors in config file '",
             configFN, "'; details below.\n");

// Now dump the whole thing
//
   while(tList)
        {eDest.Say(tList->text);
         tList = tList->next;
        }
   eDest.Say("--------------\n");
}

/******************************************************************************/
/* Private:                   W a r n P l u g i n                             */
/******************************************************************************/
  
void XrdOucPsx::WarnPlugin(XrdSysError &eDest, XrdOucTList *tList,
                           const char  *txt1,  const char *txt2)
{

// Indicate we have a problem
//
   eDest.Say("\n--------------");

   eDest.Say("Config problem: unable to load ", txt1, " ", txt2,
             "'; details below.\n");

// Now dump the whole thing
//
   while(tList)
        {eDest.Say(tList->text);
         tList = tList->next;
        }
   eDest.Say("--------------\n");
}
