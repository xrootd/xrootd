/******************************************************************************/
/*                                                                            */
/*                     X r d P o s i x C o n f i g . c c                      */
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

#include <errno.h>
#include <iostream>
#include <stdio.h>

#include "XrdCl/XrdClDefaultEnv.hh"

#include "XrdOuc/XrdOucCache2.hh"
#include "XrdOuc/XrdOucCacheDram.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPsx.hh"
#include "XrdOuc/XrdOucTList.hh"

#include "XrdPosix/XrdPosixCache.hh"
#include "XrdPosix/XrdPosixCacheBC.hh"
#include "XrdPosix/XrdPosixConfig.hh"
#include "XrdPosix/XrdPosixFileRH.hh"
#include "XrdPosix/XrdPosixInfo.hh"
#include "XrdPosix/XrdPosixMap.hh"
#include "XrdPosix/XrdPosixPrepIO.hh"
#include "XrdPosix/XrdPosixTrace.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTrace.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern XrdScheduler              *schedP;
extern XrdOucCache2              *theCache;
extern XrdOucCache               *myCache;
extern XrdOucCache2              *myCache2;
extern XrdOucName2Name           *theN2N;
extern XrdCl::DirListFlags::Flags dlFlag;
extern XrdSysLogger              *theLogger;
extern XrdSysError               *eDest;
extern XrdSysTrace                Trace;
extern int                        ddInterval;
extern int                        ddMaxTries;
extern bool                       oidsOK;
};
  
/******************************************************************************/
/*                               E n v I n f o                                */
/******************************************************************************/
  
void XrdPosixConfig::EnvInfo(XrdOucEnv &theEnv)
{

// Extract the pointer to the scheduler from the passed environment
//
   XrdPosixGlobals::schedP = (XrdScheduler *)theEnv.GetPtr("XrdScheduler*");

// If we have a new-style cache, propogate the environment to it
//
   if (XrdPosixGlobals::myCache2) XrdPosixGlobals::myCache2->EnvInfo(theEnv);
}

/******************************************************************************/
/* Private:                      i n i t C C M                                */
/******************************************************************************/

bool XrdPosixConfig::initCCM(XrdOucPsx &parms)
{
   static XrdPosixCache pCache;
   const char *eTxt = "Unable to initialize cache context manager in";
   const char *mPath;

// Initialize the cache context manager
//
   if ((*parms.initCCM)(pCache, parms.theLogger, parms.configFN,
                        parms.CCMInfo(mPath), parms.theEnv)) return true;

// Issue error message and return failure
//
   if (parms.theLogger)
      {XrdSysError eDest(parms.theLogger, "Posix");
       eDest.Emsg("InitCCM", eTxt, mPath);
      } else {
       std::cerr <<"Posix_InitCCM: " <<eTxt <<' ' <<mPath <<std::endl;
      }
    return false;
}

/******************************************************************************/
/* Private:                      i n i t E n v                                */
/******************************************************************************/

// Parse options specified as a cgi string (i.e. var=val&var=val&...). Vars:

// aprcalc=n   - bytes at which to recalculate preread performance
// aprminp     - auto preread min read pages
// aprperf     - auto preread performance
// aprtrig=n   - auto preread min read length   (can be suffized in k, m, g).
// cachesz=n   - the size of the cache in bytes (can be suffized in k, m, g).
// debug=n     - debug level (0 off, 1 low, 2 medium, 3 high).
// max2cache=n - maximum read to cache          (can be suffized in k, m, g).
// maxfiles=n  - maximum number of files to support.
// minp=n      - minimum number of pages needed.
// mode={c|s}  - running as a client (default) or server.
// optlg=1     - log statistics
// optpr=1     - enable pre-reads
// optsf=<val> - optimize structured file: 1 = all, 0 = off, .<sfx> specific
// optwr=1     - cache can be written to.
// pagesz=n    - individual byte size of a page (can be suffized in k, m, g).
//

void XrdPosixConfig::initEnv(char *eData)
{
   static XrdOucCacheDram dramCache;
   XrdOucEnv theEnv(eData);
   XrdOucCache::Parms myParms;
   XrdOucCacheIO::aprParms apParms;
   XrdOucCache *v1Cache;
   long long Val;
   char * tP;

// Get numeric type variable (errors force a default)
//
   initEnv(theEnv, "aprcalc",   Val); if (Val >= 0) apParms.prRecalc  = Val;
   initEnv(theEnv, "aprminp",   Val); if (Val >= 0) apParms.minPages  = Val;
   initEnv(theEnv, "aprperf",   Val); if (Val >= 0) apParms.minPerf   = Val;
   initEnv(theEnv, "aprtrig",   Val); if (Val >= 0) apParms.Trigger   = Val;
   initEnv(theEnv, "cachesz",   Val); if (Val >= 0) myParms.CacheSize = Val;
   initEnv(theEnv, "maxfiles",  Val); if (Val >= 0) myParms.MaxFiles  = Val;
   initEnv(theEnv, "max2cache", Val); if (Val >= 0) myParms.Max2Cache = Val;
   initEnv(theEnv, "minpages",  Val); if (Val >= 0)
                                         {if (Val > 32767) Val = 32767;
                                          myParms.minPages = Val;
                                         }
   initEnv(theEnv, "pagesz",    Val); if (Val >= 0) myParms.PageSize  = Val;

// Get Debug setting
//
   if ((tP = theEnv.Get("debug")))
      {if (*tP >= '0' && *tP <= '3') myParms.Options |= (*tP - '0');
          else DMSG("initEnv", "'XRDPOSIX_CACHE=debug=" <<tP <<"' is invalid.");
      }

// Get Mode
//
   if ((tP = theEnv.Get("mode")))
      {if (*tP == 's') myParms.Options |= XrdOucCache::isServer;
          else if (*tP != 'c') DMSG("initEnv","'XRDPOSIX_CACHE=mode=" <<tP
                                    <<"' is invalid.");
      }

// Get the structured file option
//
   if ((tP = theEnv.Get("optsf")) && *tP && *tP != '0')
      {     if (*tP == '1') myParms.Options |= XrdOucCache::isStructured;
       else if (*tP == '.') {XrdPosixFile::sfSFX = strdup(tP);
                             XrdPosixFile::sfSLN = strlen(tP);
                            }
       else DMSG("initEnv", "'XRDPOSIX_CACHE=optfs=" <<tP
                            <<"' is invalid.");
      }

// Get final options, any non-zero value will do here
//
   if ((tP = theEnv.Get("optlg")) && *tP && *tP != '0')
      myParms.Options |= XrdOucCache::logStats;
   if ((tP = theEnv.Get("optpr")) && *tP && *tP != '0')
      myParms.Options |= XrdOucCache::canPreRead;
// if ((tP = theEnv.Get("optwr")) && *tP && *tP != '0') isRW = 1;

// Use the default cache if one was not provided
//
   if (!XrdPosixGlobals::myCache) XrdPosixGlobals::myCache = &dramCache;

// Now allocate a cache. Indicate that we already serialize the I/O to avoid
// additional but unnecessary locking.
//
   myParms.Options |= XrdOucCache::Serialized;
   if (!(v1Cache = XrdPosixGlobals::myCache->Create(myParms, &apParms)))
      {DMSG("initEnv", strerror(errno) <<" creating cache.");}
      else XrdPosixGlobals::theCache = new XrdPosixCacheBC(v1Cache);
}

/******************************************************************************/

void XrdPosixConfig::initEnv(XrdOucEnv &theEnv, const char *vName, long long &Dest)
{
   char *eP, *tP;

// Extract variable
//
   Dest = -1;
   if (!(tP = theEnv.Get(vName)) || !(*tP)) return;

// Convert the value
//
   errno = 0;
   Dest = strtoll(tP, &eP, 10);
   if (Dest > 0 || (!errno && tP != eP))
      {if (!(*eP)) return;
            if (*eP == 'k' || *eP == 'K') Dest *= 1024LL;
       else if (*eP == 'm' || *eP == 'M') Dest *= 1024LL*1024LL;
       else if (*eP == 'g' || *eP == 'G') Dest *= 1024LL*1024LL*1024LL;
       else if (*eP == 't' || *eP == 'T') Dest *= 1024LL*1024LL*1024LL*1024LL;
       else eP--;
       if (*(eP+1))
          {DMSG("initEnv", "'XRDPOSIX_CACHE=" <<vName <<'=' <<tP
                           <<"' is invalid.");
           Dest = -1;
          }
      }
}
  
/******************************************************************************/
/*                                O p e n F C                                 */
/******************************************************************************/

bool XrdPosixConfig::OpenFC(const char *path, int oflag, mode_t mode,
                            XrdPosixInfo &Info)
{
   int rc = XrdPosixXrootd::Open(path, oflag, mode, Info.cbP, &Info);

// Check if we actually can open the file directly via the cache
//
   if (rc == -3)
      {if (*Info.cachePath && errno == 0 && Info.ffReady) return true;
       rc = -1;
       if (!errno) errno = ENOPROTOOPT;
      }

// Return actual result
//
   Info.fileFD = rc;
   return false;
}
  
/******************************************************************************/
/*                             S e t C o n f i g                              */
/******************************************************************************/
  
bool XrdPosixConfig::SetConfig(XrdOucPsx &parms)
{
   XrdOucTList *tP;

// Set log routing
//
   XrdPosixGlobals::Trace.SetLogger(parms.theLogger);
   XrdPosixGlobals::theLogger = parms.theLogger;

// Create an error object if we have a logger
//
   if (parms.theLogger)
      XrdPosixGlobals::eDest = new XrdSysError(parms.theLogger, "Posix");

// Set networking mode
//
   SetIPV4(parms.useV4);

// Handle the Name2Name for pfn2lfn translations.
//
   if (parms.xPfn2Lfn) XrdPosixGlobals::theN2N = parms.theN2N;

// Handle client settings
//
   if ((tP = parms.setFirst))
      do {SetEnv(tP->text, tP->val);
          tP = tP->next;
         } while(tP);

// Handle debug and trace settings
//
   if (parms.traceLvl || parms.debugLvl)
      {if (parms.debugLvl) SetDebug(parms.debugLvl);
          else             SetDebug(parms.traceLvl);
       if (parms.traceLvl) XrdPosixGlobals::Trace.What = TRACE_Debug;
      }

// Handle number of response handlers we should keep
//
   if (parms.maxRHCB > 0) XrdPosixFileRH::SetMax(parms.maxRHCB);

// Set delayed destro parameters if present
//
   if (parms.cioWait > 0 && parms.cioTries > 0)
      {XrdPosixGlobals::ddMaxTries = (parms.cioTries <  2 ?  2 : parms.cioTries);
       XrdPosixGlobals::ddInterval = (parms.cioWait  < 10 ? 10 : parms.cioWait);
      }

// Handle the caching options
//
        if (parms.theCache2)
           {XrdPosixGlobals::myCache2 = parms.theCache2;
            XrdPosixGlobals::theCache = parms.theCache2;
            if (parms.initCCM) return initCCM(parms);
            return true;
           }
   else if (parms.theCache)
           {char ebuf[] = {0};
            XrdPosixGlobals::myCache  = parms.theCache;
            initEnv(ebuf);
           }
   else if (parms.mCache && *parms.mCache) initEnv(parms.mCache);

   return true;
}

/******************************************************************************/
/* Private:                     S e t D e b u g                               */
/******************************************************************************/

void XrdPosixConfig::SetDebug(int val)
{
   const std::string dbgType[] = {"Info", "Warning", "Error", "Debug", "Dump"};

// The default is none but once set it cannot be unset in the client
//
   if (val > 0)
      {if (val > 5) val = 5;
       XrdCl::DefaultEnv::SetLogLevel(dbgType[val-1]);
      }

// Now set the internal one which can be toggled
//
   XrdPosixMap::SetDebug(val > 0);
}
  
/******************************************************************************/
/*                                S e t E n v                                 */
/******************************************************************************/

void XrdPosixConfig::SetEnv(const char *kword, int kval)
{
   XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
   static bool dlfSet = false;

// Check for internal envars before setting the external one
//
        if (!strcmp(kword, "DirlistAll"))
           {XrdPosixGlobals::dlFlag = (kval ? XrdCl::DirListFlags::Locate
                                            : XrdCl::DirListFlags::None);
            dlfSet = true;
           }
   else if (!strcmp(kword, "DirlistDflt"))
           {if (!dlfSet)
            XrdPosixGlobals::dlFlag = (kval ? XrdCl::DirListFlags::Locate
                                            : XrdCl::DirListFlags::None);
           }
   else env->PutInt((std::string)kword, kval);
}
  
/******************************************************************************/
/* Private:                      S e t I P V 4                                */
/******************************************************************************/

void XrdPosixConfig::SetIPV4(bool usev4)
{
   const char *ipmode = (usev4 ? "IPv4" : "IPAll");
   XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

// Set the env value
//
   env->PutString((std::string)"NetworkStack", (const std::string)ipmode);
}

/******************************************************************************/
/*                               s e t O i d s                                */
/******************************************************************************/

void XrdPosixConfig::setOids(bool isok)
{
    XrdPosixGlobals::oidsOK = isok;
}
