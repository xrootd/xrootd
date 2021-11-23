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
#include <cctype>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <cstdlib>
#include <strings.h>
#include <cstdio>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "XrdVersion.hh"
#include "XProtocol/XProtocol.hh"

#include "XrdCks/XrdCks.hh"

#include "XrdNet/XrdNetUtils.hh"

#include "XrdSfs/XrdSfsFlags.hh"

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsConfigCP.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdOfs/XrdOfsEvs.hh"
#include "XrdOfs/XrdOfsPoscq.hh"
#include "XrdOfs/XrdOfsStats.hh"
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdOfs/XrdOfsTPCConfig.hh"
#include "XrdOfs/XrdOfsTrace.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucNSWalk.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdCms/XrdCmsClient.hh"
#include "XrdCms/XrdCmsFinder.hh"
#include "XrdCms/XrdCmsRole.hh"

#include "XrdAcc/XrdAccAuthorize.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

extern XrdOfsStats OfsStats;

extern XrdSysTrace OfsTrace;
  
class  XrdOss;
extern XrdOss     *XrdOfsOss;

class  XrdScheduler;
       XrdScheduler *ofsSchedP;

XrdVERSIONINFO(XrdOfs,XrdOfs);

namespace XrdOfsTPCParms
{
extern XrdOfsTPCConfig Cfg;
}

namespace
{
int SetMode(const char *path, mode_t mode) {return chmod(path, mode);}
}

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)   if (!strcmp(x,var)) return m(Config,Eroute);

#define TS_XPI(x,m)   if (!strcmp(x,var))\
                         return !ofsConfig->Parse(XrdOfsConfigPI:: m);

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
   char *var;
   const char *tmp;
   int   cfgFD, retc, NoGo = 0;
   XrdOucEnv myEnv;
   XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Print warm-up message
//
   Eroute.Say("++++++ File system initialization started.");

// Start off with no POSC log. Note that XrdSfsGetDefaultFileSystem nakes sure
// that we are configured only once.
//
   poscLog = NULL;

// Establish the network interface that the caller must provide
//
   if (!EnvInfo || !(myIF = (XrdNetIF *)EnvInfo->GetPtr("XrdNetIF*")))
      {Eroute.Emsg("Finder", "Network i/f undefined; unable to self-locate.");
       NoGo = 1;
      }
   ofsSchedP = (XrdScheduler *)EnvInfo->GetPtr("XrdScheduler*");

// Preset all variables with common defaults
//
   Options            = 0;
   if (getenv("XRDDEBUG")) OfsTrace.What = TRACE_MOST | TRACE_debug;

// Allocate a our plugin configurator
//
   ofsConfig = XrdOfsConfigPI::New(ConfigFN, &Config, &Eroute, 0, this);

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
           static const char *cvec[] = {"*** ofs plugin config:",0};
           Config.Capture(cvec);

           // Now start reading records until eof.
           //
           while((var = Config.GetMyFirstWord()))
                {if (!strncmp(var, "ofs.", 4)
                 ||  !strcmp(var, "all.role")
                 ||  !strcmp(var, "all.subcluster"))
                   {if (ConfigXeq(var+4,Config,Eroute)) {Config.Echo();NoGo=1;}}
                    else if (!strcmp(var, "oss.defaults")
                         ||  !strcmp(var, "all.export"))
                            {xexp(Config, Eroute, *var == 'a');
                             Config.noEcho();
                            }
                }

           // Now check if any errors occurred during file i/o
           //
           if ((retc = Config.LastError()))
           NoGo = Eroute.Emsg("Config", -retc, "read config file",
                              ConfigFN);
           Config.Close();
          }

// If no exports were specified, the default is that we are writable
//
   if (ossRW == ' ') ossRW = 'w';

// Export our role if we actually have one
//
   if (myRole) XrdOucEnv::Export("XRDROLE", myRole);

// Set the redirect option for other layers
//
   if (Options & isManager)
           XrdOucEnv::Export("XRDREDIRECT", (Options & isMeta ? "M" : "R"));
      else XrdOucEnv::Export("XRDREDIRECT", "0");

// If we are a proxy, then figure out where the prosy storge system resides
//
   if ((Options & isProxy) && !(Options & isManager))
      {char buff[2048], *bp, *libofs = getenv("XRDOFSLIB");
       if (!libofs) bp = buff;
          else {strcpy(buff, libofs); bp = buff+strlen(buff)-1;
                while(bp != buff && *(bp-1) != '/') bp--;
               }
       strcpy(bp, "libXrdPss.so");
       ofsConfig->Default(XrdOfsConfigPI::theOssLib, buff, 0);
       ofsConfig->Default(XrdOfsConfigPI::theCksLib, buff, 0);
      }

// Configure third party copy but only if we are not a manager. Phase 1 needs
// to be done before we load the plugins as they may need this info.
//
   if ((Options & ThirdPC) && !(Options & isManager))
      NoGo |= ConfigTPC(Eroute, EnvInfo);

// We need to do pre-initialization for event recording as the oss needs some
// environmental information from that initialization to initialize the frm,
// should it need to be used. We will do full evr initialization after the oss
// and the finder are initialized. A bit messy in the current plug-in world.
//
   if (!(Options & isManager) && !evrObject.Init(&Eroute)) NoGo = 1;

// Determine whether we should load authorization
//
   int piOpts = XrdOfsConfigPI::allXXXLib;
   if (!(Options & Authorize)) piOpts &= ~XrdOfsConfigPI::theAutLib;

// We need to export plugins to other protocols which means we need to
// record them in the outmost environment. So get it.
//
   XrdOucEnv *xrdEnv = 0;
   if (EnvInfo) xrdEnv = (XrdOucEnv*)EnvInfo->GetPtr("xrdEnv*");

// Now load all of the required plugins
//
   if (!ofsConfig->Load(piOpts, EnvInfo)) NoGo = 1;
      else {ofsConfig->Plugin(XrdOfsOss);
            ossFeatures = XrdOfsOss->Features();
            if (ossFeatures & XRDOSS_HASNOSF)  FeatureSet |= XrdSfs::hasNOSF;
            if (ossFeatures & XRDOSS_HASCACH)  FeatureSet |= XrdSfs::hasCACH;
            if (ossFeatures & XRDOSS_HASNAIO)  FeatureSet |= XrdSfs::hasNAIO;
            if (xrdEnv) xrdEnv->PutPtr("XrdOss*", XrdOfsOss);
            ofsConfig->Plugin(Cks);
            CksPfn = !ofsConfig->OssCks();
            CksRdr = !ofsConfig->LclCks();
            if (ofsConfig->Plugin(prepHandler))
               {prepAuth = ofsConfig->PrepAuth();
                FeatureSet |= XrdSfs::hasPRP2;
               }
            if (Options & Authorize)
               {ofsConfig->Plugin(Authorization);
                XrdOfsTPC::Init(Authorization);
                if (xrdEnv) xrdEnv->PutPtr("XrdAccAuthorize*",Authorization);
                FeatureSet |= XrdSfs::hasAUTZ;
               }
           }

// Configure third party copy phase 2, but only if we are not a manager.
//
   if ((Options & ThirdPC) && !(Options & isManager)) NoGo |= ConfigTPC(Eroute);

// Extract out the export list should it have been supplied by the oss plugin
//
   ossRPList = (XrdOucPListAnchor *)EnvInfo->GetPtr("XrdOssRPList*");

// Initialize redirection.  We type te herald here to minimize confusion
//
   if (Options & haveRole)
      {Eroute.Say("++++++ Configuring ", myRole, " role. . .");
       if (ConfigRedir(Eroute, EnvInfo))
          {Eroute.Emsg("Config", "Unable to create cluster management client.");
           NoGo = 1;
          }
      }

// Initialize the FSctl plugin if we have one. Note that we needed to defer
// until now because we needed to configure the cms plugin first (see above).
//
   if (ofsConfig->Plugin(FSctl_PI) && !ofsConfig->ConfigCtl(Finder, EnvInfo))
      {Eroute.Emsg("Config", "Unable to configure FSctl plugin.");
       NoGo = 1;
      }

// Initialize th Evr object if we are an actual server
//
   if (!(Options & isManager) && !evrObject.Init(Balancer)) NoGo = 1;

// Turn off forwarding if we are not a pure remote redirector or a peer
//
   if (Options & Forwarding)
      {const char *why = 0;
       if (!(Options & Authorize)) why = "authorization not enabled";
           else if (!(Options & isPeer) && (Options & (isServer | isProxy)))
                   why = "not a pure manager";
       if (why)
         {Eroute.Say("Config warning: forwarding turned off; ", why);
          Options &= ~(Forwarding);
          fwdCHMOD.Reset(); fwdMKDIR.Reset(); fwdMKPATH.Reset();
          fwdMV.Reset();    fwdRM.Reset();    fwdRMDIR.Reset();
          fwdTRUNC.Reset();
         }
      }

// If we need to send notifications, initialize the interface
//
   if (!NoGo && evsObject) NoGo = evsObject->Start(&Eroute);

// If the OSS plugin is really a proxy. If it is, it will export its origin.
// We also suppress translating lfn to pfn (usually done via osslib +cksio).
// Note: consulting the ENVAR below is historic and remains for compatibility
// Otherwise we can configure checkpointing if we are a data server.
//
   if (ossFeatures & XRDOSS_HASPRXY || getenv("XRDXROOTD_PROXY"))
      {OssIsProxy = 1;
       CksPfn = false;
       FeatureSet |= XrdSfs::hasPRXY;
      } else if (!(Options & isManager) && !XrdOfsConfigCP::Init()) NoGo = 1;

// Indicate wheter oss implements pgrw or it has to be simulated
//
   OssHasPGrw = (ossFeatures & XRDOSS_HASPGRW) != 0;

// If POSC processing is enabled (as by default) do it. Warning! This must be
// the last item in the configuration list as we need a working filesystem.
// Note that in proxy mode we always disable posc!
//
   if (OssIsProxy || getenv("XRDXROOTD_NOPOSC"))
      {if (poscAuto != -1 && !NoGo)
          Eroute.Say("Config POSC has been disabled by the osslib plugin.");
      } else if (poscAuto != -1 && !NoGo) NoGo |= ConfigPosc(Eroute);

// Setup statistical monitoring
//
   OfsStats.setRole(myRole);

// Display final configuration
//
   if (!NoGo) Config_Display(Eroute);
   delete ofsConfig; ofsConfig = 0;

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
                                  "       all.role %s\n"
                                  "%s"
                                  "       ofs.maxdelay   %d\n"
                                  "       ofs.persist    %s hold %d%s%s\n"
                                  "       ofs.trace      %x",
              cloc, myRole,
              (Options & Authorize ? "       ofs.authorize\n" : ""),
               MaxDelay,
               pval, poscHold, (poscLog ? " logdir " : ""),
               (poscLog ? poscLog    : ""), OfsTrace.What);

     Eroute.Say(buff);
     ofsConfig->Display();

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
   poscQ = new XrdOfsPoscq(&Eroute, XrdOfsOss, poscLog, int(poscSync));
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
   if (!NoGo) FeatureSet |= XrdSfs::hasPOSC;
   return NoGo;
}

/******************************************************************************/
/*                           C o n f i g R e d i r                            */
/******************************************************************************/
  
int XrdOfs::ConfigRedir(XrdSysError &Eroute, XrdOucEnv *EnvInfo)
{
   XrdCmsClient_t CmsPI;
   XrdSysLogger *myLogger = Eroute.logger();
   int isRedir = Options & isManager;
   int RMTopts = (Options & isServer ? XrdCms::IsTarget : 0)
               | (Options & isProxy  ? XrdCms::IsProxy  : 0)
               | (Options & isMeta   ? XrdCms::IsMeta   : 0);
   int TRGopts = (Options & isProxy  ? XrdCms::IsProxy  : 0)
               | (isRedir ? XrdCms::IsRedir : 0) | XrdCms::IsTarget;

// Get the cms object creator plugin
//
   ofsConfig->Plugin(CmsPI);

// For manager roles, we simply do a standard config
//
   if (isRedir) 
      {     if (CmsPI)  Finder = CmsPI(myLogger, RMTopts, myPort, XrdOfsOss);
       else if (XrdCmsFinderRMT::VCheck(XrdVERSIONINFOVAR(XrdOfs)))
                        Finder = (XrdCmsClient *)new XrdCmsFinderRMT(myLogger,
                                                                RMTopts,myPort);
       else return 1;
       if (!Finder) return 1;
       if (!ofsConfig->Configure(Finder, EnvInfo))
          {delete Finder; Finder = 0; return 1;}
       if (EnvInfo) EnvInfo->PutPtr("XRDCMSMANLIST", Finder->Managers());
      }

// If we are a subcluster for another cluster then we can only be so if we
// are a pure manager. If a subcluster directive was encountered and this is
// not true we need to turn that off here. Subclusters need a target finder
// just like supervisors eventhough we are not a supervisor.
//
   if ((Options & haveRole) != isManager) Options &= ~SubCluster;

// For server roles find the port number and create the object. We used to pass
// the storage system object to the finder to allow it to process cms storage
// requests. The cms no longer sends such requests so there is no need to do
// so. And, in fact, we need to defer creating a storage system until after the
// finder is created. So, it's just as well we pass a numm pointer. At some
// point the finder should remove all storage system related code.
//
   if (Options & (isServer | SubCluster | (isPeer & ~isManager)))
      {if (!myPort)
          {Eroute.Emsg("Config", "Unable to determine server's port number.");
           return 1;
          }
            if (CmsPI)  Balancer = CmsPI(myLogger, TRGopts, myPort, XrdOfsOss);
       else if (XrdCmsFinderTRG::VCheck(XrdVERSIONINFOVAR(XrdOfs)))
                        Balancer = (XrdCmsClient *)new XrdCmsFinderTRG(myLogger,
                                                                TRGopts,myPort);
       else return 1;
       if (!Balancer) return 1;
       if (!ofsConfig->Configure(Balancer, EnvInfo))
          {delete Balancer; Balancer = 0; return 1;}
       if (Options & (isProxy | SubCluster))
          Balancer = 0; // No chatting for proxies or subclusters
      }

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                             C o n f i g T P C                              */
/******************************************************************************/
  
  
int XrdOfs::ConfigTPC(XrdSysError &Eroute, XrdOucEnv *envP)
{
   XrdOfsTPCConfig &Cfg = XrdOfsTPCParms::Cfg;

// Check if we need to configure rge credentials directory
//
   if (Cfg.fCreds)
      {char *cpath = Cfg.cPath;
       if (!(Cfg.cPath = ConfigTPCDir(Eroute, ".ofs/.tpccreds/", cpath)))
          return 1;
       free(cpath);
      }

// Construct the reproxy path. We always do this as need to solve the cart-horse
// problem of plugin loading. If we don't need it it will be ignored later.
//
   if (!(Cfg.rPath = ConfigTPCDir(Eroute, ".ofs/.tpcproxy"))) return 1;
   if (envP) envP->Put("tpc.rpdir", Cfg.rPath);

// All done
//
   return 0;
}

/******************************************************************************/
  
int XrdOfs::ConfigTPC(XrdSysError &Eroute)
{
   XrdOfsTPCConfig &Cfg = XrdOfsTPCParms::Cfg;

// If the oss plugin does not use a reproxy then remove it from the TPC config.
// Otherwise, complete it.
//
   if (ossFeatures & XRDOSS_HASRPXY && Cfg.rPath)
      {char rPBuff[1024];
       reProxy = true;
       snprintf(rPBuff,sizeof(rPBuff),"%s/%x-%%d.rpx",Cfg.rPath,int(time(0)));
       free(Cfg.rPath);
       Cfg.rPath = strdup(rPBuff);
      } else {
       if (Cfg.rPath) free(Cfg.rPath);
       Cfg.rPath = 0;
      }

// Initialize the TPC object
//
   XrdOfsTPC::Init();

// Start TPC operations
//
   return (XrdOfsTPC::Start() ? 0 : 1);
}
/******************************************************************************/
/*                          C o n f i g T P C D i r                           */
/******************************************************************************/

char *XrdOfs::ConfigTPCDir(XrdSysError &Eroute, const char *sfx,
                                                const char *xPath)
{
  
   const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
   const int BMode = S_IRWXU|        S_IRGRP|S_IXGRP; // 750
   const int nswOpt= XrdOucNSWalk::retFile | XrdOucNSWalk::retLink;
   const char *iName;
   char pBuff[MAXPATHLEN], *aPath;
   int rc;

// Construct the proper path to stored credentials
//
   iName = XrdOucUtils::InstName(-1);
   if (xPath) aPath = XrdOucUtils::genPath(xPath, iName, sfx);
      else {if (!(aPath = getenv("XRDADMINPATH")))
               {XrdOucUtils::genPath(pBuff, MAXPATHLEN, "/tmp", iName);
                aPath = pBuff;
               }
            aPath = XrdOucUtils::genPath(aPath, (char *)0, sfx);
           }

// Make sure directory path exists
//
   if ((rc = XrdOucUtils::makePath(aPath, AMode)))
      {Eroute.Emsg("Config", rc, "create TPC path", aPath);
       free(aPath);
       return 0;
      }

// Protect the last component
//
   if (SetMode(aPath, BMode))
      {Eroute.Emsg("Config", errno, "protect TPC path", aPath);
       free(aPath);
       return 0;
      }

// list the contents of teh directory
//
   XrdOucNSWalk nsWalk(&Eroute, aPath, 0, nswOpt);
   XrdOucNSWalk::NSEnt *nsX, *nsP = nsWalk.Index(rc);
   if (rc)
      {Eroute.Emsg("Config", rc, "list TPC path", aPath);
       free(aPath);
       return 0;
      }

// Remove directory contents of all files
//
   bool isBad = false;
   while((nsX = nsP))
        {nsP = nsP->Next;
         if (unlink(nsX->Path))
            {Eroute.Emsg("Config", errno, "remove TPC creds", nsX->Path);
             isBad = true;
            }
         delete nsX;
        }

// Check if all went well
//
   if (isBad) {free(aPath); return 0;}

// All done
//
   return aPath;
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
    TS_XPI("authlib",       theAutLib);
    TS_XPI("ckslib",        theCksLib);
    TS_Xeq("cksrdsz",       xcrds);
    TS_XPI("cmslib",        theCmsLib);
    TS_XPI("ctllib",        theCtlLib);
    TS_Xeq("dirlist",       xdirl);
    TS_Xeq("forward",       xforward);
    TS_Xeq("maxdelay",      xmaxd);
    TS_Xeq("notify",        xnot);
    TS_Xeq("notifymsg",     xnmsg);
    TS_XPI("osslib",        theOssLib);
    TS_Xeq("persist",       xpers);
    TS_XPI("preplib",       thePrpLib);
    TS_Xeq("role",          xrole);
    TS_Xeq("tpc",           xtpc);
    TS_Xeq("trace",         xtrace);
    TS_Xeq("xattr",         xatr);
    TS_XPI("xattrlib",      theAtrLib);

    // Process miscellaneous directives handled elsemwhere
    //
    if (!strcmp("chkpnt", var)) return (XrdOfsConfigCP::Parse(Config) ? 0 : 1);

    // Screen out the subcluster directive (we need to track that)
    //
    TS_Bit("subcluster",Options,SubCluster);

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
   ofsConfig->SetCksRdSz(static_cast<int>(rdsz));
   return 0;
}
  
/******************************************************************************/
/*                                 x d i r l                                  */
/******************************************************************************/
  
/* Function: xdirl

   Purpose:  To parse the directive: dirlist {local | remote}

             local   processes directory listings locally. The oss plugin
                     must be capable of doing this. This is the default.
             remote  if clustering is enabled, directory listings are
                     processed as directed by the cmsd.

  Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xdirl(XrdOucStream &Config, XrdSysError &Eroute)
{
   char *val;

// Get the parameter
//
   if (!(val = Config.GetWord()) || !val[0])
      {Eroute.Emsg("Config", "dirlist parameter not specified"); return 1;}

// Set appropriate option
//
        if (!strcmp(val, "local"))  DirRdr = false;
   else if (!strcmp(val, "remote")) DirRdr = true;
   else {Eroute.Emsg("Config", "Invalid dirlist parameter -", val); return 1;}

   return 0;
}
  
/******************************************************************************/
/*                                  x e x p                                   */
/******************************************************************************/
  
/* Function: xexp

   Purpose:  To prescan the all.export and oss.defaults directives to determine
             if we have any writable paths.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xexp(XrdOucStream &Config, XrdSysError &Eroute, bool isExport)
{
   static struct rwOpts {const char *opname; int isRW;} rwtab[] =
                        {{"r/o",      0}, {"readonly",    0},
                         {"forcero",  0}, {"notwritable", 0},
                         {"writable", 1}, {"r/w",         1}
                        };
   static bool defRW = true;
   int isrw = -1, numopts = sizeof(rwtab)/sizeof(struct rwOpts);
   char *val;

// If this is an export and we already know that we have a writable path, return
// Otherwise, scan over the path argument.
//
   if (isExport && (ossRW == 'w' || !(val = Config.GetWord()))) return 0;

// Throw away path and scan all the options looking for something of interest
//
   while((val = Config.GetWord()))
        {for (int i = 0; i < numopts; i++)
             if (!strcmp(val, rwtab[i].opname))  isrw = rwtab[i].isRW;
                else if (!strcmp(val, "cache")) {isrw = 0; break;}
        }

// Handle result depending if this is an export or a defaults
//
   if (isrw < 0) isrw = defRW;
   if (isExport) ossRW = (isrw ? 'w'  : 'r');
      else      {defRW = (isrw ? true : false);
                 if (ossRW == ' ' && !isrw) ossRW = 'r';
                }
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
       {Eroute.Emsg("Config", "forward option not specified"); return 1;}
    if ((is2way = !strcmp("2way", val)) || !strcmp("1way", val)
    ||  (is3way = !strcmp("3way", val)))
       if (!(val = Config.GetWord()))
          {Eroute.Emsg("Config", "forward operation not specified"); return 1;}

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
           {Eroute.Emsg("Config", "forward operation not specified"); return 1;}
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
                      Eroute.Say("Config warning: ignoring invalid forward option '",val,"'.");
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
/*                                 x p e r s                                  */
/******************************************************************************/
  
/* Function: xpers

   Purpose:  To parse the directive: persist [auto | manual | off]
                                             [hold <sec>] [logdir <dirp>]
                                             [sync <snum>]

             auto      POSC processing always on for creation requests
             manual    POSC processing must be requested (default)
             off       POSC processing is disabled
             <sec>     Seconds inclomplete files held (default 10m)
             <dirp>    Directory to hold POSC recovery log (default adminpath)
             <snum>    Number of outstanding equests before syncing to disk.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xpers(XrdOucStream &Config, XrdSysError &Eroute)
{
   char *val;
   int snum = -1, htime = -1, popt = -2;

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
         else if (!strcmp(val, "sync"))
                 {if (!(val = Config.GetWord()))
                     {Eroute.Emsg("Config","sync value not specified");
                      return 1;
                     }
                  if (XrdOuca2x::a2i(Eroute,"sync value",val,&snum,0,32767))
                      return 1;
                 }
         else Eroute.Say("Config warning: ignoring invalid persist option '",val,"'.");
         val = Config.GetWord();
        }

// Set values as needed
//
   if (htime >= 0) poscHold = htime;
   if (popt  > -2) poscAuto = popt;
   if (snum  > -1) poscSync = snum;
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

// Release storage and return if an error occurred
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
                                         [restrict <path>]
                                         [streams <num>[,<max>]]
                                         [echo] [scan {stderr | stdout}]
                                         [autorm] [pgm <path> [parms]]
                                         [fcreds  [?]<auth> =<evar>]
                                         [fcpath <path>] [oids]

                                     tpc redirect [xdlg] <host>:<port> [<cgi>]

             xdlg:  delegated | undelegated

             parms: [dn <name>] [group <grp>] [host <hn>] [vo <vo>]

             <dflt>  the default seconds a tpc authorization may be valid.
             <max>   the maximum seconds a tpc authorization may be valid.
             cksum   checksum incoming files using <type> checksum.
             logok   log successful authorizations.
             allow   only allow destinations that match the specified
                     authentication specification.
             <n>     maximum number of simultaneous transfers.
             <num>   the default number of TCP streams to use for the copy.
             <max>   The maximum number of TCP streams to use for the copy/
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
             fcreds  Forward destination credentials for protocol <auth>. The
                     request fails if thee are no credentials for <auth>. If a
                     question mark preceeds <auth> then if the client has not
                     forwarded its credentials, the server's credentials are
                     used. Otherwise, the copy fails.
             =<evar> the name of the envar to be set with the path to the
                     credentials to be forwarded.
             fcpath  where creds are stored (default <adminpath>/.ofs/.tpccreds).
             oids    Object ID's are acceptable for the source lfn.
             <host>  The redirection target host which may be localhost.
             <port>  The redirection target port.
             <cgi>   Optional cgi information.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xtpc(XrdOucStream &Config, XrdSysError &Eroute)
{
   char *val, pgm[1024];
   XrdOfsTPCConfig &Parms = XrdOfsTPCParms::Cfg;
   *pgm = 0;
   int  reqType;
   bool rdrok = true;

   while((val =  Config.GetWord()))
        {if (!strcmp(val, "redirect"))
            {if (rdrok) return xtpcr(Config, Eroute);
             Eroute.Emsg("Config", "tpc redirect must be seprately specified.");
             return 1;
            }
         rdrok = false;
         if (!strcmp(val, "allow"))
            {if (!xtpcal(Config, Eroute)) return 1;
             continue;
            }
         if (!strcmp(val, "cksum"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","cksum type not specified"); return 1;}
             if (Parms.cksType) free(Parms.cksType);
             Parms.cksType = strdup(val);
             continue;
            }
         if (!strcmp(val, "scan"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","scan type not specified"); return 1;}
                  if (strcmp(val, "stderr")) Parms.errMon = -2;
             else if (strcmp(val, "stdout")) Parms.errMon = -1;
             else if (strcmp(val, "all"   )) Parms.errMon =  0;
             else {Eroute.Emsg("Config","invalid scan type -",val); return 1;}
             continue;
            }
         if (!strcmp(val, "echo"))  {Parms.doEcho = true; continue;}
         if (!strcmp(val, "logok")) {Parms.LogOK  = true; continue;}
         if (!strcmp(val, "autorm")){Parms.autoRM = true; continue;}
         if (!strcmp(val, "oids"))  {Parms.noids  = false;continue;}
         if (!strcmp(val, "pgm"))
            {if (!Config.GetRest(pgm, sizeof(pgm)))
                {Eroute.Emsg("Config", "tpc command line too long"); return 1;}
             if (!*pgm)
                {Eroute.Emsg("Config", "tpc program not specified"); return 1;}
             if (Parms.XfrProg) free(Parms.XfrProg);
             Parms.XfrProg = strdup( pgm );
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
             if (XrdOuca2x::a2tm(Eroute,"tpc ttl default",val,&Parms.dflTTL,1))
                 return 1;
             if (!(val = Config.GetWord())) break;
             if (!(isdigit(*val))) {Config.RetToken(); continue;}
             if (XrdOuca2x::a2tm(Eroute,"tpc ttl maximum",val,&Parms.maxTTL,1))
                 return 1;
             continue;
            }
         if (!strcmp(val, "xfr"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc xfr value not specified"); return 1;}
             if (XrdOuca2x::a2i(Eroute,"tpc xfr",val,&Parms.xfrMax,1)) return 1;
             continue;
            }
         if (!strcmp(val, "streams"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc streams value not specified"); return 1;}
             char *comma = index(val,',');
             if (comma)
                {*comma++ = 0;
                 if (!(*comma))
                    {Eroute.Emsg("Config","tpc streams max value missing"); return 1;}
                 if (XrdOuca2x::a2i(Eroute,"tpc max streams",comma,&Parms.tcpSMax,0,15))
                    return 1;
                }
             if (XrdOuca2x::a2i(Eroute,"tpc streams",val,&Parms.tcpSTRM,0,15)) return 1;
             continue;
            }
         if (!strcmp(val, "fcreds"))
            {char aBuff[64];
             Parms.fCreds = true;
             if (!(val = Config.GetWord()) || (*val == '?' && *(val+1) == '\0'))
                {Eroute.Emsg("Config","tpc fcreds auth not specified"); return 1;}
             if (strlen(val) >= sizeof(aBuff))
                {Eroute.Emsg("Config","invalid fcreds auth -", val); return 1;}
             strcpy(aBuff, val);
             if (!(val = Config.GetWord()) || *val != '=' || *(val+1) == 0)
                {Eroute.Emsg("Config","tpc fcreds envar not specified"); return 1;}
             const char *emsg = XrdOfsTPC::AddAuth(aBuff,val+1);
             if (emsg) {Eroute.Emsg("Config",emsg,"-", val); return 1;}
             continue;
            }
         if (!strcmp(val, "fcpath"))
            {if (!(val = Config.GetWord()))
                {Eroute.Emsg("Config","tpc fcpath arg not specified"); return 1;}
             if (Parms.cPath) free(Parms.cPath);
             Parms.cPath = strdup(val);
             continue;
            }
         Eroute.Say("Config warning: ignoring invalid tpc option '",val,"'.");
        }

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
/*                                 x t p c r                                  */
/******************************************************************************/

int XrdOfs::xtpcr(XrdOucStream &Config, XrdSysError &Eroute)
{
   char hname[256];
   const char *cgi, *cgisep, *hBeg, *hEnd, *pBeg, *pEnd, *eText;
   char *val;
   int  n, port, dlgI;

// Get the next token
//
   if (!(val = Config.GetWord()))
      {Eroute.Emsg("Config", "tpc redirect host not specified"); return 1;}

// See if this is for delegated or undelegated (all is the default)
//
   if (!strcmp(val, "delegated")) dlgI = 0;
      else if (!strcmp(val, "undelegated")) dlgI = 1;
           else dlgI = -1;

// Get host and port
//
   if (dlgI >= 0 && !(val = Config.GetWord()))
      {Eroute.Emsg("Config", "tpc redirect host not specified"); return 1;}

// Parse this as it may be complicated.
//
   if (!XrdNetUtils::Parse(val, &hBeg, &hEnd, &pBeg, &pEnd))
      {Eroute.Emsg("Config", "Invalid tpc redirect target -", val); return 1;}

// Copy out the host target (make sure it's not too long)
//
   n = hEnd - hBeg;
   if (*val == '[') n += 2;
   if (n >= (int)sizeof(hname))
      {Eroute.Emsg("Config", "Invalid tpc redirect target -", val); return 1;}
   strncpy(hname, val, n);
   hname[n] = 0;

// Substitute our hostname for localhost if present
//
   if (!strcmp(hname, "localhost"))
      {char *myHost = XrdNetUtils::MyHostName(0, &eText);
       if (!myHost)
          {Eroute.Emsg("Config", "Unable to determine tpc localhost;",eText);
           return 1;
          }
       n = snprintf(hname, sizeof(hname), "%s", myHost);
       free(myHost);
       if (n >= (int)sizeof(hname))
          {Eroute.Emsg("Config", "Invalid tpc localhost resolution -", hname);
           return 1;
          }
      }

// Make sure a port was specified
//
   if (pBeg == hEnd)
      {Eroute.Emsg("Config", "tpc redirect port not specified"); return 1;}

// Get the numeric version of the port number
//
   if (!(port = XrdNetUtils::ServPort(pBeg, false, &eText)))
      {Eroute.Emsg("Config", "Invalid tpc redirect port;",eText); return 1;}

// Check if there is cgi that must be included
//
   if (!(cgi = Config.GetWord())) cgisep =  cgi = (char *)"";
      else cgisep = (*cgi != '?' ? "?" : "");

// Copy out the hostname to be used
//
   int k = (dlgI < 0 ? 0 : dlgI);
do{if (tpcRdrHost[k]) {free(tpcRdrHost[k]); tpcRdrHost[k] = 0;}

   n = strlen(hname) + strlen(cgisep) + strlen(cgi) + 1;
   tpcRdrHost[k] = (char *)malloc(n);
   snprintf(tpcRdrHost[k], n, "%s%s%s", hname, cgisep, cgi);
   tpcRdrPort[k] = port;
   k++;
  } while(dlgI < 0 && k < 2);

// All done
//
   Options |= RdrTPC;
   return 0;
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
       {{"aio",      TRACE_aio},
        {"all",      TRACE_ALL},
        {"chkpnt",   TRACE_chkpnt},
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
/*                                  x a t r                                   */
/******************************************************************************/

/* Function: xatr

   Purpose:  To parse the directive: xattr [maxnsz <nsz>] [maxvsz <vsz>]

                                           [uset {on|off}]

             on       enables  user settable extended attributes.

             off      disaables user settable extended attributes.

             <nsz>    maximum length of an attribute name. The user
                      specifiable limit will be 8 less.

             <vsz>    maximum length of an attribute value.

   Notes:    1. This directive is not cummalative.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xatr(XrdOucStream &Config, XrdSysError &Eroute)
{
   char *val;
   static const int xanRsv = 7;
   long long vtmp;
   int maxN = kXR_faMaxNlen, maxV = kXR_faMaxVlen;
   bool isOn = true;

   while((val =  Config.GetWord()))
        {     if (!strcmp("maxnsz", val))
                 {if (!(val = Config.GetWord()))
                     {Eroute.Emsg("Config","xattr maxnsz value not specified");
                      return 1;
                     }
                  if (XrdOuca2x::a2sz(Eroute,"maxnsz",val,&vtmp,
                                     xanRsv+1,kXR_faMaxNlen+xanRsv)) return 1;
                  maxN = static_cast<int>(vtmp);
                 }
         else if (!strcmp("maxvsz", val))
                 {if (!(val = Config.GetWord()))
                     {Eroute.Emsg("Config","xattr maxvsz value not specified");
                      return 1;
                     }
                  if (XrdOuca2x::a2sz(Eroute,"maxvsz",val,&vtmp,0,kXR_faMaxVlen))
                     return 1;
                  maxV = static_cast<int>(vtmp);
                 }
         else if (!strcmp("uset",   val))
                 {if (!(val = Config.GetWord()))
                     {Eroute.Emsg("Config","xattr uset value not specified");
                      return 1;
                     }
                       if (!strcmp("on",     val)) isOn = true;
                  else if (!strcmp("off",    val)) isOn = false;
                  else {Eroute.Emsg("Config", "invalid xattr uset value -", val);
                        return 1;
                       }
                 }
         else {Eroute.Emsg("Config", "invalid xattr option -", val);
               return 1;
              }
        }

   usxMaxNsz = (isOn ? maxN-xanRsv : 0);
   usxMaxVsz = maxV;
   return 0;
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
     else if (opts & isProxy)  {return "proxy";}
                                return "server";
}
