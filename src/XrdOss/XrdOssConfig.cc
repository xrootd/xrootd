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

#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssMio.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucStream.hh"

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOssSys    XrdOssSS;

extern XrdOucTrace  OssTrace;

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
       XRDOSS_T8025
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

#define TS_Add(x,m,v,s) if (!strcmp(x,var)) {m |= (v|s); return 0;}
#define TS_Rem(x,m,v,s) if (!strcmp(x,var)) {m = (m & ~v) | s; return 0;}

#define TS_Set(x,m,v)  if (!strcmp(x,var)) {m = v; return 0;}

#define xrdmax(a,b)       (a < b ? b : a)

#define XRDOSS_Prefix    "oss."
#define XRDOSS_PrefLen   sizeof(XRDOSS_Prefix)-1

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOssxfr(void *carg)       {return XrdOssSS.Stage_In(carg);}

void *XrdOssCacheScan(void *carg) {return XrdOssSS.CacheScan(carg);}

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
   XrdOucError_Table *ETab = new XrdOucError_Table(XRDOSS_EBASE, XRDOSS_ELAST,
                                                   XrdOssErrorText);
   char *val;
   int  retc, NoGo = XrdOssOK;
   pthread_t tid;

// Do the herald thing
//
   Eroute.Emsg("config", "Storage system initialization started.");
   Eroute.addTable(ETab);

// Preset all variables with common defaults
//
   ConfigDefaults();
   ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);

// Process the configuration file
//
   NoGo = ConfigProc(Eroute);

// Establish the FD limit
//
   {struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
       Eroute.Emsg("config", errno, "get resource limits");
       else Hard_FD_Limit = rlim.rlim_max;

    if (FDLimit <= 0) FDLimit = rlim.rlim_cur;
       else {rlim.rlim_cur = FDLimit;
            if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
               NoGo = Eroute.Emsg("config", errno,"set FD limit");
            }
    if (FDFence < 0 || FDFence >= FDLimit) FDFence = FDLimit >> 1;
   }

// Establish cached filesystems
//
   ReCache();

// Configure the MSS interface including staging
//
   if (!NoGo) NoGo = ConfigStage(Eroute);

// Configure async I/O
//
   if (!NoGo) NoGo = !AioInit();

// Initialize memory mapping setting to speed execution
//
   if (!NoGo) ConfigMio(Eroute);

// Start up the cache scan thread
//
   if ((retc = XrdOucThread::Run(&tid, XrdOssCacheScan, (void *)0,
                                 0, "cache scan")))
      Eroute.Emsg("config", retc, "create cache scan thread");

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
     XrdOucPList *fp;

     // Preset some tests
     //
     int HaveMSSgwCmd   = (MSSgwCmd   && MSSgwCmd[0]);
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
             OssTrace.What,
             xfrthreads, xfrspeed, xfrovhd, xfrhold);

     Eroute.Say(buff);

     XrdOssMio::Display(Eroute);

     fp = RPList.First();
     while(fp)
          {List_Path(fp->Path(), fp->Flag(), Eroute);
           fp = fp->Next();
          }
     if (!(XeqFlags & XrdOssROOTDIR)) List_Path((char *)"/", XeqFlags, Eroute);
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
       StageCmd      = 0;
       StageRealTime = 1;

   if (Configured && MSSgwCmd) free(MSSgwCmd);
       MSSgwCmd      = 0;

       cscanint      = XrdOssCSCANINT;
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
       xfrkeep       = 20*60;
       xfrthreads    = XrdOssXFRTHREADS;

   if (ConfigFN) {free(ConfigFN); ConfigFN = 0;}
   Configured = 1;
  if (getenv("XRDDEBUG")) OssTrace.What = TRACE_ALL;
}
  
/******************************************************************************/
/*                             C o n f i g M i o                              */
/******************************************************************************/
  
void XrdOssSys::ConfigMio(XrdOucError &Eroute)
{
     XrdOucPList *fp;
     int flags = 0, setoff = 0;

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
   if (XeqFlags & XrdOssMEMAP && !(XeqFlags & XrdOssNOTRW))
      XeqFlags |= XrdOssFORCERO;
   if (!(XeqFlags & XrdOssROOTDIR)) flags |= XeqFlags;
   if (XeqFlags & (XrdOssMLOK | XrdOssMKEEP)) XeqFlags |= XrdOssMMAP;

// Produce warnings if unsupported features have been selected
//
#if !defined(_POSIX_MAPPED_FILES)
   if (flags & XrdOssMEMAP)
      {Eroute.Emsg("Config", "Warning! Memory mapped files not supported; "
                             "feature disabled.");
       setoff = 1;
      }
#elif !defined(_POSIX_MEMLOCK)
   if (flags & XrdOssMLOK)
      {Eroute.Emsg("Config", "Warning! Memory locked files not supported; "
                             "feature disabled.");
       fp = RPList.First();
       while(fp)
            {fp->Set(fp->Flag() & ~XrdOssMLOK);
             fp = fp->Next();
            }
       XeqFlags = XeqFlags & ~XrdOssNLOK;
      }
#endif

// If no memory flags are set, turn off memory mapped files
//
   if (!(flags & XrdOssMEMAP) || setoff)
     {XrdOssMio::Set(0, 0, 0, 0, 0);
      tryMmap = 0; chkMmap = 0;
     }
}
  
/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdOssSys::ConfigProc(XrdOucError &Eroute)
{
  char *var;
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
      {Eroute.Emsg("config", errno, "open config file", ConfigFN);
       return 1;
      }
   Config.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = Config.GetFirstWord()))
        {if (!strncmp(var, XRDOSS_Prefix, XRDOSS_PrefLen))
            {var += XRDOSS_PrefLen;
             NoGo |= ConfigXeq(var, Config, Eroute);
            }
        }

// All done scanning the file, set dependent parameters.
//
   RemoteRootLen = strlen(RemoteRoot);
   LocalRootLen  = strlen(LocalRoot);

// Now check if any errors occured during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = Eroute.Emsg("config", retc, "read config file", ConfigFN);
   Config.Close();

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/*                           C o n f i g S t a g e                            */
/******************************************************************************/

int XrdOssSys::ConfigStage(XrdOucError &Eroute)
{
   char *tp, *gwp = 0, *stgp = 0;
   int dflags, flags, retc, numt, NoGo = 0;
   pthread_t tid;
   XrdOucPList *fp;

// A mssgwcmd implies mig and a stagecmd implies stage as defaults
//
   dflags = (MSSgwCmd ? XrdOssMIG : XrdOssNOCHECK|XrdOssNODREAD);
   if (!StageCmd) dflags |= XrdOssNOSTAGE;
   XeqFlags = XeqFlags | (dflags & (~(XeqFlags >> XrdOssMASKSHIFT)));
   if (MSSgwCmd && (XeqFlags & XrdOssMIG)) XeqFlags |= XrdOssREMOTE;
   RPList.Default(XeqFlags);

// Reprocess the paths to set correct defaults
//
   fp = RPList.First();
   while(fp) 
        {flags = fp->Flag();
         flags = flags | (dflags & (~(flags >> XrdOssMASKSHIFT)));
         if (!(flags & XrdOssNOSTAGE)) stgp = fp->Path();
         if (!(flags & XrdOssNOCHECK) || !(flags & XrdOssNODREAD) ||
              (flags & XrdOssRCREATE))  gwp = fp->Path();
         if (MSSgwCmd && (flags & XrdOssMIG)) flags |= XrdOssREMOTE;
         fp->Set(flags);
         fp = fp->Next();
        }

// Include the defaults if a root directory was not specified
//
   if (!(XeqFlags & XrdOssROOTDIR))
      {if (!(XeqFlags & XrdOssNOSTAGE)) stgp = (char *)"/";
       if (!(XeqFlags & XrdOssNOCHECK) || !(XeqFlags & XrdOssNODREAD) ||
           (XeqFlags & XrdOssRCREATE))   gwp = (char *)"/";
      }

// Check if we need or don't need the stagecmd
//
   if (stgp && !StageCmd)
      {Eroute.Emsg("config","Stageable path", stgp,
                            "present but stagecmd not specified.");
       NoGo = 1;
      }
      else if (StageCmd && !stgp)
              {Eroute.Emsg("config", "stagecmd ignored; no stageable paths present.");
               free(StageCmd); StageCmd = 0;
              }

// Check if we need or don't need the gateway
//
   if (gwp && !MSSgwCmd)
      {Eroute.Emsg("config","MSS path", gwp,
                            "present but mssgwcmd not specified.");
       NoGo = 1;
      }
      else if (MSSgwCmd && !gwp)
              {Eroute.Emsg("config", "mssgwcmd ignored; no MSS paths present.");
               free(MSSgwCmd); MSSgwCmd = 0;
              }

// If we have any errors at this point, just return failure
//
   if (NoGo) return 1;
   if (!MSSgwCmd && !StageCmd) return 0;
   Eroute.Emsg("config", "Mass Storage System interface initialization started.");

// Allocate a prgram object for the gateway command
//
   if (MSSgwCmd)
      {MSSgwProg = new XrdOucProg(&Eroute);
       if (MSSgwProg->Setup(MSSgwCmd)) NoGo = 1;
      }

// Initialize staging if we need to
//
   if (!NoGo && StageCmd)
      {
       // The stage command is interactive if it starts with an | (i.e., pipe in)
       //
          tp = StageCmd;
          while(*tp && *tp == ' ') tp++;
          if (*tp == '|') {StageRealTime = 0; StageCmd = tp+1;}

      // Set up a program object for the command
      //
         StageProg = new XrdOucProg(&Eroute);
         if (StageProg->Setup(StageCmd)) NoGo = 1;

      // For old-style real-time staging, create threads to handle the staging
      // For queue-style staging, start the program that handles the queue
      //
         if (!NoGo)
            if (StageRealTime)
               {if ((numt = xfrthreads - xfrtcount) > 0) while(numt--)
                    if ((retc = XrdOucThread::Run(&tid,XrdOssxfr,(void *)0,0,"staging")))
                       Eroute.Emsg("config", retc, "create staging thread");
                       else xfrtcount++;
               } else NoGo = StageProg->Start();
     }

// All done
//
   tp = (NoGo ? (char *)"failed." : (char *)"completed.");
   Eroute.Emsg("config", "Mass Storage System interface initialization", tp);
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
   TS_Add("compchk",       XeqFlags, XrdOssCOMPCHK, 0);
   TS_Add("forcero",       XeqFlags, XrdOssFORCERO,  XrdOssROW_X);
   TS_Add("readonly",      XeqFlags, XrdOssREADONLY, XrdOssROW_X);
   TS_Add("notwritable",   XeqFlags, XrdOssREADONLY, XrdOssROW_X);
   TS_Rem("writable",      XeqFlags, XrdOssNOTRW,    XrdOssROW_X);

   TS_Add("mig",           XeqFlags, XrdOssMIG,    XrdOssMIG_X);
   TS_Rem("nomig",         XeqFlags, XrdOssMIG,    XrdOssMIG_X);
   TS_Add("migratable",    XeqFlags, XrdOssMIG,    XrdOssMIG_X);
   TS_Rem("notmigratable", XeqFlags, XrdOssMIG,    XrdOssMIG_X);

   TS_Add("mkeep",         XeqFlags, XrdOssMKEEP,  XrdOssMKEEP_X);
   TS_Rem("nomkeep",       XeqFlags, XrdOssMKEEP,  XrdOssMKEEP_X);

   TS_Add("mlock",         XeqFlags, XrdOssMLOK,   XrdOssMLOK_X);
   TS_Rem("nomlock",       XeqFlags, XrdOssMLOK,   XrdOssMLOK_X);

   TS_Add("mmap",          XeqFlags, XrdOssMMAP,   XrdOssMMAP_X);
   TS_Rem("nommap",        XeqFlags, XrdOssMMAP,   XrdOssMMAP_X);

   TS_Rem("check",         XeqFlags, XrdOssNOCHECK, XrdOssCHECK_X);
   TS_Add("nocheck",       XeqFlags, XrdOssNOCHECK, XrdOssCHECK_X);

   TS_Rem("dread",         XeqFlags, XrdOssNODREAD, XrdOssDREAD_X);
   TS_Add("nodread",       XeqFlags, XrdOssNODREAD, XrdOssDREAD_X);

   TS_Rem("ssdec",         XeqFlags, XrdOssNOSSDEC, 0);
   TS_Add("nossdec",       XeqFlags, XrdOssNOSSDEC, 0);

   TS_Rem("stage",         XeqFlags, XrdOssNOSTAGE, XrdOssSTAGE_X);
   TS_Add("nostage",       XeqFlags, XrdOssNOSTAGE, XrdOssSTAGE_X);

   TS_Add("rcreate",       XeqFlags, XrdOssRCREATE, XrdOssRCREATE_X);
   TS_Rem("norcreate",     XeqFlags, XrdOssRCREATE, XrdOssRCREATE_X);

   TS_Add("userprty",      XeqFlags, XrdOssUSRPRTY, 0);

   TS_Xeq("alloc",         xalloc);
   TS_Xeq("cache",         xcache);
   TS_Xeq("cachescan",     xcachescan);
   TS_Xeq("compdetect",    xcompdct);
   TS_Xeq("fdlimit",       xfdlimit);
   TS_Xeq("maxsize",       xmaxdbsz);
   TS_Xeq("memfile",       xmemf);
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

   // Accepts options that used to be valid
   //
   if (!strcmp("mssgwpath", var)) return 0;
   if (!strcmp("gwbacklog", var)) return 0;

   // We need to suck all the tokens to the end of the line for remaining
   // options. Do so, until we run out of space in the buffer
   //
   bp = buff; blen = sizeof(buff)-1;

   do {if ((vlen = strlen(val)) >= blen)
          {Eroute.Emsg("config", "arguments too long for", var); return 1;}
       *bp = ' '; bp++; strcpy(bp, val); bp += vlen;
       } while((val = Config.GetWord()));

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
                         100 - reduces to simple round-robin allocation

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
    if (k >= (int)(sizeof(fn)-1) || val[0] != '/' || k < 2)
       {Eroute.Emsg("config", "invalid cache path - ", val); return 1;}

    if (val[k-1] != '*')
       {for (i = k-1; i; i--) if (val[i] != '/') break;
        fn[i+1] = '/'; fn[i+2] = '\0';
        while (i >= 0) {fn[i] = val[i]; i--;}
        return !xcacheBuild(grp, fn, Eroute);
       }

    for (i = k-1; i; i--) if (val[i] == '/') break;
    i++; strncpy(fn, val, i); fn[i] = '\0';
    sfxdir = &fn[i]; pfxdir = &val[i]; pfxln = strlen(pfxdir)-1;
    if (!(DFD = opendir(fn)))
       {Eroute.Emsg("config", errno, "open cache directory", fn); return 1;}

    errno = 0;
    while((dp = readdir(DFD)))
         {if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")
          || (pfxln && strncmp(dp->d_name, pfxdir, pfxln)))
             continue;
          strcpy(sfxdir, dp->d_name);
          if (stat(fn, &buff)) break;
          if (buff.st_mode & S_IFDIR)
             {val = sfxdir + strlen(sfxdir) - 1;
             if (*val++ != '/') {*val++ = '/'; *val = '\0';}
             if (xcacheBuild(grp, fn, Eroute)) cnum++;
                else {closedir(DFD); return 1;}
             }
          errno = 0;
         }

    if ((rc = errno))
       Eroute.Emsg("config", errno, "process cache directory", fn);
       else if (!cnum) Eroute.Emsg("config","no cache directories found in ",val);

    closedir(DFD);
    return rc != 0;
}

int XrdOssSys::xcacheBuild(char *grp, char *fn, XrdOucError &Eroute)
{
    XrdOssCache_FS *fsp;
    int rc;
    if (!(fsp = new XrdOssCache_FS(rc, grp, fn)))
       {Eroute.Emsg("config", ENOMEM, "create cache", fn); return 0;}
    if (rc)
       {Eroute.Emsg("config", rc, "create cache", fn);
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
    if (XrdOuca2x::a2tm(Eroute, "cachescan", val, &cscan, 30)) return 1;
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
         else if (XrdOuca2x::a2i(Eroute,"fdlimit fence",val,&fence,0)) return 1;

      if (!(val = Config.GetWord())) fdmax = -1;
         else if (!strcmp(val, "max")) fdmax = Hard_FD_Limit;
                 else if (XrdOuca2x::a2i(Eroute, "fdlimit value", val, &fdmax,
                              xrdmax(fence,XrdOssFDMINLIM))) return -EINVAL;
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
    if (XrdOuca2x::a2sz(Eroute, "maxdbsize", val, &mdbsz, 1024*1024)) return 1;
    MaxDBsize = mdbsz;
    return 0;
}

/******************************************************************************/
/*                                 x m e m f                                  */
/******************************************************************************/
  
/* Function: xmemf

   Purpose:  Parse the directive: memfile [off] [max <msz>]
                                          [check {keep | lock | map}] [preload]

             check keep Maps files that have ".mkeep" shadow file, premanently.
             check lock Maps and locks files that have ".mlock" shadow file.
             check map  Maps files that have ".mmap" shadow file.
             all        Preloads the complete file into memory.
             off        Disables memory mapping regardless of other options.
             on         Enables memory mapping
             preload    Preloads the file after every opn reference.
             <msz>      Maximum amount of memory to use (can be n% or real mem).

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xmemf(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    int i, j, V_autolok=-1, V_automap=-1, V_autokeep=-1, V_preld = -1, V_on=-1;
    long long V_max = 0;

    static struct mmapopts {const char *opname; int otyp;
                            const char *opmsg;} mmopts[] =
       {
        {"off",        0, ""},
        {"preload",    1, "memfile preload"},
        {"check",      2, "memfile check"},
        {"max",        3, "memfile max"}};
    int numopts = sizeof(mmopts)/sizeof(struct mmapopts);

    if (!(val = Config.GetToken()))
       {Eroute.Emsg("Config", "memfile option not specified"); return 1;}

    while (val)
         {for (i = 0; i < numopts; i++)
              if (!strcmp(val, mmopts[i].opname)) break;
          if (i >= numopts)
             Eroute.Emsg("Config", "Warning, invalid memfile option", val);
             else {if (mmopts[i].otyp >  1 && !(val = Config.GetToken()))
                      {Eroute.Emsg("Config","memfile",mmopts[i].opname,
                                   "value not specified");
                       return 1;
                      }
                   switch(mmopts[i].otyp)
                         {case 1: V_preld = 1;
                                  break;
                          case 2:     if (!strcmp("lock", val)) V_autolok=1;
                                 else if (!strcmp("map",  val)) V_automap=1;
                                 else if (!strcmp("keep", val)) V_autokeep=1;
                                 else {Eroute.Emsg("Config",
                                       "mmap auto neither keep, lock, nor map");
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
                  val = Config.GetToken();
                 }
         }

// Set the values
//
   XrdOssMio::Set(V_on, V_preld, V_autolok, V_automap, V_autokeep);
   XrdOssMio::Set(V_max);
   return 0;
}

/******************************************************************************/
/*                                 x p a t h                                  */
/******************************************************************************/

/* Function: xpath

   Purpose:  To parse the directive: path <path> [<options>]

             <path>    the full path that resides in a remote system.
             <options> a blank separated list of options:
                       [no]dread    - [don't] read actual directory contents
                           forcero  - force r/w opens to r/o opens
                           inplace  - do not use extended cache for creation
                       [no]mig      - this is [not] a migratable name space
                       [no]mkeep    - this is [not] a memory keepable name space
                       [no]mlock    - this is [not] a memory lockable name space
                       [no]mmap     - this is [not] a memory mappable name space
                       [no]check    - [don't] check remote filesystem when creating
                       [no]stage    - [don't] stage in files.
                           r/o      - do not allow modifications (read/only)
                           r/w      - path is writable/modifiable

   Output: 0 upon success or !0 upon failure.
*/

int XrdOssSys::xpath(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val, *path;
    static struct rpathopts 
           {const char *opname; int oprem; int opadd; int opset;} rpopts[] =
       {
        {"compchk",       0,             XrdOssCOMPCHK, 0},
        {"r/o",           0,             XrdOssREADONLY,XrdOssROW_X},
        {"forcero",       0,             XrdOssFORCERO, XrdOssROW_X},
        {"notwritable",   0,             XrdOssREADONLY,XrdOssROW_X},
        {"writable",      XrdOssNOTRW,   0,             XrdOssROW_X},
        {"r/w",           XrdOssNOTRW,   0,             XrdOssROW_X},
        {"inplace",       0,             XrdOssINPLACE, 0},
        {"nomig",         XrdOssMIG,     0,             XrdOssMIG_X},
        {"mig",           0,             XrdOssMIG,     XrdOssMIG_X},
        {"notmigratable", XrdOssMIG,     0,             XrdOssMIG_X},
        {"migratable",    0,             XrdOssMIG,     XrdOssMIG_X},
        {"nomkeep",       XrdOssMKEEP,   0,             XrdOssMKEEP_X},
        {"mkeep",         0,             XrdOssMKEEP,   XrdOssMKEEP_X},
        {"nomlock",       XrdOssMLOK,    0,             XrdOssMLOK_X},
        {"mlock",         0,             XrdOssMLOK,    XrdOssMLOK_X},
        {"nommap",        XrdOssMMAP,    0,             XrdOssMMAP_X},
        {"mmap",          0,             XrdOssMMAP,    XrdOssMMAP_X},
        {"nostage",       0,             XrdOssNOSTAGE, XrdOssSTAGE_X},
        {"stage",         XrdOssNOSTAGE, 0,             XrdOssSTAGE_X},
        {"dread",         XrdOssNODREAD, 0,             XrdOssDREAD_X},
        {"nodread",       0,             XrdOssNODREAD, XrdOssDREAD_X},
        {"check",         XrdOssNOCHECK, 0,             XrdOssCHECK_X},
        {"nocheck",       0,             XrdOssNOCHECK, XrdOssCHECK_X},
        {"rcreate",       0,             XrdOssRCREATE, XrdOssRCREATE_X},
        {"norcreate",     XrdOssRCREATE, 0,             XrdOssRCREATE_X}
       };
    int xspec, i, rpval = 0;
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
                  {rpval = (rpval & ~rpopts[i].oprem)|rpopts[i].opadd|rpopts[i].opset;
                   break;
                  }
              }
         if (i >= numopts) 
            Eroute.Emsg("config", "warning, invalid path option", val);
         val = Config.GetWord();
         }

// Include current defaults for unspecified options
//
   xspec = rpval >> XrdOssMASKSHIFT;
   rpval = rpval | (XeqFlags & ~xspec);
   if (!strcmp("/", path)) XeqFlags |= XrdOssROOTDIR;

// Make sure that we have no conflicting options
//
   if ((rpval & XrdOssMEMAP) && !(rpval & XrdOssNOTRW))
      {Eroute.Emsg("config", "warning, file memory mapping forced path", path,
                             "to be readonly");
       rpval |= XrdOssFORCERO;
      }
   if (rpval & (XrdOssMLOK | XrdOssMKEEP)) rpval |= XrdOssMMAP;

// Add the path to the list of paths
//
   RPList.Insert(new XrdOucPList(path, rpval));
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
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug},
        {"open",     TRACE_Open},
        {"opendir",  TRACE_Opendir}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("config", "trace option not specified"); return 1;}
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

   Purpose:  To parse the directive: xfr [keep <sec>] 
                                         [<threads> [<speed> [<ovhd> [<hold>]]]]

             keep      number of seconds to keep queued requests
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
    int       ktime;
    int       haveparm = 0;

    while((val = Config.GetWord()))        // <threads> | keep
         {if (!strcmp("keep", val))
             {if ((val = Config.GetWord()))     // keep time
                 if (XrdOuca2x::a2tm(Eroute,"xfr keep",val,&ktime,0)) return 1;
                    else {xfrkeep=ktime; haveparm=1;}
             }
             else break;
         };

    if (!val)
       if (haveparm) return 0;
          else {Eroute.Emsg("config", "xfr parameter not specified");
                return 1;
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

      xfrthreads = thrds;
      xfrspeed   = speed;
      xfrovhd    = ovhd;
      xfrhold    = htime;
      return 0;
}

/******************************************************************************/
/*                            L i s t _ F l i s t                             */
/******************************************************************************/
  
void XrdOssSys::List_Path(char *pname, int flags, XrdOucError &Eroute)
{
     char buff[4096], *rwmode;

     if (flags & XrdOssFORCERO) rwmode = (char *)" forcero";
        else if (flags & XrdOssREADONLY) rwmode = (char *)" r/o ";
                else rwmode = (char *)" r/w ";
                                   //        0 1 2 3 4 5 6 7 8 9 0 1
     snprintf(buff, sizeof(buff), "oss.path %s%s%s%s%s%s%s%s%s%s%s%s",
              pname,                                               // 0
              (flags & XrdOssCOMPCHK ?  " compchk" : ""),          // 1
              rwmode,                                              // 2
              (flags & XrdOssINPLACE  ? " inplace" : ""),          // 3
              (flags & XrdOssNOCHECK  ? " nocheck" : " check"),    // 4
              (flags & XrdOssNODREAD  ? " nodread" : " dread"),    // 5
              (flags & XrdOssMIG      ? " mig"     : " nomig"),    // 6
              (flags & XrdOssMKEEP    ? " mkeep"   : " nomkeep"),  // 7
              (flags & XrdOssMLOK     ? " mlock"   : " nomlock"),  // 8
              (flags & XrdOssMMAP     ? " mmap"    : " nommap"),   // 9
              (flags & XrdOssRCREATE  ? " rcreate" : " norcreate"),// 0
              (flags & XrdOssNOSTAGE  ? " nostage" : " stage")     // 1
              );
     Eroute.Say(buff); 
}
