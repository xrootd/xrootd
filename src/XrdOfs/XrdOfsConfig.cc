/******************************************************************************/
/*                                                                            */
/*                       X r d O f s C o n f i g . c c                        */
/*                                                                            */
/* (C) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/param.h>

#include "XrdVersion.hh"

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksConfig.hh"

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsEvs.hh"
#include "XrdOfs/XrdOfsPoscq.hh"
#include "XrdOfs/XrdOfsStats.hh"
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdOfs/XrdOfsTrace.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdCms/XrdCmsClient.hh"
#include "XrdCms/XrdCmsFinder.hh"
#include "XrdCms/XrdCmsRole.hh"

#include "XrdAcc/XrdAccAuthorize.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

extern XrdOfsStats OfsStats;

extern XrdOucTrace OfsTrace;
  
class  XrdOss;
extern XrdOss     *XrdOfsOss;

XrdVERSIONINFO(XrdOfs,XrdOfs);

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)   if (!strcmp(x,var)) return m(Config,Eroute);

#define TS_Str(x,m)   if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_PList(x,m)  if (!strcmp(x,var)) \
                          {m.Insert(new XrdOucPList(val,1)); return 0;}

#define TS_Chr(x,m)   if (!strcmp(x,var)) {m = val[0]; return 0;}

#define TS_Bit(x,m,v) if (!strcmp(x,var)) {m |= v; Config.Echo(); return 0;}

#define Max(x,y) (x > y ? x : y)

/******************************************************************************/
/*                            g e t V e r s i o n                             */
/******************************************************************************/
  
const char *XrdOfs::getVersion() {return XrdVERSION;}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOfs::Configure(XrdSysError &Eroute) {return Configure(Eroute, 0);}

int XrdOfs::Configure(XrdSysError &Eroute, XrdOucEnv *EnvInfo) {
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   extern XrdOss *XrdOssGetSS(XrdSysLogger *, const char *, const char *,
                              const char   *, XrdOucEnv  *, XrdVersionInfo &);
   char *var;
   const char *tmp;
   int  i, j, cfgFD, retc, NoGo = 0;
   XrdOucEnv myEnv;
   XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Print warm-up message
//
   Eroute.Say("++++++ File system initialization started.");

// Establish the network interface that the caller must provide
//
   if (!EnvInfo || !(myIF = (XrdNetIF *)EnvInfo->GetPtr("XrdNetIF*")))
      {Eroute.Emsg("Finder", "Network i/f undefined; unable to self-locate.");
       NoGo = 1;
      }

// Preset all variables with common defaults
//
   Options            = 0;
   if (getenv("XRDDEBUG")) OfsTrace.What = TRACE_MOST | TRACE_debug;

// Allocate a checksum configurator at this point
//
   CksConfig = new XrdCksConfig(ConfigFN, &Eroute, retc,
                                XrdVERSIONINFOVAR(XrdOfs));
   if (!retc) NoGo = 1;

// If there is no config file, return with the defaults sets.
//
   if( !ConfigFN || !*ConfigFN)
     Eroute.Emsg("Config", "Configuration file not specified.");
     else {
           // Try to open the configuration file.
           //
           if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
              return Eroute.Emsg("Config", errno, "open config file",
                                 ConfigFN);
           Config.Attach(cfgFD);

           // Now start reading records until eof.
           //
           while((var = Config.GetMyFirstWord()))
                {if (!strncmp(var, "ofs.", 4)
                 ||  !strcmp(var, "all.role"))
                    if (ConfigXeq(var+4,Config,Eroute)) {Config.Echo();NoGo=1;}
                }

           // Now check if any errors occured during file i/o
           //
           if ((retc = Config.LastError()))
           NoGo = Eroute.Emsg("Config", -retc, "read config file",
                              ConfigFN);
           Config.Close();
          }

// Determine whether we should initialize authorization
//
   if (Options & Authorize)
      {if (setupAuth(Eroute)) NoGo = 1;
          else XrdOfsTPC::Init(Authorization);
      }

// Check if redirection wanted
//
   if (getenv("XRDREDIRECT")) i  = isManager;
      else i = 0;
   if (getenv("XRDRETARGET")) i |= isServer;
   if (getenv("XRDREDPROXY")) i |= isProxy;
   if (i)
      {if ((j = Options & haveRole) && (i ^ j))
          {free(myRole); myRole = strdup(theRole(i));
           Eroute.Say("Config warning: command line role options override "
                       "config file; 'all.role", myRole, "' in effect.");
          }
       Options &= ~(haveRole);
       Options |= i;
      }

// Export our role if we actually have one
//
   if (myRole) XrdOucEnv::Export("XRDROLE", myRole);

// Set the redirect option for other layers
//
   if (Options & isManager)
           XrdOucEnv::Export("XRDREDIRECT", (Options & isMeta ? "M" : "R"));
      else XrdOucEnv::Export("XRDREDIRECT", "0");

// Configure the storage system at this point. This must be done prior to
// configuring cluster processing. First check if we will be proxying.
// If so, then substitute the proxy plug-in where we can.
//
   if ((Options & isProxy) && !(Options & isManager))
      {char buff[2048], *bp, *libofs = getenv("XRDOFSLIB");
       if (!libofs) bp = buff;
          else {strcpy(buff, libofs); bp = buff+strlen(buff)-1;
                while(bp != buff && *(bp-1) != '/') bp--;
               }
       strcpy(bp, "libXrdPss.so");
       if (OssLib) Eroute.Say("Config warning: ",
                   "specified osslib overrides default proxy lib.");
          else OssLib = strdup(buff);
       if (CksConfig->Manager()) Eroute.Say("Config warning: ",
                   "specified ckslib overrides default proxy lib.");
          else CksConfig->Manager(buff, 0);
      }

// Configure third party copy but only if we are not a manager or a proxy
//
   if ((Options & ThirdPC) && !(Options & isProxy) && !(Options & isManager))
      if (!XrdOfsTPC::Start()) NoGo = 1;

// We need to do pre-initialization for event recording as the oss needs some
// environmental information from that initialization to initialize the frm,
// should it need to be used. We will do full evr initialization after the oss
// and the finder are initialized. A bit messy in the current plug-in world.
//
   if (!(Options & isManager) && !evrObject.Init(&Eroute)) NoGo = 1;

// Now configure the storage system
//
   if (!(XrdOfsOss = XrdOssGetSS(Eroute.logger(), ConfigFN, OssLib, OssParms,
                                 EnvInfo, XrdVERSIONINFOVAR(XrdOfs)))) NoGo = 1;

// Initialize redirection.  We type te herald here to minimize confusion
//
   if (Options & haveRole)
      {Eroute.Say("++++++ Configuring ", myRole, " role. . .");
       if (ConfigRedir(Eroute, EnvInfo))
          {Eroute.Emsg("Config", "Unable to create cms client object.");
           NoGo = 1;
          }
      }

// Initialize th Evr object if we are an actual server
//
   if (!(Options & isManager) && !evrObject.Init(Balancer)) NoGo = 1;

// Turn off forwarding if we are not a pure remote redirector or a peer
//
   if (Options & Forwarding)
      if (!(Options & isPeer)
      && (Options & (isServer | isProxy)))
         {Eroute.Say("Config warning: forwarding turned off; not a pure manager");
          Options &= ~(Forwarding);
          fwdCHMOD.Reset(); fwdMKDIR.Reset(); fwdMKPATH.Reset();
          fwdMV.Reset();    fwdRM.Reset();    fwdRMDIR.Reset();
          fwdTRUNC.Reset();
         }

// Configure checksums even for managers
//
   if (!NoGo)
      NoGo |= (Cks = CksConfig->Configure(0, CksRdsz)) == 0;
   delete CksConfig;
   CksConfig = 0;

// If we need to send notifications, initialize the interface
//
   if (!NoGo && evsObject) NoGo = evsObject->Start(&Eroute);

// If POSC processing is enabled (as by default) do it. Warning! This must be
// the last item in the configuration list as we need a working filesystem.
//
   if (poscAuto != -1 && !NoGo) NoGo |= ConfigPosc(Eroute);

// Setup statistical monitoring
//
   OfsStats.setRole(myRole);

// Display final configuration
//
   if (!NoGo) Config_Display(Eroute);

// All done
//
   tmp = (NoGo ? " initialization failed." : " initialization completed.");
   Eroute.Say("------ File system ", myRole, tmp);
   return NoGo;
}

/******************************************************************************/
/*                        C o n f i g _ D i s p l a y                         */
/******************************************************************************/

#define setBuff(x,y) {strcpy(bp, x); bp += y;}
  
void XrdOfs::Config_Display(XrdSysError &Eroute)
{
     const char *cloc, *pval;
     char buff[8192], fwbuff[512], *bp;
     int i;

     if (!ConfigFN || !ConfigFN[0]) cloc = "default";
        else cloc = ConfigFN;
     if (!poscQ) pval = "off";
        else     pval = (poscAuto ? "auto" : "manual");

     snprintf(buff, sizeof(buff), "Config effective %s ofs configuration:\n"
                                  "       ofs.role %s\n"
                                  "%s"
                                  "%s%s%s"
                                  "       ofs.maxdelay   %d\n"
                                  "%s%s%s%s%s"
                                  "%s%s%s"
                                  "       ofs.persist    %s hold %d%s%s%s"
                                  "       ofs.trace      %x",
              cloc, myRole,
              (Options & Authorize ? "       ofs.authorize\n" : ""),
              (AuthLib                   ? "       ofs.authlib " : ""),
              (AuthLib ? AuthLib : ""), (AuthLib ? "\n" : ""),
               MaxDelay,
              (CmsLib                    ? "       ofs.cmslib " : ""),
              (CmsLib ? CmsLib : ""), (CmsParms?" " : ""),
              (CmsParms? CmsParms : ""), (CmsLib ? "\n" : ""),
              (OssLib                    ? "       ofs.osslib " : ""),
              (OssLib ? OssLib : ""), (OssLib ? "\n" : ""),
               pval, poscHold, (poscLog ? " logdir " : ""),
               (poscLog ? poscLog    : ""), (poscLog ? "\n" : ""),
              OfsTrace.What);

     Eroute.Say(buff);

     if (Options & Forwarding)
        {*fwbuff = 0;
         if (ConfigDispFwd(buff, fwdCHMOD))
            {Eroute.Say(buff); strcat(fwbuff, " ch");}
         if (ConfigDispFwd(buff, fwdMKDIR))
            {Eroute.Say(buff); strcat(fwbuff, " mk");}
         if (ConfigDispFwd(buff, fwdMV))
            {Eroute.Say(buff); strcat(fwbuff, " mv");}
         if (ConfigDispFwd(buff, fwdRM))
            {Eroute.Say(buff); strcat(fwbuff, " rm");}
         if (ConfigDispFwd(buff, fwdRMDIR))
            {Eroute.Say(buff); strcat(fwbuff, " rd");}
         if (ConfigDispFwd(buff, fwdTRUNC))
            {Eroute.Say(buff); strcat(fwbuff, " tr");}
         if (*fwbuff) XrdOucEnv::Export("XRDOFS_FWD", fwbuff);
        }

     if (evsObject)
        {bp = buff;
         setBuff("       ofs.notify ", 18);              //  1234567890
         if (evsObject->Enabled(XrdOfsEvs::Chmod))  setBuff("chmod ",  6);
         if (evsObject->Enabled(XrdOfsEvs::Closer)) setBuff("closer ", 7);
         if (evsObject->Enabled(XrdOfsEvs::Closew)) setBuff("closew ", 7);
         if (evsObject->Enabled(XrdOfsEvs::Create)) setBuff("create ", 7);
         if (evsObject->Enabled(XrdOfsEvs::Mkdir))  setBuff("mkdir ",  6);
         if (evsObject->Enabled(XrdOfsEvs::Mv))     setBuff("mv ",     3);
         if (evsObject->Enabled(XrdOfsEvs::Openr))  setBuff("openr ",  6);
         if (evsObject->Enabled(XrdOfsEvs::Openw))  setBuff("openw ",  6);
         if (evsObject->Enabled(XrdOfsEvs::Rm))     setBuff("rm ",     3);
         if (evsObject->Enabled(XrdOfsEvs::Rmdir))  setBuff("rmdir ",  6);
         if (evsObject->Enabled(XrdOfsEvs::Trunc))  setBuff("trunc ",  6);
         if (evsObject->Enabled(XrdOfsEvs::Fwrite)) setBuff("fwrite ", 7);
         setBuff("msgs ", 5);
         i=sprintf(fwbuff,"%d %d ",evsObject->maxSmsg(),evsObject->maxLmsg());
         setBuff(fwbuff, i);
         cloc = evsObject->Prog();
         if (*cloc != '>') setBuff("|",1);
         setBuff(cloc, strlen(cloc));
         setBuff("\0", 1);
         Eroute.Say(buff);
        }
}

/******************************************************************************/
/*                     p r i v a t e   f u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                         C o n f i g D i s p F w d                          */
/******************************************************************************/
  
int XrdOfs::ConfigDispFwd(char *buff, struct fwdOpt &Fwd)
{
   const char *cP;
   char pbuff[16], *bp;

// Return if this is not being forwarded
//
   if (!(cP = Fwd.Cmd)) return 0;
   bp = buff;
   setBuff("       ofs.forward ", 19);

// Chck which way this is being forwarded
//
         if (*Fwd.Cmd == '+'){setBuff("2way ",5); cP++;}
   else  if (!Fwd.Port)      {setBuff("1way ",5);}
   else {                     setBuff("3way ",5);
         if (Fwd.Port < 0)   {setBuff("local ",6);}
            else {int n = sprintf(pbuff, ":%d ", Fwd.Port);
                  setBuff(Fwd.Host, strlen(Fwd.Host));
                  setBuff(pbuff, n);
                 }
        }
   setBuff(cP, strlen(cP));
   return 1;
}

/******************************************************************************/
/*                            C o n f i g P o s c                             */
/******************************************************************************/
  
int XrdOfs::ConfigPosc(XrdSysError &Eroute)
{
   extern XrdOfs* XrdOfsFS;
   const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
   class  CloseFH : public XrdOfsHanCB
         {public: void Retired(XrdOfsHandle *hP) {XrdOfsFS->Unpersist(hP);}};
   static XrdOfsHanCB *hCB = static_cast<XrdOfsHanCB *>(new CloseFH);

   XrdOfsPoscq::recEnt  *rP, *rPP;
   XrdOfsPoscq::Request *qP;
   XrdOfsHandle *hP;
   const char *iName;
   char pBuff[MAXPATHLEN], *aPath;
   int NoGo, rc;

// Construct the proper path to the recovery file
//
   iName = XrdOucUtils::InstName(-1);
   if (poscLog) aPath = XrdOucUtils::genPath(poscLog, iName, ".ofs/posc.log");
      else {if (!(aPath = getenv("XRDADMINPATH")))
               {XrdOucUtils::genPath(pBuff, MAXPATHLEN, "/tmp", iName);
                aPath = pBuff;
               }
            aPath = XrdOucUtils::genPath(aPath, (char *)0, ".ofs/posc.log");
           }
   rc = strlen(aPath)-1;
   if (aPath[rc] == '/') aPath[rc] = '\0';
   free(poscLog); poscLog = aPath;

// Make sure directory path exists
//
   if ((rc = XrdOucUtils::makePath(poscLog, AMode)))
      {Eroute.Emsg("Config", rc, "create path for", poscLog);
       return 1;
      }

// Create object then initialize it
//
   poscQ = new XrdOfsPoscq(&Eroute, XrdOfsOss, poscLog);
   rP = poscQ->Init(rc);
   if (!rc) return 1;

// Get file handles and put then in pending delete for all recovered records
//
   NoGo = 0;
   while(rP)
        {qP = &(rP->reqData);
         if (qP->addT && poscHold)
            {if (XrdOfsHandle::Alloc(qP->LFN, XrdOfsHandle::opPC, &hP))
                {Eroute.Emsg("Config", "Unable to persist", qP->User, qP->LFN);
                 qP->addT = 0;
                } else {
                 hP->PoscSet(qP->User, rP->Offset, rP->Mode);
                 hP->Retire(hCB, poscHold);
                }
            }
         if (!(qP->addT) || !poscHold)
            {if ((rc = XrdOfsOss->Unlink(qP->LFN)) && rc != -ENOENT)
                {Eroute.Emsg("Config", rc, "unpersist", qP->LFN); NoGo = 1;}
                else {Eroute.Emsg("Config", "Unpersisted", qP->User, qP->LFN);
                      poscQ->Del(qP->LFN, rP->Offset);
                     }
            }
         rPP = rP; rP = rP->Next; delete rPP;
        }

// All done
//
   return NoGo;
}

/******************************************************************************/
/*                           C o n f i g R e d i r                            */
/******************************************************************************/
  
int XrdOfs::ConfigRedir(XrdSysError &Eroute, XrdOucEnv *EnvInfo)
{
   XrdCmsClient *(*CmsPI)(XrdSysLogger *, int, int, XrdOss *);
   CmsPI = 0;
   XrdSysLogger *myLogger = Eroute.logger();
   int isRedir = Options & isManager;
   int RMTopts = (Options & isServer ? XrdCms::IsTarget : 0)
               | (Options & isProxy  ? XrdCms::IsProxy  : 0)
               | (Options & isMeta   ? XrdCms::IsMeta   : 0);
   int TRGopts = (Options & isProxy  ? XrdCms::IsProxy  : 0)
               | (isRedir ? XrdCms::IsRedir : 0) | XrdCms::IsTarget;

// If a cmslib was specified then create a plugin object
//
   if (CmsLib)
      {XrdSysPlugin myLib(&Eroute,CmsLib,"cmslib",&XrdVERSIONINFOVAR(XrdOfs));
       CmsPI = (XrdCmsClient *(*)(XrdSysLogger *, int, int, XrdOss *))
                                  (myLib.getPlugin("XrdCmsGetClient"));
       if (!CmsPI) return 1;
       myLib.Persist();
      }

// For manager roles, we simply do a standard config
//
   if (isRedir) 
      {     if (CmsLib) Finder = CmsPI(myLogger, RMTopts, myPort, XrdOfsOss);
       else if (XrdCmsFinderRMT::VCheck(XrdVERSIONINFOVAR(XrdOfs)))
                        Finder = (XrdCmsClient *)new XrdCmsFinderRMT(myLogger,
                                                                RMTopts,myPort);
       else return 1;
       if (!Finder) return 1;
       if (!Finder->Configure(ConfigFN, CmsParms, EnvInfo))
          {delete Finder; Finder = 0; return 1;}
       if (EnvInfo) EnvInfo->PutPtr("XRDCMSMANLIST", Finder->Managers());
      }

// For server roles find the port number and create the object. We used to pass
// the storage system object to the finder to allow it to process cms storage
// requests. The cms no longer sends such requests so there is no need to do
// so. And, in fact, we need to defer creating a storage system until after the
// finder is created. So, it's just as well we pass a numm pointer. At some
// point the finder should remove all storage system related code.
//
   if (Options & (isServer | (isPeer & ~isManager)))
      {if (!myPort)
          {Eroute.Emsg("Config", "Unable to determine server's port number.");
           return 1;
          }
            if (CmsLib) Balancer = CmsPI(myLogger, TRGopts, myPort, XrdOfsOss);
       else if (XrdCmsFinderTRG::VCheck(XrdVERSIONINFOVAR(XrdOfs)))
                        Balancer = (XrdCmsClient *)new XrdCmsFinderTRG(myLogger,
                                                                TRGopts,myPort);
       else return 1;
       if (!Balancer) return 1;
       if (!Balancer->Configure(ConfigFN, CmsParms, EnvInfo))
          {delete Balancer; Balancer = 0; return 1;}
       if (Options & isProxy) Balancer = 0; // No chatting for proxies
      }

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/
  
int XrdOfs::ConfigXeq(char *var, XrdOucStream &Config,
                                 XrdSysError &Eroute)
{
    char *val, vBuff[64];

    // Now assign the appropriate global variable
    //
    TS_Bit("authorize",     Options, Authorize);
    TS_Xeq("authlib",       xalib);
    TS_Xeq("ckslib",        xclib);
    TS_Xeq("cksrdsz",       xcrds);
    TS_Xeq("cmslib",        xcmsl);
    TS_Xeq("forward",       xforward);
    TS_Xeq("maxdelay",      xmaxd);
    TS_Xeq("notify",        xnot);
    TS_Xeq("notifymsg",     xnmsg);
    TS_Xeq("osslib",        xolib);
    TS_Xeq("persist",       xpers);
    TS_Xeq("role",          xrole);
    TS_Xeq("tpc",           xtpc);
    TS_Xeq("trace",         xtrace);

    // Get the actual value for simple directives
    //
    strlcpy(vBuff, var, sizeof(vBuff)); var = vBuff;
    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "value not specified for", var); return 1;}

    // No match found, complain.
    //
    Eroute.Say("Config warning: ignoring unknown directive '",var,"'.");
    Config.Echo();
    return 0;
}

/******************************************************************************/
/*                                 x a l i b                                  */
/******************************************************************************/
  
/* Function: xalib

   Purpose:  To parse the directive: authlib <path> [<parms>]

             <path>    the path of the authorization library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xalib(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val, parms[1024];

// Get the path
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "authlib not specified"); return 1;}

// Record the path
//
   if (AuthLib) free(AuthLib);
   AuthLib = strdup(val);

// Record any parms
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "authlib parameters too long"); return 1;}
   if (AuthParm) free(AuthParm);
   AuthParm = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/*                                 x c l i b                                  */
/******************************************************************************/
  
/* Function: xclib

   Purpose:  To parse the directive: ckslib <digest> <path> [<parms>]

             <digest>  the name of the checksum. The special name "*" is used
                       load the checksum manager library.
             <path>    the path of the checksum library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xclib(XrdOucStream &Config, XrdSysError &Eroute)
{
// Return the result
//
   return CksConfig->ParseLib(Config);
}
  
/******************************************************************************/
/*                                 x c m s l                                  */
/******************************************************************************/
  
/* Function: xcmsl

   Purpose:  To parse the directive: cmslib <path> [<parms>]

             <path>    the path of the cms library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xcmsl(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val, parms[2048];

// Get the path and parms
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "cmslib not specified"); return 1;}

// Set the CmsLib pointer
//
   if (CmsLib) free(CmsLib);
   CmsLib = strdup(val);

// Combine the path and parameters
//
   *parms = 0;
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "cmslib parameters too long"); return 1;}

// Record the parameters, if any
//
   if (CmsParms) free(CmsParms);
   CmsParms = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/*                                 x c r d s                                  */
/******************************************************************************/
  
/* Function: xcrds

   Purpose:  To parse the directive: cksrdsz <size>

             <size>  number of bytes to segment reads when calclulating a
                     checksum. Can be suffixed by k,m,g. Maximum is 1g and
                     is automatically set to be atleast 64k and to be a
                     multiple of 64k.

  Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xcrds(XrdOucStream &Config, XrdSysError &Eroute)
{
   static const long long maxRds = 1024*1024*1024;
   char *val;
   long long rdsz;

// Get the size
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "cksrdsz size not specified"); return 1;}

// Now convert it
//
   if (XrdOuca2x::a2sz(Eroute, "cksrdsz size", val, &rdsz, 1, maxRds)) return 1;
   CksRdsz = rdsz;
   return 0;
}
  
/******************************************************************************/
/*                              x f o r w a r d                               */
/******************************************************************************/
  
/* Function: xforward

   Purpose:  To parse the directive: forward [<handling>] <metaops>

             handling: 1way | 2way | 3way {local | <host>:<port>}

             1way      forward does not respond (the default)
             2way      forward responds; relay response back.
             3way      forward 1way and execute locally or redirect to <host>
             <metaops> list of meta-file operations to forward to manager

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xforward(XrdOucStream &Config, XrdSysError &Eroute)
{
    enum fwdType {OfsFWDALL = 0x3f, OfsFWDCHMOD = 0x01, OfsFWDMKDIR = 0x02,
                  OfsFWDMV  = 0x04, OfsFWDRM    = 0x08, OfsFWDRMDIR = 0x10,
                  OfsFWDREM = 0x18, OfsFWDTRUNC = 0x20, OfsFWDNONE  = 0};

    static struct fwdopts {const char *opname; fwdType opval;} fwopts[] =
       {
        {"all",      OfsFWDALL},
        {"chmod",    OfsFWDCHMOD},
        {"mkdir",    OfsFWDMKDIR},
        {"mv",       OfsFWDMV},
        {"rm",       OfsFWDRM},
        {"rmdir",    OfsFWDRMDIR},
        {"remove",   OfsFWDREM},
        {"trunc",    OfsFWDTRUNC}
       };
    int fwval = OfsFWDNONE, fwspec = OfsFWDNONE;
    int numopts = sizeof(fwopts)/sizeof(struct fwdopts);
    int i, neg, rPort = 0, is2way = 0, is3way = 0;
    char *val, *pp, rHost[512];

    *rHost = '\0';
    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "foward option not specified"); return 1;}
    if ((is2way = !strcmp("2way", val)) || !strcmp("1way", val)
    ||  (is3way = !strcmp("3way", val)))
       if (!(val = Config.GetWord()))
          {Eroute.Emsg("Config", "foward operation not specified"); return 1;}

    if (is3way)
       {if (!strcmp("local", val)) rPort = -1;
        else
       {if (*val == ':')
           {Eroute.Emsg("Config", "redirect host not specified"); return 1;}
        if (!(pp = index(val, ':')))
           {Eroute.Emsg("Config", "redirect port not specified"); return 1;}
        if ((rPort = atoi(pp+1)) <= 0)
           {Eroute.Emsg("Config", "redirect port is invalid");    return 1;}
        *pp = '\0';
        strlcpy(rHost, val, sizeof(rHost));
       }
        if (!(val = Config.GetWord()))
           {Eroute.Emsg("Config", "foward operation not specified"); return 1;}
       }

    while (val)
         {if (!strcmp(val, "off")) {fwval = OfsFWDNONE; fwspec = OfsFWDALL;}
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, fwopts[i].opname))
                           {if (neg) fwval &= ~fwopts[i].opval;
                               else  fwval |=  fwopts[i].opval;
                            fwspec |= fwopts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      Eroute.Say("Config warning: ignoring invalid foward option '",val,"'.");
                  }
          val = Config.GetWord();
         }

    if (fwspec & OfsFWDCHMOD) 
       {fwdCHMOD.Cmd = (fwval&OfsFWDCHMOD ? (is2way ? "+chmod" :"chmod")  : 0);
        if (fwdCHMOD.Host) free(fwdCHMOD.Host);
        fwdCHMOD.Host = strdup(rHost); fwdCHMOD.Port = rPort;
       }
    if (fwspec&OfsFWDMKDIR) 
       {fwdMKDIR.Cmd = (fwval&OfsFWDMKDIR ? (is2way ? "+mkdir" :"mkdir")  : 0);
        if (fwdMKDIR.Host) free(fwdMKDIR.Host);
        fwdMKDIR.Host = strdup(rHost); fwdMKDIR.Port = rPort;
        fwdMKPATH.Cmd= (fwval&OfsFWDMKDIR ? (is2way ? "+mkpath":"mkpath") : 0);
        if (fwdMKPATH.Host) free(fwdMKPATH.Host);
        fwdMKPATH.Host = strdup(rHost); fwdMKPATH.Port = rPort;
       }
    if (fwspec&OfsFWDMV)    
       {fwdMV   .Cmd = (fwval&OfsFWDMV    ? (is2way ? "+mv"    :"mv")     : 0);
        if (fwdMV.Host) free(fwdMV.Host);
        fwdMV.Host = strdup(rHost); fwdMV.Port = rPort;
       }
    if (fwspec&OfsFWDRM)    
       {fwdRM   .Cmd = (fwval&OfsFWDRM    ? (is2way ? "+rm"    :"rm")     : 0);
        if (fwdRM.Host) free(fwdRM.Host);
        fwdRM.Host = strdup(rHost); fwdRM.Port = rPort;
       }
    if (fwspec&OfsFWDRMDIR) 
       {fwdRMDIR.Cmd = (fwval&OfsFWDRMDIR ? (is2way ? "+rmdir" :"rmdir")  : 0);
        if (fwdRMDIR.Host) free(fwdRMDIR.Host);
        fwdRMDIR.Host = strdup(rHost); fwdRMDIR.Port = rPort;
       }
    if (fwspec&OfsFWDTRUNC) 
       {fwdTRUNC.Cmd = (fwval&OfsFWDTRUNC ? (is2way ? "+trunc" :"trunc")  : 0);
        if (fwdTRUNC.Host) free(fwdTRUNC.Host);
        fwdTRUNC.Host = strdup(rHost); fwdTRUNC.Port = rPort;
       }

// All done
//
   Options |= Forwarding;
   return 0;
}
  
/******************************************************************************/
/*                                 x m a x d                                  */
/******************************************************************************/

/* Function: xmaxd

   Purpose:  To parse the directive: maxdelay <secs>

             <secs>    maximum delay imposed for staging

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xmaxd(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int maxd;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config","maxdelay value not specified");return 1;}
      if (XrdOuca2x::a2i(Eroute, "maxdelay", val, &maxd, 30)) return 1;

      MaxDelay = maxd;
      return 0;
}

/******************************************************************************/
/*                                 x n m s g                                  */
/******************************************************************************/

/* Function: xnmsg

   Purpose:  To parse the directive: notifymsg <event> <msg>

   Args:     <events> - one or more of: all chmod closer closew close mkdir mv
                                        openr openw open rm rmdir fwrite
             <msg>      the notification message to be sent (see notify).

   Type: Manager only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xnmsg(XrdOucStream &Config, XrdSysError &Eroute)
{
    static struct notopts {const char *opname; XrdOfsEvs::Event opval;}
        noopts[] = {
        {"chmod",    XrdOfsEvs::Chmod},
        {"closer",   XrdOfsEvs::Closer},
        {"closew",   XrdOfsEvs::Closew},
        {"create",   XrdOfsEvs::Create},
        {"mkdir",    XrdOfsEvs::Mkdir},
        {"mv",       XrdOfsEvs::Mv},
        {"openr",    XrdOfsEvs::Openr},
        {"openw",    XrdOfsEvs::Openw},
        {"rm",       XrdOfsEvs::Rm},
        {"rmdir",    XrdOfsEvs::Rmdir},
        {"trunc",    XrdOfsEvs::Trunc},
        {"fwrite",   XrdOfsEvs::Fwrite}
       };
    XrdOfsEvs::Event noval;
    int numopts = sizeof(noopts)/sizeof(struct notopts);
    char *val, buff[1024];
    XrdOucEnv *myEnv;
    int i;

   // At this point, make sure we have a value
   //
   if (!(val = Config.GetWord()))
      {Eroute.Emsg("Config", "notifymsg event not specified");
       return 1;
      }

   // Get the evant number
   //
   for (i = 0; i < numopts; i++) if (!strcmp(val, noopts[i].opname)) break;
   if (i >= numopts)
      {Eroute.Say("Config warning: ignoring invalid notify event '",val,"'.");
       return 1;
      }
   noval = noopts[i].opval;

   // We need to suck all the tokens to the end of the line for remaining
   // options. Do so, until we run out of space in the buffer.
   //
   myEnv = Config.SetEnv(0);
   if (!Config.GetRest(buff, sizeof(buff)))
      {Eroute.Emsg("Config", "notifymsg arguments too long");
       Config.SetEnv(myEnv);
       return 1;
      }

   // Restore substitutions and parse the message
   //
   Config.SetEnv(myEnv);
   return XrdOfsEvs::Parse(Eroute, noval, buff);
}
  
/******************************************************************************/
/*                                  x n o t                                   */
/* Based on code developed by Derek Feichtinger, CERN.                        */
/******************************************************************************/

/* Function: xnot

   Purpose:  Parse directive: notify <events> [msgs <min> [<max>]] 
                                     {|<prog> | ><path>}

   Args:     <events> - one or more of: all chmod closer closew close mkdir mv
                                        openr openw open rm rmdir fwrite
                        opaque and other possible information to be sent.
             msgs     - Maximum number of messages to keep and queue. The
                        <min> if for small messages (default 90) and <max> is
                        for big messages (default 10).
             <prog>   - is the program to execute and dynamically feed messages
                        about the indicated events. Messages are piped to prog.
             <path>   - is the udp named socket to receive the message. The
                        server creates the path if it's not present.

   Output: 0 upon success or !0 upon failure.
*/
int XrdOfs::xnot(XrdOucStream &Config, XrdSysError &Eroute)
{
    static struct notopts {const char *opname; XrdOfsEvs::Event opval;}
        noopts[] = {
        {"all",      XrdOfsEvs::All},
        {"chmod",    XrdOfsEvs::Chmod},
        {"close",    XrdOfsEvs::Close},
        {"closer",   XrdOfsEvs::Closer},
        {"closew",   XrdOfsEvs::Closew},
        {"create",   XrdOfsEvs::Create},
        {"mkdir",    XrdOfsEvs::Mkdir},
        {"mv",       XrdOfsEvs::Mv},
        {"open",     XrdOfsEvs::Open},
        {"openr",    XrdOfsEvs::Openr},
        {"openw",    XrdOfsEvs::Openw},
        {"rm",       XrdOfsEvs::Rm},
        {"rmdir",    XrdOfsEvs::Rmdir},
        {"trunc",    XrdOfsEvs::Trunc},
        {"fwrite",   XrdOfsEvs::Fwrite}
       };
    XrdOfsEvs::Event noval = XrdOfsEvs::None;
    int numopts = sizeof(noopts)/sizeof(struct notopts);
    int i, neg, msgL = 90, msgB = 10;
    char *val, parms[1024];

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "notify parameters not specified"); return 1;}
    while (val && *val != '|' && *val != '>')
         {if (!strcmp(val, "msgs"))
             {if (!(val = Config.GetWord()))
                 {Eroute.Emsg("Config", "notify msgs value not specified");
                  return 1;
                 }
              if (XrdOuca2x::a2i(Eroute, "msg count", val, &msgL, 0)) return 1;
              if (!(val = Config.GetWord())) break;
              if (isdigit(*val)
              && XrdOuca2x::a2i(Eroute, "msg count", val, &msgB, 0)) return 1;
              if (!(val = Config.GetWord())) break;
              continue;
             }
          if ((neg = (val[0] == '-' && val[1]))) val++;
          i = strlen(val);
          for (i = 0; i < numopts; i++)
              {if (!strcmp(val, noopts[i].opname))
                  {if (neg) noval = static_cast<XrdOfsEvs::Event>(~noopts[i].opval&noval);
                      else  noval = static_cast<XrdOfsEvs::Event>( noopts[i].opval|noval);
                   break;
                  }
              }
          if (i >= numopts)
             Eroute.Say("Config warning: ignoring invalid notify event '",val,"'.");
          val = Config.GetWord();
         }

// Check if we have a program here and some events
//
   if (!val)   {Eroute.Emsg("Config","notify program not specified");return 1;}
   if (!noval) {Eroute.Emsg("Config","notify events not specified"); return 1;}

// Get the remaining parameters
//
   Config.RetToken();
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "notify parameters too long"); return 1;}
   val = (*parms == '|' ? parms+1 : parms);

// Create an notification object
//
   if (evsObject) delete evsObject;
   evsObject = new XrdOfsEvs(noval, val, msgL, msgB);

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                 x o l i b                                  */
/******************************************************************************/
  
/* Function: xolib

   Purpose:  To parse the directive: osslib <path> [<parms>]

             <path>    the path of the oss library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xolib(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val, parms[2048];

// Get the path and parms
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "osslib not specified"); return 1;}

// Record the path
//
   if (OssLib) free(OssLib);
   OssLib = strdup(val);

// Get any parameters
//
   *parms = 0;
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "osslib parameters too long"); return 1;}

// Record the parameters
//
   if (OssParms) free(OssParms);
   OssParms = (*parms ? strdup(parms) : 0);
   return 0;
}

/******************************************************************************/
/*                                 x p e r s                                  */
/******************************************************************************/
  
/* Function: xpers

   Purpose:  To parse the directive: persist [auto | manual | off]
                                             [hold <sec>] [logdir <dirp>]

             auto      POSC processing always on for creation requests
             manual    POSC processing must be requested (default)
             off       POSC processing is disabled
             <sec>     Seconds inclomplete files held (default 10m)
             <dirp>    Directory to hold POSC recovery log (default adminpath)

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xpers(XrdOucStream &Config, XrdSysError &Eroute)
{
   char *val;
   int htime = -1, popt = -2;

   if (!(val = Config.GetWord()))
      {Eroute.Emsg("Config","persist option not specified");return 1;}

// Check for valid option
//
        if (!strcmp(val, "auto"   )) popt =  1;
   else if (!strcmp(val, "off"    )) popt = -1;
   else if (!strcmp(val, "manual" )) popt =  0;

// Check if we should get the next token
//
   if (popt > -2) val = Config.GetWord();

// Check for hold or log
//
   while(val)
        {     if (!strcmp(val, "hold"))
                 {if (!(val = Config.GetWord()))
                     {Eroute.Emsg("Config","persist hold value not specified");
                      return 1;
                     }
                  if (XrdOuca2x::a2tm(Eroute,"persist hold",val,&htime,0))
                      return 1;
                 }
         else if (!strcmp(val, "logdir"))
                 {if (!(val = Config.GetWord()))
                     {Eroute.Emsg("Config","persist logdir path not specified");
                      return 1;
                     }
                  if (poscLog) free(poscLog);
                  poscLog = strdup(val);
                 }
         else Eroute.Say("Config warning: ignoring invalid persist option '",val,"'.");
         val = Config.GetWord();
        }

// Set values as needed
//
   if (htime >= 0) poscHold = htime;
   if (popt  > -2) poscAuto = popt;
   return 0;
}

/******************************************************************************/
/*                                 x r o l e                                  */
/******************************************************************************/

/* Function: xrole

   Purpose:  Parse: role { {[meta] | [proxy]} manager
                           |         [proxy]  server
                           |         [proxy]  supervisor
                         } [if ...]

             manager    xrootd: act as a manager (redirecting server). Prefixes:
                                meta  - connect only to manager meta's
                                proxy - ignored
                        cmsd:   accept server subscribes and redirectors. Prefix
                                modifiers do the following:
                                meta  - No other managers apply
                                proxy - manage a cluster of proxy servers

             server     xrootd: act as a server (supply local data). Prefix
                                modifications do the following:
                                proxy - server is part of a cluster. A local
                                        cmsd is required.
                        cmsd:   subscribe to a manager, possibly as a proxy.

             supervisor xrootd: equivalent to manager. The prefix modification
                                is ignored.
                        cmsd:   equivalent to manager but also subscribe to a
                                manager. When proxy is specified, then subscribe
                                as a proxy and only accept proxies.

             if         Apply the manager directive if "if" is true. See
                        XrdOucUtils:doIf() for "if" syntax.

   Notes  1. The peer designation only affects how the olbd communicates.

   Type: Server only, non-dynamic.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xrole(XrdOucStream &Config, XrdSysError &Eroute)
{
   const int resetit = ~haveRole;
    XrdCmsRole::RoleID roleID;
    char *val, *Tok1, *Tok2;
    int rc, ropt = 0;

// Get the first token
//
   if (!(val = Config.GetWord()) || !strcmp(val, "if"))
      {Eroute.Emsg("Config", "role not specified"); return 1;}
   Tok1 = strdup(val);

// Get second token which might be an "if"
//
   if ((val = Config.GetWord()) && strcmp(val, "if"))
      {Tok2 = strdup(val);
       val = Config.GetWord();
      } else Tok2 = 0;

// Process the if at this point
//
   if (val && !strcmp("if", val))
      {if ((rc = XrdOucUtils::doIf(&Eroute,Config,"role directive",
                                   getenv("XRDHOST"), XrdOucUtils::InstName(1),
                                   getenv("XRDPROG"))) <= 0)
          {free(Tok1); if (Tok2) free(Tok2);
           if (!rc) Config.noEcho();
           return (rc < 0);
          }
      }

// Convert the role names to a role ID, if possible
//
   roleID = XrdCmsRole::Convert(Tok1, Tok2);

// Set markers based on the role we have
//
   rc = 0;
   switch(roleID)
         {case XrdCmsRole::MetaManager:  ropt = isManager | isMeta ; break;
          case XrdCmsRole::Manager:      ropt = isManager          ; break;
          case XrdCmsRole::Supervisor:   ropt = isSuper            ; break;
          case XrdCmsRole::Server:       ropt = isServer           ; break;
          case XrdCmsRole::ProxyManager: ropt = isManager | isProxy; break;
          case XrdCmsRole::ProxySuper:   ropt = isSuper   | isProxy; break;
          case XrdCmsRole::ProxyServer:  ropt = isServer  | isProxy; break;
          default: Eroute.Emsg("Config", "invalid role -", Tok1, Tok2); rc = 1;
         }

// Release storage and return if an error occured
//
   free(Tok1);
   if (Tok2) free(Tok2);
   if (rc) return rc;

// Set values
//
    free(myRole);
    myRole = strdup(XrdCmsRole::Name(roleID));
    strcpy(myRType, XrdCmsRole::Type(roleID));
    Options &= resetit;
    Options |= ropt;
    return 0;
}

/******************************************************************************/
/*                                  x t p c                                   */
/******************************************************************************/
  
/* Function: xtpc

   Purpose:  To parse the directive: tpc [cksum <type>] [ttl <dflt> [<max>]]
                                         [logok] [xfr <n>] [allow <parms>]
                                         [require {all|client|dest} <auth>[+]]
                                         [restrict <path>] [streams <num>]
                                         [echo] [scan {stderr | stdout}]
                                         [autorm] [pgm <path> [parms]]

             parms: [dn <name>] [group <grp>] [host <hn>] [vo <vo>]

             <dflt>  the default seconds a tpc authorization may be valid.
             <max>   the maximum seconds a tpc authorization may be valid.
             cksum   checksum incomming files using <type> checksum.
             logok   log successful authorizations.
             allow   only allow destinations that match the specified
                     authentication specification.
             <n>     maximum number of simultaneous transfers.
             <num>   the number of TCP streams to use for the copy.
             <auth>  require that the client, destination, or both (i.e. all)
                     use the specified authentication protocol. Additional
                     require statements may be specified to add additional
                     valid authentication mechanisms. If the <auth> is suffixed
                     by a plus, then the request must also be encrypted using
                     the authentication's session key.
             echo    echo the pgm's output to the log.
             autorm  Remove file when copy fails.
             scan    scan fr error messages either in stderr or stdout. The
                     default is to scan both.
             pgm     specifies the transfer command with optional paramaters.
                     It must be the last parameter on the line.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xtpc(XrdOucStream &Config, XrdSysError &Eroute)
{
   XrdOfsTPC::iParm Parms;
   char *val, pgm[1024];
   int  reqType;
   *pgm = 0;

   while((val =  Config.GetWord()))
        {if (!strcmp(val, "allow"))
            {if (!xtpcal(Config, Eroute)) return 1;
             continue;
            }
         if (!strcmp(val, "cksum"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","cksum type not specified"); return 1;}
             if (Parms.Ckst) free(Parms.Ckst);
             Parms.Ckst = strdup(val);
             continue;
            }
         if (!strcmp(val, "scan"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","scan type not specified"); return 1;}
                  if (strcmp(val, "stderr")) Parms.Grab = -2;
             else if (strcmp(val, "stdout")) Parms.Grab = -1;
                {Eroute.Emsg("Config","invalid scan type -",val); return 1;}
             continue;
            }
         if (!strcmp(val, "echo"))  {Parms.xEcho = 1; continue;}
         if (!strcmp(val, "logok")) {Parms.Logok = 1; continue;}
         if (!strcmp(val, "autorm")){Parms.autoRM = 1; continue;}
         if (!strcmp(val, "pgm"))
            {if (!Config.GetRest(pgm, sizeof(pgm)))
                {Eroute.Emsg("Config", "tpc command line too long"); return 1;}
             if (!*pgm)
                {Eroute.Emsg("Config", "tpc program not specified"); return 1;}
             Parms.Pgm = pgm;
             break;
            }
         if (!strcmp(val, "require"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc require parameter not specified"); return 1;}
                  if (!strcmp(val, "all"))    reqType = XrdOfsTPC::reqALL;
             else if (!strcmp(val, "client")) reqType = XrdOfsTPC::reqORG;
             else if (!strcmp(val, "dest"))   reqType = XrdOfsTPC::reqDST;
             else {Eroute.Emsg("Config", "invalid tpc require type -", val); return 1;}
             break;
             if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc require auth not specified"); return 1;}
             XrdOfsTPC::Require(val, reqType);
             continue;
            }
         if (!strcmp(val, "restrict"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc restrict path not specified"); return 1;}
             if (*val != '/')
                {Eroute.Emsg("Config","tpc restrict path not absolute");  return 1;}
             if (!XrdOfsTPC::Restrict(val)) return 1;
             continue;
            }
         if (!strcmp(val, "ttl"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc ttl value not specified"); return 1;}
             if (XrdOuca2x::a2tm(Eroute,"tpc ttl default",val,&Parms.Dflttl,1))
                 return 1;
             if (!(val = Config.GetWord())) break;
             if (!(isdigit(*val))) {Config.RetToken(); continue;}
             if (XrdOuca2x::a2tm(Eroute,"tpc ttl maximum",val,&Parms.Maxttl,1))
                 return 1;
             continue;
            }
         if (!strcmp(val, "xfr"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc xfr value not specified"); return 1;}
             if (XrdOuca2x::a2i(Eroute,"tpc xfr",val,&Parms.Xmax,1)) return 1;
             continue;
            }
         if (!strcmp(val, "streams"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc streams value not specified"); return 1;}
             if (XrdOuca2x::a2i(Eroute,"tpc streams",val,&Parms.Strm,1)) return 1;
             continue;
            }
         Eroute.Say("Config warning: ignoring invalid tpc option '",val,"'.");
        }

   XrdOfsTPC::Init(Parms);
   Options |= ThirdPC;
   return 0;
}

/******************************************************************************/
/*                                x t p c a l                                 */
/******************************************************************************/

int XrdOfs::xtpcal(XrdOucStream &Config, XrdSysError &Eroute)
{
   struct tpcalopts {const char *opname; char *opval;} tpopts[] =
         {{"dn", 0}, {"group", 0}, {"host", 0}, {"vo", 0}};
   int i, spec = 0, numopts = sizeof(tpopts)/sizeof(struct tpcalopts);
   char *val;

   while((val =  Config.GetWord()))
        {for (i = 0; i < numopts && strcmp(tpopts[i].opname, val); i++) {}
         if (i > numopts) {Config.RetToken(); break;}
            {Eroute.Emsg("Config", "invalid tpc allow parameter -", val);
             return 0;
            }
         if (!(val = Config.GetWord()))
            {Eroute.Emsg("Config","tpc allow",tpopts[i].opname,"value not specified");
             return 0;
            }
         if (tpopts[i].opval) free(tpopts[i].opval);
         tpopts[i].opval = strdup(val);
         spec = 1;
        }

   if (!spec) {Eroute.Emsg("Config","tpc allow parms not specified"); return 1;}

   XrdOfsTPC::Allow(tpopts[0].opval, tpopts[1].opval,
                    tpopts[2].opval, tpopts[3].opval);
   return 1;
}
  
/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xtrace(XrdOucStream &Config, XrdSysError &Eroute)
{
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"aio",      TRACE_aio},
        {"all",      TRACE_ALL},
        {"chmod",    TRACE_chmod},
        {"close",    TRACE_close},
        {"closedir", TRACE_closedir},
        {"debug",    TRACE_debug},
        {"delay",    TRACE_delay},
        {"dir",      TRACE_dir},
        {"exists",   TRACE_exists},
        {"getstats", TRACE_getstats},
        {"fsctl",    TRACE_fsctl},
        {"io",       TRACE_IO},
        {"mkdir",    TRACE_mkdir},
        {"most",     TRACE_MOST},
        {"open",     TRACE_open},
        {"opendir",  TRACE_opendir},
        {"qscan",    TRACE_qscan},
        {"read",     TRACE_read},
        {"readdir",  TRACE_readdir},
        {"redirect", TRACE_redirect},
        {"remove",   TRACE_remove},
        {"rename",   TRACE_rename},
        {"sync",     TRACE_sync},
        {"truncate", TRACE_truncate},
        {"write",    TRACE_write}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);
    char *val;

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
    OfsTrace.What = trval;

// All done
//
   return 0;
}

/******************************************************************************/
/*                             s e t u p A u t h                              */
/******************************************************************************/

int XrdOfs::setupAuth(XrdSysError &Eroute)
{
   extern XrdAccAuthorize *XrdAccDefaultAuthorizeObject
                          (XrdSysLogger   *lp,    const char   *cfn,
                           const char     *parm,  XrdVersionInfo &vInfo);

   XrdAccAuthorize *(*ep)(XrdSysLogger *, const char *, const char *);

// Authorization comes from the library or we use the default
//
   if (!AuthLib) return 0 == (Authorization = XrdAccDefaultAuthorizeObject
                             (Eroute.logger(), ConfigFN, AuthParm,
                              XrdVERSIONINFOVAR(XrdOfs)));

// Create a plugin object
//
  {XrdSysPlugin myLib(&Eroute, AuthLib, "authlib", &XrdVERSIONINFOVAR(XrdOfs));
   ep = (XrdAccAuthorize *(*)(XrdSysLogger *, const char *, const char *))
                             (myLib.getPlugin("XrdAccAuthorizeObject"));
   if (!ep) return 1;
   myLib.Persist();
  }

// Get the Object now
//
   return 0 == (Authorization = ep(Eroute.logger(), ConfigFN, AuthParm));
}
  
/******************************************************************************/
/*                               t h e R o l e                                */
/******************************************************************************/
  
const char *XrdOfs::theRole(int opts)
{
          if (opts & isPeer)    return "peer";
     else if (opts & isManager
          &&  opts & isServer)  return "supervisor";
     else if (opts & isManager) return "manager";
     else if (opts & isProxy)   return "proxy";
                                return "server";
}
