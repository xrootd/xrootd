/******************************************************************************/
/*                                                                            */
/*                       X r d O s s C o n f i g . c c                        */
/*                                                                            */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

const char *XrdOssConfigCVSID = "$Id$";

/*
   The routines in this file handle initialization. They get the
   configuration values either from configuration file or XrdOssconfig.h (in that
   order of precedence).

   These routines are thread-safe if compiled with:
   AIX: -D_THREAD_SAFE
   SUN: -D_REENTRANT
*/
  
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <iostream.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "Experiment/Experiment.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucStream.hh"

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOssSys    XrdOssSS;

extern XrdOucTrace     OssTrace;

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
       XRDOSS_T8022
      };

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define Duplicate(x,y) if (y) free(y); y = strdup(x)

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(Config, Eroute);

#define TS_String(x,m) if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_List(x,m,v) if (!strcmp(x,var)) \
                          {m.Insert(new XrdOucPList(val, v); return 0;}

#define TS_Char(x,m)   if (!strcmp(x,var)) {m = val[0]; return 0;}

#define TS_Add(x,m,v)  if (!strcmp(x,var)) {m |= v; return 0;}

#define TS_Set(x,m,v)  if (!strcmp(x,var)) {m = v; return 0;}

#define max(a,b)       (a < b ? b : a)

#define XRDOSS_Prefix    "oss."
#define XRDOSS_PrefLen   sizeof(XRDOSS_Prefix)-1

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
extern "C"
{

void *XrdOssxfr(void *carg)       {return XrdOssSS.Stage_In(carg);}

void *XrdOssCacheScan(void *carg) {return XrdOssSS.CacheScan(carg);}

}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOssSys::Configure(const char *configfn, XrdOucError &Eroute)
{
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   const char *epname = "Configure";
   XrdOucError_Table *ETab = new XrdOucError_Table(XRDOSS_EBASE, XRDOSS_ELAST,
                                                   XrdOssErrorText);
   char *val;
   int  retc, numt, NoGo = XrdOssOK;
   pthread_t tid;

// Do the herald thing
//
   Eroute.Emsg("config", "Storage system initialization started.");
   Eroute.addTable(ETab);

// Serialize all execution by simply locking the remote file list. We also
// empty the list at this point.
//
   RPList.Empty();
   RPList.Lock();

// Preset all variables with common defaults
//
   ConfigDefaults();
   ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);

// Process the configuration file
//
   NoGo = ConfigProc(Eroute);

// Qualify the gatheway path to make it unique
//
   {char buff[256]; int gwl;
    gwl = strlen(MSSgwPath);
    if (gwl > sizeof(buff)-16)
       NoGo = Eroute.Emsg("config", ENAMETOOLONG, "excessive gateway path -",
                          MSSgwPath);
       else {sprintf(buff, "%s.%d", MSSgwPath, getpid());
             Duplicate(buff, MSSgwPath);
            }
   }

// Establish the FD limit
//
   {struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
       Eroute.Emsg("config", errno, "getting resource limits");
       else Hard_FD_Limit = rlim.rlim_max;

    if (FDLimit <= 0) FDLimit = rlim.rlim_cur;
       else {rlim.rlim_cur = FDLimit;
            if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
               NoGo = Eroute.Emsg("config", errno,"setting FD limit");
            }
    if (FDFence < 0 || FDFence >= FDLimit) FDFence = FDLimit >> 1;
   }

// Establish cacheed filesystems
//
   ReCache();

// Start-up the required number of staging threads
//
   if ((numt = xfrthreads - xfrtcount) > 0) while(numt--)
      if ((retc = XrdOucThread_Run(&tid, XrdOssxfr, (void *)0))<0)
         Eroute.Emsg("config", retc, "creating staging thread");
         else {DEBUG("started staging thread; tid=" <<(unsigned int)tid);
               xfrtcount++;
              }

// Start up the cache scan thread
//
   if ((retc = XrdOucThread_Run(&tid, XrdOssCacheScan, (void *)0))<0)
      Eroute.Emsg("config", retc, "creating cache scan thread");
      else DEBUG("started cache scan thread; tid=" <<(unsigned int)tid);

// Reinitialize the remote list. This must be the last act
//
   RPList.Swap(Config_RPList);
   RPList.UnLock();
   Config_RPList.Empty();

// All done, close the stream and return the return code.
//
   val = (NoGo ? (char *)"failed." : (char *)"completed.");
   Eroute.Emsg("config", "Storage system initialization", val);
   return NoGo;
}
  
/******************************************************************************/
/*                   o o s s _ C o n f i g _ D i s p l a y                    */
/******************************************************************************/
  
#define XrdOssConfig_Val(base, opt) \
             (Have ## base  ? "oss." #opt " " : ""), \
             (Have ## base  ? base     : ""), \
             (Have ## base  ? "\n"     : "")

void XrdOssSys::Config_Display(XrdOucError &Eroute)
{
     char buff[4096], *cloc;

     // Preset some tests
     //
     int HaveMSSgwCmd   = (MSSgwCmd   && MSSgwCmd[0]);
     int HaveMSSgwPath  = (MSSgwPath  && MSSgwPath[0]);
     int HaveStageCmd   = (StageCmd   && StageCmd[0]);
     int HaveRemoteRoot = (RemoteRoot && RemoteRoot[0]);
     int HaveLocalRoot  = (LocalRoot  && LocalRoot[0]);

     if (!ConfigFN || !ConfigFN[0]) cloc = (char *)"Default";
        else cloc = ConfigFN;

     snprintf(buff, sizeof(buff), "%s oss configuration:\n"
                                  "oss.alloc        %lld %d %d\n"
                                  "oss.cachescan    %d\n"
                                  "oss.compdetect   %s\n"
                                  "oss.fdlimit      %d %d\n"
                                  "oss.maxdbsize    %lld\n"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s"
                                  "%s%s%s%s%s%s%s%s%s"
                                  "oss.trace        %x\n"
                                  "oss.xfr          %d %d %d %d",
             cloc,
             minalloc, ovhalloc, fuzalloc,
             cscanint,
             (CompSuffix ? CompSuffix : "*"),
             FDFence, FDLimit, MaxDBsize,
             XrdOssConfig_Val(LocalRoot,  localroot),
             XrdOssConfig_Val(RemoteRoot, remoteroot),
             XrdOssConfig_Val(StageCmd,   stagecmd),
             XrdOssConfig_Val(MSSgwCmd,   mssgwcmd),
             XrdOssConfig_Val(MSSgwPath,  mssgwpath),
             (XeqFlags & XrdOssCOMPCHK ? "oss.compchk\n" : ""),
             (XeqFlags & XrdOssFORCERO ? "oss.forcero\n" : ""),
             (XeqFlags & XrdOssNOCHECK ? "oss.nocheck\n" : ""),
             (XeqFlags & XrdOssNODREAD ? "oss.nodread\n" : ""),
             (XeqFlags & XrdOssNOSSDEC ? "oss.nossdec\n" : ""),
             (XeqFlags & XrdOssNOSTAGE ? "oss.nostage\n" : ""),
             (XeqFlags & XrdOssPRUNED  ? "oss.pruned\n"  : ""),
             (XeqFlags & XrdOssRCREATE ? "oss.rcreate\n" : ""),
             (XeqFlags & XrdOssREADONLY? "oss.readonly\n": ""),
             OssTrace.What,
             xfrthreads, xfrspeed, xfrovhd, xfrhold);

     Eroute.Say(buff);

     List_Flist((char *)"oss.path  ", RPList, Eroute);
     List_Cache((char *)"oss.cache ", 0, Eroute);
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/

/******************************************************************************/
/*                        C o n f i g D e f a u l t s                         */
/******************************************************************************/
  
void XrdOssSys::ConfigDefaults(void)
{

// Preset all variables with common defaults
//
   if (Configured && LocalRoot) free(LocalRoot);
       LocalRoot     = strdup(XrdOssLOCALROOT);
       LocalRootLen  = strlen(LocalRoot);

   if (Configured && RemoteRoot) free(RemoteRoot);
       RemoteRoot    = strdup(XrdOssREMOTEROOT);
       RemoteRootLen = strlen(RemoteRoot);

   if (Configured && StageCmd) free(StageCmd);
       StageCmd      = strdup(XrdOssSTAGECMD);

   if (Configured && MSSgwCmd) free(MSSgwCmd);
       MSSgwCmd      = 0;
       MSSgwCmdLen   = 0;

   if (Configured && MSSgwPath) free(MSSgwPath);
       MSSgwPath     = strdup(XrdOssMSSGWPATH);

       cscanint      = XrdOssCSCANINT;
       gwBacklog     = XrdOssGWBACKLOG;
       FDFence       = -1;
       FDLimit       = XrdOssFDLIMIT;
       XeqFlags      = XrdOssXEQFLAGS;
       MaxDBsize     = XrdOssMAXDBSIZE;
       minalloc      = XrdOssMINALLOC;
       ovhalloc      = XrdOssOVRALLOC;
       fuzalloc      = XrdOssFUZALLOC;
       xfrspeed      = XrdOssXFRSPEED;
       xfrovhd       = XrdOssXFROVHD;
       xfrhold       = XrdOssXFRHOLD;
       xfrthreads    = XrdOssXFRTHREADS;

   if (ConfigFN) {free(ConfigFN); ConfigFN = 0;}
   Configured = 1;
  if (getenv("XRDDEBUG")) OssTrace.What = TRACE_ALL;
}
  
/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdOssSys::ConfigProc(XrdOucError &Eroute)
{
  char *bp, *var;
  int  cfgFD, retc, NoGo = XrdOssOK;
  XrdOucStream Config(&Eroute);

// If there is no config file, return with the defaults sets.
//
   if( !ConfigFN || !*ConfigFN)
     {Eroute.Emsg("config", "Config file not specified; defaults assumed.");
      return XrdOssOK;
     }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Eroute.Emsg("config", errno, "opening config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while( var = Config.GetFirstWord())
        {if (!strncmp(var, XRDOSS_Prefix, XRDOSS_PrefLen))
            {var += XRDOSS_PrefLen;
             NoGo |= ConfigXeq(var, Config, Eroute);
            }
        }

// All done scanning the file, set dependent parameters.
//
   RemoteRootLen = strlen(RemoteRoot);
   LocalRootLen  = strlen(LocalRoot);

// Make sure we are consistent here
//
   if ((!MSSgwCmd || !(MSSgwCmdLen = strlen(MSSgwCmd)))
   && XeqFlags & (XrdOssNEEDMSS | XrdOssREMOTE))
      {Eroute.Emsg("config", "MSS interface has been enabled but "
                                 "mssgwcmd not specified.");
       NoGo = 1;
      }

// Now check if any errors occured during file i/o
//
   if (retc = Config.LastError())
      NoGo = Eroute.Emsg("config", retc, "reading config file", ConfigFN);
   Config.Close();

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/

int XrdOssSys::ConfigXeq(char *var, XrdOucStream &Config, XrdOucError &Eroute)
{
    char  buff[2048], *bp, *val;
    int vlen, blen;

   // Process items that don't need a vlaue
   //
   TS_Add("compchk",       XeqFlags, XrdOssCOMPCHK);
   TS_Add("forcero",       XeqFlags, XrdOssFORCERO);
   TS_Add("mig",           XeqFlags, XrdOssREMOTE);
   TS_Add("migratable",    XeqFlags, XrdOssREMOTE);
   TS_Add("nocheck",       XeqFlags, XrdOssNOCHECK);
   TS_Add("nodread",       XeqFlags, XrdOssNODREAD);
   TS_Add("nossdec",       XeqFlags, XrdOssNOSSDEC);
   TS_Add("nostage",       XeqFlags, XrdOssNOSTAGE|XrdOssREMOTE);
   TS_Add("pruned",        XeqFlags, XrdOssPRUNED);
   TS_Add("rcreate",       XeqFlags, XrdOssRCREATE|XrdOssREMOTE);
   TS_Add("readonly",      XeqFlags, XrdOssREADONLY);
   TS_Add("userprty",      XeqFlags, XrdOssUSRPRTY);

   TS_Xeq("alloc",         xalloc);
   TS_Xeq("cache",         xcache);
   TS_Xeq("cachescan",     xcachescan);
   TS_Xeq("compdetect",    xcompdct);
   TS_Xeq("fdlimit",       xfdlimit);
   TS_Xeq("gwbacklog",     xgwbklg);
   TS_Xeq("maxsize",       xmaxdbsz);
   TS_Xeq("path",          xpath);
   TS_Xeq("trace",         xtrace);
   TS_Xeq("xfr",           xxfr);

   // At this point, make sure we have a value
   //
   if (!(val = Config.GetWord()))
      {Eroute.Emsg("config", "no value for", var); return 1;}

   // Now assign the appropriate global variable
   //
   TS_String("localroot",  LocalRoot);
   TS_String("remoteroot", RemoteRoot);
   TS_String("mssgwpath",  MSSgwPath);

   // We need to suck all the tokens to the end of the line for remaining
   // options. Do so, until we run out of space in the buffer
   //
   bp = buff; blen = sizeof(buff)-1;

   do {if ((vlen = strlen(val)) >= blen)
          {Eroute.Emsg("config", "arguments too long for", var); return 1;}
       *bp = ' '; bp++; strcpy(bp, val); bp += vlen;
       } while(val = Config.GetWord());

    *bp = '\0'; val = buff+1;

   // Check for tokens taking a variable number of parameters
   //
   TS_String("stagecmd",   StageCmd);
   TS_String("mssgwcmd",   MSSgwCmd);

   // No match found, complain.
   //
   Eroute.Emsg("config", "Warning, unknown directive", var);
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
                         quantities that may be ignored when selecting a cache
                           0 - reduces to finding the largest free space
                         100 - reduces to simple round-robbing allocation

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xalloc(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    long long mina = XrdOssMINALLOC;
    int       fuzz = XrdOssFUZALLOC;
    int       hdrm = XrdOssOVRALLOC;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "alloc minfree not specified"); return 1;}
    if (strcmp(val, "*") &&
        XrdOuca2x::a2sz(Eroute, "invalid alloc minfree", val, &mina, 0)) return 1;

    if (val = Config.GetWord())
       {if (strcmp(val, "*") &&
            XrdOuca2x::a2i(Eroute,"invalid alloc headroom",val,&hdrm,0,100)) return 1;

        if (val = Config.GetWord())
           {if (strcmp(val, "*") &&
            XrdOuca2x::a2i(Eroute, "invalid alloc fuzz", val, &fuzz, 0, 100)) return 1;
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

   Purpose:  To parse the directive: cache <group> <path>

             <group>  logical group name for the cache filesystem.
             <path>   path to the cache.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xcache(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val, *pfxdir, *sfxdir, grp[17], fn[XrdOssMAX_PATH_LEN+1];
    int i, k, rc, pfxln, cnum = 0;
    struct dirent *dp;
    struct stat buff;
    DIR *DFD;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "cache group not specified"); return 1;}
    if (strlen(val) >= sizeof(grp))
       {Eroute.Emsg("config", "invalid cache group - ", val); return 1;}
    strcpy(grp, val);

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "cache path not specified"); return 1;}

    k = strlen(val);
    if (k >= sizeof(fn)-1 || val[0] != '/' || k < 2)
       {Eroute.Emsg("config", "invalid cache path - ", val); return 1;}

    if (val[k-1] != '*')
       {for (i = k-1; i; i--) if (val[i] != '/') break;
        fn[i+1] = '/'; fn[i+2] = '\0';
        while (i >= 0) {fn[i] = val[i]; i--;}
        return !xcacheBuild(grp, fn, Eroute);
       }

    for (i = k-1; i; i--) if (val[i] == '/') break;
    i++; strncpy(fn, (const char *)val, i); fn[i] = '\0';
    sfxdir = &fn[i]; pfxdir = &val[i]; pfxln = strlen(pfxdir)-1;
    if (!(DFD = opendir((const char *)fn)))
       {Eroute.Emsg("config", errno, "opening cache directory", fn); return 1;}

    errno = 0;
    while(dp = readdir(DFD))
         {if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")
          || (pfxln && strncmp(dp->d_name, (const char *)pfxdir, pfxln)))
             continue;
          strcpy(sfxdir, dp->d_name);
          if (stat((const char *)fn, &buff)) break;
          if (buff.st_mode & S_IFDIR)
             if (xcacheBuild(grp, fn, Eroute)) cnum++;
                else {closedir(DFD); return 1;}
          errno = 0;
         }

    if (rc = errno)
       Eroute.Emsg("config", errno, "processing cache directory", fn);
       else if (!cnum) Eroute.Emsg("config","no cache directories found in ",val);

    closedir(DFD);
    return rc != 0;
}

int XrdOssSys::xcacheBuild(char *grp, char *fn, XrdOucError &Eroute)
{
    XrdOssCache_FS *fsp;
    if (!(fsp = new XrdOssCache_FS((const char *)grp, (const char *)fn)))
       {Eroute.Emsg("config", ENOMEM, "creating cache", fn); return 0;}
    if (!(fsp->path))
       {Eroute.Emsg("config", (int)fsp->fsdata, "creating cache", fn);
        delete fsp;
        return 0;
       }
    return 1;
}

/******************************************************************************/
/*                              x c o m p d c t                               */
/******************************************************************************/

/* Function: xcompdct

   Purpose:  To parse the directive: compdetect { * | <sfx>}

             *        perform autodetect for compression
             <sfx>    path suffix to indicate that file is compressed

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xcompdct(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    XrdOssCache_FS *fsp;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "compdetect suffix not specified"); return 1;}

    if (CompSuffix) free(CompSuffix);
    CompSuffix = 0; CompSuflen = 0;

    if (!strcmp("*", val))
       {CompSuffix = strdup(val); CompSuflen = strlen(val);}

    return 0;
}

/******************************************************************************/
/*                            x c a c h e s c a n                             */
/******************************************************************************/

/* Function: xcachescan

   Purpose:  To parse the directive: cachescan <num>

             <num>     number of seconds between cache scans.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOssSys::xcachescan(XrdOucStream &Config, XrdOucError &Eroute)
{   int cscan = 0;
    char *val;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "cachescan not specified"); return 1;}
    if (XrdOuca2x::a2tm(Eroute, "invalid cachescan", val, &cscan, 30)) return 1;
    cscanint = cscan;
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

int XrdOssSys::xfdlimit(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    int fence = 0, fdmax = XrdOssFDLIMIT;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("config", "fdlimit fence not specified"); return 1;}

      if (!strcmp(val, "*")) fence = -1;
         else if (XrdOuca2x::a2i(Eroute,"invalid fdlimit fence",val,&fence,0)) return 1;

      if (!(val = Config.GetWord())) fdmax = -1;
         else if (!strcmp(val, "max")) fdmax = Hard_FD_Limit;
                 else if (XrdOuca2x::a2i(Eroute, "invalid fdlimit value", val, &fdmax,
                              max(fence,XrdOssFDMINLIM))) return -EINVAL;
                         else if (fdmax > Hard_FD_Limit)
                                 {fdmax = Hard_FD_Limit;
                                  Eroute.Emsg("config", 
                                              "fdlimit forced to hard max");
                                 }
      FDFence = fence;
      FDLimit = fdmax;
      return 0;
}

/******************************************************************************/
/*                                g w b k l g                                 */
/******************************************************************************/

/* Function: gwbklg

   Purpose:  To parse the directive: gwbklg <num>

             <num>     maximum backlog allowed for gateway requests.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xgwbklg(XrdOucStream &Config, XrdOucError &Eroute)
{   int gwbklg = 0;
    char *val;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "gwbacklog not specified"); return 1;}
    if (XrdOuca2x::a2i(Eroute, "invalid gwbacklog", val, &gwbklg, 0)) return 1;
    gwBacklog = gwbklg;
    return 0;
}
  
/******************************************************************************/
/*                              x m a x d b s z                               */
/******************************************************************************/

/* Function: xmaxdbsz

   Purpose:  Parse the directive:  maxdbsize <num>

             <num> Maximum number of bytes in a database file.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xmaxdbsz(XrdOucStream &Config, XrdOucError &Eroute)
{   long long mdbsz;
    char *val;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "maxdbsize value not specified"); return 1;}
    if (XrdOuca2x::a2sz(Eroute, "invalid maxdbsize", val, &mdbsz, 1024*1024)) return 1;
    MaxDBsize = mdbsz;
    return 0;
}

/******************************************************************************/
/*                                 x p a t h                                  */
/******************************************************************************/

/* Function: xpath

   Purpose:  To parse the directive: path <path> [<options>]

             <path>    the full path that resides in a remote system.
             <options> a blank separated list of options:
                       dread     - read actual directory contents
                       forcero   - force r/w opens to r/o opens
                       inplace   - do not use extended cache for creation
                       mig       - this is a migratable name space
                       nocheck   - don't check remote filesystem when creating
                       nostage   - do not stage in files.
                       r/o       - do not allow modifications (read/only)

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xpath(XrdOucStream &Config, XrdOucError &Eroute, int rpval)
{
    char *val, *path;
    struct Flist *fp;
    static struct rpathopts { char * opname; int opval;} rpopts[] =
       {
       (char *)"compchk",    XrdOssCOMPCHK,
       (char *)"r/o",        XrdOssREADONLY,
       (char *)"forcero",    XrdOssFORCERO,
       (char *)"inplace",    XrdOssINPLACE,
       (char *)"mig",        XrdOssREMOTE,
       (char *)"migratable", XrdOssREMOTE,
       (char *)"nostage",    XrdOssNOSTAGE|XrdOssREMOTE,
       (char *)"nodread",    XrdOssNODREAD,
       (char *)"nocheck",    XrdOssNOCHECK,
       (char *)"pruned",     XrdOssPRUNED,
       (char *)"rcreate",    XrdOssRCREATE|XrdOssREMOTE
       };
    int i, pl;
    int numopts = sizeof(rpopts)/sizeof(struct rpathopts);

// Get the remote path
//
   path = Config.GetWord();
   if (!path || !path[0])
      {Eroute.Emsg("config", "remote path not specified"); return 1;}

// Process remaining options
//
   val = Config.GetWord();
   while (val)
         {for (i = 0; i < numopts; i++)
              {if (!strcmp(val, rpopts[i].opname))
                  {rpval |= rpopts[i].opval; break;}
              }
         if (i >= numopts) 
            Eroute.Emsg("config", "warning, invalid path option", val);
         val = Config.GetWord();
         }

// Add the path to the list of paths
//
   if (rpval & XrdOssREMOTE) XeqFlags |= XrdOssNEEDMSS;
   Config_RPList.Insert(new XrdOucPList(path, rpval));
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

int XrdOssSys::xtrace(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    static struct traceopts { char * opname; int opval;} tropts[] =
       {
       (char *)"all",      TRACE_ALL,
       (char *)"debug",    TRACE_Debug,
       (char *)"open",     TRACE_Open,
       (char *)"opendir",  TRACE_Opendir
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "trace option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) trval = 0;
             else {if (neg = (val[0] == '-' && val[1])) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, tropts[i].opname))
                           {if (neg) trval &= ~tropts[i].opval;
                               else  trval |=  tropts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      Eroute.Emsg("config", "invalid trace option", val);
                  }
          val = Config.GetWord();
         }
    OssTrace.What = trval;
    return 0;
}

/******************************************************************************/
/*                                  x x f r                                   */
/******************************************************************************/
  
/* Function: xxfr

   Purpose:  To parse the directive: xfr <threads> [<speed> [<ovhd> [<hold>]]]

             <threads> number of threads for staging (* uses default).
             <speed>   average speed in bytes/second (* uses default).
             <ovhd>    minimum seconds of overhead (* uses default).
             <hold>    seconds to hold failing requests (* uses default).

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xxfr(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    int       thrds = XrdOssXFRTHREADS;
    long long speed = XrdOssXFRSPEED;
    int       ovhd  = XrdOssXFROVHD;
    int       htime = XrdOssXFRHOLD;

      if (!(val = Config.GetWord()))       // <threads>
         {Eroute.Emsg("config", "xfr threads not specified"); return 1;}

      if (strcmp(val, "*") && XrdOuca2x::a2i(Eroute,"invalid xfr threads",val,&thrds,1))
         return 1;

      if (val = Config.GetWord())         // <speed>
         {if (strcmp(val, "*") && 
              XrdOuca2x::a2sz(Eroute,"invalid xfr speed",val,&speed,1024)) return 1;

          if (val = Config.GetWord())     // <ovhd>
             {if (strcmp(val, "*") && 
                  XrdOuca2x::a2tm(Eroute,"invalid xfr overhead",val,&ovhd,0)) return 1;

              if (val = Config.GetWord()) // <hold>
                 if (strcmp(val, "*") && 
                    XrdOuca2x::a2tm(Eroute,"invalid xfr hold",val,&htime,0)) return 1;
             }
         }

      xfrthreads = thrds;
      xfrspeed   = speed;
      xfrovhd    = ovhd;
      xfrhold    = htime;
      return 0;
}

/******************************************************************************/
/*                            L i s t _ F l i s t                             */
/******************************************************************************/
  
void XrdOssSys::List_Flist(char *lname, XrdOucPListAnchor &flist,
                          XrdOucError &Eroute)
{
     char buff[4096]; int flags;
     XrdOucPList *fp;

     flist.Lock();
     fp = flist.First();
     while(fp) {flags = fp->Flag();           //     0 1 2 3 4 5 6 7 8 9
                snprintf(buff, sizeof(buff), "%s %s %s%s%s%s%s%s%s%s%s%s",
                         lname, fp->Path(),
                         (flags & XrdOssCOMPCHK ?  " compchk" : ""),  // 0
                         (flags & XrdOssFORCERO  ? " forcero" : ""),  // 1
                         (flags & XrdOssREADONLY ? " r/o"     : ""),  // 2
                         (flags & XrdOssINPLACE  ? " inplace" : ""),  // 3
                         (flags & XrdOssREMOTE   ? " mig"     : ""),  // 4
                         (flags & XrdOssNOCHECK  ? " nocheck" : ""),  // 5
                         (flags & XrdOssNODREAD  ? " nodread" : ""),  // 6
                         (flags & XrdOssNOSTAGE  ? " nostage" : ""),  // 7
                         (flags & XrdOssPRUNED   ? " pruned"  : ""),  // 8
                         (flags & XrdOssRCREATE  ? " rcreate" : "")   // 9
                         );
                Eroute.Say(buff); 
                fp = fp->Next();
               }
     flist.UnLock();
}
