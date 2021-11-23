/******************************************************************************/
/*                                                                            */
/*                       X r d O s s C o n f i g . c c                        */
/*                                                                            */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <unistd.h>
#include <cctype>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <strings.h>
#include <cstdio>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdVersion.hh"

#include "XrdFrc/XrdFrcProxy.hh"
#include "XrdOss/XrdOssPath.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssCache.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssMio.hh"
#include "XrdOss/XrdOssOpaque.hh"
#include "XrdOss/XrdOssSpace.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucMsubs.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOssSys   *XrdOssSS;

extern XrdSysTrace  OssTrace;

XrdOucPListAnchor  *XrdOssRPList;

/******************************************************************************/
/*                            E r r o r   T e x t                             */
/******************************************************************************/
  
const char *XrdOssErrorText[] =
      {XRDOSS_T8001,
       XRDOSS_T8002,
       XRDOSS_T8003,
       XRDOSS_T8004,
       XRDOSS_T8005,
       XRDOSS_T8006,
       XRDOSS_T8007,
       XRDOSS_T8008,
       XRDOSS_T8009,
       XRDOSS_T8010,
       XRDOSS_T8011,
       XRDOSS_T8012,
       XRDOSS_T8013,
       XRDOSS_T8014,
       XRDOSS_T8015,
       XRDOSS_T8016,
       XRDOSS_T8017,
       XRDOSS_T8018,
       XRDOSS_T8019,
       XRDOSS_T8020,
       XRDOSS_T8021,
       XRDOSS_T8022,
       XRDOSS_T8023,
       XRDOSS_T8024,
       XRDOSS_T8025,
       XRDOSS_T8026
      };

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define Duplicate(x,y) if (y) free(y); y = strdup(x)

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(Config, Eroute);

#define TS_String(x,m) if (!strcmp(x,var)) {Duplicate(val,m); return 0;}

#define TS_List(x,m,v) if (!strcmp(x,var)) \
                          {m.Insert(new XrdOucPList(val, v); return 0;}

#define TS_Char(x,m)   if (!strcmp(x,var)) {m = val[0]; return 0;}

#define TS_Add(x,m,v,s) if (!strcmp(x,var)) {m |= (v|s); return 0;}
#define TS_Ade(x,m,v,s) if (!strcmp(x,var)) {m |= (v|s); Config.Echo(); return 0;}
#define TS_Rem(x,m,v,s) if (!strcmp(x,var)) {m = (m & ~v) | s; return 0;}

#define TS_Set(x,m,v)  if (!strcmp(x,var)) {m = v; Config.Echo(); return 0;}

#define xrdmax(a,b)       (a < b ? b : a)

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOssxfr(void *carg)       {return XrdOssSS->Stage_In(carg);}

void *XrdOssCacheScan(void *carg) {return XrdOssCache::Scan(*((int *)carg));}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOssSys::XrdOssSys()
{
   static XrdVERSIONINFODEF(myVer, XrdOss, XrdVNUMBER, XrdVERSION);
   myVersion     = &myVer;
   xfrtcount     = 0;
   pndbytes      = 0;
   stgbytes      = 0;
   totbytes      = 0;
   totreqs       = 0;
   badreqs       = 0;
   MaxTwiddle    = 3;
   tryMmap       = 0;
   chkMmap       = 0;
   lcl_N2N = rmt_N2N = the_N2N = 0; 
   N2N_Lib = N2N_Parms         = 0;
   StageCmd      = 0;
   StageMsg      = 0; 
   StageSnd      = 0;
   StageFrm      = 0;
   StageRealTime = 1;
   StageAsync    = 0;
   StageCreate   = 0;
   StageEvents   = (char *)"-";
   StageEvSize   = 1;
   StageAction   = (char *)"wq "; 
   StageActLen   = 3;
   RSSCmd        = 0;
   isMSSC        = 0;
   RSSTout       =15*1000;
   DirFlags      = 0; 
   OptFlags      = 0;
   LocalRoot     = 0;
   RemoteRoot    = 0;
   cscanint      = 600;
   FDFence       = -1;
   FDLimit       = -1;
   MaxSize       = 0;
   minalloc      = 0;
   ovhalloc      = 0;
   fuzalloc      = 0;
   xfrspeed      = 9*1024*1024;
   xfrovhd       = 30;
   xfrhold       =  3*60*60;
   xfrkeep       = 20*60;
   xfrthreads    = 1;
   ConfigFN      = 0;
   QFile         = 0;
   UDir          = 0;
   USync         = 0;
   Solitary      = 0;
   DPList        = 0;
   lenDP         = 0;
   numCG = numDP = 0;
   xfrFdir       = 0;
   xfrFdln       = 0;
   pfcMode       = false;
   RSSProg       = 0;
   StageProg     = 0;
   prPBits       = (long long)sysconf(_SC_PAGESIZE);
   prPSize       = static_cast<int>(prPBits);
   prPBits--;
   prPMask       = ~prPBits;
   prBytes       = 0;
   prActive      = 0;
   prDepth       = 0;
   prQSize       = 0;
   STT_Lib       = 0;
   STT_Parms     = 0;
   STT_Func      = 0;
   STT_Fund      = 0;
   STT_PreOp     = 0;
   STT_DoN2N     = 1;
   STT_V2        = 0;
   STT_DoARE     = 0;
}
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOssSys::Configure(const char *configfn, XrdSysError &Eroute,
                                               XrdOucEnv   *envP)
{
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   XrdSysError_Table *ETab = new XrdSysError_Table(XRDOSS_EBASE, XRDOSS_ELAST,
                                                   XrdOssErrorText);
   static const int maxFD = 1048576;
   struct rlimit rlim;
   char *val;
   int  retc, NoGo = XrdOssOK;
   pthread_t tid;
   bool setfd = false;

// Do the herald thing
//
   Eroute.Say("++++++ Storage system initialization started.");
   Eroute.addTable(ETab);
   if (getenv("XRDDEBUG")) OssTrace.What = TRACE_ALL;

// Preset all variables with common defaults
//
   ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);

// Establish the FD limit and the fence (half way)
//
  if (getrlimit(RLIMIT_NOFILE, &rlim))
     {Eroute.Emsg("Config", errno, "get fd limit");
      rlim.rlim_cur = maxFD;
     }
     else {if (rlim.rlim_max == RLIM_INFINITY)
              {rlim.rlim_cur = maxFD;
               setfd = true;
              } else {
               if (rlim.rlim_cur != rlim.rlim_max)
                  {rlim.rlim_cur  = rlim.rlim_max;
                   setfd = true;
                  }
              }
           if (setfd)
              {if (setrlimit(RLIMIT_NOFILE, &rlim))
                  Eroute.Emsg("Config", errno, "set fd limit");
                  else FDLimit = rlim.rlim_cur;
              } else {FDFence = static_cast<int>(rlim.rlim_cur)>>1;
                      FDLimit = rlim.rlim_cur;
                     }
          }
   if (FDFence < 0 || FDFence >= FDLimit) FDFence = FDLimit >> 1;

// Configure devices
//
   XrdOssCache::MapDevs(OssTrace.What != 0);

// Process the configuration file
//
   NoGo = ConfigProc(Eroute);

// Configure dependent plugins
//
   if (!NoGo)
      {if (N2N_Lib || LocalRoot || RemoteRoot) NoGo |= ConfigN2N(Eroute, envP);
       if (STT_Lib && !NoGo) NoGo |= ConfigStatLib(Eroute, envP);
      }

// If the export list is empty, add at least "/tmp" to it otherwise we will
// fail to correctly track space.
//
   if (RPList.First() == 0)
      RPList.Insert(new XrdOucPList("/tmp", (unsigned long long)0));

// Establish usage tracking and quotas, if need be. Note that if we are not
// a true data server, those services will be initialized but then disabled.
//
   Solitary = ((val = getenv("XRDREDIRECT")) && !strcmp(val, "Q"));
   pfcMode  = (envP && (val = envP->Get("oss.runmode")) && !strcmp(val,"pfc"));
  {const char *m1 = (Solitary ? "standalone " : 0);
   const char *m2 = (pfcMode  ? "pfc " : 0);
   if (m1 || m2) Eroute.Say("++++++ Configuring ", m1, m2, "mode . . .");
  }
   NoGo |= XrdOssCache::Init(UDir, QFile, Solitary, USync)
          |XrdOssCache::Init(minalloc, ovhalloc, fuzalloc);

// Configure the MSS interface including staging
//
   if (!NoGo) NoGo = ConfigStage(Eroute);

// Configure async I/O
//
   if (!NoGo) NoGo = !AioInit();

// Initialize memory mapping setting to speed execution
//
   if (!NoGo) ConfigMio(Eroute);

// Provide support for the PFC. This also resolve cache attribute conflicts.
//
   if (!NoGo) ConfigCache(Eroute);

// Establish the actual default path settings (modified by the above)
//
   RPList.Set(DirFlags);

// Configure space (final pass)
//
   ConfigSpace(Eroute);

// Set the prefix for files in cache file systems  
   if ( OptFlags & XrdOss_CacheFS ) 
       if (!NoGo) {
           NoGo = XrdOssPath::InitPrefix();
           if (NoGo) Eroute.Emsg("Config", "space initialization failed");
       }

// Configure statiscal reporting
//
   if (!NoGo) ConfigStats(Eroute);

// Start up the space scan thread unless specifically told not to. Some programs
// like the cmsd manually handle space updates.
//
   if (!(val = getenv("XRDOSSCSCAN")) || strcmp(val, "off"))
      {if ((retc = XrdSysThread::Run(&tid, XrdOssCacheScan,
                                    (void *)&cscanint, 0, "space scan")))
          Eroute.Emsg("Config", retc, "create space scan thread");
      }

// Display the final config if we can continue
//
   if (!NoGo) Config_Display(Eroute);

// Do final reset of paths if we are in proxy file cache mode
//
   if (pfcMode && !NoGo) ConfigCache(Eroute, true);

// Export the real path list (for frm et. al.)
//
   XrdOssRPList = &RPList;
   if (envP) envP->PutPtr("XrdOssRPList*", &RPList);

// All done, close the stream and return the return code.
//
   val = (NoGo ? (char *)"failed." : (char *)"completed.");
   Eroute.Say("------ Storage system initialization ", val);
   return NoGo;
}
  
/******************************************************************************/
/*                   o o s s _ C o n f i g _ D i s p l a y                    */
/******************************************************************************/
  
#define XrdOssConfig_Val(base, opt) \
             (Have ## base  ? "       oss." #opt " " : ""), \
             (Have ## base  ? base     : ""), \
             (Have ## base  ? "\n"     : "")
  
#define XrdOssConfig_Vop(base, opt, optchk0, opt1, opt2, optchk1, opt3, opt4) \
             (Have ## base  ? "       oss." #opt " " : ""), \
             (Have ## base  ? (optchk0 ? opt1 : opt2) : ""), \
             (Have ## base  ? (optchk1 ? opt3 : opt4) : ""), \
             (Have ## base  ? base     : ""), \
             (Have ## base  ? "\n"     : "")

void XrdOssSys::Config_Display(XrdSysError &Eroute)
{
     char buff[4096], *cloc;
     XrdOucPList *fp;

     // Preset some tests
     //
     int HaveRSSCmd     = (RSSCmd     && RSSCmd[0]);
     int HaveStageCmd   = (StageCmd   && StageCmd[0]);
     int HaveRemoteRoot = (RemoteRoot && RemoteRoot[0]);
     int HaveLocalRoot  = (LocalRoot  && LocalRoot[0]);
     int HaveStageMsg   = (StageMsg   && StageMsg[0]);
     int HaveN2N_Lib    = (N2N_Lib != 0);

     if (!ConfigFN || !ConfigFN[0]) cloc = (char *)"Default";
        else cloc = ConfigFN;

     snprintf(buff, sizeof(buff), "Config effective %s oss configuration:\n"
                                  "       oss.alloc        %lld %d %d\n"
                                  "       oss.spacescan    %d\n"
                                  "       oss.fdlimit      %d %d\n"
                                  "       oss.maxsize      %lld\n"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "       oss.trace        %x\n"
                                  "       oss.xfr          %d deny %d keep %d",
             cloc,
             minalloc, ovhalloc, fuzalloc,
             cscanint,
             FDFence, FDLimit, MaxSize,
             XrdOssConfig_Val(N2N_Lib,    namelib),
             XrdOssConfig_Val(LocalRoot,  localroot),
             XrdOssConfig_Val(RemoteRoot, remoteroot),
             XrdOssConfig_Vop(StageCmd,   stagecmd, StageAsync,  "async ","sync ",
                                                    StageCreate, "creates ", ""),
             XrdOssConfig_Val(StageMsg,   stagemsg),
             XrdOssConfig_Val(RSSCmd,     rsscmd),
             OssTrace.What,
             xfrthreads, xfrhold, xfrkeep);

     Eroute.Say(buff);

     XrdOssMio::Display(Eroute);

     XrdOssCache::List("       oss.", Eroute);
           List_Path("       oss.defaults ", "", DirFlags, Eroute);
     fp = RPList.First();
     while(fp)
          {List_Path("       oss.path ", fp->Path(), fp->Flag(), Eroute);
           fp = fp->Next();
          }
     fp = SPList.First();
     while(fp)
          {Eroute.Say("       oss.space ", fp->Name(),
                      (fp->Attr() == spAssign ? " assign  " : " default "),
                       fp->Path());
           fp = fp->Next();
          }
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                           C o n f i g C a c h e                            */
/******************************************************************************/
  
void XrdOssSys::ConfigCache(XrdSysError &Eroute, bool pass2)
{
     const unsigned long long conFlags = 
                    XRDEXP_NOCHECK | XRDEXP_NODREAD |
                    XRDEXP_MLOK    | XRDEXP_MKEEP   | XRDEXP_MMAP  |
                    XRDEXP_MIG     | XRDEXP_MWMODE  | XRDEXP_PURGE |
                    XRDEXP_RCREATE | XRDEXP_STAGE   | XRDEXP_STAGEMM;

     XrdOucPList *fp = RPList.First();
     unsigned long long oflag, pflag;

// If this is pass 2 then if we are in pfcMode, then reset r/o flag to r/w
// to allow the pfc to actually write into the cache paths.
//
   if (pass2)
      {if (pfcMode)
          {while(fp)
                {pflag = fp->Flag();
                 if (pflag & XRDEXP_PFCACHE) fp->Set(pflag & ~XRDEXP_NOTRW);
                 fp = fp->Next();
                }
          }
       return;
      }

// Run through all the paths and resolve any conflicts with a cache
//
   while(fp)
        {oflag = pflag = fp->Flag();
         if ((pflag & XRDEXP_PFCACHE)
         ||  (pfcMode && !(pflag & XRDEXP_PFCACHE_X)))
            {if (!(pflag & XRDEXP_NOTRW)) pflag |= XRDEXP_READONLY;
             pflag &= ~conFlags;
             pflag |=  XRDEXP_PFCACHE;
             if (oflag != pflag) fp->Set(pflag);
            }
         fp = fp->Next();
        }

// Handle default settings
//
   if (DirFlags & XRDEXP_PFCACHE)
      {DirFlags |=  XRDEXP_READONLY;
       DirFlags &= ~conFlags;
      }
}
  
/******************************************************************************/
/*                             C o n f i g M i o                              */
/******************************************************************************/
  
void XrdOssSys::ConfigMio(XrdSysError &Eroute)
{
     XrdOucPList *fp;
     unsigned long long flags = 0;
     int setoff = 0;

// Initialize memory mapping setting to speed execution
//
   if (!(tryMmap = XrdOssMio::isOn())) return;
   chkMmap = XrdOssMio::isAuto();

// Run through all the paths and get the composite flags
//
   fp = RPList.First();
   while(fp)
        {flags |= fp->Flag();
         fp = fp->Next();
        }

// Handle default settings
//
   if (DirFlags & XRDEXP_MEMAP && !(DirFlags & XRDEXP_NOTRW))
      DirFlags |= XRDEXP_FORCERO;
   flags |= DirFlags;
   if (DirFlags & (XRDEXP_MLOK | XRDEXP_MKEEP)) DirFlags |= XRDEXP_MMAP;

// Produce warnings if unsupported features have been selected
//
#if !defined(_POSIX_MAPPED_FILES)
   if (flags & XRDEXP_MEMAP)
      {Eroute.Say("Config warning: memory mapped files not supported; "
                             "feature disabled.");
       setoff = 1;
       fp = RPList.First();
       while(fp)
            {fp->Set(fp->Flag() & ~XRDEXP_MEMAP);
             fp = fp->Next();
            }
       DirFlags = DirFlags & ~XRDEXP_MEMAP;
      }
#elif !defined(_POSIX_MEMLOCK)
   if (flags & XRDEXP_MLOK)
      {Eroute.Say("Config warning: memory locked files not supported; "
                             "feature disabled.");
       fp = RPList.First();
       while(fp)
            {fp->Set(fp->Flag() & ~XRDEXP_MLOK);
             fp = fp->Next();
            }
       DirFlags = DirFlags & ~XRDEXP_MLOK;
      }
#endif

// If no memory flags are set, turn off memory mapped files
//
   if (!(flags & XRDEXP_MEMAP) || setoff)
     {XrdOssMio::Set(0, 0, 0);
      tryMmap = 0; chkMmap = 0;
     }
}
  
/******************************************************************************/
/*                             C o n f i g N 2 N                              */
/******************************************************************************/

int XrdOssSys::ConfigN2N(XrdSysError &Eroute, XrdOucEnv *envP)
{
   XrdOucN2NLoader n2nLoader(&Eroute,ConfigFN,N2N_Parms,LocalRoot,RemoteRoot);

// Get the plugin
//
   if (!(the_N2N = n2nLoader.Load(N2N_Lib, *myVersion, envP))) return 1;

// Optimize the local case
//
   if (N2N_Lib)   rmt_N2N = lcl_N2N = the_N2N;
      else {if (LocalRoot)  lcl_N2N = the_N2N;
            if (RemoteRoot) rmt_N2N = the_N2N;
           }

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdOssSys::ConfigProc(XrdSysError &Eroute)
{
  char *var;
  int  cfgFD, retc, NoGo = XrdOssOK;
  XrdOucEnv myEnv;
  XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// If there is no config file, return with the defaults sets.
//
   if( !ConfigFN || !*ConfigFN)
     {Eroute.Say("Config warning: config file not specified; defaults assumed.");
      return XrdOssOK;
     }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Eroute.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);
   static const char *cvec[] = { "*** oss plugin config:", 0 };
   Config.Capture(cvec);

// Now start reading records until eof.
//
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, "oss.", 4))
            {if (ConfigXeq(var+4, Config, Eroute)) {Config.Echo(); NoGo = 1;}}
            else if (!strcmp(var,"all.export")
                 &&  xpath(Config, Eroute)) {Config.Echo(); NoGo = 1;}
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = Eroute.Emsg("Config", retc, "read config file", ConfigFN);
   Config.Close();

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                           C o n f i g S p a c e                            */
/******************************************************************************/

void XrdOssSys::ConfigSpace(XrdSysError &Eroute)
{
   XrdOucPList *fp = RPList.First();
   int noCacheFS = !(OptFlags & XrdOss_CacheFS);

// Configure space for each non-cached exported path. We only keep track of
// space that can actually be modified in some way.
//
   while(fp)
        {if ( ((noCacheFS || (fp->Flag() & XRDEXP_INPLACE)) &&
               (fp->Flag() & (XRDEXP_STAGE | XRDEXP_PURGE)))
         ||   !(fp->Flag() &  XRDEXP_NOTRW)
         ||    (fp->Flag() &  XRDEXP_PFCACHE) )
            ConfigSpace(fp->Path());
         fp = fp->Next();
        }

// If there is a space list then verify it
//
   if ((fp = SPList.First()))
      {XrdOssCache_Group  *fsg;
       const char *what;
       bool zAssign = false;
       while(fp)
            {if (fp->Attr() != spAssign) what = "default space ";
                else {zAssign = true;    what = "assign space ";}
             const char *grp = fp->Name();
             fsg = XrdOssCache_Group::fsgroups;
             while(fsg) {if (!strcmp(fsg->group,grp)) break; fsg = fsg->next;}
             if (!fsg) Eroute.Say("Config warning: unable to ", what, grp,
                                  " to ", fp->Path(), "; space not defined.");
             fp = fp->Next();
            }
       if (zAssign) SPList.Default(static_cast<unsigned long long>(spAssign));
      }
}

/******************************************************************************/

void XrdOssSys::ConfigSpace(const char *Lfn)
{
   struct stat statbuff;
   char Pfn[MAXPATHLEN+1+8], *Slash;

// Get local path for this lfn
//
   if (GenLocalPath(Lfn, Pfn)) return;

// Now try to find the actual existing base path
//
   while(stat(Pfn, &statbuff))
        {if (!(Slash = rindex(Pfn, '/')) || Slash == Pfn) return;
         *Slash = '\0';
        }

// Add this path to the file system data. We need to do this to track space
//
   XrdOssCache_FS::Add(Pfn);
}
  
/******************************************************************************/
/*                           C o n f i g S p a t h                            */
/******************************************************************************/

void XrdOssSys::ConfigSpath(XrdSysError &Eroute, const char *Path,
                           unsigned long long &flags, int noMSS)
{
// mig+r/w -> check unless nocheck was specified
//
   if (!(flags & XRDEXP_CHECK_X))
      {if ((flags & XRDEXP_MIG) && !(flags & XRDEXP_NOTRW))
               flags &= ~XRDEXP_NOCHECK;
          else flags |=  XRDEXP_NOCHECK;
      }
// rsscmd  -> dread unless nodread was specified
//
   if (!(flags & XRDEXP_DREAD_X))
      {if (RSSCmd) flags &= ~XRDEXP_NODREAD;
          else     flags |=  XRDEXP_NODREAD;
      }

// If there is no mss then turn off all mss related optionss, otherwise check
// if the options may leave the system in an inconsistent state
//
   if (noMSS) flags=(flags & ~XRDEXP_RCREATE)|XRDEXP_NOCHECK|XRDEXP_NODREAD;
      else if ((flags & XRDEXP_MIG)   &&  (flags & XRDEXP_NOCHECK)
           && !(flags & XRDEXP_NOTRW))
              Eroute.Say("Config warning: 'all.export ", Path,
                          " nocheck mig r/w' allows file inconsistentcy!");
}
  
/******************************************************************************/
/*                           C o n f i g S t a g e                            */
/******************************************************************************/

int XrdOssSys::ConfigStage(XrdSysError &Eroute)
{
   const char *What;
   char *tp, *stgp = 0;
   unsigned long long flags;
   int noMSS, needRSS = 0, NoGo = 0;
   XrdOucPList *fp;

// Determine if we are a manager/supervisor. These never stage files so we
// really don't need (nor want) a stagecmd or an msscmd.
//
   noMSS = ((tp = getenv("XRDREDIRECT"))
            && (!strcmp(tp, "R") || !strcmp(tp, "M"))) | Solitary;

// A rsscmd implies check+dread. Note that nostage is now always the default.
//
   flags = (RSSCmd ? 0 : XRDEXP_NOCHECK | XRDEXP_NODREAD);
   DirFlags = DirFlags | (flags & (~(DirFlags >> XRDEXP_MASKSHIFT)));

// Set default flags
//
   RPList.Default(DirFlags);

// Reprocess the paths to set correct defaults
//
   fp = RPList.First();
   while(fp) 
        {flags = fp->Flag(); ConfigSpath(Eroute, fp->Path(), flags, noMSS);

         // Record the fact that we have a stageable path
         //
         if (flags & XRDEXP_STAGE) stgp = fp->Path();

         // Check if path requires rsscmd and complain if we don't have one
         //
              if (!(flags & XRDEXP_NOCHECK)) What = "has check";
         else if (!(flags & XRDEXP_NODREAD)) What = "has dread";
         else if   (flags & XRDEXP_RCREATE)  What = "has recreate";
         else                                What = 0;
         if (!noMSS && !RSSCmd && What)
            {Eroute.Emsg("Config", fp->Path(), What,
                         "export attribute but rsscmd not specified.");
             NoGo  = 1;
            } else if (What) needRSS = 1;

         // Update flags and proceed to next path
         //
         fp->Set(flags); fp = fp->Next();
        }

// If we are a manager/supervisor, short circuit MSS initialization
//
   if (noMSS)
      {if (RSSCmd)   {free(RSSCmd);   RSSCmd   = 0;}
       if (StageCmd) {free(StageCmd); StageCmd = 0;}
       RSSProg = 0; StageCreate = 0;
       return NoGo;
      }

// Check if we don't need the stagecmd but one was specified
//
   if (StageCmd && !stgp)
      {Eroute.Say("Config warning: 'stagecmd' ignored; no stageable paths present.");
       free(StageCmd); StageCmd = 0;
      }

// Check if we don't need a remote storage service but one was specified
//
   if (RSSCmd && !needRSS)
      {Eroute.Say("Config warning: 'rsscmd' ignored; no path exported with "
                                           "check, dread, or rcreate.");
       free(RSSCmd); RSSCmd = 0;
      }

// If we have any errors at this point, just return failure
//
   if (NoGo) return 1;
   if (!RSSCmd && !StageCmd && !stgp) return 0;
   Eroute.Say("++++++ Remote Storage System interface initialization started.");

// Allocate a pr0gram object for the gateway command
//
   if (RSSCmd)
      {RSSProg = new XrdOucProg(&Eroute);
       if (RSSProg->Setup(RSSCmd)) NoGo = 1;
      }

// Initialize staging if we need to
//
   if (!NoGo && (StageCmd || stgp))
      {const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
       if (StageCmd && *StageCmd) NoGo = ConfigStageC(Eroute);
          else {StageFrm = new XrdFrcProxy(Eroute.logger(),
                           XrdOucUtils::InstName(),OssTrace.What & TRACE_Debug);
                NoGo = !StageFrm->Init(XrdFrcProxy::opStg,
                                       getenv("XRDADMINPATH"), AMode);
                StageRealTime = 0; StageAsync = 1;
               }

      // Set up the event path
      //
         StageAction = (char *)"wfn "; StageActLen = 4;
         if ((tp = getenv("XRDOFSEVENTS")))
            {char sebuff[MAXPATHLEN+8];
             StageEvSize = sprintf(sebuff, "file:///%s", tp);
             StageEvents = strdup(sebuff);
            } else {StageEvents = (char *)"-"; StageEvSize = 1;}
      }

// All done
//
   tp = (NoGo ? (char *)"failed." : (char *)"completed.");
   Eroute.Say("------ Remote Storage System interface initialization ", tp);
   return NoGo;
}
  
/******************************************************************************/
/*                          C o n f i g S t a g e C                           */
/******************************************************************************/

int XrdOssSys::ConfigStageC(XrdSysError &Eroute)
{
   pthread_t tid;
   char *sp, *tp;
   int numt, retc, NoGo = 0;

// The stage command is interactive if it starts with an | (i.e., pipe in)
//
   tp = StageCmd;
   while(*tp && *tp == ' ') tp++;
   if (*tp == '|') {StageRealTime = 0;
                    do {tp++;} while(*tp == ' ');
                   }
   StageCmd = tp;

// This is a bit of hackery to get the traceid sent over to the
// new file residency manager (frm). Keeps the config simple.
//
   if ((sp = index(StageCmd, ' '))) *sp = '\0';
   if (!(tp = rindex (StageCmd, '/'))) tp = StageCmd;
      else tp++;
   if (!strncmp("frm_", tp, 4)) StageFormat = 1;
   if (sp) *sp = ' ';

// Set up a program object for the command
//
   StageProg = new XrdOucProg(&Eroute);
   if (StageProg->Setup(StageCmd)) NoGo = 1;

// For old-style real-time staging, create threads to handle the staging
// For queue-style staging, start the program that handles the queue
//
   if (!NoGo)
      {if (StageRealTime)
          {if ((numt = xfrthreads - xfrtcount) > 0) while(numt--)
              {if ((retc = XrdSysThread::Run(&tid,XrdOssxfr,(void *)0,0,"staging")))
                  Eroute.Emsg("Config", retc, "create staging thread");
                  else xfrtcount++;
              }
          } else NoGo = StageProg->Start();
      }

// Setup the additional stage information vector. Variable substitution:
// <data>$var;<data>.... (max of MaxArgs substitutions). This is only relevant
// when using an actual stagecmd.
//
   if (!NoGo && !StageRealTime && StageMsg)
      {XrdOucMsubs *msubs = new XrdOucMsubs(&Eroute);
       if (msubs->Parse("stagemsg", StageMsg)) StageSnd = msubs;
          else NoGo = 1;  // We will exit no need to delete msubs
      }

// All done
//
   return NoGo;
}

/******************************************************************************/
/*                         C o n f i g S t a t L i b                          */
/******************************************************************************/

int XrdOssSys::ConfigStatLib(XrdSysError &Eroute, XrdOucEnv *envP)
{
   XrdOucPinLoader myLib(&Eroute, myVersion, "statlib", STT_Lib);
   const char *stName2 = "?XrdOssStatInfoInit2";

// Get the plugin and stat function. Let's try version 2 first
//
   XrdOssStatInfoInit2_t siGet2;
   if (STT_V2) stName2++;
   if ((siGet2=(XrdOssStatInfoInit2_t)myLib.Resolve(stName2)))
      {if (!(STT_Fund = siGet2(this,Eroute.logger(),ConfigFN,STT_Parms,envP)))
          return 1;
       if (STT_DoARE) envP->PutPtr("XrdOssStatInfo2*", (void *)STT_Fund);
       STT_V2 = 1;
       return 0;
      }

// If we are here but the -2 was specified on the config then we fail
//
   if (STT_V2) return 1;

// OK, so we better find version 1 in the shared library
//
   XrdOssStatInfoInit_t siGet;
   if (!(siGet = (XrdOssStatInfoInit_t)myLib.Resolve("XrdOssStatInfoInit"))
   ||  !(STT_Func = siGet (this,Eroute.logger(),ConfigFN,STT_Parms)))
      return 1;

// Return success
//
   return 0;
}
  
/******************************************************************************/
/*                           C o n f i g S t a t s                            */
/******************************************************************************/

void XrdOssSys::ConfigStats(XrdSysError &Eroute)
{
   struct StatsDev
         {StatsDev *Next;
          dev_t     st_dev;
          StatsDev(StatsDev *dP, dev_t dn) : Next(dP), st_dev(dn) {}
         };

   XrdOssCache_Group  *fsg = XrdOssCache_Group::fsgroups;
   XrdOucPList        *fP = RPList.First();
   StatsDev           *dP1st = 0, *dP, *dPp;
   struct stat         Stat;
   char LPath[MAXPATHLEN+1], PPath[MAXPATHLEN+1], *cP;

// Count actual cache groups
//
   while(fsg) {numCG++; fsg = fsg->next;}

// Develop the list of paths that we will report on
//
   if (fP) do
      {strcpy(LPath, fP->Path());
       if (GenLocalPath(LPath, PPath)) continue;
       if (stat(PPath, &Stat) && (cP = rindex(LPath, '/')))
          {*cP = '\0';
           if (GenLocalPath(LPath, PPath) || stat(PPath, &Stat)) continue;
          }
       dP = dP1st;
       while(dP && dP->st_dev != Stat.st_dev) dP = dP->Next;
       if (dP) continue;
       ConfigStats(Stat.st_dev, LPath);
       if (GenLocalPath(LPath, PPath)) continue;
       DPList = new OssDPath(DPList, strdup(LPath), strdup(PPath));
       lenDP += strlen(LPath) + strlen(PPath); numDP++;
       dP1st  = new StatsDev(dP1st, Stat.st_dev);
      } while ((fP = fP->Next()));

// If we have no exported paths then create a simple /tmp object
//
   if (!numDP)
      {DPList = new OssDPath(0, strdup("/tmp"), strdup("/tmp"));
       lenDP = 4; numDP = 1;
      }

// Now delete all of the device objects
//
   dP = dP1st;
   while(dP) {dPp = dP; dP = dP->Next; delete dPp;}
}
  
/******************************************************************************/

void XrdOssSys::ConfigStats(dev_t Devnum, char *lP)
{
   struct stat Stat;
   char *Slash, pP[MAXPATHLEN+1];

// Minimize the path
//
   while((Slash = rindex(lP+1, '/')))
        {*Slash = '\0';
         if (GenLocalPath(lP, pP) || stat(pP, &Stat) || Stat.st_dev != Devnum)
            break;
        }

// Extend path if need be and return
//
   if (Slash) *Slash = '/';
}
  
/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdOssSys::ConfigXeq(char *var, XrdOucStream &Config, XrdSysError &Eroute)
{
    char  myVar[80], buff[2048], *val;
    int nosubs;
    XrdOucEnv *myEnv = 0;

   TS_Xeq("alloc",         xalloc);
   TS_Xeq("cache",         xcache);
   TS_Xeq("cachescan",     xcachescan); // Backward compatibility
   TS_Xeq("spacescan",     xcachescan);
   TS_Xeq("defaults",      xdefault);
   TS_Xeq("fdlimit",       xfdlimit);
   TS_Xeq("maxsize",       xmaxsz);
   TS_Xeq("memfile",       xmemf);
   TS_Xeq("namelib",       xnml);
   TS_Xeq("path",          xpath);
   TS_Xeq("preread",       xprerd);
   TS_Xeq("space",         xspace);
   TS_Xeq("stagecmd",      xstg);
   TS_Xeq("statlib",       xstl);
   TS_Xeq("trace",         xtrace);
   TS_Xeq("usage",         xusage);
   TS_Xeq("xfr",           xxfr);

   // Check if var substitutions are prohibited (e.g., stagemsg). Note that
   // TS_String() returns upon success so be careful when adding new opts.
   //
   if ((nosubs = !strcmp(var, "stagemsg"))) myEnv = Config.SetEnv(0);

   // Copy the variable name as this may change because it points to an
   // internal buffer in Config. The vagaries of effeciency.
   //
   strlcpy(myVar, var, sizeof(myVar));
   var = myVar;

   // We need to suck all the tokens to the end of the line for remaining
   // options. Do so, until we run out of space in the buffer.
   //
   if (!Config.GetRest(buff, sizeof(buff)))
      {Eroute.Emsg("Config", "arguments too long for", var);
       if (nosubs) Config.SetEnv(myEnv);
       return 1;
      }
   val = buff;

   // Restore substititions at this point if need be
   //
   if (nosubs) Config.SetEnv(myEnv);

   // At this point, make sure we have a value
   //
   if (!(*val))
      {Eroute.Emsg("Config", "no value for directive", var);
       return 1;
      }

   // Check for tokens taking a variable number of parameters
   //
   TS_String("localroot",  LocalRoot);
   TS_String("remoteroot", RemoteRoot);
   TS_String("stagemsg",   StageMsg);

   // The following differentiates between a deprecated and a preferred command
   //
   if (!strcmp("msscmd", var)) {isMSSC = 1; Duplicate(val, RSSCmd); return 0;}
   if (!strcmp("rsscmd", var)) {isMSSC = 0; Duplicate(val, RSSCmd); return 0;}

   // No match found, complain.
   //
   Eroute.Say("Config warning: ignoring unknown directive '",var,"'.");
   Config.Echo();
   return 0;
}

/******************************************************************************/
/*                                x a l l o c                                 */
/******************************************************************************/

/* Function: aalloc

   Purpose:  To parse the directive: alloc <min> [<headroom> [<fuzz>]]

             <min>       minimum amount of free space needed in a partition.
                         (asterisk uses default).
             <headroom>  percentage of requested space to be added to the
                         free space amount (asterisk uses default).
             <fuzz>      the percentage difference between two free space
                         quantities that may be ignored when selecting a space
                           0 - reduces to finding the largest free space
                         100 - reduces to simple round-robin allocation

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xalloc(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    long long mina = 0;
    int       fuzz = 0;
    int       hdrm = 0;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "alloc minfree not specified"); return 1;}
    if (strcmp(val, "*") &&
        XrdOuca2x::a2sz(Eroute, "alloc minfree", val, &mina, 0)) return 1;

    if ((val = Config.GetWord()))
       {if (strcmp(val, "*") &&
            XrdOuca2x::a2i(Eroute,"alloc headroom",val,&hdrm,0,100)) return 1;

        if ((val = Config.GetWord()))
           {if (strcmp(val, "*") &&
            XrdOuca2x::a2i(Eroute, "alloc fuzz", val, &fuzz, 0, 100)) return 1;
           }
       }

    minalloc = mina;
    ovhalloc = hdrm;
    fuzalloc = fuzz;
    return 0;
}

/******************************************************************************/
/*                                x c a c h e                                 */
/******************************************************************************/

/* Function: xcache

   Purpose:  To parse the directive: cache <group> <path> [xa]

             <group>  logical group name for the cache filesystem.
             <path>   path to the cache.
             xa       support extended attributes

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xcache(XrdOucStream &Config, XrdSysError &Eroute)
{
   int rc, isXA = 0;

// Skip out to process this entry and upon success indicate that it is
// deprecated and "space" should be used instead if an XA-style space defined.
//
   if (!(rc = xspace(Config, Eroute, &isXA)))
      {if (isXA) Eroute.Say("Config warning: 'oss.cache' is deprecated; "
                            "use 'oss.space' instead!");
          else  {Eroute.Say("Config failure: non-xa spaces are no longer "
                            "supported!");
                 rc = 1;
                }
      }
    return rc;
}

/******************************************************************************/
/*                            x c a c h e s c a n                             */
/******************************************************************************/

/* Function: xcachescan

   Purpose:  To parse the directive: cachescan <num>

             <num>     number of seconds between cache scans.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOssSys::xcachescan(XrdOucStream &Config, XrdSysError &Eroute)
{   int cscan = 0;
    char *val;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "cachescan not specified"); return 1;}
    if (XrdOuca2x::a2tm(Eroute, "cachescan", val, &cscan, 30)) return 1;
    cscanint = cscan;
    return 0;
}

/******************************************************************************/
/*                              x d e f a u l t                               */
/******************************************************************************/

/* Function: xdefault

   Purpose:  Parse: defaults <default options>
                              
   Notes: See the oss configuration manual for the meaning of each option.
          The actual implementation is defined in XrdOucExport.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xdefault(XrdOucStream &Config, XrdSysError &Eroute)
{
   DirFlags = XrdOucExport::ParseDefs(Config, Eroute, DirFlags);
   return 0;
}
  
/******************************************************************************/
/*                              x f d l i m i t                               */
/******************************************************************************/

/* Function: xfdlimit

   Purpose:  To parse the directive: fdlimit <fence> [ <max> ]

             <fence>  lowest number to use for file fd's (0 -> max). If
                      specified as * then max/2 is used.
             <max>    highest number that can be used. The soft rlimit is set
                      to this value. If not supplied, the limit is not changed.
                      If supplied as 'max' then the hard limit is used.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xfdlimit(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int fence = 0, FDHalf = FDLimit>>1;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "fdlimit fence not specified"); return 1;}

    if (!strcmp(val, "*")) FDFence = FDHalf;
       else {if (XrdOuca2x::a2i(Eroute,"fdlimit fence",val,&fence,0)) return 1;
             FDFence = (fence < FDHalf ? fence : FDHalf);
            }

    while(Config.GetWord()) {}

//  Eroute.Say("Config warning: ", "fdlimit directive no longer supported.");

    return 0;
}
  
/******************************************************************************/
/*                                x m a x s z                                 */
/******************************************************************************/

/* Function: xmaxsz

   Purpose:  Parse the directive:  maxsize <num>

             <num> Maximum number of bytes in a file.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xmaxsz(XrdOucStream &Config, XrdSysError &Eroute)
{   long long msz;
    char *val;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "maxsize value not specified"); return 1;}
    if (XrdOuca2x::a2sz(Eroute, "maxsize", val, &msz, 1024*1024)) return 1;
    MaxSize = msz;
    return 0;
}

/******************************************************************************/
/*                                 x m e m f                                  */
/******************************************************************************/
  
/* Function: xmemf

   Purpose:  Parse the directive: memfile [off] [max <msz>]
                                          [check xattr] [preload]

             check      Applies memory mapping options based on file's xattrs.
                        For backward compatibility, we also accept:
                        "[check {keep | lock | map}]" which implies check xattr.
             all        Preloads the complete file into memory.
             off        Disables memory mapping regardless of other options.
             on         Enables memory mapping
             preload    Preloads the file after every opn reference.
             <msz>      Maximum amount of memory to use (can be n% or real mem).

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xmemf(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int i, j, V_check=-1, V_preld = -1, V_on=-1;
    long long V_max = 0;

    static struct mmapopts {const char *opname; int otyp;
                            const char *opmsg;} mmopts[] =
       {
        {"off",        0, ""},
        {"preload",    1, "memfile preload"},
        {"check",      2, "memfile check"},
        {"max",        3, "memfile max"}};
    int numopts = sizeof(mmopts)/sizeof(struct mmapopts);

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "memfile option not specified"); return 1;}

    while (val)
         {for (i = 0; i < numopts; i++)
              if (!strcmp(val, mmopts[i].opname)) break;
          if (i >= numopts)
             Eroute.Say("Config warning: ignoring invalid memfile option '",val,"'.");
             else {if (mmopts[i].otyp >  1 && !(val = Config.GetWord()))
                      {Eroute.Emsg("Config","memfile",mmopts[i].opname,
                                   "value not specified");
                       return 1;
                      }
                   switch(mmopts[i].otyp)
                         {case 1: V_preld = 1;
                                  break;
                          case 2: if (!strcmp("xattr",val)
                                  ||  !strcmp("lock", val)
                                  ||  !strcmp("map",  val)
                                  ||  !strcmp("keep", val)) V_check=1;
                                 else {Eroute.Emsg("Config",
                                       "mmap check argument not xattr");
                                       return 1;
                                      }
                                  break;
                          case 3: j = strlen(val);
                                  if (val[j-1] == '%')
                                     {val[j-1] = '\0';
                                      if (XrdOuca2x::a2i(Eroute,mmopts[i].opmsg,
                                                     val, &j, 1, 1000)) return 1;
                                      V_max = -j;
                                     } else if (XrdOuca2x::a2sz(Eroute,
                                                mmopts[i].opmsg, val, &V_max,
                                                10*1024*1024)) return 1;
                                  break;
                          default: V_on = 0; break;
                         }
                  val = Config.GetWord();
                 }
         }

// Set the values
//
   XrdOssMio::Set(V_on, V_preld, V_check);
   XrdOssMio::Set(V_max);
   return 0;
}

/******************************************************************************/
/*                                  x n m l                                   */
/******************************************************************************/

/* Function: xnml

   Purpose:  To parse the directive: namelib <path> [<parms>]

             <path>    the path of the filesystem library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xnml(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val, parms[1040];

// Get the path
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "namelib not specified"); return 1;}

// Record the path
//
   if (N2N_Lib) free(N2N_Lib);
   N2N_Lib = strdup(val);

// Record any parms
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "namelib parameters too long"); return 1;}
   if (N2N_Parms) free(N2N_Parms);
   N2N_Parms = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/*                                 x p a t h                                  */
/******************************************************************************/

/* Function: xpath

   Purpose:  To parse the directive: {export | path} <path> [<options>]

             <path>    the full path that resides in a remote system.
             <options> a blank separated list of options (see XrdOucExport)

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xpath(XrdOucStream &Config, XrdSysError &Eroute)
{
   XrdOucPList *pP;

// Parse the arguments
//
   pP = XrdOucExport::ParsePath(Config, Eroute, RPList, DirFlags);
   if (!pP) return 1;

// If this is an absolute path, we are done
//
   if (*(pP->Path()) == '/') return 0;

// If this is an objectid path then make sure to set the default for these
//
   if (*(pP->Path()) == '*')
      {RPList.Defstar(pP->Flag());
       return 0;
      }

// We do not (yet) support exporting specific object ID's
//
   Eroute.Emsg("Config", "Unsupported export -", pP->Path());
   return 1;
}

/******************************************************************************/
/*                                x p r e r d                                 */
/******************************************************************************/

/* Function: xprerd

   Purpose:  To parse the directive: preread {<depth> | on} [limit <bytes>]
                                             [ qsize [=]<qsz> ]

             <depth>  the number of request to preread ahead of the read.
                      A value of 0, the inital default, turns off prereads.
                      Specifying "on" sets the value (currently) to 3.
             <bytes>  Maximum number of bytes to preread. Prereading stops,
                      regardless of depth, once <bytes> have been preread.
                      The default is 1M (i.e.1 megabyte). The max is 16M.
             <qsz>    the queue size after which preread blocking would occur.
                      The value must be greater than or equal to <depth>.
                      The value is adjusted to max(<qsz>/(<depth>/2+1),<depth>)
                      unless the number is preceeded by an equal sign. The
                      default <qsz> is 128.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xprerd(XrdOucStream &Config, XrdSysError &Eroute)
{
    static const long long m16 = 16777216LL;
    char *val;
    long long lim = 1048576;
    int depth, qeq = 0, qsz = 128;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config", "preread depth not specified"); return 1;}

      if (!strcmp(val, "on")) depth = 3;
         else if (XrdOuca2x::a2i(Eroute,"preread depth",val,&depth,0, 1024))
                 return 1;

      while((val = Config.GetWord()))
           {     if (!strcmp(val, "limit"))
                    {if (!(val = Config.GetWord()))
                        {Eroute.Emsg("Config","preread limit not specified");
                         return 1;
                        }
                     if (XrdOuca2x::a2sz(Eroute,"preread limit",val,&lim,0,m16))
                        return 1;
                    }
            else if (!strcmp(val, "qsize"))
                    {if (!(val = Config.GetWord()))
                        {Eroute.Emsg("Config","preread qsize not specified");
                         return 1;
                        }
                     if (XrdOuca2x::a2i(Eroute,"preread qsize",val,&qsz,0,1024))
                        return 1;
                     if (qsz < depth)
                        {Eroute.Emsg("Config","preread qsize must be >= depth");
                         return 1;
                        }
                    }
            else {Eroute.Emsg("Config","invalid preread option -",val); return 1;}
         }

      if (lim < prPSize || !qsz) depth = 0;
      if (!qeq && depth)
         {qsz = qsz/(depth/2+1);
          if (qsz < depth) qsz = depth;
         }

      prDepth = depth;
      prQSize = qsz;
      prBytes = lim;
      return 0;
}
  
/******************************************************************************/
/*                                x s p a c e                                 */
/******************************************************************************/

/* Function: xspace

   Purpose:  To parse the directive: space <name> <path> {chkmount <id> [nofail]
                                 or: space <name> {assign}default} <lfn> [...]

             <name>   logical name for the filesystem.
             <path>   path to the filesystem.
             <id>     mountpoint name in order to be considered valid

   Output: 0 upon success or !0 upon failure.

   Note: This is the new and prefered way to say "cache <group> <path> xa".
*/

int XrdOssSys::xspace(XrdOucStream &Config, XrdSysError &Eroute, int *isCD)
{
   XrdOucString grp, fn, mn;
   OssSpaceConfig sInfo(grp, fn, mn);
   char *val;
   int  k;
   bool isAsgn, isStar;

// Get the space name
//
   if (!(val = Config.GetWord()))
      {Eroute.Emsg("Config", "space name not specified"); return 1;}
   if ((int)strlen(val) > XrdOssSpace::maxSNlen)
      {Eroute.Emsg("Config","excessively long space name - ",val); return 1;}
   grp = val;

// Get the path to the space
//
   if (!(val = Config.GetWord()) || !(*val))
      {Eroute.Emsg("Config", "space path not specified"); return 1;}

// Check if assignment
//
   if (((isAsgn = !strcmp("assign",val)) || ! strcmp("default",val)) && !isCD)
      return xspace(Config, Eroute, grp.c_str(), isAsgn);

// Preprocess this path and validate it
//
   k = strlen(val)-1;
   if ((isStar = val[k] == '*')) val[k--] = 0;
      else while(k > 0 && val[k] == '/') val[k--] = 0;

   if (k >= MAXPATHLEN || val[0] != '/' || (k < 2 && !isStar))
      {Eroute.Emsg("Config", "invalid space path - ", val); return 1;}
   fn = val;

// Sanitize the path as we are sensitive to proper placement of slashes
//
   do {k = fn.replace("/./", "/");} while(k);
   do {k = fn.replace("//",  "/");} while(k);

// Additional options (for now) are only available to the old-style cache
// directive. So, ignore any unless we entered via the directive.
//
   if (isCD)
      {if ((val = Config.GetWord()))
          {if (strcmp("xa", val))
              {Eroute.Emsg("Config","invalid cache option - ",val); return 1;}
              else *isCD = 1;
          } else  {*isCD = 0; sInfo.isXA = false;}
      } else {
       if ((val = Config.GetWord()) && !strcmp("chkmount", val))
          {if (!(val = Config.GetWord()))
              {Eroute.Emsg("Config","chkmount ID not specified"); return 1;}
           if ((int)strlen(val) > XrdOssSpace::maxSNlen)
              {Eroute.Emsg("Config","excessively long mount name - ",val);
               return 1;
              }
           mn = val;
           sInfo.chkMnt = true;
           if ((val = Config.GetWord()))
              {if (!strcmp("nofail", val)) sInfo.noFail = true;
                  else {Eroute.Emsg("Config","invalid space option - ",val);
                        return 1;
                       }
              }
          }
      }

// Check if this directory in the parent is only to be used for the space
//
   if (!isStar)
      {if (!fn.endswith('/')) fn += '/';
       return !xspaceBuild(sInfo, Eroute);
      }

// We now need to build a space for each directory in the parent
//
   struct dirent *dp;
   struct stat    Stat;
   XrdOucString   pfx, basepath(fn);
   DIR  *dirP;
   int   dFD, rc, snum = 0;
   bool  chkPfx, failed = false;

   if (basepath.endswith('/')) chkPfx = false;
      else {int pos = basepath.rfind('/');
            pfx = &basepath[pos+1];
            basepath.keep(0, pos+1);
            chkPfx = true;
           }

   if ((dFD=open(basepath.c_str(),O_DIRECTORY)) < 0 || !(dirP=fdopendir(dFD)))
      {Eroute.Emsg("Config",errno,"open space directory",fn.c_str()); return 1;}

   errno = 0;
   while((dp = readdir(dirP)))
        {if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")
         || (chkPfx && strncmp(dp->d_name,pfx.c_str(),pfx.length()))) continue;

         if (fstatat(dFD, dp->d_name, &Stat, AT_SYMLINK_NOFOLLOW))
            {basepath += dp->d_name;
             break;
            }

         if ((Stat.st_mode & S_IFMT) == S_IFDIR)
            {fn = basepath; fn += dp->d_name; fn += '/';
             if (!xspaceBuild(sInfo, Eroute)) failed = true;
             snum++;
            }
         errno = 0;
        }

// Make sure we built all space successfully and have at least one space
//
   if ((rc = errno))
      Eroute.Emsg("Config", errno, "process space directory", fn.c_str());
      else if (!snum)
              Eroute.Say("Config warning: no space directories found in ",
                         fn.c_str());

   closedir(dirP);
   return rc != 0 || failed;
}

/******************************************************************************/

int XrdOssSys::xspace(XrdOucStream &Config, XrdSysError &Eroute,
                      const char *grp, bool isAsgn)
{
   XrdOucPList *pl;
   char *path;

// Get the path
//
   path = Config.GetWord();
   if (!path || !path[0])
      {Eroute.Emsg("Config", "space path not specified"); return 1;}

// Create a new path list object and add it to list of paths
//
do{if ((pl = SPList.Match(path))) pl->Set(path, grp);
      else {pl = new XrdOucPList(path, grp);
            SPList.Insert(pl);
           }
   pl->Set((isAsgn ? spAssign : 0));
  } while((path = Config.GetWord()));

// All done
//
   return 0;
}

/******************************************************************************/

int XrdOssSys::xspaceBuild(OssSpaceConfig &sInfo, XrdSysError &Eroute)
{
    XrdOssCache_FS::FSOpts fopts = (sInfo.isXA ? XrdOssCache_FS::isXA
                                               : XrdOssCache_FS::None);
    int rc = 0;

// Check if we need to verify the mount. Note: sPath must end with a '/'!
//
   if (sInfo.chkMnt)
      {XrdOucString mFile(sInfo.mName), mPath(sInfo.sPath);
       struct stat Stat;
       mPath.erasefromend(1);
       mFile += '.';
       mFile += rindex(mPath.c_str(), '/')+1;
       mPath += '/'; mPath += mFile;
       if (stat(mPath.c_str(), &Stat))
          {char buff[2048];
           snprintf(buff, sizeof(buff), "%s@%s; ",
                          mFile.c_str(), sInfo.sPath.c_str());
           Eroute.Say((sInfo.noFail ? "Config warning:" : "Config failure:"),
                      " Unable to verify mount point ", buff, XrdSysE2T(errno));
           return (sInfo.noFail ? 1 : 0);
          }
      }

// Add the space to the configuration

    XrdOssCache_FS *fsp = new XrdOssCache_FS(rc, sInfo.sName.c_str(),
                                                 sInfo.sPath.c_str(), fopts);
    if (rc)
       {char buff[256];
        snprintf(buff, sizeof(buff), "create %s space at", sInfo.sName.c_str());
        Eroute.Emsg("Config", rc, buff, sInfo.sPath.c_str());
        if (fsp) delete fsp;
        return 0;
       }
    OptFlags |= XrdOss_CacheFS;
    return 1;
}

/******************************************************************************/
/*                                  x s t g                                   */
/******************************************************************************/

/* Function: xstg

   Purpose:  To parse the directive: 
                stagecmd [async | sync] [creates] [|]<cmd>

             async     Client is to be notified when <cmd> sends an event
             sync      Client is to poll for <cmd> completion.
             creates   Route file creation requests to <cmd>.
             <cmd>     The command and args to stage in the file. If the
                       <cmd> is prefixed ny '|' then pipe in the requests.

  Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xstg(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val, buff[2048], *bp = buff;
    int vlen, blen = sizeof(buff)-1, isAsync = 0, isCreate = 0;

// Get the aync or async option
//
   if ((val = Config.GetWord()))
       if ((isAsync = !strcmp(val, "async")) || !strcmp(val, "sync"))
          val = Config.GetWord();

// Get the create option
//
   if (val)
       if ((isCreate = !strcmp(val, "creates"))) val = Config.GetWord();

// Get the command
//
   if (!val) {Eroute.Emsg("Config", "stagecmd not specified"); return 1;}

// Copy the command and all of it's arguments
//
   do {if ((vlen = strlen(val)) >= blen)
          {Eroute.Emsg("Config", "stagecmd arguments too long"); break;}
       *bp = ' '; bp++; strcpy(bp, val); bp += vlen; blen -= vlen;
      } while((val = Config.GetWord()));

    if (val) return 1;
    *bp = '\0'; val = buff+1;

// Record the command and operating mode
//
   StageAsync = (isAsync ? 1 : 0);
   StageCreate= isCreate;
   if (StageCmd) free(StageCmd);
   StageCmd = strdup(val);
   return 0;
}

/******************************************************************************/
/*                                  x s t l                                   */
/******************************************************************************/

/* Function: xstl

   Purpose:  To parse the directive: statlib <Options> <path> [<parms>]

   Options:  -2        use version 2 initialization interface (deprecated).
             -arevents forward add/remove events (server role cmsd only)
             -non2n    do not apply name2name prior to calling plug-in.
             -preopen  issue the stat() prior to opening the file.

             <path>    the path of the stat library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xstl(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val, parms[1040];

// Get the path or preopen option
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "statlib not specified"); return 1;}

// Check for options we support the old and new versions here
//
   STT_V2 = 0; STT_PreOp = 0; STT_DoN2N = 1; STT_DoARE = 0;
do{     if (!strcmp(val, "-2")) STT_V2    = 1;
   else if (!strcmp(val, "arevents") || !strcmp(val, "-arevents")) STT_DoARE=1;
   else if (!strcmp(val, "non2n")    || !strcmp(val, "-non2n"))    STT_DoN2N=0;
   else if (!strcmp(val, "preopen")  || !strcmp(val, "-preopen"))  STT_PreOp=1;
   else break;
  } while((val = Config.GetWord()) && val[0]);

// Make sure we have a statlib
//
   if (!val || !(*val))
      {Eroute.Emsg("Config", "statlib not specified"); return 1;}

// Record the path
//
   if (STT_Lib) free(STT_Lib);
   STT_Lib = strdup(val);

// Record any parms
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "statlib parameters too long"); return 1;}
   if (STT_Parms) free(STT_Parms);
   STT_Parms = (*parms ? strdup(parms) : 0);
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

int XrdOssSys::xtrace(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"open",     TRACE_Open},
        {"opendir",  TRACE_Opendir}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "trace option not specified"); return 1;}
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
                      Eroute.Say("Config warning: ignoring invalid trace option '",val,"'.");
                  }
          val = Config.GetWord();
         }
    OssTrace.What = trval;
    return 0;
}

/******************************************************************************/
/*                                x u s a g e                                 */
/******************************************************************************/

/* Function: xusage

   Purpose:  To parse the directive: usage <parms>

             <parms>: [nolog | log <path> [sync <num>]]
                      [noquotafile | quotafile <qfile>]

             nolog    does not save usage info across restarts
             log      saves usages information in the <path> directory
             sync     sync the usage file to disk every <num> changes.
             qfile    where the quota file resides.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xusage(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int usval;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "usage option not specified"); return 1;}

    while(val)
         {     if (!strcmp("nolog", val))
                  {if (UDir)  {free(UDir);  UDir = 0;}}
          else if (!strcmp("log"  , val))
                  {if (UDir)  {free(UDir);  UDir = 0;}
                   if (!(val = Config.GetWord()))
                      {Eroute.Emsg("Config", "usage log path not specified");
                       return 1;
                      }
                   if (*val != '/')
                      {Eroute.Emsg("Config", "usage log path not absolute");
                       return 1;
                      }
                   UDir = strdup(val);
                   if (!(val = Config.GetWord()) || strcmp("sync", val))
                      continue;
                   if (!(val = Config.GetWord()))
                      {Eroute.Emsg("Config", "log sync value not specified");
                       return 1;
                      }
                    if (XrdOuca2x::a2i(Eroute,"sync value",val,&usval,1,32767))
                       return 1;
                    USync = usval;
                  }
          else if (!strcmp("noquotafile",val))
                  {if (QFile) {free(QFile); QFile= 0;}}
          else if (!strcmp("quotafile",val))
                  {if (QFile) {free(QFile); QFile= 0;}
                   if (!(val = Config.GetWord()))
                      {Eroute.Emsg("Config", "quota file not specified");
                       return 1;
                      }
                   QFile = strdup(val);
                  }
          else {Eroute.Emsg("Config", "invalid usage option -",val); return 1;}

          val = Config.GetWord();
         }
    return 0;
}

/******************************************************************************/
/*                                  x x f r                                   */
/******************************************************************************/
  
/* Function: xxfr

   Purpose:  To parse the directive: xfr [deny <sec>] [keep <sec>] [up]
                                         [fdir <path>]
                                         [<threads> [<speed> [<ovhd> [<hold>]]]]

             deny      number of seconds to deny staging requests in the
                       presence of a '.fail' file.
             keep      number of seconds to keep queued requests
             fdir      the base directory where '.fail' files are to be written
             <threads> number of threads for staging (* uses default).

The following are deprecated and allowed for backward compatibility:

             <speed>   average speed in bytes/second (* uses default).
             <ovhd>    minimum seconds of overhead (* uses default).
             <hold>    seconds to hold failing requests (* uses default).

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xxfr(XrdOucStream &Config, XrdSysError &Eroute)
{
    static const int maxfdln = 256;
    const char *wantParm = 0;
    char *val;
    int       thrds = 1;
    long long speed = 9*1024*1024;
    int       ovhd  = 30;
    int       htime = 3*60*60;
    int       ktime;
    int       upon = 0;

    while((val = Config.GetWord()))        // deny |fdir | keep | up
         {     if (!strcmp("deny", val))
                  {wantParm = "xfr deny";
                   if ((val = Config.GetWord()))     // keep time
                      {if (XrdOuca2x::a2tm(Eroute,wantParm,val,&htime,0))
                          return 1;
                       wantParm=0;
                      }
                  }
          else if (!strcmp("fdir", val))
                  {wantParm = "xfr fdir";
                   if ((val = Config.GetWord()))     // fdir path
                      {if (xfrFdir) free(xfrFdir);
                       xfrFdln = strlen(val);
                       if (xfrFdln > maxfdln)
                          {Eroute.Emsg("Config","xfr fdir path too long");
                           xfrFdir = 0; xfrFdln = 0; return 1;
                          }
                       xfrFdir = strdup(val); 
                       wantParm = 0;
                      }
                  }
          else if (!strcmp("keep", val))
                  {wantParm = "xfr keep";
                   if ((val = Config.GetWord()))     // keep time
                      {if (XrdOuca2x::a2tm(Eroute,wantParm,val,&ktime,0))
                          return 1;
                       xfrkeep=ktime; wantParm=0;
                      }
                  }
          else if (!strcmp("up", val)) {upon = 1; wantParm = 0;}
          else break;
         };

    xfrhold    = htime;
    if (upon) OptFlags |= XrdOss_USRPRTY;

    if (!val) {if (!wantParm) return 0;
                  else {Eroute.Emsg("Config", wantParm, "value not specified");
                        return 1;
                       }
              }

      if (strcmp(val, "*") && XrdOuca2x::a2i(Eroute,"xfr threads",val,&thrds,1))
         return 1;

      if ((val = Config.GetWord()))         // <speed>
         {if (strcmp(val, "*") && 
              XrdOuca2x::a2sz(Eroute,"xfr speed",val,&speed,1024)) return 1;

          if ((val = Config.GetWord()))     // <ovhd>
             {if (strcmp(val, "*") && 
                  XrdOuca2x::a2tm(Eroute,"xfr overhead",val,&ovhd,0)) return 1;

              if ((val = Config.GetWord())) // <hold>
                 if (strcmp(val, "*") && 
                    XrdOuca2x::a2tm(Eroute,"xfr hold",val,&htime,0)) return 1;
             }
         }

      xfrhold    = htime;
      xfrthreads = thrds;
      xfrspeed   = speed;
      xfrovhd    = ovhd;
      return 0;
}

/******************************************************************************/
/*                            L i s t _ P a t h                               */
/******************************************************************************/
  
void XrdOssSys::List_Path(const char *pfx, const char *pname,
                          unsigned long long flags, XrdSysError &Eroute)
{
     std::string ss;
     const char *rwmode;

     if (flags & XRDEXP_FORCERO) rwmode = " forcero";
        else if (flags & XRDEXP_READONLY) rwmode = " r/o";
                else rwmode = " r/w";

     if (flags & XRDEXP_INPLACE) ss += " inplace";
     if (flags & XRDEXP_LOCAL)   ss += " local";
     if (flags & XRDEXP_GLBLRO)  ss += " globalro";

     if (!(flags & XRDEXP_PFCACHE))
        {if (flags & XRDEXP_PFCACHE_X) ss += " nocache";
         ss += (flags & XRDEXP_NOCHECK  ? " nocheck" : " check");
         ss += (flags & XRDEXP_NODREAD  ? " nodread" : " dread");
         ss += (flags & XRDEXP_MIG      ? " mig"     : " nomig");
         ss += (flags & XRDEXP_PURGE    ? " purge"   : " nopurge");
         ss += (flags & XRDEXP_RCREATE  ? " rcreate" : " norcreate");
         ss += (flags & XRDEXP_STAGE    ? " stage"   : " nostage");
        } else ss += " cache";


     if (flags & XRDEXP_MMAP)
        {ss += " mmap";
         ss += (flags & XRDEXP_MKEEP    ? " mkeep"   : " nomkeep");
         ss += (flags & XRDEXP_MLOK     ? " mlock"   : " nomlock");
        }

     Eroute.Say(pfx, pname, rwmode, ss.c_str());
}
