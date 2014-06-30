/******************************************************************************/
/*                                                                            */
/*                       X r d F r m C o n f i g . c c                        */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdVersion.hh"

#include "Xrd/XrdInfo.hh"
#include "XrdCks/XrdCksConfig.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdFrm/XrdFrmCns.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmMonitor.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetCmsNotify.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssSpace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucMsubs.hh"
#include "XrdOuc/XrdOucN2NLoader.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucSiteName.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysUtils.hh"

using namespace XrdFrc;
using namespace XrdFrm;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdFrmConfigSE
{
public:

XrdSysSemaphore mySem;
int             myFD;
int             seFD;
int             BLen;
char            Buff[32000];

                XrdFrmConfigSE() : mySem(0), myFD(-1), seFD(-1), BLen(0) {}
               ~XrdFrmConfigSE() {}
};

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/

void *XrdFrmConfigMum(void *parg)
{
   XrdFrmConfigSE *theSE = (XrdFrmConfigSE *)parg;
   char *bp = theSE->Buff;
   int  n, bleft = sizeof(theSE->Buff)-2;

// Let the calling thread continue at this point
//
   theSE->mySem.Post();

// Read everything we can
//
   do {if ((n = read(theSE->myFD, bp, bleft)) <= 0)
          {if (!n || (n < 0 && errno != EINTR)) break;}
       bp += n;
      } while ((bleft -= n));

// Refalgomize everything
//
   dup2(theSE->seFD, STDERR_FILENO);
   close(theSE->seFD);

// Check if we should add a newline character
//
   if (theSE->Buff[bp-(theSE->Buff)-1L] != '\n') *bp++ = '\n';
   theSE->BLen = bp-(theSE->Buff);

// All done
//
   theSE->mySem.Post();
   return (void *)0;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

  
XrdFrmConfig::XrdFrmConfig(SubSys ss, const char *vopts, const char *uinfo)
             : dfltPolicy("*", -2, -3, 72000, 0)
{
   static XrdVERSIONINFODEF(myVer, XrdFrm, XrdVNUMBER, XrdVERSION);
   char *sP, buff[128];

// Preset all variables with common defaults
//
   myVersion= &myVer;
   mySite   = 0;
   vOpts    = vopts;
   uInfo    = uinfo;
   ssID     = ss;
   AdminPath= 0;
   QPath    = 0;
   AdminMode= 0740;
   xfrMax   = 2;
   FailHold = 3*60*60;
   IdleHold = 10*60;
   WaitMigr = 60*60;
   WaitPurge= 600;
   WaitQChk = 300;
   MSSCmd   = 0;
   memset(&xfrCmd, 0, sizeof(xfrCmd));
   xfrCmd[0].Desc = "copycmd in";     xfrCmd[1].Desc = "copycmd out";
   xfrCmd[2].Desc = "copycmd in url"; xfrCmd[3].Desc = "copycmd out url";
   xfrIN    = xfrOUT = 0;
   isAgent  = (getenv("XRDADMINPATH") ? 1 : 0);
   ossLib   = 0;
   ossParms = 0;
   cmsPath  = 0;
   haveCMS  = 0;
   isOTO    = 0;
   Test     = 0;
   Verbose  = 0;
   pathList = 0;
   spacList = 0;
   lockFN   = "DIR_LOCK";  // May be ".DIR_LOCK" if hidden
   cmdHold  = -1;
   cmdFree  = 0;
   pVecNum  = 0;
   pProg    = 0;
   Fix      = 0;
   dirHold  = 40*60*60;
   runOld   = 0;
   runNew   = 1;
   nonXA    = 0;
   doStatPF = 0;

   myUid    = geteuid();
   myGid    = getegid();

   LocalRoot= RemoteRoot = 0;
   lcl_N2N  = rmt_N2N = the_N2N = 0;
   N2N_Lib  = N2N_Parms         = 0;
   CksAlg   = 0;
   CksCfg   = 0;
   CksMan   = 0;

   xfrFdir  = 0;
   xfrFdln  = 0;

// Establish our instance name
//
   myInst = XrdOucUtils::InstName(-1);

// Establish default config file
//
   if (!(sP = getenv("XRDCONFIGFN")) || !*sP) 
            ConfigFN = 0;
      else {ConfigFN = strdup(sP); isAgent = 1;}

// Establish directive prefix
//
        if (ss == ssAdmin) {myFrmid = "admin"; myFrmID = "ADMIN";}
   else if (ss == ssPurg)  {myFrmid = "purge"; myFrmID = "PURG";}
   else if (ss == ssXfr)   {myFrmid = "xfr";   myFrmID = "XFR"; }
   else                    {myFrmid = "frm";   myFrmID = "FRM";}

// Set correct error prefix
//
   strcpy(buff, myFrmid);
   strcat(buff, "_");
   Say.SetPrefix(strdup(buff));

// Set correct option prefix
//
   strcpy(buff, "frm.");
   strcat(buff, myFrmid);
   strcat(buff, ".");
   pfxDTS = strdup(buff); plnDTS = strlen(buff);
}
  
/******************************************************************************/
/* Public:                     C o n f i g u r e                              */
/******************************************************************************/
  
int XrdFrmConfig::Configure(int argc, char **argv, int (*ppf)())
{
   extern XrdOss *XrdOssGetSS(XrdSysLogger *, const char *, const char *,
                              const char   *, XrdOucEnv *,  XrdVersionInfo &);
   extern int *XrdOssRunMode;
   static XrdNetAddr myAddr(0);
   XrdFrmConfigSE theSE;
   int retc, isMum = 0, myXfrMax = -1, NoGo = 0, optBG = 0;
   const char *temp;
   char c, buff[1024], *logfn = 0;
   extern char *optarg;
   extern int opterr, optopt;
   int pipeFD[2] = {-1, -1}, bindArg = -1, pureLFN = 0;
   const char *pidFN = 0;

// Obtain the program name (used for logging)
//
    retc = strlen(argv[0]);
    while(retc--) if (argv[0][retc] == '/') break;
    myProg = &argv[0][retc+1];
    vectArg = argv; numcArg = argc;

// Process the options
//
   opterr = 0; nextArg = 1;
   while(nextArg < argc && '-' == *argv[nextArg]
         && (c=getopt(argc,argv,vOpts)) && (c != (char)-1))
     { switch(c)
       {
       case 'b': optBG = 1;
                 break;
       case 'c': if (ConfigFN) free(ConfigFN);
                 ConfigFN = strdup(optarg);
                 break;
       case 'd': Trace.What |= TRACE_ALL;
                 XrdOucEnv::Export("XRDDEBUG","1");
                 break;
       case 'f': Fix = 1;
                 break;
       case 'h': Usage(0);
       case 'k': if (!(bindArg = Say.logger()->ParseKeep(optarg)))
                    {Say.Emsg("Config","Invalid -k argument -",optarg);
                     Usage(1);
                    }
                 break;
       case 'l': if ((pureLFN = *optarg == '=')) optarg++;
                 if (!*optarg)
                    {Say.Emsg("Config", "Logfile name not specified.");
                     Usage(1);
                    }
                 if (logfn) free(logfn);
                 logfn = strdup(optarg);
                 break;
       case 'm': if (XrdOuca2x::a2i(Say,"max number",optarg,&myXfrMax))
                    Usage(1);
                 break;
       case 'n': myInst = (!strcmp(optarg,"anon")||!strcmp(optarg,"default")
                          ? 0 : optarg);
                 break;
       case 'O': isOTO = 1;
                 if (!ConfigOTO(optarg)) Usage(1);
                 break;
       case 'T': Test  = 1;
                 break;
       case 'v': Verbose = 1;
                 break;
       case 'w': if (XrdOuca2x::a2tm(Say,"wait time",optarg,&WaitPurge))
                    Usage(1);
                 break;
       case 's': pidFN = optarg;
                 break;
       case 'S': mySite= optarg;
                 break;
       case 'z': Say.logger()->setHiRes();
                 break;
       default:  sprintf(buff,"'%c'", optopt);
                 if (c == ':') Say.Emsg("Config", buff, "value not specified.");
                    else Say.Emsg("Config", buff, "option is invalid");
                 Usage(1);
       }
     nextArg = optind;
     }

// Set the site name if we have it at this point
//
   if (mySite) mySite = XrdOucSiteName::Set(mySite);

// If we are an agent without a logfile and one is actually defined for the
// underlying system, use the directory of the underlying system.
//
   if (ssID != ssAdmin)
      {if (!logfn)
          {if (isAgent && (logfn = getenv("XRDLOGDIR")))
              {snprintf(buff, sizeof(buff), "%s%s%clog", logfn, myFrmid,
                                           (isAgent ? 'a' : 'd'));
               logfn = strdup(buff);
              }
          } else if (!pureLFN
                 && !(logfn=XrdOucUtils::subLogfn(Say,myInst,logfn))) _exit(16);

   // If undercover desired and we are not an agent, do so
   //
       if (optBG && !isAgent)
       {
#ifdef WIN32
          XrdOucUtils::Undercover( Say, !logfn );
#else
          if (pipe( pipeFD ) == -1)
             {Say.Emsg("Config", errno, "create a pipe"); exit(17);}
          XrdOucUtils::Undercover( Say, !logfn, pipeFD );
#endif
       }

   // Bind the log file if we have one
   //
       if (logfn)
          {Say.logger()->AddMsg(XrdBANNER);
           if (Say.logger()->Bind(logfn, bindArg)) _exit(19);
          }
      }

// Get the full host name. In theory, we should always get some kind of name.
//
   if (!(myName = myAddr.Name()))
      {Say.Emsg("Config","Unable to determine host name; execution terminated.");
       _exit(16);
      }

// Set the Environmental variables to hold some config information
// XRDINSTANCE=<pgm> <instance name>@<host name>
//
   snprintf(buff,sizeof(buff), "XRDINSTANCE=%s %s@%s",myProg,
                 XrdOucUtils::InstName(myInst), myName);
   putenv(strdup(buff)); // XRDINSTANCE
   myInstance = strdup(index(buff,'=')+1);
   XrdOucEnv::Export("XRDHOST", myName);
   XrdOucEnv::Export("XRDPROG", myProg);
   XrdOucEnv::Export("XRDNAME", XrdOucUtils::InstName(myInst));

// We need to divert the output if we are in admin mode with no logfile
//
   if (!logfn && (ssID == ssAdmin || isOTO) && !Trace.What)
      isMum = ConfigMum(theSE);

// Add final message to the logger
//
   if (logfn)
      {char msgBuff[2048];
       strcpy(msgBuff, myInstance); strcat(msgBuff, " running.");
       Say.logger()->AddMsg(msgBuff);
      }

// Put out the herald
//
   sprintf(buff, "Scalla %s is starting. . .", myProg);
   Say.Say(0, buff);
   Say.Say(XrdBANNER);

// Process the configuration file.
//
   Say.Say("++++++ ", myInstance, " initialization started.");
   if (!ConfigFN || !*ConfigFN) ConfigFN = strdup("/opt/xrootd/etc/xrootd.cf");
   Say.Say("Config using configuration file ", ConfigFN);
   NoGo = ConfigProc();

// Create the correct admin path
//
   if (!NoGo) NoGo = ConfigPaths();

// Obtain and configure the oss (lightweight option only)
//
   if (!isAgent)
      {XrdOucEnv::Export("XRDREDIRECT", "Q");
       XrdOucEnv::Export("XRDOSSTYPE",  myFrmID);
       if (ssID == ssPurg) XrdOucEnv::Export("XRDOSSCSCAN", "off");
       if (!NoGo)
          {if (!(ossFS=XrdOssGetSS(Say.logger(), ConfigFN, ossLib, ossParms,
                                   0, *myVersion))) NoGo = 1;
              else {struct stat Stat;
                    doStatPF = ossFS->StatPF("/", &Stat) != -ENOTSUP;
                    runNew = !(runOld = XrdOssRunMode ? *XrdOssRunMode : 0);
                   }
          }
      }

// Now we can create a home directory for core files and do a cwd to it
//
   if (myInst) XrdOucUtils::makeHome(Say, myInst);

// Configure each specific component
//
   if (!NoGo) switch(ssID)
      {case ssAdmin: NoGo = (ConfigN2N() || ConfigMss() || ConfigCks());
                     break;
       case ssPurg:  if (!(NoGo = (ConfigMon(0) || ConfigMP("purgeable"))))
                        ConfigPF("frm_purged");
                     break;
       case ssXfr:   if (!isAgent && !(NoGo = (ConfigMon(1) || ConfigXfr())))
                        ConfigPF("frm_xfrd");
                     break;
       default:      break;
      }

// If we have a post-processing routine, invoke it
//
   if (!NoGo && ppf) NoGo = ppf();

   // if we call this it means that the daemon has forked and we are
   // in the child process
#ifndef WIN32
   if (optBG && !isAgent)
   {
      if (pidFN && !XrdOucUtils::PidFile( Say, pidFN ) )
         NoGo = 1;

      int status = NoGo ? 1 : 0;
      if(write( pipeFD[1], &status, sizeof( status ) )) {};
      close( pipeFD[1]);
   }
#endif

// Print ending message
//
   temp = (NoGo ? " initialization failed." : " initialization completed.");
   Say.Say("------ ", myInstance, temp);

// Finish up mum processing
//
   if (isMum)
      {close(STDERR_FILENO);
       theSE.mySem.Wait();
       if (NoGo && write(STDERR_FILENO, theSE.Buff, theSE.BLen)) {}
      }

// All done
//
   return !NoGo;
}
  
/******************************************************************************/
/* Public:                     L o c a l P a t h                              */
/******************************************************************************/
  
int XrdFrmConfig::LocalPath(const char *oldp, char *newp, int newpsz)
{
    int rc = 0;

    if (lcl_N2N) rc = lcl_N2N->lfn2pfn(oldp, newp, newpsz);
       else if (((int)strlen(oldp)) >= newpsz) rc = ENAMETOOLONG;
               else strcpy(newp, oldp);
    if (rc) {Say.Emsg("Config", rc, "generate local path from", oldp);
             return 0;
            }
    return 1;
}
  
/******************************************************************************/
/* Public:                   L o g i c a l P a t h                            */
/******************************************************************************/
  
int XrdFrmConfig::LogicalPath(const char *oldp, char *newp, int newpsz)
{
    int rc = 0;

    if (lcl_N2N) rc = lcl_N2N->pfn2lfn(oldp, newp, newpsz);
       else if (((int)strlen(oldp)) >= newpsz) rc = ENAMETOOLONG;
               else strcpy(newp, oldp);
    if (rc) {Say.Emsg("Config", rc, "generate logical path from", oldp);
             return 0;
            }
    return 1;
}

/******************************************************************************/
/* Public:                      N e e d s C T A                               */
/******************************************************************************/

int XrdFrmConfig::NeedsCTA(const char *Lfn)
{
   extern XrdOucPListAnchor *XrdOssRPList;

   return (XrdOssRPList->Find(Lfn) & XRDEXP_MAKELF) != 0;
}

/******************************************************************************/
/* Public:                      P a t h O p t s                               */
/******************************************************************************/

unsigned long long XrdFrmConfig::PathOpts(const char *Lfn)
{
   extern XrdOucPListAnchor *XrdOssRPList;

   return XrdOssRPList->Find(Lfn);
}

/******************************************************************************/
/* Public:                    R e m o t e P a t h                             */
/******************************************************************************/
  
int XrdFrmConfig::RemotePath(const char *oldp, char *newp, int newpsz)
{
    int rc = 0;

    if (rmt_N2N) rc = rmt_N2N->lfn2rfn(oldp, newp, newpsz);
       else if (((int)strlen(oldp)) >= newpsz) rc = ENAMETOOLONG;
               else strcpy(newp, oldp);
    if (rc) {Say.Emsg("Config", rc, "generate rmote path from", oldp);
             return 0;
            }
    return 1;
}
  
/******************************************************************************/
/*                                 S p a c e                                  */
/******************************************************************************/
  
XrdOucTList *XrdFrmConfig::Space(const char *Name, const char *Path)
{
   static XrdOucTList nullEnt;
   struct VPInfo *vP = VPList;
          XrdOucTList *tP;
   char buff[1032];
   int n;

// First find the space entry
//
   while(vP && strcmp(vP->Name, Name)) vP = vP->Next;
   if (!vP) return 0;

// Check if we should find a particular path
//
   if (!Path) return vP->Dir;

// Make sure it nds with a slash (it usually does not)
//
   n = strlen(Path)-1;
   if (Path[n] != '/')
      {if (n >= (int)sizeof(buff)-2) return &nullEnt;
       strcpy(buff, Path); buff[n+1] = '/'; buff[n+2] = '\0';
       Path = buff;
      }

// Find the path
//
   tP = vP->Dir;
   while(tP && strcmp(Path, tP->text)) tP = tP->next;
   return (tP ? tP : &nullEnt);
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/
  
int XrdFrmConfig::Stat(const char *xLfn, const char *xPfn, struct stat *buff)
{
   return (doStatPF ? ossFS->StatPF(xPfn, buff)
                    : ossFS->Stat  (xLfn, buff, XRDOSS_resonly));
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                             C o n f i g C k s                              */
/******************************************************************************/

int XrdFrmConfig::ConfigCks()
{

// If we have no algorithm, we are done
//
   if (!CksAlg) return 0;

// Configure the algorithm
//
   return !(CksMan = CksCfg->Configure(CksAlg));
}
  
/******************************************************************************/
/* Private:                    C o n f i g C m d                              */
/******************************************************************************/
  
XrdOucMsubs *XrdFrmConfig::ConfigCmd(const char *cname, char *cdata)
{
   XrdOucMsubs *msubs;
   char *cP;

   if (!cdata) {Say.Emsg("Config", cname, "not specified."); return 0;}

   if ((cP = index(cdata, ' '))) *cP = '\0';

   if (access(cdata, X_OK))
      {Say.Emsg("Config", errno, "set up", cdata);
       return 0;
      }
   if (cP) *cP = ' ';

   msubs = new XrdOucMsubs(&Say);
   if (msubs->Parse(cname, cdata)) return msubs;

   return 0;  // We will exit no need to delete msubs
}

/******************************************************************************/
/* Private:                    C o n f i g M o n                              */
/******************************************************************************/

int XrdFrmConfig::ConfigMon(int isXfr)
{
// We configure the name2name here
//
   if (ConfigN2N()) return 1;

// If we need to configure monitoring, do so here
//
   if (( isXfr && (XrdFrmMonitor::monSTAGE||XrdFrmMonitor::monMIGR))
   ||  (!isXfr &&  XrdFrmMonitor::monPURGE))
      {if (!XrdFrmMonitor::Init(myName, myProg, myInst)) return 1;
          else {if (!XrdFrmMonitor::monSTAGE)
                   {xfrCmd[0].Opts &= ~cmdXPD; xfrCmd[2].Opts &= ~cmdXPD;}
                if (!XrdFrmMonitor::monMIGR)
                   {xfrCmd[1].Opts &= ~cmdXPD; xfrCmd[3].Opts &= ~cmdXPD;}
               }
      }

// All done
//
   return 0;
}

/******************************************************************************/
/* Private:                     C o n f i g M P                               */
/******************************************************************************/

int XrdFrmConfig::ConfigMP(const char *pType)
{
   EPNAME("ConfigMP");
   extern XrdOucPListAnchor *XrdOssRPList;
   XrdOucTList *nP, *tP, *mypList = 0, *expList = 0;
   char pDir[MAXPATHLEN+1];
   long long pOpts, xOpt = (*pType == 'm' ? XRDEXP_MIG : XRDEXP_PURGE);
   int i, NoGo = 0;

// Verify that we have an RPList
//
   if (!XrdOssRPList)
      {Say.Emsg("Config", "Cannot determine", pType, "paths."); return 1;}

// Parse the arguments which consist of space names and paths
//
   for (i = nextArg; i < numcArg; i++)
       {char *psVal = vectArg[i];
        int   psLen = strlen(psVal);
        if (*psVal == '/')
           {pOpts = XrdOssRPList->Find(psVal);
            if (pOpts & xOpt) mypList = InsertPL(mypList, psVal, psLen,
                                                (pOpts & XRDEXP_MAKELF ? 1:0));
               else {Say.Say("Config", psVal, "not marked", pType); NoGo = 1;}
           } else {
            VPInfo *vP = VPList;
            while(vP && strcmp(psVal, vP->Name)) vP = vP->Next;
            if (vP) spacList = new XrdOucTList(psVal, psLen, spacList);
               else {Say.Emsg("Config", "Space", psVal, "not defined.");
                     NoGo = 1;
                    }
           }
       }

// Check if we should continue
//
   if (NoGo) return 1;

// Get correct path list
//
   if (!mypList)
      {XrdOucPList *fP = XrdOssRPList->First();
       short sval[4];    // Last two elements are unused
       while(fP)
            {sval[0] = (fP->Flag() & XRDEXP_MAKELF ? 1 : 0);
             sval[1] = fP->Plen();
             if (fP->Flag() & xOpt)
                 mypList = new XrdOucTList(fP->Path(), sval, mypList);
                 else
                 expList = new XrdOucTList(fP->Path(), sval, expList);
             fP = fP->Next();
            }
//     if (!mypList)
 //       {Say.Emsg("Config", "No", pType, "paths found."); return 1;}
      }

// Now we need to construct a search list which may include excludes which
// hapen when we get nested subtrees with different options
//
   while((tP = mypList))
        {if (!LocalPath(tP->text, pDir, sizeof(pDir))) NoGo = 1;
            else {pathList = new VPInfo(pDir, int(tP->sval[0]), pathList);
                  DEBUG("Will scan " <<(tP->sval[0]?"r/w: ":"r/o: ") <<pDir);
                  nP = expList;
                  while(nP)
                       {if (!strncmp(tP->text, nP->text, tP->sval[1]))
                           InsertXD(nP->text);
                        nP = nP->next;
                       }
                  mypList = tP->next; delete tP;
                 }
        }

// Delete the explist
//
   while((tP = expList)) {expList = tP->next; delete tP;}

// For purging, make sure we have at least one path to purge
//
   if (xOpt == XRDEXP_PURGE && !pathList)
      {Say.Emsg("Config","No purgeable paths specified!");
       NoGo = 1;
      }

// The oss would have already set NORCREATE and NOCHECK for all stageable paths.
// But now, we must also off the R/O flag on every purgeable and stageable path
// to prevent oss complaints. This needs to be defered to here because we need
// to know which paths are actually r/o and r/w.
//
   if (!NoGo)
      {XrdOucPList *fp = XrdOssRPList->First();
       while(fp)
            {if (fp->Flag() & (XRDEXP_STAGE | XRDEXP_PURGE))
                fp->Set(fp->Flag() & ~XRDEXP_NOTRW);
             fp = fp->Next();
            }
      }

// All done now
//
   return NoGo;
}

/******************************************************************************/
/* Private:                    C o n f i g M s s                              */
/******************************************************************************/
  
int XrdFrmConfig::ConfigMss()
{
   if (MSSCmd)
      {MSSProg = new XrdOucProg(&Say);
       if (MSSProg->Setup(MSSCmd)) return 1;
      }
   return 0;
}

/******************************************************************************/
/* Private:                    C o n f i g M u m                              */
/******************************************************************************/

int XrdFrmConfig::ConfigMum(XrdFrmConfigSE &theSE)
{
   class Recover
        {public:
         int fdvec[2];
         int stdErr;
             Recover() : stdErr(-1) {fdvec[0] = -1; fdvec[1] = -1;}
            ~Recover() {if (fdvec[0] >= 0) close(fdvec[0]);
                        if (fdvec[1] >= 0) close(fdvec[1]);
                        if (stdErr >= 0)   {dup2(stdErr, STDERR_FILENO);
                                            close(stdErr);
                                           }
                       }
        };
   Recover FD;
   pthread_t tid;
   int rc;

// Create a pipe
//
   if (pipe(FD.fdvec) < 0) return 0;
   fcntl(FD.fdvec[0], F_SETFD, FD_CLOEXEC);

// Save the current standard error FD
//
   if ((FD.stdErr = dup(STDERR_FILENO)) < 0) return 0;

// Now hook-up the pipe to standard error
//
   if (dup2(FD.fdvec[1], STDERR_FILENO) < 0) return 0;
   close(FD.fdvec[1]); FD.fdvec[1] = -1;

// Prepare arguments to the thread that will suck up the output
//
   theSE.myFD = FD.fdvec[0];
   theSE.seFD = FD.stdErr;

// Start a thread to read all of the output
//
    if ((rc = XrdSysThread::Run(&tid, XrdFrmConfigMum, (void *)&theSE,
                                XRDSYSTHREAD_BIND, "Mumify"))) return 0;

// Now fixup to return correctly
//
   theSE.mySem.Wait();
   FD.fdvec[0] = -1;
   FD.stdErr = -1;
   return 1;
}

/******************************************************************************/
/* Private:                    C o n f i g N 2 N                              */
/******************************************************************************/

int XrdFrmConfig::ConfigN2N()
{
   XrdOucN2NLoader n2nLoader(&Say,ConfigFN,N2N_Parms,LocalRoot,RemoteRoot);

// Check if we really need to configure this
//
   if (!N2N_Lib && !LocalRoot && !RemoteRoot) return 0;

// Get the plugin
//
   if (!(the_N2N = n2nLoader.Load(N2N_Lib, *myVersion))) return 1;

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
/*                             C o n f i g O T O                              */
/******************************************************************************/
  
int XrdFrmConfig::ConfigOTO(char *Parms)
{
   char *Comma;

// Pick up free argument
//
   if ((Comma = index(Parms, ','))) *Comma = '\0';
   if (XrdOuca2x::a2sp(Say, "free value", Parms, &cmdFree, 1)) return 0;

// Pick up hold argument
//
   if (!Comma || !(*(Comma+1))) return 1;
   if (*(Comma+1) == ',') Comma++;
      else {Parms = Comma+1;
            if ((Comma = index(Parms, ','))) *Comma = '\0';
            if (XrdOuca2x::a2i(Say,"hold value",Parms,&cmdHold,0)) return 0;
           }

// All done
//
   return 1;
}

/******************************************************************************/
/*                           C o n f i g P a t h s                            */
/******************************************************************************/
  
int XrdFrmConfig::ConfigPaths()
{
   char *xPath, buff[MAXPATHLEN];
   const char *insName;

// Get the directory for the meta information. If we don't get it from the
// config, then use XRDADMINPATH which already contains the instance name.
//
// Set the directory where the meta information is to go
// XRDADMINPATH already contains the instance name

        if ((xPath = AdminPath))              insName = myInst;
   else if ((xPath = getenv("XRDADMINPATH"))) insName = 0;
   else     {xPath = (char *)"/tmp/";         insName = myInst;}

// Do post initialization for the cnsd
//
   if (XrdFrmCns::Init(myFrmid, xPath, insName)) return 1;
   
// Establish the cmsd notification object. We need to do this using an
// unqualified admin path that we determined above.
//
   if (haveCMS)
      cmsPath = new XrdNetCmsNotify(&Say,xPath,insName,XrdNetCmsNotify::isServ);

// Create the admin directory if it does not exists and set QPath
//
   if (!(xPath = XrdFrcUtils::makePath(insName, xPath, AdminMode))) return 1;
   if (AdminPath) free(AdminPath); AdminPath = xPath;
   if (!QPath) QPath = AdminPath;

// Create the purge stop file name
//
   strcpy(buff, Config.AdminPath); strcat(buff, "STOPPURGE");
   StopPurge = strdup(buff);

// All done
//
   return 0;
}

/******************************************************************************/
/*                              C o n f i g P F                               */
/******************************************************************************/
  
void XrdFrmConfig::ConfigPF(const char *pFN)
{
   static const int Mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   const char *ppP = (PidPath ? PidPath : "/tmp");
   char buff[1032], data[24];
   int pfFD, n;

// Construct pidfile name
//
   if (myInst) snprintf(buff, sizeof(buff), "%s/%s/%s.pid", ppP, myInst, pFN);
      else sprintf(buff, "%s/%s.pid", ppP, pFN);

// Open the pidfile creating it if necessary
//
   if ((pfFD = open(buff, O_WRONLY|O_CREAT|O_TRUNC, Mode)) < 0)
      {Say.Emsg("Config",errno,"open",buff); return;}

// Write out our pid
//
   n = sprintf(data, "%lld", static_cast<long long>(getpid()));
   if (write(pfFD, data, n) < 0) Say.Emsg("Config",errno,"writing",buff);
   close(pfFD);
}

/******************************************************************************/
/* Private:                   C o n f i g P r o c                             */
/******************************************************************************/
  
int XrdFrmConfig::ConfigProc()
{
  char *var;
  int  cfgFD, retc, mbok, NoGo = 0;
  XrdOucEnv myEnv;
  XrdOucStream cfgFile(&Say, myInstance, &myEnv, "=====> ");

// Allocate a chksum configurator if needed
//
   if (ssID == ssAdmin)
      {CksCfg = new XrdCksConfig(ConfigFN, &Say, retc, *myVersion);
       if (!retc) return 1;
      }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Say.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   cfgFile.Attach(cfgFD); cFile = &cfgFile;

// Now start reading records until eof.
//
   while((var = cFile->GetMyFirstWord()))
        {mbok = 0;
         if (!strncmp(var, pfxDTS, plnDTS)) {var += plnDTS; mbok = 1;}
         if(ConfigXeq(var, mbok)) {cfgFile.Echo(); NoGo = 1;}
        }

// Now check if any errors occured during file i/o
//
   if ((retc = cfgFile.LastError()))
      NoGo = Say.Emsg("Config", retc, "read config file", ConfigFN);
   cfgFile.Close(); cFile = 0;

// Return final return code
//
   return NoGo;
}

/******************************************************************************/
/* Prvate:                     C o n f i g X e q                              */
/******************************************************************************/

int XrdFrmConfig::ConfigXeq(char *var, int mbok)
{

// Process common items to all subsystems
//
   if (!strcmp(var, "all.adminpath" )) return xapath();
   if (!strcmp(var, "all.pidpath"   )) return Grab(var, &PidPath, 0);
   if (!strcmp(var, "all.manager"   )) {haveCMS = 1; return 0;}
   if (!strcmp(var, "frm.all.cnsd"  )) return xcnsd();

// Process directives specific to each subsystem
//
   if (ssID == ssAdmin)
      {
       if (!strcmp(var, "frm.xfr.qcheck")) return xqchk();
       if (!strcmp(var, "ofs.ckslib"    )) return xcks(1);
       if (!strcmp(var, "ofs.osslib"    )) return xoss();
       if (!strcmp(var, "oss.cache"     )){hasCache = 1; // runOld
                                           return xspace(0,0);
                                          }
       if (!strcmp(var, "oss.localroot" )) return Grab(var, &LocalRoot, 0);
       if (!strcmp(var, "oss.namelib"   )) return xnml();
       if (!strcmp(var, "oss.remoteroot")) return Grab(var, &RemoteRoot, 0);
       if (!strcmp(var, "oss.space"     )) return xspace();
       if (!strcmp(var, "xrootd.chksum" )) return xcks();
//     if (!strcmp(var, "oss.mssgwcmd"  )) return Grab(var, &MSSCmd,    0);
//     if (!strcmp(var, "oss.msscmd"    )) return Grab(var, &MSSCmd,    0);
       if (!strcmp(var, "oss.xfr"       )) return xxfr();
      }

   if (ssID == ssXfr)
      {
       if (!strcmp(var, "qcheck"        )) return xqchk();
       if (isAgent) return 0;           // Server-oriented directives

       if (!strcmp(var, "all.sitename"  )) return xsit();
       if (!strcmp(var, "ofs.osslib"    )) return Grab(var, &ossLib,    0);
       if (!strcmp(var, "oss.cache"     )) return xspace(0,0);
       if (!strcmp(var, "oss.localroot" )) return Grab(var, &LocalRoot, 0);
       if (!strcmp(var, "oss.namelib"   )) return xnml();
       if (!strcmp(var, "oss.remoteroot")) return Grab(var, &RemoteRoot, 0);
       if (!strcmp(var, "oss.xfr"       )) return xxfr();
       if (!strcmp(var, "frm.all.monitor"))return xmon();

       if (!strcmp(var, "copycmd"       )) return xcopy();
       if (!strcmp(var, "copymax"       )) return xcmax();
       if (!strcmp(var, "oss.space"     )) return xspace();

       if (!strncmp(var, "migr.", 5))   // xfr.migr
      {char *vas = var+5;
       if (!strcmp(vas, "idlehold"      )) return xitm("idle time", IdleHold);
       if (!strcmp(vas, "waittime"      )) return xitm("migr wait", WaitMigr);
      }
      }

   if (ssID == ssPurg)
      {
       if (!strcmp(var, "all.sitename"  )) return xsit();
       if (!strcmp(var, "dirhold"       )) return xdpol();
       if (!strcmp(var, "oss.cache"     )) return xspace(1,0);
       if (!strcmp(var, "oss.localroot" )) return Grab(var, &LocalRoot, 0);
       if (!strcmp(var, "ofs.osslib"    )) return Grab(var, &ossLib,    0);
       if (!strcmp(var, "policy"        )) return xpol();
       if (!strcmp(var, "polprog"       )) return xpolprog();
       if (!strcmp(var, "oss.space"     )) return xspace(1);
       if (!strcmp(var, "waittime"      )) return xitm("purge wait",WaitPurge);
       if (!strcmp(var, "frm.all.monitor"))return xmon();
      }

   // No match found, complain.
   //
   if (!mbok) cFile->noEcho();
      else {Say.Say("Config warning: ignoring unknown frm directive '",var,"'.");
            cFile->Echo();
           }
   return 0;
}

/******************************************************************************/
/* Private:                    C o n f i g X f r                              */
/******************************************************************************/
  
int XrdFrmConfig::ConfigXfr()
{
   int i, isBad, ioOK[2] = {0};

// Configure the name2name library and migratable paths and mass storage
//
   isBad = ConfigMP("migratable") ||  ConfigMss();

// Make sure

// Configure all of the transfer commands
//
   for (i = 0; i < 4; i++)
       {if (xfrCmd[i].theCmd)
           {if ((xfrCmd[i].theVec=ConfigCmd(xfrCmd[i].Desc, xfrCmd[i].theCmd)))
               ioOK[i%2]  = 1;
               else isBad = 1;
           }
       }

// Verify that we can actually do something
//
   if (!(ioOK[0] | ioOK[1]))
      {Say.Emsg("Config",
                "No copy commands specified; execution is meaningless!");
       return 1;
      }

// Verify that input copies are OK
//
   if (!(xfrIN = ioOK[0]))
      Say.Emsg("Config", "Input copy command not specified; "
                         "incoming transfers prohibited!");

// Verify that input copies are OK
//
   if (!(xfrOUT = ioOK[1]))
      Say.Emsg("Config", "Output copy command not specified; "
                         "outgoing transfers prohibited!");

// All done
//
   return isBad;
}

/******************************************************************************/
/* Private:                      g e t T i m e                                */
/******************************************************************************/

int XrdFrmConfig::getTime(const char *emsg, const char *item, int *val,
                          int minv, int maxv)
{
    if (strcmp(item, "forever"))
       return  XrdOuca2x::a2tm(Say, emsg, item, val, minv, maxv);
    *val = -1;
    return 0;
}
  
/******************************************************************************/
/* Private:                         G r a b                                   */
/******************************************************************************/
  
int XrdFrmConfig::Grab(const char *var, char **Dest, int nosubs)
{
    char  myVar[1024], buff[2048], *val;
    XrdOucEnv *myEnv = 0;

// Copy the variable name as this may change because it points to an
// internal buffer in Config. The vagaries of effeciency.
//
   strlcpy(myVar, var, sizeof(myVar));
   var = myVar;

// If substitutions allowed then we need to grab a single token else grab
// the remainder of the line but suppress substitutions.
//
   if (!nosubs) val = cFile->GetWord();
      else {myEnv = cFile->SetEnv(0);
            if (!cFile->GetRest(buff, sizeof(buff)))
               {Say.Emsg("Config", "arguments too long for", var);
                cFile->SetEnv(myEnv);
                return 1;
               }
            val = buff;
            cFile->SetEnv(myEnv);
           }

// At this point, make sure we have a value
//
   if (!val || !(*val))
      {if (nosubs < 0) Say.Emsg("Config", "no arguments for", var);
          else         Say.Emsg("Config", "no value for directive", var);
       return 1;
      }

// Set the value. Either this is a simple string or a compund string
//
   if (*Dest) {free(*Dest); Dest = 0;}
   if (nosubs < 0)
      {char fBuff[2048];
       int n = strlen(myVar);
       if (n + strlen(val) > sizeof(fBuff)-1)
          {Say.Emsg("Config", "arguments too long for", var); return 1;}
       strcpy(fBuff, myVar); *(fBuff+n) = ' '; strcpy(fBuff+n+1, val);
       *Dest = strdup(fBuff);
      } else *Dest = strdup(val);

// All done
//
   return 0;
}

/******************************************************************************/
/* Private:                     I n s e r t P L                               */
/******************************************************************************/
  
XrdOucTList *XrdFrmConfig::InsertPL(XrdOucTList *pL, const char *Path,
                                    int Plen, int isRW)
{
   short sval[4] = {static_cast<short>(isRW), static_cast<short>(Plen)};
   XrdOucTList *pP = 0, *tP = pL;

// Find insertion point
//
   while(tP && tP->sval[1] < Plen) {pP = tP; tP = tP->next;}

// Insert new element
//
   if (pP) pP->next = new XrdOucTList(Path, sval, tP);
      else       pL = new XrdOucTList(Path, sval, tP);

// Return the new list
//
   return pL;
}

/******************************************************************************/
/* Private:                     I n s e r t X D                               */
/******************************************************************************/

void XrdFrmConfig::InsertXD(const char *Path)
{
   EPNAME("InsertXD");
   char pBuff[MAXPATHLEN], *pP;
   int n = strlen(Path);

// Make sure this does not end with a slash
//
   strcpy(pBuff, Path);
   pP = pBuff + n - 1;
   while(*pP == '/' && pP != pBuff) {*pP-- = '\0'; n--;}

// Insert this directory into the exclude list for the current path
//
   pathList->Dir = new XrdOucTList(pBuff, n, pathList->Dir);
   DEBUG("Excluding '" <<pBuff <<"'");
}
  
/******************************************************************************/
/* Private:                        U s a g e                                  */
/******************************************************************************/
  
void XrdFrmConfig::Usage(int rc)
{
     cerr <<"\nUsage: " <<myProg <<" " <<uInfo <<endl;
     _exit(rc);
}

/******************************************************************************/
/* Private:                       x a p a t h                                 */
/******************************************************************************/

/* Function: xapath

   Purpose:  To parse the directive: adminpath <path> [group]

             <path>    the path of the FIFO to use for admin requests.

             group     allows group access to the admin path

   Output: 0 upon success or !0 upon failure.
*/

int XrdFrmConfig::xapath()
{
    char *pval, *val;
    mode_t mode = S_IRWXU;

// Get the path
//
   pval = cFile->GetWord();
   if (!pval || !pval[0])
      {Say.Emsg("Config", "adminpath not specified"); return 1;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {Say.Emsg("Config", "adminpath not absolute"); return 1;}

// Record the path
//
   if (AdminPath) free(AdminPath);
   AdminPath = strdup(pval);

// Get the optional access rights
//
   if ((val = cFile->GetWord()) && val[0])
      {if (!strcmp("group", val)) mode |= S_IRWXG;
          else {Say.Emsg("Config", "invalid admin path modifier -", val);
                return 1;
               }
      }
   AdminMode = mode;
   return 0;
}

/******************************************************************************/
/* Private:                         x c k s                                   */
/******************************************************************************/

/* Function: xcks

   Purpose:  To parse the directive: chksum [max <n>] <type> <path>

             max       maximum number of simultaneous jobs
             <type>    algorithm of checksum (e.g., md5)
             <path>    the path of the program performing the checksum

  Output: 0 upon success or !0 upon failure.
*/

int XrdFrmConfig::xcks(int isOfs)
{
   char *palg;

// If this is the ofs cks directive, then process as such
//
   if (isOfs) return CksCfg->ParseLib(*cFile);

// Get the algorithm name and the program implementing it
//
   while ((palg = cFile->GetWord()) && *palg != '/')
         {if (strcmp(palg, "max")) break;
          if (!(palg = cFile->GetWord()))
             {Say.Emsg("Config", "chksum max not specified"); return 1;}
         }

// Verify we have an algoritm
//
   if (!palg || *palg == '/')
      {Say.Emsg("Config", "chksum algorithm not specified"); return 1;}
   if (CksAlg) free(CksAlg);
   CksAlg = strdup(palg);
   return 0;
}
  
/******************************************************************************/
/* Private:                        x c n s d                                  */
/******************************************************************************/

/* Function: cnsd

   Purpose:  To parse the directive: cnsd {auto | ignore | require} [Options]

   Options:  [apath <path>]

             auto      use the cnsd if present, ignore otherwise.
             ignore    never use the cnsd, even if present.
             require   always use the cnsd. If not present, wait for it.
             apath     The path specified on the -a option of the cnsd.

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xcnsd()
{
   int cnsMode;
   char *val, *cnsPath = 0;
   struct cnsdopts {const char *opname; int opval;} cnsopt[] =
         {
          {"auto",    XrdFrmCns::cnsAuto},
          {"ignore",  XrdFrmCns::cnsIgnore},
          {"require", XrdFrmCns::cnsRequire}
         };
   int i, numopts = sizeof(cnsopt)/sizeof(struct cnsdopts);

// Pick up required parameter
//
   if (!(val = cFile->GetWord()))
      {Say.Emsg("Config", "cnsd mode not specified"); return 1;}

// Now match that option
//
   for (i = 0; i < numopts; i++)
       if (!strcmp(val,cnsopt[i].opname)) {cnsMode = cnsopt[i].opval; break;}
   if (i >= numopts)
      {Say.Emsg("Config", "invalid cnsd mode '",val,"'."); return 1;}

// Check if we have an apath now
//
   if ((val = cFile->GetWord()))
      {if (strcmp("apath", val))
          {Say.Emsg("Config", "invalid cnsd option '",val,"'."); return 1;}
       if (!(cnsPath = cFile->GetWord()))
          {Say.Emsg("Config", "cnsd apath not specified"); return 1;}
      }

// Preset the cnsd options and return
//
   return XrdFrmCns::Init(cnsPath, cnsMode);
}
  
/******************************************************************************/
/* Private:                        x c o p y                                  */
/******************************************************************************/

/* Function: copycmd

   Purpose:  To parse the directive: copycmd [Options] cmd [args]

   Options:  [in] [noalloc] [out] [rmerr] [stats] [timeout <sec>] [url] [xpd]

             in        use command for incomming copies.
             noalloc   do not pre-allocate space for incomming copies.
             out       use command for outgoing copies.
             rmerr     remove incomming file when copy ends with an error.
                       Default unless noalloc is specified.
             stats     print transfer statistics in the log.
             timeout   how long the cmd can run before it is killed.
             url       use command for url-based transfers.
             xpd       extend monitoring with program data.

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xcopy()
{  int cmdIO[2] = {0,0}, TLim=0, Stats=0, hasMDP=0, cmdUrl=0, noAlo=0, rmErr=0;
   int monPD = 0;
   char *val, *theCmd = 0;
   struct copyopts {const char *opname; int *oploc;} cpopts[] =
         {
          {"in",     &cmdIO[0]},
          {"out",    &cmdIO[1]},
          {"noalloc",&noAlo},
          {"rmerr",  &rmErr},
          {"stats",  &Stats},
          {"timeout",&TLim},
          {"url",    &cmdUrl},
          {"xpd",    &monPD}
         };
   int i, n, numopts = sizeof(cpopts)/sizeof(struct copyopts);

// Pick up options
//
   val = cFile->GetWord();
   while(val && *val != '/')
        {for (i = 0; i < numopts; i++)
             {if (!strcmp(val,cpopts[i].opname))
                 {if (strcmp("timeout", val)) {*cpopts[i].oploc = 1; break;}
                     else if (!xcopy(TLim)) return 1;
                 }
             }
         if (i >= numopts)
            Say.Say("Config warning: ignoring invalid copycmd option '",val,"'.");
         val = cFile->GetWord();
        }

// Pick up the program
//
   if (!val || !*val)
      {Say.Emsg("Config", "copy command not specified"); return 1;}
   if (Grab(val, &theCmd, -1)) return 1;

// Find if $MDP is present here
//
   if (!cmdIO[0] && !cmdIO[1]) cmdIO[0] = cmdIO[1] = 1;
   if (cmdIO[1]) hasMDP = (strstr(theCmd, "$MDP") != 0);

// Initialzie the appropriate command structures
//
   n = (cmdUrl ? 3 : 1);
   i = 1;
   do {if (cmdIO[i])
          {if (xfrCmd[n].theCmd) free(xfrCmd[n].theCmd);
           xfrCmd[n].theCmd = strdup(theCmd);
           if (Stats)  xfrCmd[n].Opts  |= cmdStats;
           if (monPD)  xfrCmd[n].Opts  |= cmdXPD;
           if (hasMDP) xfrCmd[n].Opts  |= cmdMDP;
           if (rmErr)  xfrCmd[n].Opts  |= cmdRME;
           if (noAlo)  xfrCmd[n].Opts  &=~cmdAlloc;
              else     xfrCmd[n].Opts  |= cmdAlloc;
           xfrCmd[n].TLimit = TLim;
          }
       n--;
      } while(i--);

// All done
//
   free(theCmd);
   return 0;
}

/******************************************************************************/

int XrdFrmConfig::xcopy(int &TLim)
{
   char *val;

   if (!(val = cFile->GetWord()) || !*val)
      {Say.Emsg("Config", "copy command timeout not specified"); return 0;}
   if (XrdOuca2x::a2tm(Say,"copy command timeout", val, &TLim, 0)) return 0;
   return 1;
}

/******************************************************************************/
/* Private:                        x c m a x                                  */
/******************************************************************************/

/* Function: copymax

   Purpose:  To parse the directive: copymax  <num>

             <num>     maximum number of simultaneous transfers

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xcmax()
{   int xmax = 1;
    char *val;

    if (!(val = cFile->GetWord()))
       {Say.Emsg("Config", "maxio value not specified"); return 1;}
    if (XrdOuca2x::a2i(Say, "maxio", val, &xmax, 1)) return 1;
    xfrMax = xmax;
    return 0;
}

  
/******************************************************************************/
/* Private:                        x d p o l                                  */
/******************************************************************************/
  

/* Function: xdpol

   Purpose:  To parse the directive: dirpolicy <sec>

             <sec>     number of seconds to hold an empty directory or the
                       word 'forever'.

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xdpol()
{   int htm;
    char *val;

    if (!(val = cFile->GetWord()))
       {Say.Emsg("Config",  "dirpolicy hold time not specified"); return 1;}
    if (XrdOuca2x::a2tm(Say,"dirpolicy hold time", val, &htm, 0)) return 1;
    dirHold = htm;
    return 0;
}

/******************************************************************************/
/* Private:                         x i t m                                   */
/******************************************************************************/

/* Function: xitm

   Purpose:  To parse the directive: xxxxtime <sec>

             <sec>     number of seconds applicable to the directive.

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xitm(const char *What, int &tDest)
{   int itime;
    char *val;

    if (!(val = cFile->GetWord()))
       {Say.Emsg("Config", What, "not specified"); return 1;}
    if (XrdOuca2x::a2tm(Say, What, val, &itime)) return 1;
    tDest = itime;
    return 0;
}

/******************************************************************************/
/*                                  x m o n                                   */
/******************************************************************************/

/* Function: xmon

   Purpose:  Parse directive: monitor [ident <sec>] dest [Events] <host:port>

   Events: [migr] [purge] [stage]

         ident  <sec>       time (seconds, M, H) between ident records.
         dest               specified routing information. Up to two dests
                            may be specified.
         migr               monitors file migr  operations
         purge              monitors file purge operations
         stage              monitors file stage operations
         <host:port>        where monitor records are to be sentvia UDP.

   Output: 0 upon success or !0 upon failure. Ignored by master.
*/
int XrdFrmConfig::xmon()
{   char  *val, *cp, *monDest[2] = {0, 0};
    int i, monIdent=3600, monMode[2] = {0, 0};

    while((val = cFile->GetWord()))

         {     if (!strcmp("ident", val))
                {if (!(val = cFile->GetWord()))
                    {Say.Emsg("Config", "monitor ident value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(Say,"monitor ident",val,
                                         &monIdent,0)) return 1;
                }
          else break;
         }

    if (!val) {Say.Emsg("Config", "monitor dest not specified"); return 1;}

    for (i = 0; i < 2; i++)
        {if (strcmp("dest", val)) break;
         while((val = cFile->GetWord()))
                   if (!strcmp("stage",val)) monMode[i] |=  XROOTD_MON_STAGE;
              else if (!strcmp("migr", val)) monMode[i] |=  XROOTD_MON_MIGR;
              else if (!strcmp("purge",val)) monMode[i] |=  XROOTD_MON_PURGE;
              else break;
          if (!val) {Say.Emsg("Config","monitor dest value not specified");
                     return 1;
                    }
          if (!(cp = index(val, (int)':')) || !atoi(cp+1))
             {Say.Emsg("Config","monitor dest port missing or invalid in",val);
              return 1;
             }
          monDest[i] = strdup(val);
         if (!(val = cFile->GetWord())) break;
        }

    if (val)
       {if (!strcmp("dest", val))
           Say.Emsg("Config", "Warning, a maximum of two dest values allowed.");
           else Say.Emsg("Config", "Warning, invalid monitor option", val);
       }

// Make sure dests differ
//
   if (monDest[0] && monDest[1] && !strcmp(monDest[0], monDest[1]))
      {Say.Emsg("Config", "Warning, monitor dests are identical.");
       monMode[0] |= monMode[1]; monMode[1] = 0;
       free(monDest[1]); monDest[1] = 0;
      }

// Don't bother doing any more if monitoring is not enabled
//
   if (!monMode[0] && !monMode[1]) return 0;

// Set the monitor defaults
//
   XrdFrmMonitor::Defaults(monDest[0],monMode[0],monDest[1],monMode[1],monIdent);
   return 0;
}

/******************************************************************************/
/* Private:                         x n m l                                   */
/******************************************************************************/

/* Function: xnml

   Purpose:  To parse the directive: namelib <path> [<parms>]

             <path>    the path of the filesystem library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdFrmConfig::xnml()
{
    char *val, parms[1024];

// Get the path
//
   if (!(val = cFile->GetWord()) || !val[0])
      {Say.Emsg("Config", "namelib not specified"); return 1;}

// Record the path
//
   if (N2N_Lib) free(N2N_Lib);
   N2N_Lib = strdup(val);

// Record any parms
//
   if (!cFile->GetRest(parms, sizeof(parms)))
      {Say.Emsg("Config", "namelib parameters too long"); return 1;}
   if (N2N_Parms) free(N2N_Parms);
   N2N_Parms = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/* Private:                         x o s s                                   */
/******************************************************************************/
  

/* Function: xoss

   Purpose:  To parse the directive: osslib <path>

             <path>    the path of the filesystem library to be used.

  Output: 0 upon success or !0 upon failure.
*/

int XrdFrmConfig::xoss()
{
    char *val, parms[1024];

// Get the path
//
   if (!(val = cFile->GetWord()) || !val[0])
      {Say.Emsg("Config", "osslib not specified"); return 1;}
   if (ossLib) free(ossLib);
   ossLib = strdup(val);

// Record any parms
//
   if (!cFile->GetRest(parms, sizeof(parms)))
      {Say.Emsg("Config", "osslib parameters too long"); return 1;}
   if (ossParms) free(ossParms);
   ossParms = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/* Private:                         x p o l                                   */
/******************************************************************************/

/* Function: xpol

   Purpose:  To parse the directive: policy {*|sname} {nopurge|min [max]] [opts]

             *         The default policy for all spaces.

             sname     The policy to apply for this space. Defaults apply for
                       unspecified values. To make sure the specified default
                       is used, the '*' entry must appear first.

             nopurge   Turns off purging.

             min%      Minimum free space; purge starts when less available.
                       Can be specified as a percentage (i.e., n%) or an
                       absolute size value (with k, m, g, t suffix).
                       Default: 5%

             max%      Maximum free space; purge stops  when more available.
                       Must be specified in the same units as min and must be
                       greater than min.
                       Default: min% + 2 or min * 1.2

       opts: hold <tm> Time to hold a file before it can be purged. The <tm>
                       can be a suffixed number or 'forever'.
                       Default: 20h (20*3600)s

             polprog   Invoke the policy program to do final determination.


   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xpol()
{
   Policy *pP = &dfltPolicy;
   char *val, sname[XrdOssSpace::minSNbsz];
   long long minP = dfltPolicy.minFree, maxP = dfltPolicy.maxFree;
   int       Hold = dfltPolicy.Hold, Ext = 0;
   struct purgeopts {const char *opname; int isTime; int *oploc;} pgopts[] =
      {
       {"polprog", -1, &Ext},
       {"hold",     1, &Hold}
      };
   int i, rc, numopts = sizeof(pgopts)/sizeof(struct purgeopts);

// Get the space name
//
   if (!(val = cFile->GetWord()))
      {Say.Emsg("Config", "space name not specified"); return 1;}
   if (strlen(val) >= sizeof(sname))
      {Say.Emsg("Config", "space name '", val, "' too long"); return 1;}

// If we have an equal sign then an external policy is being defined
//
   if (!strcmp("=", val)) return xpolprog();
   strcpy(sname, val);

// The next item may be minimum percentage followed by a maximum percentage
// Otherwise, it may be 'nopurge'.
//
   if (    (val = cFile->GetWord()) && isdigit(*val))
      {if (    XrdOuca2x::a2sp(Say, "min free", val, &minP, 1)) return 1;
       if ((val = cFile->GetWord()) && isdigit(*val))
          {if (XrdOuca2x::a2sp(Say, "max free", val, &maxP, 1)) return 1;
           if ((minP < 0 && maxP >= 0) || (minP >= 0 && maxP < 0))
              {Say.Emsg("Config", "purge min/max may not differ in type.");
               return 1;
              }
           if (XRDABS(minP) >= XRDABS(maxP))
              {Say.Emsg("Config", "purge min must be < max value."); return 1;}
           val = cFile->GetWord();
          } else {
           if (minP < 0) maxP = (minP < -99 ? -100 : minP - 1);
              else       maxP = (minP * 120LL)/100LL;
          }
      } else if (val && !strcmp(val, "nopurge"))
                {minP = maxP = 0;
                 if ((val = cFile->GetWord()))
                    {Say.Say("Config warning: ignoring extraneous policy option '",val,"'.");
                     val = 0;
                    }
                }

// Pick up the remining options
//
   while(val)
        {for (i = 0; i < numopts; i++) if (!strcmp(val,pgopts[i].opname)) break;
         if (i >= numopts)
            {Say.Say("Config warning: ignoring invalid policy option '",val,"'.");
             val = cFile->GetWord();
             continue;
            }
         if (pgopts[i].isTime < 0) *(pgopts[i].oploc) = 1;
            else {if (!(val = cFile->GetWord()))
                     {Say.Emsg("Config", "policy", pgopts[i].opname,
                                         "argument not specified.");
                      return 1;
                     }
                  rc = (pgopts[i].isTime
                     ?         getTime(    "purge value",val,pgopts[i].oploc,0)
                     : XrdOuca2x::a2i (Say,"purge value",val,pgopts[i].oploc,0));
                  if (rc) return 1;
                 }
         val = cFile->GetWord();
        }

// If an external policy applies, it must be present
//
   if (Ext && !pProg)
      {Say.Emsg("Config", "External policy has not been pre-defined.");
       return 1;
      }

// Add this policy definition
//
   while(pP && strcmp(pP->Sname, sname)) pP = pP->Next;
   if (pP) {pP->minFree=minP; pP->maxFree=maxP; pP->Hold=Hold; pP->Ext=Ext;}
      else {pP = new Policy(sname, minP, maxP, Hold, Ext);
            pP->Next = dfltPolicy.Next; dfltPolicy.Next = pP;
           }
    return 0;
}

/******************************************************************************/
/* Private:                     x p o l p r o g                               */
/******************************************************************************/
  
/* Function: xpolprog

   Purpose:  To parse the directive: policy = [vars] |<prog> [args]

   Where:
             =         Defines an external policy via a program, as follows:

             vars      The information to ship to the program via stdin:
                       atime   - access time
                       ctime   - create time
                       fname   - the filename itself
                       fsize   - file size
                       fspace  - free  space
                       mtime   - modify time
                       pfn     - physical file name
                       sname   - space name
                       tspace  - total space

             |<prog>   The name of the policy program to receive the info.

             args      Optional program arguments (substituted), up to 8.

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xpolprog()
{
   char *val, pBuff[4096], *pbP = pBuff;
   struct polopts {const char *opname; int opval;} plopts[] =
      {
       {"atime",  PP_atime },
       {"ctime",  PP_ctime },
       {"fname",  PP_fname },
       {"fsize",  PP_fsize },
       {"fspace", PP_fspace},
       {"mtime",  PP_mtime },
       {"pfn",    PP_pfn   },
       {"sname",  PP_sname },
       {"tspace", PP_tspace},
       {"usage",  PP_usage}
      };
   int i, n, numopts = sizeof(plopts)/sizeof(struct polopts);

// Get the first token
//
   if (!(val = cFile->GetWord()))
      {Say.Emsg("Config", "policy program not specified"); return 1;}
   pVecNum = 0;

// Pick up the remining options
//
   while(val && *val != '|')
        {for (i = 0; i < numopts; i++) if (!strcmp(val,plopts[i].opname)) break;
         if (i >= numopts)
            {Say.Say("Config warning: ignoring invalid policy option '",val,"'.");
             val = cFile->GetWord();
             continue;
            }
         if (pVecNum >= pVecMax)
            {Say.Emsg("Config", "To many policy program variables specified.");
             return 1;
            }
         pVec[pVecNum++] = static_cast<char>(plopts[i].opval);
         val = cFile->GetWord();
        }

// Pick up the program
//
   if (val) val++;
   if (val && !(*val)) val = cFile->GetWord();
   if (!val)
      {Say.Emsg("Config", "policy program not specified."); return 1;}
   i = strlen(val);
   if (i >= (int)sizeof(pBuff)-8)
      {Say.Emsg("Config", "policy program name is too long."); return 1;}
   strcpy(pBuff, val); pbP = pBuff+i; *(pbP+1) = '\0';

// Now get any optional arguments
//
   n = sizeof(pBuff) - i - 1;
   if (!cFile->GetRest(pbP+1, n))
      {Say.Emsg("Config", "policy program args are too long."); return 1;}
   if (*(pbP+1)) *pbP = ' ';

// Record the program
//
   if (pProg) free(pProg);
   pProg = strdup(pBuff);
   return 0;
}

/******************************************************************************/
/* Private:                        x q c h k                                  */
/******************************************************************************/

/* Function: xqchk

   Purpose:  To parse the directive: qcheck <sec> <path>

             <sec>     number of seconds between forced queue checks. This is
                       optional is <path> is specified.
             <path>    the absolute location of the queue directory.

   Output: 0 upon success or !0 upon failure.
*/
int XrdFrmConfig::xqchk()
{   int itime;
    char *val;

// Get the next token, we must have one here
//
   if (!(val = cFile->GetWord()))
      {Say.Emsg("Config", "qcheck time not specified"); return 1;}

// If not a path, then it must be a time
//
   if (*val != '/')
      {if (XrdOuca2x::a2tm(Say, "qcheck time", val, &itime)) return 1;
       WaitQChk = itime;
       if (!(val = cFile->GetWord())) return 0;
      }

// The next token has to be an absolute path if it is present at all
//
   if (*val != '/')
      {Say.Emsg("Config", "qcheck path not absolute"); return 1;}
   if (QPath) free(QPath);
   QPath = strdup(val);
   return 0;
}

/******************************************************************************/
/*                                  x s i t                                   */
/******************************************************************************/

/* Function: xsit

   Purpose:  To parse directive: sitename <name>

             <name>   is the 1- to 15-character site name to be included in
                      monitoring information. This can also come from the
                      command line -N option. The first such name is used.

   Output: 0 upon success or 1 upon failure.
*/

int XrdFrmConfig::xsit()
{
    char *val;

    if (!(val = cFile->GetWord()))
       {Say.Emsg("Config", "sitename value not specified"); return 1;}

    if (mySite) Say.Emsg("Config", "sitename already specified, using '",
                         mySite, "'.");
       else mySite = XrdOucSiteName::Set(val);
    return 0;
}

/******************************************************************************/
/*                                x s p a c e                                 */
/******************************************************************************/

/* Function: xspace

   Purpose:  To parse the directive: space <group> <path>

             <group>  logical group name for the filesystem.
             <path>   path to the filesystem.

   Output: 0 upon success or !0 upon failure.
*/

int XrdFrmConfig::xspace(int isPrg, int isXA)
{
   char *val, *pfxdir, *sfxdir;
   char grp[XrdOssSpace::minSNbsz], fn[MAXPATHLEN], dn[MAXNAMLEN];
   int i, k, rc, pfxln, cnum = 0;
   struct dirent *dp;
   struct stat buff;
   DIR *DFD;

   if (!(val = cFile->GetWord()))
      {Say.Emsg("Config", "space name not specified"); return 1;}
   if (strlen(val) >= (int)sizeof(grp))
      {Say.Emsg("Config","excessively long space name - ",val); return 1;}
   strcpy(grp, val);

   if (!(val = cFile->GetWord()))
      {Say.Emsg("Config", "path to space not specified"); return 1;}

   k = strlen(val);
   if (k >= (int)(sizeof(fn)-1) || val[0] != '/' || k < 2)
      {Say.Emsg("Config", "invalid space path - ", val); return 1;}
   strcpy(fn, val);

   if (!isXA && (val = cFile->GetWord()))
      {if (strcmp("xa", val))
          {Say.Emsg("Config","invalid cache option - ",val); return 1;}
          else isXA = 1;
      }

   if (fn[k-1] != '*')
      {for (i = k-1; i; i--) if (fn[i] != '/') break;
       fn[i+1] = '/'; fn[i+2] = '\0';
       xspaceBuild(grp, fn, isXA);
       return 0;
      }

   for (i = k-1; i; i--) if (fn[i] == '/') break;
   i++; strcpy(dn, &fn[i]); fn[i] = '\0';
   sfxdir = &fn[i]; pfxdir = dn; pfxln = strlen(dn)-1;
   if (!(DFD = opendir(fn)))
      {Say.Emsg("Config", errno, "open space directory", fn); return 1;}

   errno = 0;
   while((dp = readdir(DFD)))
        {if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")
         || (pfxln && strncmp(dp->d_name, pfxdir, pfxln)))
            continue;
         strcpy(sfxdir, dp->d_name);
         if (stat(fn, &buff)) break;
         if ((buff.st_mode & S_IFMT) == S_IFDIR)
            {val = sfxdir + strlen(sfxdir) - 1;
            if (*val++ != '/') {*val++ = '/'; *val = '\0';}
            xspaceBuild(grp, fn, isXA);
            cnum++;
            }
         errno = 0;
        }

   if ((rc = errno))
      Say.Emsg("Config", errno, "process space directory", fn);
      else if (!cnum) Say.Say("Config warning: no space directories found in ",val);

   closedir(DFD);
   return rc != 0;
}

void XrdFrmConfig::xspaceBuild(char *grp, char *fn, int isxa)
{
   struct VPInfo *nP = VPList;
   XrdOucTList *tP;

   while(nP && strcmp(nP->Name, grp)) nP = nP->Next;

   if (!nP) VPList = nP = new VPInfo(grp, 0, VPList);

   tP = nP->Dir;
   while(tP && strcmp(tP->text, fn)) tP = tP->next;
   if (!tP) nP->Dir = new XrdOucTList(fn, isxa, nP->Dir);

   if (!isxa) nonXA = 1;
}

/******************************************************************************/
/*                                  x x f r                                   */
/******************************************************************************/
  
/* Function: xxfr

   Purpose:  To parse the directive: xfr [deny <sec>] [fdir <path>] [keep <sec>]

             deny      number of seconds that a fail file rejects a request
             fdir      base directory where fail files are kept
             keep      number of seconds to keep queued requests (ignored)

   Output: 0 upon success or !0 upon failure.
*/

int XrdFrmConfig::xxfr()
{
    static const int maxfdln = 256;
    const char *wantParm = 0;
    char *val;
    int       htime = 3*60*60;

    while((val = cFile->GetWord()))        // deny | keep
         {     if (!strcmp("deny", val))
                  {wantParm = "xfr deny";
                   if ((val = cFile->GetWord()))     // keep time
                      {if (XrdOuca2x::a2tm(Say,wantParm,val,&htime,0)) return 1;
                       FailHold = htime, wantParm=0;
                      }
                  }
          else if (!strcmp("fdir", val))
                  {wantParm = "xfr fdir";
                   if ((val = cFile->GetWord()))     // fdir path
                      {if (xfrFdir) free(xfrFdir);
                       xfrFdln = strlen(val);
                       if (xfrFdln > maxfdln)
                          {Say.Emsg("Config","xfr fdir path too long");
                           xfrFdir = 0; xfrFdln = 0; return 1;
                          }
                       xfrFdir = strdup(val);
                       wantParm = 0;
                      }
                  }
          else if (!strcmp("keep", val))
                  {wantParm = "xfr keep";
                   if ((val = cFile->GetWord()))     // keep time
                      {if (XrdOuca2x::a2tm(Say,wantParm,val,&htime,0)) return 1;
                       wantParm=0;
                      }
                  }
          else break;
         };

    if (!val && wantParm)
       {Say.Emsg("Config", wantParm, "value not specified"); return 1;}

    return 0;
}
