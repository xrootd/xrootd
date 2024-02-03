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

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClURL.hh"

#include "XrdOuc/XrdOucCache.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPsx.hh"
#include "XrdOuc/XrdOucTList.hh"

#include "XrdPosix/XrdPosixCache.hh"
#include "XrdPosix/XrdPosixConfig.hh"
#include "XrdPosix/XrdPosixFileRH.hh"
#include "XrdPosix/XrdPosixInfo.hh"
#include "XrdPosix/XrdPosixMap.hh"
#include "XrdPosix/XrdPosixPrepIO.hh"
#include "XrdPosix/XrdPosixStats.hh"
#include "XrdPosix/XrdPosixTrace.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdPosix/XrdPosixXrootdPath.hh"

#include "XrdRmc/XrdRmc.hh"

#include "XrdSecsss/XrdSecsssCon.hh"

#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTrace.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdPosixGlobals
{
extern XrdScheduler              *schedP;
extern XrdOucCache               *theCache;
extern XrdOucName2Name           *theN2N;
extern XrdSysLogger              *theLogger;
extern XrdSysError               *eDest;
extern XrdPosixStats              Stats;
extern XrdSysTrace                Trace;
extern int                        ddInterval;
extern int                        ddMaxTries;
extern XrdCl::DirListFlags::Flags dlFlag;
extern bool                       oidsOK;
extern bool                       p2lSRC;
extern bool                       p2lSGI;
extern bool                       autoPGRD;
extern bool                       usingEC;
};
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
class ConCleanup : public XrdSecsssCon
{
public:

using XrdSecsssCon::Contact;

void Cleanup(const std::set<std::string> &Contacts, const XrdSecEntity &Entity)
{
   std::set<std::string>::iterator it;

   for (it = Contacts.begin(); it != Contacts.end(); it++)
       {if (Blab) DMSG("Cleanup", "Disconnecting " <<(*it).c_str());
        PostMaster->ForceDisconnect(XrdCl::URL(*it));}
}

      ConCleanup(XrdCl::PostMaster *pm, bool dbg) : PostMaster(pm), Blab(dbg) {}
     ~ConCleanup() {}

private:

XrdCl::PostMaster *PostMaster;
bool               Blab;
};

class ConTrack : public XrdCl::Job
{
public:

void Run( void *ptr )
        {XrdCl::URL *url = reinterpret_cast<XrdCl::URL*>( ptr );
         const std::string &user = url->GetUserName();
         const std::string  host = url->GetHostId();
         if (Blab) DMSG("Tracker", "Connecting to " <<host);
         if (user.size()) sssCon.Contact(user, host);
         delete url;
        }

      ConTrack(ConCleanup &cm, bool dbg) : sssCon(cm), Blab(dbg) {}
     ~ConTrack() {}

private:

XrdSecsssCon &sssCon;
bool          Blab;
};
}
  
/******************************************************************************/
/*                            C o n T r a c k e r                             */
/******************************************************************************/

XrdSecsssCon *XrdPosixConfig::conTracker(bool dbg)
{
   XrdCl::PostMaster *pm =  XrdCl::DefaultEnv::GetPostMaster();
   ConCleanup *cuHandler = new ConCleanup(pm, dbg);
   std::unique_ptr<ConTrack> ctHandler(new ConTrack(*cuHandler, dbg));

// Set the callback for new connections
//
   pm->SetOnConnectHandler( std::move( ctHandler ) );

// Return the connection cleanup handler. Note that we split the task into
// two objects so that we don't violate the semantics of unique pointer.
//
   return cuHandler;
}

/******************************************************************************/
/*                               E n v I n f o                                */
/******************************************************************************/
  
void XrdPosixConfig::EnvInfo(XrdOucEnv &theEnv)
{

// Extract the pointer to the scheduler from the passed environment
//
   XrdPosixGlobals::schedP = (XrdScheduler *)theEnv.GetPtr("XrdScheduler*");

// We no longer propogate the environment to the new-style cache via this
// method as it picks it up during init time. We leave the code for historical
// reasons but we really should have gotten rid of EnvInfo()!
// if (XrdPosixGlobals::myCache2) XrdPosixGlobals::myCache2->EnvInfo(theEnv);

// Test if XRDCL_EC is set. That env var. is set at XrdCl::PlugInManager::LoadFactory
// in XrdClPlugInManager.cc, which is called (by XrdOssGetSS while loading 
// libXrdPss.so) before this function. 
   XrdPosixGlobals::usingEC = getenv("XRDCL_EC")? true : false;
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
   static XrdRmc dramCache;
   XrdRmc::Parms myParms;
   XrdOucEnv theEnv(eData);
   XrdOucCacheIO::aprParms apParms;
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
      {if (*tP == 's') myParms.Options |= XrdRmc::isServer;
          else if (*tP != 'c') DMSG("initEnv","'XRDPOSIX_CACHE=mode=" <<tP
                                    <<"' is invalid.");
      }

// Get the structured file option
//
   if ((tP = theEnv.Get("optsf")) && *tP && *tP != '0')
      {     if (*tP == '1') myParms.Options |= XrdRmc::isStructured;
       else if (*tP == '.') {XrdPosixFile::sfSFX = strdup(tP);
                             XrdPosixFile::sfSLN = strlen(tP);
                            }
       else DMSG("initEnv", "'XRDPOSIX_CACHE=optfs=" <<tP
                            <<"' is invalid.");
      }

// Get final options, any non-zero value will do here
//
   if ((tP = theEnv.Get("optlg")) && *tP && *tP != '0')
      myParms.Options |= XrdRmc::logStats;
   if ((tP = theEnv.Get("optpr")) && *tP && *tP != '0')
      myParms.Options |= XrdRmc::canPreRead;
// if ((tP = theEnv.Get("optwr")) && *tP && *tP != '0') isRW = 1;

// Now allocate a cache. Indicate that we already serialize the I/O to avoid
// additional but unnecessary locking.
//
   myParms.Options |= XrdRmc::Serialized;
   if (!(XrdPosixGlobals::theCache = dramCache.Create(myParms, &apParms)))
      {DMSG("initEnv", XrdSysE2T(errno) <<" creating cache.");}
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
/*                              i n i t S t a t                               */
/******************************************************************************/

void XrdPosixConfig::initStat(struct stat *buf)
{
   static int initStat = 0;
   static dev_t st_rdev;
   static dev_t st_dev;
   static uid_t myUID = getuid();
   static gid_t myGID = getgid();

// Initialize the xdev fields. This cannot be done in the constructor because
// we may not yet have resolved the C-library symbols.
//
   if (!initStat) {initStat = 1; initXdev(st_dev, st_rdev);}
   memset(buf, 0, sizeof(struct stat));

// Preset common fields
//
   buf->st_blksize= 64*1024;
   buf->st_dev    = st_dev;
   buf->st_rdev   = st_rdev;
   buf->st_nlink  = 1;
   buf->st_uid    = myUID;
   buf->st_gid    = myGID;
}
  
/******************************************************************************/
/*                              i n i t X d e v                               */
/******************************************************************************/
  
void XrdPosixConfig::initXdev(dev_t &st_dev, dev_t &st_rdev)
{
   static dev_t tDev, trDev;
   static bool aOK = false;
   struct stat buf;

// Get the device id for /tmp used by stat()
//
   if (aOK) {st_dev = tDev; st_rdev = trDev;}
      else if (stat("/tmp", &buf)) {st_dev = 0; st_rdev = 0;}
              else {st_dev  = tDev  = buf.st_dev;
                    st_rdev = trDev = buf.st_rdev;
                    aOK = true;
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
   const char *val;

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
   if (parms.xPfn2Lfn)
      {XrdPosixGlobals::theN2N = parms.theN2N;
       if (parms.xPfn2Lfn == parms.xP2Lsrc || parms.xPfn2Lfn == parms.xP2Lsgi)
          {XrdPosixGlobals::p2lSRC = true;
           XrdPosixGlobals::p2lSGI = parms.xPfn2Lfn == parms.xP2Lsgi;
          } else XrdPosixGlobals::p2lSRC = XrdPosixGlobals::p2lSGI = false;
      }

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

// Set auto conversion of read to pgread
//
   if (parms.theCache && parms.theEnv && (val = parms.theEnv->Get("psx.CSNet")))
      {if (*val == '1' || *val == '2')
          {XrdPosixGlobals::autoPGRD = true;
           if (*val == '2') SetEnv("WantTlsOnNoPgrw", 1);
          }
      }

// Handle the caching options (library or builin memory).
// TODO: Make the memory cache a library plugin as well.
//
        if (parms.theCache)
           {XrdPosixGlobals::theCache = parms.theCache;
            if (parms.initCCM) return initCCM(parms);
            return true;
           }
   else if (parms.mCache && *parms.mCache) initEnv(parms.mCache);

   return true;
}

/******************************************************************************/
/* Private:                     S e t D e b u g                               */
/******************************************************************************/

void XrdPosixConfig::SetDebug(int val)
{
   const std::string dbgType[] = {"Error", "Warning", "Info", "Debug", "Dump"};

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

/******************************************************************************/
/*                            S t a t s C a c h e                             */
/******************************************************************************/
  
int XrdPosixConfig::Stats(const char *theID, char *buff, int blen)
{
   static const char stats1[] = "<stats id=\"%s\">"
          "<open>%lld<errs>%lld</errs></open>"
          "<close>%lld<errs>%lld</errs></close>"
          "</stats>";

   static const char stats2[] = "<stats id=\"cache\" type=\"%s\">"
          "<prerd><in>%lld</in><hits>%lld</hits><miss>%lld</miss></prerd>"
          "<rd><in>%lld</in><out>%lld</out>"
              "<hits>%lld></hits><miss>%lld</miss>"
          "</rd>"
          "<pass>%lld<cnt>%lld</cnt></pass>"
          "<wr><out>%lld</out><updt>%lld</updt></wr>"
          "<saved>%lld</saved><purge>%lld</purge>"
          "<files><opened>%lld</opened><closed>%lld</closed><new>%lld</new>"
                  "<del>%lld</del><now>%lld</now><full>%lld</full>"
          "</files>"
          "<store><size>%lld</size><used>%lld</used>"
                  "<min>%lld</min><max>%lld</max>"
          "</store>"
          "<mem><size>%lld</size><used>%lld</used><wq>%lld</wq></mem>"
          "<opcl><odefer>%lld</odefer><defero>%lld</defero>"
                "<cdefer>%lld</cdefer><clost>%lld</clost>"
          "</opcl>"
          "</stats>";

// If the caller want the maximum length, then provide it.
//
   if (!blen)
      {size_t n;
       int len1, digitsLL = strlen("9223372036854775807");
       std::string fmt = stats1;
       n = std::count(fmt.begin(), fmt.end(), '%');
       len1 = fmt.size() + (digitsLL*n) - (n*3) + strlen(theID);
       if (!XrdPosixGlobals::theCache) return len1;
       fmt = stats2;
       n = std::count(fmt.begin(), fmt.end(), '%');
       return len1 + fmt.size() + (digitsLL*n) - (n*3) + 8;
      }

// Get the standard statistics
//
   XrdPosixStats Y;
   XrdPosixGlobals::Stats.Get(Y);

// Format the line
//
   int k = snprintf(buff, blen, stats1, theID,
                    Y.X.Opens, Y.X.OpenErrs, Y.X.Closes, Y.X.CloseErrs);

// If there is no cache then there nothing to return
//
   if (!XrdPosixGlobals::theCache) return k;
   buff += k; blen -= k;

// Get the statistics
//
   XrdOucCacheStats Z;
   XrdPosixGlobals::theCache->Statistics.Get(Z);

// Format the statisics into the supplied buffer
//
   int n = snprintf(buff, blen, stats2, XrdPosixGlobals::theCache->CacheType,
                    Z.X.BytesPead,   Z.X.HitsPR,       Z.X.MissPR,
                    Z.X.BytesRead,   Z.X.BytesGet,     Z.X.Hits, Z.X.Miss,
                    Z.X.BytesPass,   Z.X.Pass,
                    Z.X.BytesWrite,  Z.X.BytesPut,
                    Z.X.BytesSaved,  Z.X.BytesPurged,
                    Z.X.FilesOpened, Z.X.FilesClosed,  Z.X.FilesCreated,
                    Z.X.FilesPurged, Z.X.FilesInCache, Z.X.FilesAreFull,
                    Z.X.DiskSize,    Z.X.DiskUsed,
                    Z.X.DiskMin,     Z.X.DiskMax,
                    Z.X.MemSize,     Z.X.MemUsed,      Z.X.MemWriteQ,
                    Z.X.OpenDefers,  Z.X.DeferOpens,
                    Z.X.ClosDefers,  Z.X.ClosedLost
                   );

// Return the right value
//
   return (n < blen ? n+k : 0);
}
