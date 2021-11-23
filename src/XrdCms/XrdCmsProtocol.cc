/******************************************************************************/
/*                                                                            */
/*                     X r d C m s P r o t o c o l . c c                      */
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
#include <cctype>
#include <cerrno>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <netinet/in.h>
#include <sys/param.h>

#include "XProtocol/YProtocol.hh"

#include "XrdVersion.hh"

#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"

#include "XrdCms/XrdCmsBaseFS.hh"
#include "XrdCms/XrdCmsCache.hh"
#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsConfig.hh"
#include "XrdCms/XrdCmsJob.hh"
#include "XrdCms/XrdCmsLogin.hh"
#include "XrdCms/XrdCmsManager.hh"
#include "XrdCms/XrdCmsManTree.hh"
#include "XrdCms/XrdCmsMeter.hh"
#include "XrdCms/XrdCmsProtocol.hh"
#include "XrdCms/XrdCmsRole.hh"
#include "XrdCms/XrdCmsRouting.hh"
#include "XrdCms/XrdCmsRTable.hh"
#include "XrdCms/XrdCmsState.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPup.hh"
#include "XrdOuc/XrdOucTokenizer.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdCms;
  
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

       XrdSysMutex      XrdCmsProtocol::ProtMutex;
       XrdCmsProtocol  *XrdCmsProtocol::ProtStack = 0;

       int              XrdCmsProtocol::readWait  = 1000;

       XrdCmsParser     XrdCmsProtocol::ProtArgs;

namespace XrdCms
{
extern XrdOucEnv        theEnv;
};

/******************************************************************************/
/*                       P r o t o c o l   L o a d e r                        */
/*                        X r d g e t P r o t o c o l                         */
/******************************************************************************/
  
// This protocol can live in a shared library. It can also be statically linked
// to provide a default protocol (which, for cms protocol we do). The interface
// below is used by Xrd to obtain a copy of the protocol object that can be
// used to decide whether or not a link is talking our particular protocol.
// Phase 1 initialization occurred on the call to XrdgetProtocolPort(). At this
// point a network interface is defined and we can complete initialization.
//
XrdVERSIONINFO(XrdgetProtocol,cmsd);

extern "C"
{
XrdProtocol *XrdgetProtocol(const char *pname, char *parms,
                            XrdProtocol_Config *pi)
{
// If we failed in Phase 1 initialization, immediately fail Phase 2.
//
   if (Config.doWait < 0) return (XrdProtocol *)0;

// Initialize the network interface and get the actual port number assigned
//
   Config.PortTCP = pi->NetTCP->Port();
   Config.NetTCP  = pi->NetTCP;

// If we have a connection allow list, add it to the network object. Note that
// we clear the address because the object is lost in the add process.
//
   if (Config.Police) {pi->NetTCP->Secure(Config.Police); Config.Police = 0;}

// Complete initialization and upon success return a protocol object
//
   if (Config.Configure2()) return (XrdProtocol *)0;

// Return a new instance of this object
//
   return (XrdProtocol *)new XrdCmsProtocol();
}
}

/******************************************************************************/
/*           P r o t o c o l   P o r t   D e t e r m i n a t i o n            */
/*                    X r d g e t P r o t o c o l P o r t                     */
/******************************************************************************/
  
// Because the dcm port numbers are determined dynamically based on the role the
// dcm plays, we need to process the configration file and return the right
// port number if it differs from the one provided by the protocol driver. Only
// one port instance of the cmsd protocol is allowed.
//
XrdVERSIONINFO(XrdgetProtocolPort,cmsd);

extern "C"
{
int XrdgetProtocolPort(const char *pname, char *parms,
                       XrdProtocol_Config *pi)
{
   static int thePort = -1;
   char *cfn = pi->ConfigFN, buff[128];

// Check if we have been here before
//
   if (thePort >= 0)
      {if (pi->Port && pi->Port != thePort)
          {sprintf(buff, "%d disallowed; only using port %d",pi->Port,thePort);
           Say.Emsg("Config", "Alternate port", buff);
          }
       return thePort;
      }

// Call the level 0 configurator
//
   if (Config.Configure0(pi))
      {Config.doWait = -1; return 0;}

// The only parameter we accept is the name of an alternate config file
//
   if (parms) 
      {while(*parms == ' ') parms++;
       if (*parms) 
          {char *pp = parms;
           while(*parms != ' ' && *parms) parms++;
           cfn = pp;
          }
      }

// Put up the banner
//
   Say.Say("Copr.  2003-2020 Stanford University/SLAC cmsd.");

// Indicate failure if static init fails
//
   if (cfn) cfn = strdup(cfn);
   if (Config.Configure1(pi->argc, pi->argv, cfn))
      {Config.doWait = -1; return 0;}

// Return the port number to be used
//
   thePort = Config.PortTCP;
   return thePort;
}
}

/******************************************************************************/
/*                               E x e c u t e                                */
/******************************************************************************/
  
int XrdCmsProtocol::Execute(XrdCmsRRData &Arg)
{
   EPNAME("Execute");
   static kXR_unt32 theDelay = htonl(Config.SUPDelay);
   XrdCmsRouter::NodeMethod_t Method;
   const char *etxt;

// Check if we can continue
//
   if (CmsState.Suspended && Arg.Routing & XrdCmsRouting::Delayable)
      {Reply_Delay(Arg, theDelay); return 0;}

// Validate request code and execute the request. If successful, forward the
// request to subscribers of this node if the request is forwardable.
//
   if (!(Method = Router.getMethod(Arg.Request.rrCode)))
      Say.Emsg("Protocol", "invalid request code from", myNode->Ident);
      else if ((etxt = (myNode->*Method)(Arg)))
              if (*etxt == '!')
                 {DEBUGR(etxt+1 <<" delayed " <<Arg.waitVal <<" seconds");
                  return -EINPROGRESS;
                 } else if (*etxt == '.') return -ECONNABORTED;
                           else Reply_Error(Arg, kYR_EINVAL, etxt);
              else if (Arg.Routing & XrdCmsRouting::Forward && Cluster.NodeCnt
                   &&  !(Arg.Request.modifier & kYR_dnf)) Reissue(Arg);
   return 0;
}

/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/

XrdProtocol *XrdCmsProtocol::Match(XrdLink *lp)
{
CmsRRHdr          Hdr;
int               dlen;

// Peek at the first few bytes of data (shouldb be all zeroes)
//
   if ((dlen = lp->Peek((char *)&Hdr,sizeof(Hdr),readWait)) != sizeof(Hdr))
      {if (dlen <= 0) lp->setEtext("login not received");
       return (XrdProtocol *)0;
      }

// Verify that this is our protocol and whether a version1 client is here
//
   if (Hdr.streamid || Hdr.rrCode != kYR_login)
      {if (!strncmp((char *)&Hdr, "login ", 6))
          lp->setEtext("protocol version 1 unsupported");
       return (XrdProtocol *)0;
      }

// Return the protocol object
//
   return (XrdProtocol *)XrdCmsProtocol::Alloc();
}

/******************************************************************************/
/*                                P a n d e r                                 */
/******************************************************************************/
  
// Pander() handles all outgoing connections to a manager/supervisor

void XrdCmsProtocol::Pander(const char *manager, int mport)
{
   EPNAME("Pander");

   CmsLoginData Data, loginData;
   time_t ddmsg = time(0);
   unsigned int Mode, Role = 0;
   int myShare = Config.P_gshr << CmsLoginData::kYR_shift;
   int myTimeZ = Config.TimeZone<< CmsLoginData::kYR_shifttz;
   int Lvl=0, Netopts=0, waits=6, tries=6, fails=0, xport=mport;
   int rc, fsUtil, KickedOut, blRedir, myNID = Manager->ManTree->Register();
   int chk4Suspend = XrdCmsState::All_Suspend, TimeOut = Config.AskPing*1000;
   char manbuff[264];
   const char *Reason = 0, *manp = manager;
   const int manblen = sizeof(manbuff);
   bool terminate;

// Do some debugging
//
   DEBUG(myRole <<" services to " <<manager <<':' <<mport);

// Prefill the login data
//
   memset(&loginData, 0, sizeof(loginData));
   loginData.SID   = (kXR_char *)Config.mySID;
   loginData.Paths = (kXR_char *)Config.myPaths;
   loginData.sPort = Config.PortTCP;
   loginData.fsNum = Meter.numFS();
   loginData.tSpace= Meter.TotalSpace(loginData.mSpace);

   loginData.Version = kYR_Version; // These to keep compiler happy
   loginData.HoldTime= static_cast<int>(getpid());
   loginData.Mode    = 0;
   loginData.Size    = 0;
   loginData.ifList  = (kXR_char *)Config.ifList;
   loginData.envCGI  = (kXR_char *)Config.envCGI;

// Establish request routing based on who we are
//
        if (Config.SanList)    {Routing= &supVOps;
                                Role   = CmsLoginData::kYR_subman;
                               }
   else if (Config.asManager()) Routing= (Config.asServer() ? &supVOps:&manVOps);
   else                         Routing= (Config.asPeer()   ? &supVOps:&srvVOps);

// Compute the Manager's status (this never changes for managers/supervisors)
//
   if (Config.asPeer())                Role  = CmsLoginData::kYR_peer;
      else {if (Config.asManager())    Role |= CmsLoginData::kYR_manager;
            if (Config.asServer())     Role |= CmsLoginData::kYR_server;
           }
   if (Config.asProxy())               Role |= CmsLoginData::kYR_proxy;

// If we are a simple server, permanently add the nostage option if we are
// not able to stage any files.
//
   if (Role == CmsLoginData::kYR_server)
      {if (!Config.DiskSS) Role |=  CmsLoginData::kYR_nostage;}
      else chk4Suspend = XrdCmsState::FES_Suspend;

// Keep connecting to our manager. If suspended, wait for a resumption first
//
   do {if (Config.doWait && chk4Suspend)
          while(CmsState.Suspended & chk4Suspend)
               {if (!waits--)
                   {Say.Emsg("Pander", "Suspend state still active."); waits=6;}
                XrdSysTimer::Snooze(12);
               }

       if (!(rc = Manager->ManTree->Trying(myNID, Lvl)) && Lvl)
          {DEBUG("restarting at root node " <<manager <<':' <<mport);
           manp = manager; xport = mport; Lvl = 0;
          } else if (rc < 0) break;

       DEBUG("trying to connect to lvl " <<Lvl <<' ' <<manp <<':' <<xport);

       if (!(Link = Config.NetTCP->Connect(manp, xport, Netopts)))
          {if (!Netopts && XrdNetAddr::DynDNS() && (time(0) - ddmsg) >= 90)
              {Say.Emsg("Pander", "Is hostname", manp, "spelled correctly "
                                  "or just not running?");
               ddmsg = time(0);
              }
           if (tries--) Netopts = XRDNET_NOEMSG;
              else {tries = 6; Netopts = 0;}
           if ((Lvl = Manager->myMans->Next(xport,manbuff,manblen)))
                   {XrdSysTimer::Snooze(3); manp = manbuff;}
              else {if (manp != manager) fails++;
                    XrdSysTimer::Snooze(6); manp = manager; xport = mport;
                   }
           continue;
          }
       Netopts = 0; tries = waits = 6;

       // Verify that this node has the real DNS name if it's IPv6
       //
       if (!(Link->AddrInfo()->isRegistered())
       &&    Link->AddrInfo()->isIPType(XrdNetAddrInfo::IPv6))
          {char *oldName = strdup(Link->Host());
           Say.Emsg("Protocol", oldName, "is missing an IPv6 ptr record; "
                               "attempting local registration as", manp);
           if (!(Link->Register(manp)))
              {Say.Emsg("Protocol", oldName,
                        "registration failed; address mismatch.");
              } else {
               Say.Emsg("Protocol", oldName,
                        "is now locally registered as", manp);
              }
           free(oldName);
          }

       // Obtain a new node object for this connection
       //
       if (!(myNode = Manager->Add(Link, Lvl+1, terminate)))
          {Link->Close();
           if (terminate) break;
           Say.Emsg("Pander", "Unable to obtain node object.");
           XrdSysTimer::Snooze(15); continue;
          }

      // Compute current login mode
      //
      Mode = Role
           | (CmsState.Suspended ? int(CmsLoginData::kYR_suspend) : 0)
           | (CmsState.NoStaging ? int(CmsLoginData::kYR_nostage) : 0);
       if (fails >= 6 && manp == manager) 
          {fails = 0; Mode |=    CmsLoginData::kYR_trying;}

       // Login this node with the correct state
       //
       loginData.fSpace= Meter.FreeSpace(fsUtil);
       loginData.fsUtil= static_cast<kXR_unt16>(fsUtil);
       KickedOut = 0;
       if (!(loginData.dPort = CmsState.Port())) loginData.dPort = 1094;
       Data = loginData; Data.Mode = Mode | myShare | myTimeZ;
       if (!(rc = XrdCmsLogin::Login(Link, Data, TimeOut)))
          {if (!Manager->ManTree->Connect(myNID, myNode)) KickedOut = 1;
             else {XrdOucEnv cgiEnv((const char *)Data.envCGI);
                   const char *sname = cgiEnv.Get("site");
                   Say.Emsg("Protocol", "Logged into", sname, Link->Name());
                   if (Data.SID)
                      Manager->Verify(Link, (const char *)Data.SID, sname);
                   Reason = Dispatch(isUp, TimeOut, 2);
                   rc = 0;
                   loginData.fSpace= Meter.FreeSpace(fsUtil);
                   loginData.fsUtil= static_cast<kXR_unt16>(fsUtil);
                  }
          }
       // Release any storage left over from the login
       //
       if (Data.SID)    {free(Data.SID);    Data.SID    = 0;}
       if (Data.envCGI) {free(Data.envCGI); Data.envCGI = 0;}

       // Remove manager from the config
       //
       Manager->Remove(myNode, (rc == kYR_redirect ? "redirected"
                                  : (Reason ? Reason : "lost connection")));
       Manager->ManTree->Disc(myNID);
       Link->Close();

       // The Sync() will wait until all the threads we started complete. Then
       // ask the manager to delete the node as it must synchronize with other
       // threads relative to the manager object before being destroyed.
       //
       Sync(); Manager->Delete(myNode); myNode = 0; Reason = 0;

       // Check if we should process the redirection
       //
       if (rc == kYR_redirect)
          {if (!(blRedir = Data.Mode & CmsLoginData::kYR_blredir))
              Manager->myMans->Add(Link->NetAddr(), (char *)Data.Paths,
                                   Config.PortTCP,  Lvl+1);
              else Manager->Rerun((char *)Data.Paths);
           free(Data.Paths);
           if (blRedir) break;
          }

       // Cycle on to the next manager if we have one or snooze and try over
       //
       if (!KickedOut && (Lvl = Manager->myMans->Next(xport,manbuff,manblen)))
          {manp = manbuff; continue;}
       XrdSysTimer::Snooze((rc < 0 ? 60 : 9)); Lvl = 0;
       if (manp != manager) fails++;
       manp = manager; xport = mport;
      } while(1);

// This must have been a permanent redirect. Tell the manager we are done.
//
   Manager->Finished(manager, mport);

// Recycle the protocol object
//
   Recycle(0, 0, 0);
}
  
/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/
  
// Process is called only when we get a new connection. We only return when
// the connection drops. At that point we immediately mark he node as offline
// to prohibit its selection in the future (it may have already been selected).
// Unfortunately, we need the global selection lock to do that.
//
int XrdCmsProtocol::Process(XrdLink *lp)
{
   const char *Reason;
   Bearing     myWay;
   int         tOut;

// Now admit the login
//
   Link = lp;
   if ((Routing=Admit()))
      {loggedIn = 1;
       if (RSlot) {myWay = isLateral; tOut = -1;}
          else    {myWay = isDown;    tOut = Config.AskPing*1000;}
       myNode->UnLock();
       if ((Reason = Dispatch(myWay, tOut, 2))) lp->setEtext(Reason);
       Cluster.SLock(true); myNode->isOffline = 1; Cluster.SLock(false);
      }

// Serialize all activity on the link before we proceed. This makes sure that
// there are no outstanding tasks initiated by this node. We don't need a node
// lock for this because we are no longer reading requests so no new tasks can
// be started. Since the node is marked bound, any attempt to reconnect will be
// rejected until we finish removing this node. We get the node lock afterwards.
//
   lp->Serialize();
   if (!myNode) return -1;
   Sync();
   myNode->Lock();

// Immediately terminate redirectors (they have an Rslot). The redirector node
// can be directly deleted as all references were serialized through the
// RTable and one we remove our node there can be no references left.
//
   if (RSlot)
      {RTable.Del(myNode); RSlot  = 0;
       myNode->UnLock(); delete myNode; myNode = 0;
       return -1;
      }

// We have a node that may or may not be in the cluster at this point, or may
// need to remain in the cluster as a shadow member. In any case, the node
// object lock will be released by Remove().
//
   if (myNode)
      {myNode->isConn = 0;
       if (myNode->isBound) Cluster.Remove(0, myNode, !loggedIn);
          else if (myNode->isGone) Cluster.Remove(myNode);
              else myNode->UnLock();
      }

// All done indicate the connection is dead
//
   return -1;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdCmsProtocol::Recycle(XrdLink *lp, int consec, const char *reason)
{
   bool isLoggedIn = loggedIn != 0;

   ProtMutex.Lock();
   ProtLink  = ProtStack;
   ProtStack = this;
   ProtMutex.UnLock();

   if (!lp) return;

   if (isLoggedIn)
      if (reason) Say.Emsg("Protocol", lp->ID, "logged out;",   reason);
         else     Say.Emsg("Protocol", lp->ID, "logged out.");
      else     
      if (reason) Say.Emsg("Protocol", lp->ID, "login failed;", reason);
}
  
/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdCmsProtocol::Stats(char *buff, int blen, int do_sync)
{

// All the statistics are handled by the cluster
//

// If we are a manager then we have different information
//
   return (Config.asManager() ? Cluster.Statt(buff, blen)
                              : Cluster.Stats(buff, blen));
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 A d m i t                                  */
/******************************************************************************/

namespace
{
char *getAltName(char *sid, char *buff, int blen)
{
   char *atsign, *spacec, *retval = 0;
   int  n;
   if (sid)
   if ((atsign = index(sid, '@')))
      {atsign++;
       if ((spacec = index(atsign, ' ')))
          {*spacec = 0;
           n = strlen(atsign);
           if (n > 3 && n < blen)
              {strcpy(buff, atsign);
               retval = buff;
              }
           *spacec = ' ';
          }
      }
   return retval;
}
}

XrdCmsRouting *XrdCmsProtocol::Admit()
{
   EPNAME("Admit");
   char         *envP = 0, envBuff[256], myBuff[4096];
   XrdCmsLogin  Source(myBuff, sizeof(myBuff));
   CmsLoginData Data;
   XrdCmsRole::RoleID roleID = XrdCmsRole::noRole;
   const char  *Reason;
   SMask_t      newmask, servset(0);
   int addedp = 0, Status = 0, isPeer = 0, isProxy = 0;
   int isMan, isServ, isSubm, wasSuspended = 0, Share = 100, tZone = 0;

// Construct environment data
//
   if (Config.mySite)
      {snprintf(envBuff, sizeof(envBuff), "site=%s", Config.mySite);
       envP = envBuff;
      }

// Establish outgoing mode
//
   Data.Mode = 0;
   if (Trace.What & TRACE_Debug) Data.Mode |= CmsLoginData::kYR_debug;
   if (CmsState.Suspended)      {Data.Mode |= CmsLoginData::kYR_suspend;
                                 wasSuspended = 1;
                                }
   Data.HoldTime = Config.LUPHold;

// Do the login and get the data
//
   if (!Source.Admit(Link, Data, Config.mySID, envP)) return 0;

// Construct environment for incoming node
//
   XrdOucEnv cgiEnv((const char *)Data.envCGI);

// We have this problem hat many times the IPv6 address is missing the ptr
// record in DNS. If this node is IPv6 unregistered and the incoming node
// supplied it's host name then we can attempt to register it locally.
//
   if (!(Link->AddrInfo()->isRegistered())
   &&    Link->AddrInfo()->isIPType(XrdNetAddrInfo::IPv6))
      {const char *altName = cgiEnv.Get("myHN");
       const char *altType = "stated mapping";
       char hBF[256], *oldName = strdup(Link->Host());
       if (!altName) {altName = getAltName((char *)Data.SID, hBF, sizeof(hBF));
                      altType = "inferred mapping";
                     }
       Say.Emsg("Protocol", "DNS lookup for", oldName, "failed; "
                            "IPv6 ptr record missing!");
       if (!altName)
          {Say.Emsg("Protocol", oldName, "did not supply a fallback "
                                         "mapping; using IPv6 address.");
          } else {
           char buff[512];
           snprintf(buff, sizeof(buff), "%s -> %s", oldName, altName);
           Say.Emsg("Protocol", "Attempting to use", altType, buff);
           if (!(Link->Register(altName)))
              {Say.Emsg("Protocol", buff, altType,"failed; address mismatch.");
              } else {
               Say.Emsg("Protocol", oldName,
                        "is now locally registered as", altName);
              }
          }
       free(oldName);
      }

// Handle Redirectors here (minimal stuff to do)
//
   if (Data.Mode & CmsLoginData::kYR_director) 
      {Link->setID("redirector", Data.HoldTime);
       return Admit_Redirector(wasSuspended);
      }

// Disallow subscriptions we are are configured as a solo manager
//
   if (Config.asSolo())
      return Login_Failed("configuration disallows subscribers");

// Setup for role tests
//
   isMan  = Data.Mode & CmsLoginData::kYR_manager;
   isServ = Data.Mode & CmsLoginData::kYR_server;
   isSubm = Data.Mode & CmsLoginData::kYR_subman;

// Determine the role of this incoming login.
//
        if (isMan)
           {Status = (isServ ? CMS_isSuper|CMS_isMan : CMS_isMan);
                 if ((isPeer =  Data.Mode & CmsLoginData::kYR_peer))
                    {Status |= CMS_isPeer;  roleID = XrdCmsRole::PeerManager;}
            else if (Data.Mode & CmsLoginData::kYR_proxy)
                                            roleID = XrdCmsRole::ProxyManager;
            else if (Config.asMetaMan() || isSubm)
                                            roleID = XrdCmsRole::Manager;
            else                           {roleID = XrdCmsRole::Supervisor;
                                            Status|= CMS_isSuper;
                                           }
           }
   else if ((isServ =  Data.Mode & CmsLoginData::kYR_server))
           {if ((isProxy=  Data.Mode & CmsLoginData::kYR_proxy))
               {Status = CMS_isProxy;       roleID = XrdCmsRole::ProxyServer;}
               else                         roleID = XrdCmsRole::Server;
           }
   else if ((isPeer =  Data.Mode & CmsLoginData::kYR_peer))
           {Status |= CMS_isPeer;           roleID = XrdCmsRole::Peer;}
   else    return Login_Failed("invalid login role");

// Set the link identification
//
   myRole = XrdCmsRole::Name(roleID);
   Link->setID(myRole, Data.HoldTime);

// Make sure that our role is compatible with the incoming role
//
   Reason = 0;
        if (Config.asProxy()) {if (!isProxy || isPeer)
                                  Reason = "configuration only allows proxies";
                              }
   else if (isProxy)              Reason = "configuration disallows proxies";
   else if (Config.asServer() && isPeer)
                                  Reason = "configuration disallows peers";
   if (Reason) return Login_Failed(Reason);

// The server may specify nostage and suspend
//
   if (Data.Mode & CmsLoginData::kYR_nostage) Status |= CMS_noStage;
   if (Data.Mode & CmsLoginData::kYR_suspend) Status |= CMS_Suspend;

// The server may specify that it has been trying for a long time
//
   if (Data.Mode & CmsLoginData::kYR_trying)
      Say.Emsg("Protocol",Link->Name(),"has not yet found a cluster slot!");

// Add the node. The resulting node object will be locked and the caller will
// unlock it prior to dispatching.
//
   if (!(myNode = Cluster.Add(Link, Data.dPort, Status, Data.sPort,
                  (const char *)Data.SID, (const char *)Data.ifList)))
      return (XrdCmsRouting *)0;
   myNode->RoleID = static_cast<char>(roleID);
   myNode->setVersion(Data.Version);

// Calculate the share as the reference mininum if we are a meta-manager
//
   if (Config.asMetaMan())
      {Share  = (Data.Mode & CmsLoginData::kYR_share)>>CmsLoginData::kYR_shift;
       if (Share <= 0 || Share > 100) Share = Config.P_gsdf;
       if (Share > 0) myNode->setShare(Share);
      }

// Set the node's timezone
//
   tZone = (Data.Mode & CmsLoginData::kYR_tzone)>>CmsLoginData::kYR_shifttz;
   tZone = myNode->setTZone(tZone);

// Record the status of the server's filesystem
//
   DEBUG(Link->Name() <<" TSpace="  <<Data.tSpace <<"GB NumFS=" <<Data.fsNum
                      <<" FSpace="  <<Data.fSpace <<"MB MinFR=" <<Data.mSpace
                      <<" MB Util=" <<Data.fsUtil <<" Share="   <<Share
                      <<" TZone="   <<tZone);
   myNode->DiskTotal = Data.tSpace;
   myNode->DiskMinF  = Data.mSpace;
   myNode->DiskFree  = Data.fSpace;
   myNode->DiskNums  = Data.fsNum;
   myNode->DiskUtil  = Data.fsUtil;
   Meter.setVirtUpdt();

// Check for any configuration changes and then process all of the paths.
//
   if (Data.Paths && *Data.Paths)
      {XrdOucTokenizer thePaths((char *)Data.Paths);
       char *tp, *pp;
       ConfigCheck(Data.Paths);
       while((tp = thePaths.GetLine()))
            {DEBUG(Link->Name() <<" adding path: " <<tp);
             if (!(tp = thePaths.GetToken())
             ||  !(pp = thePaths.GetToken())) break;
             if (!(newmask = AddPath(myNode, tp, pp)))
                return Login_Failed("invalid exported path");
             servset |= newmask;
             addedp= 1;
            }
      }

// Check if we have any special paths. If none, then add the default path.
//
   if (!addedp) 
      {XrdCmsPInfo pinfo;
       ConfigCheck(0);
       pinfo.rovec = myNode->Mask();
       if (myNode->isPeer) pinfo.ssvec = myNode->Mask();
       servset = Cache.Paths.Insert("/", &pinfo);
       Say.Emsg("Protocol", myNode->Ident, "defaulted r /");
      }

// Set the reference counts for intersecting nodes to be the same.
// Additionally, indicate cache refresh will be needed because we have a new
// node that may have files the we already reported on. Note that setting
// isBad may be subject to a concurrency race, but that is still OK here.
//
   Cluster.ResetRef(servset);
   if (Config.asManager()) {Manager->Reset(); myNode->SyncSpace();}
   myNode->isBad &= ~XrdCmsNode::isDisabled;

// At this point we can switch to nonblocking sendq for this node
//
   if (Config.nbSQ && (Config.nbSQ > 1 || !myNode->inDomain()))
      isNBSQ = Link->setNB();

// Document the login
//
   const char *sname = cgiEnv.Get("site");
   const char *lfmt  = (myNode->isMan > 1 ? "Standby%s%s" : "Primary%s%s");
   snprintf(envBuff,sizeof(envBuff),lfmt,(sname ? " ":""),(sname ? sname : ""));
   Say.Emsg("Protocol", envBuff, myNode->Ident,
            (myNode->isBad & XrdCmsNode::isSuspend ? "logged in suspended."
                                                   : "logged in."));
   if (Data.SID)
      Say.Emsg("Protocol", myNode->Ident, "system ID:", (const char *)Data.SID);
   myNode->ShowIF();

// All done
//
   return &rspVOps;
}
  
/******************************************************************************/
/*                      A d m i t _ R e d i r e c t o r                       */
/******************************************************************************/
  
XrdCmsRouting *XrdCmsProtocol::Admit_Redirector(int wasSuspended)
{
   EPNAME("Admit_Redirector");
   static CmsStatusRequest newState 
                   = {{0, kYR_status, CmsStatusRequest::kYR_Resume, 0}};

// Indicate what role I have
//
   myRole = "redirector";

// Director logins have no additional parameters. We return with the node object
// locked to be consistent with the way server/suprvisors nodes are returned.
//
   myNode = new XrdCmsNode(Link); myNode->Lock();
   if (!(RSlot = RTable.Add(myNode)))
      {myNode->UnLock();
       delete myNode;
       myNode = 0;
       Say.Emsg("Protocol",myNode->Ident,"login failed; too many redirectors.");
       return 0;
      } else myNode->setSlot(RSlot);

// If we told the redirector we were suspended then we must check if that is no
// longer true and generate a reume event as the redirector may have missed it
//
   if (wasSuspended && !CmsState.Suspended)
      myNode->Send((char *)&newState, sizeof(newState));

// Login succeeded
//
   Say.Emsg("Protocol", myNode->Ident, "logged in.");
   DEBUG(myNode->Ident <<" assigned slot " <<RSlot);
   return &rdrVOps;
}

/******************************************************************************/
/*                               A d d P a t h                                */
/******************************************************************************/
  
SMask_t XrdCmsProtocol::AddPath(XrdCmsNode *nP,
                                const char *pType, const char *Path)
{
    XrdCmsPInfo pinfo;

// Process: addpath {r | w | rw}[s] path
//
   while(*pType)
        {     if ('r' == *pType) pinfo.rovec =               nP->Mask();
         else if ('w' == *pType) pinfo.rovec = pinfo.rwvec = nP->Mask();
         else if ('s' == *pType) pinfo.rovec = pinfo.ssvec = nP->Mask();
         else return 0;
         pType++;
        }

// Set node options
//
   nP->isRW = (pinfo.rwvec ? XrdCmsNode::allowsRW : 0) 
            | (pinfo.ssvec ? XrdCmsNode::allowsSS : 0);

// Add the path to the known path list
//
   return Cache.Paths.Insert(Path, &pinfo);
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdCmsProtocol *XrdCmsProtocol::Alloc(const char *theRole, XrdCmsManager *uMan,
                                      const char *theMan,
                                            int   thePort)
{
   XrdCmsProtocol *xp;

// Grab a protocol object and, if none, return a new one
//
   ProtMutex.Lock();
   if ((xp = ProtStack)) ProtStack = xp->ProtLink;
      else xp = new XrdCmsProtocol();
   ProtMutex.UnLock();

// Initialize the object if we actually got one
//
   if (!xp) Say.Emsg("Protocol","No more protocol objects.");
      else xp->Init(theRole, uMan, theMan, thePort);

// All done
//
   return xp;
}

/******************************************************************************/
/*                           C o n f i g C h e c k                            */
/******************************************************************************/
  
void XrdCmsProtocol::ConfigCheck(unsigned char *theConfig)
{
  unsigned int ConfigID;
  int tmp;

// Compute the new configuration ID
//
   if (!theConfig) ConfigID = 1;
      else ConfigID = XrdOucCRC::CRC32(theConfig, strlen((char *)theConfig));

// If the configuration chaged or a new node, then we must bounce this node
//
   if (ConfigID != myNode->ConfigID)
      {if (myNode->ConfigID) Say.Emsg("Protocol",Link->Name(),"reconfigured.");
       Cache.Paths.Remove(myNode->Mask());
       Cache.Bounce(myNode->Mask(), myNode->ID(tmp));
       myNode->ConfigID = ConfigID;
      }
}

/******************************************************************************/
/*                              D i s p a t c h                               */
/******************************************************************************/

// Dispatch is provided with three key pieces of information:
// 1) The connection bearing (isUp, isDown, isLateral) the determines how
//    timeouts are to be handled.
// 2) The maximum amount to wait for data to arrive.
// 3) The number of successive timeouts we can have before we give up.
  
const char *XrdCmsProtocol::Dispatch(Bearing cDir, int maxWait, int maxTries)
{
   EPNAME("Dispatch");
   static const int ReqSize = sizeof(CmsRRHdr);
   XrdCmsRRData *Data = XrdCmsRRData::Objectify();
   XrdCmsJob  *jp;
   const char *toRC = (cDir == isUp ? "manager not active"
                                    : "server not responding");
   const char *myArgs, *myArgt;
   char        buff[8];
   int         rc, toLeft = maxTries, lastPing = Config.PingTick;

// Dispatch runs with the current thread bound to the link.
//
// Link->Bind(XrdSysThread::ID());

// Read in the request header
//
do{if ((rc = Link->RecvAll((char *)&Data->Request, ReqSize, maxWait)) < 0)
      {if (rc != -ETIMEDOUT) return (myNode->isBad & XrdCmsNode::isBlisted ?
                                     "blacklisted" : "request read failed");
       if (!toLeft--) return toRC;
       if (cDir == isDown)
          {if (myNode->isBad & XrdCmsNode::isDoomed)
              return "server blacklisted w/ redirect";
           if (!SendPing()) return "server unreachable";
           lastPing = Config.PingTick;
          }
       continue;
      }

// Check if we need to ping as non-response activity may cause ping misses
//
   if (cDir == isDown && lastPing != Config.PingTick)
      {if (myNode->isBad & XrdCmsNode::isDoomed)
          return "server blacklisted w/ redirect";
       if (!SendPing()) return "server unreachable";
       lastPing = Config.PingTick;
      }

// Decode the length and get the rest of the data
//
   toLeft = maxTries;
   Data->Dlen = static_cast<int>(ntohs(Data->Request.datalen));
   if ((QTRACE(Debug))
   && Data->Request.rrCode != kYR_ping && Data->Request.rrCode != kYR_pong)
      DEBUG(myNode->Ident <<" for " <<Router.getName(Data->Request.rrCode)
                          <<" dlen=" <<Data->Dlen);
   if (!(Data->Dlen)) {myArgs = myArgt = 0;}
      else {if (Data->Dlen > maxReqSize)
               {Say.Emsg("Protocol","Request args too long from",Link->Name());
                return "protocol error";
               }
            if ((!Data->Buff || Data->Blen < Data->Dlen)
            &&  !Data->getBuff(Data->Dlen))
               {Say.Emsg("Protocol", "No buffers to serve", Link->Name());
                return "insufficient buffers";
               }
            if ((rc = Link->RecvAll(Data->Buff, Data->Dlen, maxWait)) < 0)
               return (rc == -ETIMEDOUT ? "read timed out" : "read failed");
            myArgs = Data->Buff; myArgt = Data->Buff + Data->Dlen;
           }

// Check if request is actually valid
//
   if (!(Data->Routing = Routing->getRoute(int(Data->Request.rrCode))))
      {sprintf(buff, "%d", Data->Request.rrCode);
       Say.Emsg("Protocol",Link->Name(),"sent an invalid request -", buff);
       continue;
      }

// Parse the arguments (we do this in the main thread to avoid overruns)
//
   if (!(Data->Routing & XrdCmsRouting::noArgs))
      {if (Data->Request.modifier & kYR_raw)
          {Data->Path = Data->Buff; Data->PathLen = Data->Dlen;}
          else if (!myArgs
               || !ProtArgs.Parse(int(Data->Request.rrCode),myArgs,myArgt,Data))
                  {Reply_Error(*Data, kYR_EINVAL, "badly formed request");
                   continue;
                  }
      }

// Insert correct identification
//
   if (!(Data->Ident) || !(*Data->Ident)) Data->Ident = myNode->Ident;

// Schedule this request if async. Otherwise, do this inline. Note that
// synchrnous requests are allowed to return status changes (e.g., redirect)
//
   if (Data->Routing & XrdCmsRouting::isSync)
      {if ((rc = Execute(*Data)) && rc == -ECONNABORTED) return "disconnected";}
      else if ((jp = XrdCmsJob::Alloc(this, Data)))
              {Ref(1);
               Sched->Schedule((XrdJob *)jp);
               Data = XrdCmsRRData::Objectify();
              }
              else Say.Emsg("Protocol", "No jobs to serve", Link->Name());
  } while(1);

// We should never get here
//
   return "logic error";
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
// Determine how we should proceed here
//
void XrdCmsProtocol::DoIt()
{

// If we have a role, then we should simply pander it
//
   if (myRole) Pander(myMan, myManPort);
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/

void XrdCmsProtocol::Init(const char *iRole, XrdCmsManager *uMan,
                          const char *iMan,  int iPort)
{
   myRole    = iRole;
   myMan     = iMan;
   myManPort = iPort;
   Manager   = uMan;
   myNode    = 0;
   loggedIn  = 0;
   RSlot     = 0;
   ProtLink  = 0;
   refCount  = 0;
   refWait   = 0;
   isNBSQ    = false;
}
  
/******************************************************************************/
/*                          L o g i n _ F a i l e d                           */
/******************************************************************************/
  
XrdCmsRouting *XrdCmsProtocol::Login_Failed(const char *reason)
{
   Link->setEtext(reason);
   return (XrdCmsRouting *)0;
}

/******************************************************************************/
/* Private:                          R e f                                    */
/******************************************************************************/
  
void XrdCmsProtocol::Ref(int rcnt)
{
// Update the reference counter
//
   refMutex.Lock();
   refCount += rcnt;

// Check if someone is waiting for the count to drop to zero
//
   if (refWait && refCount <= 0) {refWait->Post(); refWait = 0;}

// All done
//
   refMutex.UnLock();
}

/******************************************************************************/
/*                               R e i s s u e                                */
/******************************************************************************/

void XrdCmsProtocol::Reissue(XrdCmsRRData &Data)
{
   EPNAME("Resisue");
   XrdCmsPInfo pinfo;
   SMask_t amask;
   struct iovec ioB[2] = {{(char *)&Data.Request, sizeof(Data.Request)},
                          {         Data.Buff,    (size_t)Data.Dlen}
                         };

// Check if we can really reissue the command
//
   if (!((Data.Request.modifier += kYR_hopincr) & kYR_hopcount))
      {Say.Emsg("Job", Router.getName(Data.Request.rrCode),
                       "msg TTL exceeded for", Data.Path);
       return;
      }

// We do not support 2way re-issued messages
//
   Data.Request.streamid = 0;
  
// Find all the nodes that might be able to do somthing on this path
//
   if (!Cache.Paths.Find(Data.Path, pinfo)
   || (amask = pinfo.rwvec | pinfo.rovec) == 0)
      {Say.Emsg(epname, Router.getName(Data.Request.rrCode),
                       "aborted; no servers handling", Data.Path);
       return;
      }

// While destructive operations should only go to r/w servers
//
   if (Data.Request.rrCode != kYR_prepdel)
      {if (!(amask = pinfo.rwvec))
          {Say.Emsg(epname, Router.getName(Data.Request.rrCode),
                    "aborted; no r/w servers handling", Data.Path);
           return;
          }
      }

// Do some debugging
//
   DEBUG("FWD " <<Router.getName(Data.Request.rrCode) <<' ' <<Data.Path);

// Check for selective sending since DFS setups need only one notification.
// Otherwise, send this message to all nodes.
//
   if (baseFS.isDFS() && Data.Request.rrCode != kYR_prepdel)
      {Cluster.Broadsend(amask, Data.Request, Data.Buff, Data.Dlen);
      } else {
       Cluster.Broadcast(amask, ioB, 2, sizeof(Data.Request)+Data.Dlen);
      }
}
  
/******************************************************************************/
/*                           R e p l y _ D e l a y                            */
/******************************************************************************/
  
void XrdCmsProtocol::Reply_Delay(XrdCmsRRData &Data, kXR_unt32 theDelay)
{
     EPNAME("Reply_Delay");
     const char *act;

     if (Data.Request.streamid && (Data.Routing & XrdCmsRouting::Repliable))
        {CmsResponse Resp = {{Data.Request.streamid, kYR_wait, 0,
                               htons(sizeof(kXR_unt32))}, theDelay};
         act = " sent";
         Link->Send((char *)&Resp, sizeof(Resp));
        } else act = " skip";

     DEBUG(myNode->Ident <<act <<" delay " <<ntohl(theDelay));
}

/******************************************************************************/
/*                           R e p l y _ E r r o r                            */
/******************************************************************************/
  
void XrdCmsProtocol::Reply_Error(XrdCmsRRData &Data, int ecode, const char *etext)
{
     EPNAME("Reply_Error");
     const char *act;
     int n = strlen(etext)+1;

     if (Data.Request.streamid && (Data.Routing & XrdCmsRouting::Repliable))
        {CmsResponse Resp = {{Data.Request.streamid, kYR_error, 0,
                              htons((unsigned short int)(sizeof(kXR_unt32)+n))},
                             htonl(static_cast<unsigned int>(ecode))};
         struct iovec ioV[2] = {{(char *)&Resp, sizeof(Resp)},
                                {(char *)etext, (size_t)n}};
         act = " sent";
         Link->Send(ioV, 2);
        } else act = " skip";

     DEBUG(myNode->Ident <<act <<" err " <<ecode  <<' ' <<etext);
}

/******************************************************************************/
/* Private:                     S e n d P i n g                               */
/******************************************************************************/
  
bool XrdCmsProtocol::SendPing()
{
   static CmsPingRequest Ping = {{0, kYR_ping,  0, 0}};

// We would like not send ping requests to servers that are backlogged but if
// we don't (currently) it will look to one of the parties that the other party
// if not functional. We should try to fix this, sigh.
//
// if (isNBSQ && Link->Backlog()) return true;

// Send the ping
//
   if (Link->Send((char *)&Ping, sizeof(Ping)) < 0) return false;
   return true;
}
  
/******************************************************************************/
/* Private:                         S y n c                                   */
/******************************************************************************/
  
void XrdCmsProtocol::Sync()
{
   EPNAME("Sync");
   XrdSysSemaphore mySem(0);

// Make sure that all threads that we started have completed
//
   refMutex.Lock();
   if (refCount <= 0) refMutex.UnLock();
      else {refWait = &mySem;
            DEBUG("Waiting for " <<refCount <<' ' <<myNode->Ident
                  <<" thread(s) to end.");
            refMutex.UnLock();
            mySem.Wait();
           }
}
