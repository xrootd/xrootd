/******************************************************************************/
/*                                                                            */
/*                     X r d N e t P M a r k C f g . h h                      */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <map>
#include <set>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "XrdNet/XrdNetMsg.hh"
#include "XrdNet/XrdNetPMarkCfg.hh"
#include "XrdNet/XrdNetPMarkFF.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucMapP2X.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysTrace.hh"
  
/******************************************************************************/
/*                          L o c a l   M a c r o s                           */
/******************************************************************************/

#define TRACE(txt) \
        if (doTrace) SYSTRACE(Trace->, client.tident, epName, 0, txt)

#define DEBUG(txt) \
        if (doDebug) SYSTRACE(Trace->, 0, epName, 0, txt)

#define DBGID(tid, txt) \
        if (doDebug) SYSTRACE(Trace->, tid, epName, 0, txt)
  
#define EPName(ep) const char *epName = ep

/******************************************************************************/
/*                        s t a t i c   O b j e c t s                         */
/******************************************************************************/

namespace XrdNetPMarkConfig
{
class MapInfo
     {public:
      std::string Name;  // Activity name
      int         Code;  // Act code for role/user

      MapInfo() : Code(0) {}
      MapInfo(const char *name, int code) : Name(name), Code(code) {}
     ~MapInfo() {}
};

class ExpInfo
     {public:
      std::map<std::string, int>     actMap;
      std::map<std::string, MapInfo> r2aMap;
      std::map<std::string, MapInfo> u2aMap;
      short Code;
      short dAct = -1;
      bool Roles = false;
      bool Users = false;
      bool inUse = false;

      ExpInfo(int code=0) : Code(code) {}
     ~ExpInfo() {}
     };
  
// Permanent maps to determine the experiment
//
std::map<std::string, ExpInfo>  expMap;
XrdOucMapP2X<ExpInfo*>          p2eMap;
std::map<std::string, ExpInfo*> v2eMap;

// Other configuration values
//
XrdSysError  *eDest  = 0;
XrdNetMsg    *netMsg = 0;  // UDP object for collector
XrdNetMsg    *netOrg = 0;  // UDP object for origin
XrdScheduler *Sched  = 0;
XrdSysTrace  *Trace  = 0;
const char   *myHostName = "-";
const char   *myDomain   = "";


ExpInfo      *expDflt = 0;

char         *ffDest  = 0;
int           ffEcho  = 0;
static
const  int    ffPORT  = 10514;  // The default port
int           ffPortD = 0;      // The dest  port to use
int           ffPortO = 0;      // The reply port to use

static const int domAny = 0;
static const int domLcl = 1;
static const int domRmt = 2;

char          chkDom  = domRmt;
bool          tryPath = false;
bool          tryVO   = false;
bool          useDefs = false;

bool          useFLbl = false;
signed char   useFFly = -1;
bool          addFLFF = false;
bool          useSTag = true;

bool          noFail  = true;
bool          doDebug = false;
bool          doTrace = false;

struct CfgInfo
      {XrdOucString           defsFile;
       XrdOucString           pgmPath;

       static const int       pgmOptN = 6;
       const char            *pgmOpts[pgmOptN] = {0};

       int                    defsTO  =30;
       std::set<std::string>  x2aSet;
       std::set<std::string>  x2eSet;

                              CfgInfo() {}
                             ~CfgInfo() {}
      };

CfgInfo      *Cfg = 0;
}
using namespace XrdNetPMarkConfig;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

/******************************************************************************/
/*                                 B e g i n                                  */
/******************************************************************************/
  
XrdNetPMark::Handle *XrdNetPMarkCfg::Begin(XrdSecEntity &client,
                                           const char   *path,
                                           const char   *cgi,
                                           const char   *app)
{
   EPName("PMBegin");
   int eCode, aCode;

// If we need to screen out domains, do that
//
   if (chkDom)
      {XrdNetAddrInfo &addrInfo = *client.addrInfo;
       char domType = (addrInfo.isPrivate() ? domLcl : domRmt);
       if (domType == domRmt && *myDomain)
          {const char *urName = addrInfo.Name();
           if (urName && XrdNetAddrInfo::isHostName(urName))
              {const char *dot = index(urName, '.');
               if (dot && !strcmp(dot+1, myDomain)) domType = domLcl;
              }
          }
       if (domType != chkDom)
          {DBGID(client.tident, "Skipping sending flow info; unwanted domain");
           return 0;
          }
      }

// Now get the experiment and activity code. If we can't get at least the
// experiment code, then proceed without marking the flow.
//
   if (!getCodes(client, path, cgi, eCode, aCode))
      {TRACE("Unable to determine experiment; flow not marked.");
       return 0;
      }

// Continue with successor function to complete the logic
//
   XrdNetPMark::Handle handle(app, eCode, aCode);
   return Begin(*client.addrInfo, handle, client.tident);
}

/******************************************************************************/

XrdNetPMark::Handle *XrdNetPMarkCfg::Begin(XrdNetAddrInfo      &addrInfo,
                                           XrdNetPMark::Handle &handle,
                                           const char          *tident)
{

// If we are allowed to use the flow label set on the incoming connection
// then try to do so. This is only valid for IPv6 connections. Currently,
// this is not implemented.
//
// if (useFLbl && addrInfo.isIPType(XrdNetAddrInfo::IPv6)
// && !addrInfo.isMapped())
//    {
//    TODO???
//    }

// If we are allowed to use firefly, return a firefly handle
//
   if (handle.Valid() && useFFly)
      {XrdNetPMarkFF *pmFF = new XrdNetPMarkFF(handle, tident);
       if (pmFF->Start(addrInfo)) return pmFF;
       delete pmFF;
      }

// All done, nothing will be pmarked
//
   return 0;
}

/******************************************************************************/
/*                                C o n f i g                                 */
/******************************************************************************/

XrdNetPMark *XrdNetPMarkCfg::Config(XrdSysError *eLog, XrdScheduler *sched,
                                    XrdSysTrace *trc, bool &fatal)
{
   class DelCfgInfo
        {public: DelCfgInfo(CfgInfo *&cfg) : cfgInfo(cfg) {}
                ~DelCfgInfo() {if (cfgInfo) {delete cfgInfo; cfgInfo = 0;}}
         private:
         CfgInfo *&cfgInfo;
        } cleanup(Cfg);

// If we have not been configured then simply retrn nil
//
   if (!Cfg)
      {useFFly = false;
       return 0;
      }

// Save the message handler
//
   eDest = eLog;
   Sched = sched;
   Trace = trc;
   fatal = false;

// If firefly is enabled, make sure we have an ffdest
//
   if (useFFly < 0)
      {if (ffPortD || ffPortO)
          {useFFly = true;
           if (!ffPortO) ffPortO = ffPORT;
          } else {
           useFFly = false;
           eLog->Say("Config warning: firefly disabled; "
                     "configuration incomplete!");
           return 0;
          }
      } else if (useFFly && !ffPortO) ffPortO = ffPORT;

// Resolve trace and debug settings
//
   if (doDebug) doTrace = true;

// Check if we need a defsfile, if so, construct the map.
//
   if (Cfg->x2aSet.size() == 0 && Cfg->x2eSet.size() == 0)
      {if (Cfg->defsFile.length())
          eLog->Say("Config warning: ignoring defsfile; "
                                    "no mappings have been specified!");
       useDefs = false;
      } else {
       if (!Cfg->defsFile.length())
          {eLog->Say("Config invalid: pmark mappings cannot be resolved "
                     "without specifying defsfile!");
           fatal = true;
           return 0;
          }
       useDefs = true;
       if (!ConfigDefs())
          {if (useDefs)
              {fatal = true;
               return 0;
              }
           eLog->Say("Config warning: pmark ignoring defsfile; "
                     "unable to process and nofail is in effect!");
          }
      }

// At this point either we still enabled or not. We can be disabled for a
// number of reasons and appropriate messages will have been issued.
//
   if (!useFFly) return 0;

// Create a netmsg object for firefly reporting if a dest was specified
//
   bool aOK = false;
   if (ffDest)
      {XrdNetAddr spec;
       char buff[1024];
       const char *eTxt = spec.Set(ffDest, -ffPortD);
       if (eTxt)
          {snprintf(buff, sizeof(buff), "%s:%d; %s", ffDest, ffPortD, eTxt);
           eLog->Emsg("Config", "pmark unable to create UDP tunnel to", buff);
           useFFly = false;
           fatal   = true;
           return 0;
          }
       if (spec.Format(buff, sizeof(buff)))
          netMsg = new XrdNetMsg(eDest, buff, &aOK);
       if (!aOK)
          {eLog->Emsg("Config", "pmark unable to create UDP tunnel to", ffDest);
           fatal = true;
           delete netMsg;
           netMsg = 0;
           useFFly= false;
           return 0;
          }
       }

// Handle the firefly messages to origin
//
   if (ffPortO)
      {netOrg = new XrdNetMsg(eDest, 0, &aOK);
       if (!aOK)
          {eLog->Emsg("Config","pmark unable to create origin UDP tunnel");
           fatal = true;
           useFFly= false;
           return 0;
          }
      }

// Get our host name.
//
   myHostName = XrdNetUtils::MyHostName("-"); // Never deleted!

// Setup for domain checking
//
   if (chkDom)
      {const char *dot = index(myHostName, '.');
       if (dot) myDomain = dot+1;
          else eDest->Say("Config warning: Unable to determine local domain; "
                          " domain check restricted to IP address type!");
      }

// Finally, we are done. Return the packet markling stub.
//
   return new XrdNetPMarkCfg;
}

/******************************************************************************/
/* Private:                   C o n f i g D e f s                             */
/******************************************************************************/

namespace
{
bool Recover()
{
   if (!noFail) return false;
   useDefs = false;
   return true;
}
}

bool XrdNetPMarkCfg::ConfigDefs()
{
   class Const2Char
        {public:
         char *data;
               Const2Char(const char *str) : data(strdup(str)) {}
              ~Const2Char() {free(data);}
        };
   EPName("ConfigDefs");
   std::set<std::string>::iterator it;
   std::map<std::string, ExpInfo>::iterator itE;
   bool isDload, aOK = true;

// If we need tp fetch the file, do so.
//
   if ((isDload = !(Cfg->defsFile.beginswith('/'))) && !FetchFile())
      return Recover();

// Now parse the defsfile (it is a json file)
//
   aOK = LoadFile();

// Get rid of the defsfile if we dowloaded it
//
   if (isDload) unlink(Cfg->defsFile.c_str());

// Only continue if all is well
//
   if (!aOK) return Recover();

// Configure the experiment mapping
//
   for (it = Cfg->x2eSet.begin(); it != Cfg->x2eSet.end(); it++)
       {Const2Char pv(it->c_str());
        if (!ConfigPV2E(pv.data)) aOK = false;
       }
   Cfg->x2eSet.clear();

// Configure the activity mapping
//
   for (it = Cfg->x2aSet.begin(); it != Cfg->x2aSet.end(); it++)
       {Const2Char ru(it->c_str());
        if (!ConfigRU2A(ru.data)) aOK = false;
       }
   Cfg->x2aSet.clear();

// Eliminate any experiment that we will not be using. This is will restrict
// flow marking to url passed information as there will be no internal deduction
//
   itE = expMap.begin();
   while(itE != expMap.end())
       {if (itE->second.inUse)
           {itE->second.Roles = itE->second.r2aMap.size() != 0;
            itE->second.Users = itE->second.u2aMap.size() != 0;
            itE++;
           } else {
            DEBUG("Deleting unused experiment '"<<itE->first.c_str()<<"'");
            itE = expMap.erase(itE);
           }
       }
   if (aOK && expMap.size() == 0)
      {useDefs = false; useFFly = false;
       if (useSTag)
          {eDest->Say("Config warning: No experiments referenced; "
                      "packet marking restricted to scitagged url's!");
          } else {
           eDest->Say("Config warning: No experiments referenced and scitags "
                      "not enabled; packet marking has been disabled!");
           useFFly = false;
          }
      } else if (!aOK)
                {useFFly = false; useDefs = false;
                } else {tryPath = !p2eMap.isEmpty();
                        tryVO   = v2eMap.size() != 0;
                        if (doTrace) Display();
                       }
   return aOK;
}

/******************************************************************************/
/* Private:                   C o n f i g P V 2 E                             */
/******************************************************************************/
  
namespace
{
void Complain(const char *rWho, const char *rName,
              const char *uWho, const char *uName, const char *eName=0)
{
   char *et0P = 0, eText0[256], eText1[256], eText2[256];
   if (eName)
      {snprintf(eText0, sizeof(eText0), "experiment %s", eName);
       et0P = eText0;
      }
   snprintf(eText1, sizeof(eText1), "%s '%s'", rWho, rName);
   snprintf(eText2, sizeof(eText2), "%s '%s'", uWho, uName);
   eDest->Say("Config failure: ",et0P, eText1," references undefined ",eText2);
}
}

// Note: info contains {path <path> | vo <voname> | default default} >exname>

bool XrdNetPMarkCfg::ConfigPV2E(char *info)
{
   std::map<std::string, ExpInfo >::iterator itE;
   std::map<std::string, ExpInfo*>::iterator itV;
   char *eName, *xName, *xType = info;
   xName = index(info,  ' '); *xName = 0; xName++;  // path | vo name
   eName = index(xName, ' '); *eName = 0; eName++;  // experiment name

   if ((itE = expMap.find(std::string(eName))) == expMap.end())
      {Complain(xType, xName, "experiment", eName);
       return false;
      }
   itE->second.inUse = true;

   if (*xType == 'd')
      {expDflt = &itE->second;
       return true;
      }

   if (*xType == 'p')
      {XrdOucMapP2X<ExpInfo*> *p2nP = p2eMap.Find(xName);
       if (p2nP)
          {p2nP->RepName(eName);
           p2nP->RepValu(&(itE->second));
          } else {
           XrdOucMapP2X<ExpInfo*> *px = (new XrdOucMapP2X<ExpInfo*>
                                             (xName, eName, &(itE->second)));
           p2eMap.Insert(px);
          }
      } else {
       itV = v2eMap.find(std::string(xName));
       if (itV != v2eMap.end()) itV->second = &(itE->second);
          else v2eMap[xName] = &(itE->second);
      }

   return true;
}

/******************************************************************************/
/* Private:                   C o n f i g R U 2 A                             */
/******************************************************************************/

// Note: info contains <ename> {dflt dflt | {{role | user} <xname>}} <aname>

bool XrdNetPMarkCfg::ConfigRU2A(char *info)
{
   std::map<std::string, int>::iterator itA;
   std::map<std::string, ExpInfo>::iterator itE;
   std::map<std::string, MapInfo>::iterator itX;
   char *aName, *eName, *xName, *xType;
   eName = info;                                    // experiment name
   xType = index(info,  ' '); *xType = 0; xType++;  // dflt | role | user
   xName = index(xType, ' '); *xName = 0; xName++;  // role name | user name
   aName = index(xName, ' '); *aName = 0; aName++;  // activity name

   if ((itE = expMap.find(std::string(eName))) == expMap.end())
      {Complain(xType, xName, "experiment", eName);
       return false;
      }

   itA = itE->second.actMap.find(std::string(aName));
   if (itA == itE->second.actMap.end())
      {Complain(xType, xName, "activity", aName, eName);
       return false;
      }

   if (*xType == 'd') itE->second.dAct = itA->second;
      else {std::map<std::string, MapInfo> &xMap =
                 (*xType == 'r' ? itE->second.r2aMap : itE->second.u2aMap);

            itX = xMap.find(std::string(xName));
            if (itX != xMap.end())
               {itX->second.Name = aName; itX->second.Code = itA->second;}
               else xMap[std::string(xName)] = MapInfo(aName, itA->second);
           }

   return true;
}
  
/******************************************************************************/
/* Private:                      D i s p l a y                                */
/******************************************************************************/

namespace
{
const char *Code2S(int code)
{
   static char buff[16];
   snprintf(buff, sizeof(buff), " [%d]", code);
   return buff;
}

void ShowActs(std::map<std::string, MapInfo>& map, const char *hdr,
                                                   const char *mName)
{
   std::map<std::string, MapInfo>::iterator it;

   for (it = map.begin(); it != map.end(); it++)
       {eDest->Say(hdr, mName, it->first.c_str(), " activity ",
                   it->second.Name.c_str(), Code2S(it->second.Code));
       }
}
}

void XrdNetPMarkCfg::Display()
{
   std::map<std::string, ExpInfo>::iterator itE;
   std::map<int, std::vector<const char*>> pvRefs;
   const char *hdr = "       ", *hdrplu = "       ++ ";
   char buff[80];

// Build map from path to experiment
//
   std::map<int, std::vector<const char*>>::iterator it2E;
   XrdOucMapP2X<ExpInfo*> *p2e = p2eMap.theNext();

   while(p2e)
        {ExpInfo *expinfo = p2e->theValu();
         if ((it2E = pvRefs.find(expinfo->Code)) != pvRefs.end())
            it2E->second.push_back(p2e->thePath());
            else {std::vector<const char*> vec;
                  vec.push_back(p2e->thePath());
                  pvRefs[expinfo->Code] = vec;
                 }
         p2e = p2e->theNext();
        }

// Add in the vo references
//
   std::map<std::string, ExpInfo*>::iterator itV;
   for (itV = v2eMap.begin(); itV != v2eMap.end(); itV++)
       {int eCode = itV->second->Code;
         if ((it2E = pvRefs.find(eCode)) != pvRefs.end())
            it2E->second.push_back(itV->first.c_str());
            else {std::vector<const char*> vec;
                  vec.push_back(itV->first.c_str());
                  pvRefs[eCode] = vec;
                 }
       }


// Indicate the number of experiments
//
   snprintf(buff, sizeof(buff), "%d", static_cast<int>(expMap.size()));
   const char *txt = (expMap.size() == 1 ? " expirement " : " experiments ");
   eDest->Say("Config pmark results: ", buff, txt, "directly referenced:");

// Display information
//
   for (itE = expMap.begin(); itE != expMap.end(); itE++)
       {int expCode = itE->second.Code;
        eDest->Say(hdr, itE->first.c_str(), Code2S(expCode),
                   (&itE->second  == expDflt ? " (default)" : 0));
        if ((it2E = pvRefs.find(expCode)) != pvRefs.end())
           {std::vector<const char*> &vec = it2E->second;
            for (int i = 0; i < (int)vec.size(); i++)
                {const char *rType = (*vec[i] == '/' ? "path " : "vorg ");
                 eDest->Say(hdrplu, rType, vec[i]);
                }
           }
        if (itE->second.u2aMap.size() != 0)
           ShowActs(itE->second.u2aMap, hdrplu, "user ");
        if (itE->second.r2aMap.size() != 0)
           ShowActs(itE->second.r2aMap, hdrplu, "role ");
        if (itE->second.dAct >= 0)
           {std::map<std::string, int>::iterator itA;
            int aCode = itE->second.dAct;
            for (itA  = itE->second.actMap.begin();
                 itA != itE->second.actMap.end(); itA++)
                {if (aCode == itA->second)
                    {eDest->Say(hdrplu, "Default activity ",
                                itA->first.c_str(), Code2S(aCode));
                     break;
                    }
                }
            if (itA == itE->second.actMap.end()) itE->second.dAct = -1;
           }
       }
}
  
/******************************************************************************/
/* Private:                      E x t r a c t                                */
/******************************************************************************/

const char *XrdNetPMarkCfg::Extract(const char *sVec, char *buff, int blen)
{
   const char *space;

// If there is only one token in sVec then return it.
//
  if (!(space = index(sVec, ' '))) return sVec;

// Extract out the token using the supplied buffer
//
   int n = space - sVec;
   if (!n || n >= blen) return 0;
   snprintf(buff, blen, "%.*s", n, sVec);
   return buff;
}
  
/******************************************************************************/
/* Private:                    F e t c h F i l e                              */
/******************************************************************************/

bool XrdNetPMarkCfg::FetchFile()
{
   EPName("FetchFile");
   XrdOucProg fetchJob(eDest);
   char tmo[16], outfile[512];
   int rc;

// Setup the job
//
   if ((rc = fetchJob.Setup(Cfg->pgmPath.c_str(), eDest)))
      {eDest->Emsg("Config", rc, "setup job to fetch defsfile");
       return false;
      }

// Create the output file name (it willl be written to /tmp)
//
   snprintf(outfile, sizeof(outfile), "/tmp/XrdPMark-%ld.json",
                     static_cast<long>(getpid()));
   unlink(outfile);

// Insert the timeout value argument list and complete it.
//
   snprintf(tmo, sizeof(tmo), "%d", Cfg->defsTO);
   Cfg->pgmOpts[1] = tmo;  // 0:-x 1:tmo 2:-y 3:-z 4:outfile 5:defsfile
   Cfg->pgmOpts[4] = outfile;
   Cfg->pgmOpts[5] = Cfg->defsFile.c_str();

// Do some debugging
//
   if (doDebug)
      {for (int i = 0; i < CfgInfo::pgmOptN; i++)
           {Cfg->pgmPath += ' '; Cfg->pgmPath += Cfg->pgmOpts[i];}
       DEBUG("Running: " <<Cfg->pgmPath.c_str());
      }

// Run the appropriate fetch command
//
   rc = fetchJob.Run(Cfg->pgmOpts, CfgInfo::pgmOptN);
   if (rc)
       {snprintf(outfile, sizeof(outfile), "failed with rc=%d", rc);
        eDest->Emsg("Config", "Fetch via", Cfg->pgmPath.c_str(), outfile);
        return false;
       }

// Set the actual output file
//
   Cfg->defsFile = outfile;
   return true;
}
  
/******************************************************************************/
/* Private:                     g e t C o d e s                               */
/******************************************************************************/
  
bool XrdNetPMarkCfg::getCodes(XrdSecEntity &client, const char *path,
                              const char *cgi, int &ecode, int &acode)
{
   ExpInfo* expP = 0;

// If we are allowed to use scitags, then try that first
//
   if (useSTag && cgi && XrdNetPMark::getEA(cgi, ecode, acode)) return true;

// If we can use the definitions (i.e. in error) return w/o packet marking
//
   if (!useDefs) return false;

// Try to use the path argument.
//
   if (tryPath && path)
      {XrdOucMapP2X<ExpInfo*> *p2nP = p2eMap.Match(path);
       if (p2nP) expP = p2nP->theValu();
      }

// If the path did not succeed, then try the vo
//
   if (!expP && tryVO && client.vorg)
      {std::map<std::string, ExpInfo*>::iterator itV;
       char voBuff[256];
       const char *VO = Extract(client.vorg, voBuff, sizeof(voBuff));
       if (VO && (itV = v2eMap.find(std::string(client.vorg))) != v2eMap.end())
          expP = itV->second;
      }

// If there is no experiment yet, use the default if one exists
//
   if (!expP && expDflt) expP = expDflt;

// If we still have no experiment then fail. We cannot packet mark.
//
   if (!expP) return false;
   ecode = expP->Code;

// If there are user to activity mappings, see if we can use that
//
   if (expP->Users && client.name)
      {std::map<std::string, MapInfo>::iterator itU;
       itU = expP->u2aMap.find(std::string(client.name));
       if (itU != expP->u2aMap.end())
          {acode = itU->second.Code;
           return true;
          }
      }

// If there are role to activity mappings, see if we can use that
//
   if (expP->Roles && client.role)
      {std::map<std::string, MapInfo>::iterator itR;
       char roBuff[256];
       const char *RO = Extract(client.role, roBuff, sizeof(roBuff));
       if (RO)
          {itR = expP->r2aMap.find(std::string(client.role));
           if (itR != expP->r2aMap.end())
              {acode = itR->second.Code;
               return true;
              }
          }
      }

// If a default activity exists, return that. Otherwise, it's unspecified.
//
   acode = (expP->dAct >= 0 ? expP->dAct : 0);
   return true;
}
  
/******************************************************************************/
/* Private:                     L o a d F i l e                               */
/******************************************************************************/

using json = nlohmann::json;

namespace
{
const char *MsgTrim(const char *msg)
{
   const char *sP;
   if ((sP = index(msg, ' ')) && *(sP+1)) return sP+1;
   return msg;
}
}

bool XrdNetPMarkCfg::LoadFile()
{
   struct fBuff {char *buff; fBuff() : buff(0) {}
                            ~fBuff() {if (buff) free(buff);}
                } defs;
   int rc;
  
// The json file is relatively small so read the whole thing in
//
   if (!(defs.buff = XrdOucUtils::getFile(Cfg->defsFile.c_str(), rc)))
      {eDest->Emsg("Config", rc, "read defsfile", Cfg->defsFile.c_str());
       return false;
      }

// Parse the file and return result. The parser may throw an exception
// so we will catch it here.
//
   try {bool result = LoadJson(defs.buff);
        return result;
       } catch (json::exception& e)
               {eDest->Emsg("Config", "Unable to process defsfile;",
                                      MsgTrim(e.what()));
               }
   return false;
}

/******************************************************************************/
/* Private:                     L o a d J s o n                               */
/******************************************************************************/
  
bool XrdNetPMarkCfg::LoadJson(char *buff)
{
   json j;
   std::map<std::string, ExpInfo>::iterator itE;

// Parse the file; caller will catch any exceptions
//
   j = json::parse(buff);

// Extract out modification data
//
   std::string modDate;
   json j_mod = j["modified"];
   if (j_mod != 0) modDate = j_mod.get<std::string>();
      else modDate = "*unspecified*";

   eDest->Say("Config using pmark defsfile '", Cfg->defsFile.c_str(),
              "' last modified on ", modDate.c_str());

// Extract out the experiments
//
   json j_exp = j["experiments"];
   if (j_exp == 0)
      {eDest->Emsg("Config", "The defsfile does not define any experiments!");
       return false;
      }

// Now iterate through all of the experiments and the activities within
// and define our local maps for each.
//
   for (auto it : j_exp)
       {std::string expName = it["expName"].get<std::string>();
        if (expName.empty()) continue;
        if (!it["expId"].is_number() || it["expId"] < minExpID || it["expId"] > maxExpID)
           {eDest->Say("Config warning: ignoring experiment '", expName.c_str(),
                       "'; associated ID is invalid.");
            continue;
           }
        expMap[expName] = ExpInfo(it["expId"].get<int>());

        if ((itE = expMap.find(expName)) == expMap.end())
           {eDest->Say("Config warning: ignoring experiment '", expName.c_str(),
                       "'; map insertion failed!");
            continue;
           }

        json j_acts = it["activities"];
        if (j_acts == 0)
           {eDest->Say("Config warning: ignoring experiment '", expName.c_str(),
                       "'; has no activities!");
            continue;
           }

        for (unsigned int i = 0; i < j_acts.size(); i++)
            {std::string actName =  j_acts[i]["activityName"].get<std::string>();
             if (actName.empty()) continue;
             if (!j_acts[i]["activityId"].is_number()
             ||   j_acts[i]["activityId"] < minActID
             ||   j_acts[i]["activityId"] > maxActID)
                {eDest->Say("Config warning:", "ignoring ", expName.c_str(),
                            " actitivity '", actName.c_str(),
                            "'; associated ID is invalid.");
                 continue;
                }
             itE->second.actMap[actName] = j_acts[i]["activityId"].get<int>();
            }
       }

// Make sure we have at least one experiment defined
//
   if (!expMap.size())
      {eDest->Say("Config warning: unable to define any experiments via defsfile!");
       return false;
      }
   return true;
}

/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/
  
int XrdNetPMarkCfg::Parse(XrdSysError *eLog, XrdOucStream &Config)
{
// Parse pmark directive parameters:
//
// [[no]debug] [defsfile [[no]fail] {<path> | {curl | wget} [tmo] <url>}]
// [domain {any | local | remote}] [[no]fail] [ffdest <udpdest>]
// [ffecho <intvl>]
// [map2act <ename> {default | {role | user} <name>} <aname>]
// [map2exp {default | {path <path> | vo <vo>} <ename>}] [[no]trace]
// [use {[no]flowlabel | flowlabel+ff | [no]firefly | [no]scitag}
//
// <udpdest>: {origin[:<port>] | <host>[:port]} [,<udpdest>]
//
   std::string name;
   char *val;

// If this is the first time here, allocate config info object
//
   if (!Cfg) Cfg = new CfgInfo;

// Make sure we have something to parse
//
   if (!(val = Config.GetWord()))
      {eLog->Say("Config invalid: pmark argument not specified"); return 1;}

// Parse the directive options
//
do{if (!strcmp("debug", val) || !strcmp("nodebug", val))
      {doDebug =  (*val != 'n');
       continue;
      }

   if (!strcmp("defsfile", val))
      {if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark defsfile value not specified");
           return 1;
          }

       if (*val == '/')
          {Cfg->defsFile = val;
           continue;
          }

       if (strcmp("curl", val) && strcmp("wget", val))
          {eLog->Say("Config invalid: unknown defsfile transfer agent '",val,"'");
           return 1;
          }
       if (!XrdOucUtils::findPgm(val, Cfg->pgmPath))
          {eLog->Say("Config invalid: defsfile transfer agent '",val,"' not found.");
           return 1;
          }

       if (*val == 'c')
          {Cfg->pgmOpts[0]="-m"; Cfg->pgmOpts[2]="-s"; Cfg->pgmOpts[3]="-o";
          } else {
           Cfg->pgmOpts[0]="-T"; Cfg->pgmOpts[2]="-q"; Cfg->pgmOpts[3]="-O";
          }

       val = Config.GetWord();
       if (val && isdigit(*val))
          {if (XrdOuca2x::a2tm(*eLog,"defsfile timeout",val,&Cfg->defsTO,10))
              return 1;
           val = Config.GetWord();
          }

       if (!val) {eLog->Say("Config invalid: pmark defsfile url not specified");
                  return 1;
                 }
       Cfg->defsFile = val;
       continue;
      }

   if (!strcmp("domain", val))
      {if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark domain value not specified");
           return 1;
          }
            if (!strcmp(val, "any"   )
            ||  !strcmp(val, "all"   )) chkDom = domAny;
       else if (!strcmp(val, "local" )) chkDom = domLcl;
       else if (!strcmp(val, "remote")) chkDom = domRmt;
       else {eLog->Say("Config invalid: pmark invalid domain determinant '",
                       val, "'");
             return 1;
            }
       continue;
      }

    if (!strcmp("fail", val) || !strcmp("nofail", val))
       {noFail = (*val == 'n');
        continue;
       }

   // We accept 'origin' as a dest for backward compatibility. That is the
   // enforced default should 'use firefly' be specified.
   //
   if (!strcmp("ffdest", val))
      {const char *addtxt = "";
       char *colon, *comma;
       int  xPort;
       val = Config.GetWord();
       do {if (!val || *val == 0 || *val == ',' || *val == ':')
              {eLog->Say("Config invalid: pmark ffdest value not specified",
                         addtxt); return 1;
              }
           if ((comma = index(val, ','))) *comma++ = 0;
           if ((colon = index(val, ':')))
              {*colon++ = 0;
               if ((xPort = XrdOuca2x::a2p(*eLog, "udp", colon, false)) <= 0)
                  return 1;
              } else xPort = ffPORT;
           if (!strcmp(val, "origin")) ffPortO = xPort;
              else {if (ffDest) free(ffDest);
                    ffDest  = strdup(val);
                    ffPortD = xPort;
                   }
           addtxt = " after comma";
          } while((val = comma));
       if (useFFly < 0) useFFly = 1;
       continue;
      }

   if (!strcmp("ffecho", val))
      {if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark ffecho value not specified");
           return 1;
          }
       if (XrdOuca2x::a2tm(*eLog,"ffecho interval", val, &ffEcho, 0)) return 1;
       if (ffEcho < 30) ffEcho = 0;
       continue;
      }

   if (!strcmp("map2act", val))
      {if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark activity experiment not specified");
           return 1;
          }
       name = val;

       if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark activity determinant not specified");
           return 1;
          }

       const char *adet;
            if (!strcmp(val, "default")) adet = "dflt";
       else if (!strcmp(val, "role"))    adet = "role";
       else if (!strcmp(val, "user"))    adet = "user";
       else {eLog->Say("Config invalid: pmark invalid activity determinant '",
                       val, "'");
             return 1;
            }
       name += ' '; name += val;

       if (*adet != 'd' && !(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark activity", adet, "not specified");
           return 1;
          }
       name += ' '; name += val;

       if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark", adet, "activity not specified");
           return 1;
          }
       name += ' '; name += val;

       Cfg->x2aSet.insert(name);
       continue;
      }

   if (!strcmp("map2exp", val))
      {if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark map2exp type not specified");
           return 1;
          }
       if (strcmp("default", val) && strcmp("path", val)
       &&  strcmp("vo", val) && strcmp("vorg", val))
          {eLog->Say("Config invalid: invalid pmark map2exp type, '",val,"'.");
           return 1;
          }
       name = val;

       if (*val != 'd' && !(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark map2exp ", name.c_str(),
                     "not  specified");
           return 1;
          }
       name += ' '; name += val;

       if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark map2exp expirement not specified");
           return 1;
          }
       name += ' '; name += val;

       Cfg->x2eSet.insert(name);
       continue;
      }

   if (!strcmp("trace", val) || !strcmp("notrace", val))
      {doTrace = (*val != 'n');
       continue;
      }

   if (!strcmp("use", val))
      {if (!(val = Config.GetWord()))
          {eLog->Say("Config invalid: pmark use argument not specified");
           return 1;
          }
       bool argOK = false;
       char *arg;
       do {bool theval = strncmp(val, "no", 2) != 0;
           arg = (!theval ? val += 2 : val);
                if (!strcmp("flowlabel",  arg))
                   {useFLbl = theval; addFLFF = false; argOK = true;}
           else if (!strcmp("flowlabel+ff",  arg))
                   {addFLFF = useFLbl = theval; argOK = true;}
           else if (!strcmp("firefly", arg))
                   {useFFly = (theval ? 1 : 0); argOK = true;}
           else if (!strcmp("scitag",  arg)) {useSTag = theval; argOK = true;}
           else if (argOK) {Config.RetToken(); break;}
           else {eLog->Say("Config invalid: 'use ",val,"' is invalid");
                 return 1;
                }
          } while((val = Config.GetWord()));
       if (!val) break;
       continue;
      }

   eLog->Say("Config warning: ignoring unknown pmark argument'",val,"'.");

  } while ((val = Config.GetWord()));

   return 0;
}
