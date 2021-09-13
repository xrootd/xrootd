/***************************************************************************/
/*                                                                            */
/*                       X r d S e c S e r v e r . c c                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <fcntl.h>
#include <netdb.h>
#include <cstdlib>
#include <strings.h>
#include <cstdio>
#include <sys/param.h>

#include "XrdVersion.hh"

#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPinKing.hh"
#include "XrdNet/XrdNetAddr.hh"

#include "XrdSec/XrdSecEntityPin.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecProtector.hh"
#include "XrdSec/XrdSecServer.hh"
#include "XrdSec/XrdSecTrace.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

/******************************************************************************/
/*                       S e c u r i t y   L e v e l s                        */
/******************************************************************************/

namespace
{
XrdSecProtectParms lclParms;
XrdSecProtectParms rmtParms;

XrdVERSIONINFODEF(myVer, XrdSec, XrdVNUMBER, XrdVERSION);
}

/******************************************************************************/
/*                         X r d S e c P i n I n f o                          */
/******************************************************************************/

class XrdSecPinInfo
{
public:

XrdOucPinKing<XrdSecEntityPin> KingPin;

      XrdSecPinInfo(const char *drctv, const char *cfn, XrdSysError &errR)
                   : KingPin(drctv, theEnv, errR, &myVer)
                   {theEnv.Put("configFN", cfn);}

     ~XrdSecPinInfo() {}

XrdOucEnv theEnv;
};
  
/******************************************************************************/
/*                        X r d S e c P r o t B i n d                         */
/******************************************************************************/

class XrdSecProtBind 
{
public:
XrdSecProtBind        *next;
char                  *thost;
int                    tpfxlen;
char                  *thostsfx;
int                    tsfxlen;
XrdSecParameters       SecToken;
XrdSecPMask_t          ValidProts;

XrdSecProtBind        *Find(const char *hname);

int                    Match(const char *hname);

                       XrdSecProtBind(char *th, char *st, XrdSecPMask_t pmask=0);
                      ~XrdSecProtBind()
                             {free(thost);
                              if (SecToken.buffer) free(SecToken.buffer);
                             }
};
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSecProtBind::XrdSecProtBind(char *th, char *st, XrdSecPMask_t pmask)
{
   char *starp;
   next     = 0;
   thost    = th; 
   if (!(starp = index(thost, '*')))
      {tsfxlen = -1;
       thostsfx = (char *)0;
       tpfxlen = 0;
      } else {
       *starp = '\0';
       tpfxlen = strlen(thost);
       thostsfx = starp+1;
       tsfxlen = strlen(thostsfx);
      }
   if (st) {SecToken.buffer = strdup(st); SecToken.size = strlen(st);}
      else {SecToken.buffer = 0;          SecToken.size = 0;}
   ValidProts = (pmask ? pmask : ~(XrdSecPMask_t)0);
}
 
/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/

XrdSecProtBind *XrdSecProtBind::Find(const char *hname)
{
   XrdSecProtBind *bp = this;

   while(bp && !bp->Match(hname)) bp = bp->next;

   return bp;
}
  
/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/
  
int XrdSecProtBind::Match(const char *hname)
{
    int i;

// If an exact match wanted, return the result
//
   if (tsfxlen < 0) return !strcmp(thost, hname);

// Try to match the prefix
//
   if (tpfxlen && strncmp(thost, hname, tpfxlen)) return 0;

// If no suffix matching is wanted, then we have succeeded
//
   if (!(thostsfx)) return 1;

// Try to match the suffix
//
   if ((i = (strlen(hname) - tsfxlen)) < 0) return 0;
   return !strcmp(&hname[i], thostsfx);
}

/******************************************************************************/
/*                        X r d S e c P r o t P a r m                         */
/******************************************************************************/

class XrdSecProtParm
{
public:

       void            Add() {Next = First; First = this;}

       int             Cat(char *token);

static XrdSecProtParm *Find(char *pid, int remove=0);

       int             Insert(char oct);

       int             isProto(char *proto) {return !strcmp(ProtoID, proto);}

       char           *Result(int &size) {size = bp-buff; return buff;}

       void            setProt(char *pid) {strcpy(ProtoID, pid);}

static XrdSecProtParm *First;
       XrdSecProtParm *Next;

char   ProtoID[XrdSecPROTOIDSIZE+1];

       XrdSecProtParm(XrdSysError *erp, const char *cid) : who(cid)
                     {*ProtoID = '\0';
                      bsize = 4096;
                      buff = (char *)malloc(bsize);
                      *buff = '\0';
                      bp   = buff;
                      eDest = erp;
                      Next = 0;
                     }
      ~XrdSecProtParm() {free(buff);}
private:

XrdSysError *eDest;
int          bsize;
char        *buff;
char        *bp;
const char  *who;
};

XrdSecProtParm *XrdSecProtParm::First = 0;
  
/******************************************************************************/
/*                                   C a t                                    */
/******************************************************************************/
  
int XrdSecProtParm::Cat(char *token)
{
   int alen;
   alen = strlen(token);
   if (alen+1 > bsize-(bp-buff))
      {eDest->Emsg("Config",who,ProtoID,"argument string too long");
       return 0;
      }
   *bp++ = ' ';
   strcpy(bp, token); 
   bp += alen;
   return 1;
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/

XrdSecProtParm *XrdSecProtParm::Find(char *pid, int remove)
{
   XrdSecProtParm *mp, *pp;

   mp = 0; pp = First;
   while(pp && !pp->isProto(pid)){mp = pp; pp = pp->Next;}
   if (pp && remove)
      {if (mp) mp->Next  = pp->Next;
          else First     = pp->Next;
      }
   return pp;
}
  
/******************************************************************************/
/*                                I n s e r t                                 */
/******************************************************************************/
  
int XrdSecProtParm::Insert(char oct)
{
   if (bsize-(bp-buff) < 1)
      {eDest->Emsg("Config",who,ProtoID,"argument string too long");
       return 0;
      }
   *bp++ = oct;
   return 1;
}

/******************************************************************************/
/*                          X r d S e c S e r v e r                           */
/******************************************************************************/

XrdSecPManager XrdSecServer::PManager;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSecServer::XrdSecServer(XrdSysLogger *lp) : eDest(lp, "sec_")
{

// Set default values
//
   PManager.setErrP(&eDest);
   configFN    = "";
   bpFirst     = 0;
   bpLast      = 0;
   bpDefault   = 0;
   pinInfo     = 0;
   pidList     = 0;
   STBlen      = 4096;
   STBuff      = (char *)malloc(STBlen);
  *STBuff      = '\0';
   SToken      = STBuff;
   SecTrace    = new XrdOucTrace(&eDest);
   if (getenv("XRDDEBUG") || getenv("XrdSecDEBUG"))
      {SecTrace->What = TRACE_ALL;
       PManager.setDebug(1);
      }
   Enforce     = false;
   implauth    = false;
}
  
/******************************************************************************/
/*                              g e t P a r m s                               */
/******************************************************************************/
  
const char *XrdSecServer::getParms(int &size, XrdNetAddrInfo *endPoint)
{
   EPNAME("getParms")
   XrdSecProtBind *bp;
   char buff[256];

// Try to find a specific token binding for a host or return default binding
//
   if (!endPoint || !bpFirst) bp = 0;
      else {const char *hname = endPoint->Name("*unknown*");
            bp = bpFirst;
            do {if (bp->Match(hname)) break;} while((bp = bp->next));
           }

// Get endpoint info if we are debugging
//
   if (endPoint && QTRACE(Debug))
      endPoint->Format(buff, sizeof(buff), XrdNetAddrInfo::fmtAuto,
                                           XrdNetAddrInfo::noPort);
      else *buff = 0;

// If we have a binding, return that else return the default
//
   if (!bp) bp = bpDefault;
   if (bp->SecToken.buffer) 
      {DEBUG(buff <<" sectoken=" <<bp->SecToken.buffer);
       size = bp->SecToken.size;
       return bp->SecToken.buffer;
      }

   DEBUG(buff <<" sectoken=''");
   size = 0;
   return (const char *)0;
}

/******************************************************************************/
/*                           g e t P r o t o c o l                            */
/******************************************************************************/

XrdSecProtocol *XrdSecServer::getProtocol(const char              *host,
                                          XrdNetAddrInfo          &endPoint,
                                          const XrdSecCredentials *cred,
                                          XrdOucErrInfo           &einfo)
{
   XrdSecProtBind *bp;
   XrdSecPMask_t pnum;
   XrdSecCredentials myCreds;
   const char *msgv[8];

// If null credentials supplied, default to host protocol otherwise make sure
// credentials data is actually supplied.
//
   if (!cred) {myCreds.buffer=(char *)"host"; myCreds.size = 4; cred=&myCreds;}
      else if (cred->size < 1 || !(cred->buffer))
              {einfo.setErrInfo(EACCES,"No authentication credentials supplied.");
               return 0;
              }

// If protocol binding must be enforced, make sure the host is not using a
// disallowed protocol.
//
   if (Enforce)
      {if ((pnum = PManager.Find(cred->buffer)))
          {if (bpFirst && (bp = bpFirst->Find(host))
           &&  !(bp->ValidProts & pnum))
              {msgv[0] = host;
               msgv[1] = " not allowed to authenticate using ";
               msgv[2] = cred->buffer;
               msgv[3] = " protocol.";
               einfo.setErrInfo(EACCES, msgv, 4);
               return 0;
              }
          }
          else {msgv[0] = cred->buffer;
                msgv[1] = " security protocol is not supported.";
                einfo.setErrInfo(EPROTONOSUPPORT, msgv, 2);
                return 0;
               }
      }

// If we passed the protocol binding check, try to get an instance of the
// protocol the host is using
//
   return PManager.Get(host, endPoint, cred->buffer, &einfo);
}
  
/******************************************************************************/
/*                           P o s t P r o c e s s                            */
/******************************************************************************/
  
bool XrdSecServer::PostProcess(XrdSecEntity &entity, XrdOucErrInfo &einfo)
{
// Return correct result. Make sure there is some kind of message returned.
//
   if (secEntityPin && !secEntityPin->Process(entity, einfo))
      {if (*einfo.getErrText() == '\0')
          einfo.setErrInfo(EAUTH, "rejected by auth post processing");
       return false;
      }

   return true;
}

/******************************************************************************/
/*        C o n f i g   F i l e   P r o c e s s i n g   M e t h o d s         */
/******************************************************************************/
/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)   if (!strcmp(x,var)) return m(Config,Eroute);

#define TS_Str(x,m)   if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_Chr(x,m)   if (!strcmp(x,var)) {m = val[0]; return 0;}

#define TS_Bit(x,m,v) if (!strcmp(x,var)) {m = v; return 0;}

#define Max(x,y) (x > y ? x : y)

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdSecServer::Configure(const char *cfn)
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
{
   extern XrdSecProtector *XrdSecLoadProtection(XrdSysError &erP);
   static const int isRlx = XrdSecProtectParms::relax;
   static const int isFrc = XrdSecProtectParms::force;
   XrdSecProtector *protObj;
   const char *lName = "none", *rName = "none";
   char *var;
   int  NoGo;

// Print warm-up message
//
   eDest.Say("++++++ Authentication system initialization started.");

// Perform initialization
//
   NoGo = ConfigFile(cfn);

// Load the entity post processing plugin if we have one
//
   if (pinInfo && !NoGo)
      {XrdSecEntityPin *secPin = pinInfo->KingPin.Load("SecEntityPin");
       delete pinInfo;
       secEntityPin = secPin;
       if (!secPin) return 1;
      }

// Export the list of security protocols that are available
//
   if (pidList) XrdOucEnv::Export("XRDSECPROTOCOLS", pidList);

// Almost done
//
   var = (NoGo > 0 ? (char *)"failed." : (char *)"completed.");
   eDest.Say("------ Authentication system initialization ", var);

// No need to configure protect system if authentication failed
//
   if (NoGo) return 1;

// Put out another banner
//
   eDest.Say("++++++ Protection system initialization started.");

// If local level if greater than remote level, issue a warning
//
   if (lclParms.level > rmtParms.level)
      eDest.Say("Config warning: local protection level greater than "
                "remote level; are you sure?");

// Check if we need to initialize protection services
//
   if (lclParms.level == XrdSecProtectParms::secNone
   &&  rmtParms.level == XrdSecProtectParms::secNone)
      {eDest.Say("Config warning: Security level is set to none; "
                 "request protection disabled!");
      } else {
       if (!(protObj = XrdSecLoadProtection(eDest))
       ||  !(protObj->Config(lclParms, rmtParms, *eDest.logger()))) NoGo = 1;
          else {lName = protObj->LName(lclParms.level);
                rName = protObj->LName(rmtParms.level);
               }
      }

// Blurt out what we have
//
   if (!NoGo)
      {eDest.Say("Config ","Local  protection level: ",
                 (lclParms.opts & isRlx ? "relaxed " : 0), lName,
                 (lclParms.opts & isFrc ? " force"   : 0));
       eDest.Say("Config ","Remote protection level: ",
                 (rmtParms.opts & isRlx ? "relaxed " : 0), rName,
                 (rmtParms.opts & isFrc ? " force"   : 0));
      }

// Now we are done
//
   var = (NoGo > 0 ? (char *)"failed." : (char *)"completed.");
   eDest.Say("------ Protection system initialization ", var);
   return (NoGo > 0);
}

/******************************************************************************/
/*                            C o n f i g F i l e                             */
/******************************************************************************/
  
int XrdSecServer::ConfigFile(const char *ConfigFN)
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   1 - Initialization failed.
            0 - Initialization succeeded.
*/
{
   char *var;
   int  cfgFD, retc, NoGo = 0, recs = 0;
   XrdOucEnv myEnv;
   XrdOucStream Config(&eDest, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   XrdSecProtParm *pp;

// If there is no config file, return with the defaults sets.
//
   if (!ConfigFN || !*ConfigFN)
     {eDest.Emsg("Config", "Authentication configuration file not specified.");
      return 1;
     }
   configFN = ConfigFN;

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {eDest.Emsg("Config", errno, "opening config file", ConfigFN);
       return 1;
      }

// Now start reading records until eof.
//
   Config.Attach(cfgFD); Config.Tabs(0);
   static const char *cvec[] = { "*** sec plugin config:", 0 };
   Config.Capture(cvec);
   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, "sec.", 4))
            {recs++;
             if (ConfigXeq(var+4, Config, eDest)) {Config.Echo(); NoGo = 1;}
            }
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = eDest.Emsg("Config",-retc,"reading config file", ConfigFN);
      else {char buff[128];
            snprintf(buff, sizeof(buff), 
                     " %d authentication directives processed in ", recs);
            eDest.Say("Config", buff, ConfigFN);
           }
   Config.Close();

// Determine whether we should initialize security
//
   if (NoGo || ProtBind_Complete(eDest) ) NoGo = 1;
      else if ((pp = XrdSecProtParm::First))
              {NoGo = 1;
               while(pp) {eDest.Emsg("Config", "protparm", pp->ProtoID,
                                     "does not have a matching protocol.");
                          pp = pp->Next;
                         }
              }

// All done
//
   return NoGo;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/
  
int XrdSecServer::ConfigXeq(char *var, XrdOucStream &Config, XrdSysError &Eroute)
{

    // Fan out based on the variable
    //
    TS_Xeq("entitylib",     xenlib);
    TS_Xeq("level",         xlevel);
    TS_Xeq("protbind",      xpbind);
    TS_Xeq("protocol",      xprot);
    TS_Xeq("protparm",      xpparm);
    TS_Xeq("trace",         xtrace);

    // No match found, complain.
    //
    Eroute.Say("Config warning: ignoring unknown directive '",var,"'.");
    Config.Echo();
    return 0;
}
  
/******************************************************************************/
/*                                x e n l i b                                 */
/******************************************************************************/

/* Function: xenlib

   Purpose:  To parse the directive: entitylib [++] <path> [<parms>]

             <path>  absolute path to the entity plugin.
             <parms> optional parameters passed to the plugin.

   Output: 0 upon success or !0 upon failure.
*/

int XrdSecServer::xenlib(XrdOucStream &Config, XrdSysError &Eroute)
{
   std::string path;
   char *val, parms[2048];
   bool push = false;

// Get the path or the push token
//
   if ((val = Config.GetWord()))
      {if (!strcmp(val, "++"))
          {push = true;
           val =  Config.GetWord();
          }
      }

// Make sure a path was specified
//
   if (!val || !*val)
      {Eroute.Emsg("Config", "entitylib not specified"); return 1;}

// Make sure the path is absolute
//
   if (*val != '/')
      {Eroute.Emsg("Config", "entitylib path is not absolute"); return 1;}

// Sequester the path as we will get additional tokens
//
   path = val;

// Record any parms
//
   if (!Config.GetRest(parms, sizeof(parms)))
      {Eroute.Emsg("Config", "entitylib parameters too long"); return 1;}

// Check if we have a plugin info object (we will need one for this)
//
   if (!pinInfo) pinInfo = new XrdSecPinInfo("sec.entitylib",configFN,Eroute);

// Add the plugin
//
   pinInfo->KingPin.Add(path.c_str(), (*parms ? parms : 0), push);

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                x l e v e l                                 */
/******************************************************************************/

/* Function: xlevel

   Purpose:  To parse the directive: level [<type>] [relaxed] <level> [force]

             <type>  all | local | remote
             <level> none | compatible | standard | intense | pedantic

   Output: 0 upon success or !0 upon failure.
*/

int XrdSecServer::xlevel(XrdOucStream &Config, XrdSysError &Eroute)
{
   struct lvltab {const char *lname; XrdSecProtectParms::secLevel lvl;} ltab[] =
                 {{"none",       XrdSecProtectParms::secNone},
                  {"compatible", XrdSecProtectParms::secCompatible},
                  {"standard",   XrdSecProtectParms::secStandard},
                  {"intense",    XrdSecProtectParms::secIntense},
                  {"pedantic",   XrdSecProtectParms::secPedantic}
                 };
   int i, numopts = sizeof(ltab)/sizeof(struct lvltab);
   bool isLcl = true, isRmt = true, isSpec = false, isRlx = false, isFRC=false;
   char *val;

// Get the template host
//
   val = Config.GetWord();
   if (!val || !val[0])
      {Eroute.Emsg("Config","level not specified"); return 1;}

// Check for optional keyword
//
        if (!strcmp(val, "all"))    isSpec = true;
   else if (!strcmp(val, "local")) {isSpec = true; isRmt = false;}
   else if (!strcmp(val, "remote")){isSpec = true; isLcl = false;}

// Check if we need another token
//
   if (isSpec)
      {val = Config.GetWord();
       if (!val || !val[0])
          {Eroute.Emsg("Config","level not specified"); return 1;}
      }

// Check for optional relaxed keyword
//
   if (!strcmp(val, "relaxed"))
      {isRlx = true;
       val = Config.GetWord();
       if (!val || !val[0])
          {Eroute.Emsg("Config","level not specified"); return 1;}
      }

// Get the level
//
   for (i = 0; i < numopts; i++) if (!strcmp(ltab[i].lname, val)) break;
   if (i >= numopts)
      {Eroute.Emsg("Config", "invalid level option -", val); return 1;}

// Check for final keyword
//
   val = Config.GetWord();
   if (val && val[0])
      {if (strcmp(val, "force"))
          {Eroute.Emsg("Config","invalid level modifier - ", val); return 1;}
       isFRC = true;
      }

// Set appropriate levels
//
   if (isLcl)
      {lclParms.level = ltab[i].lvl;
       if (isRlx) lclParms.opts  |=  XrdSecProtectParms::relax;
          else    lclParms.opts  &= ~XrdSecProtectParms::relax;
       if (isFRC) lclParms.opts  |=  XrdSecProtectParms::force;
          else    lclParms.opts  &= ~XrdSecProtectParms::force;
      }
   if (isRmt)
      {rmtParms.level = ltab[i].lvl;
       if (isRlx) rmtParms.opts  |=  XrdSecProtectParms::relax;
          else    rmtParms.opts  &= ~XrdSecProtectParms::relax;
       if (isFRC) rmtParms.opts  |=  XrdSecProtectParms::force;
          else    rmtParms.opts  &= ~XrdSecProtectParms::force;
      }
   return 0;
}
  
/******************************************************************************/
/*                                x p b i n d                                 */
/******************************************************************************/

/* Function: xpbind

   Purpose:  To parse the directive: protbind <thost> [none | [only] <plist>]

             <thost> is a templated host name (e.g., bronco*.slac.stanford.edu)
             <plist> are the protocols to be bound to the <thost>. A special
                     protocol, none, indicates that no token is to be passed.

   Output: 0 upon success or !0 upon failure.
*/

int XrdSecServer::xpbind(XrdOucStream &Config, XrdSysError &Eroute)
{
    EPNAME("xpbind")
    char *val, *thost;
    XrdSecProtBind *bnow;
    char sectoken[4096], *secbuff = sectoken;
    int isdflt = 0, only = 0, anyprot = 0, noprot = 0, phost = 0;
    int sectlen = sizeof(sectoken)-1;
    XrdSecPMask_t PMask = 0;
    *secbuff = '\0';

// Get the template host
//
   val = Config.GetWord();
   if (!val || !val[0])
      {Eroute.Emsg("Config","protbind host not specified"); return 1;}

// Verify that this host has not been bound before
//
   if ((isdflt = !strcmp("*", val))) bnow = bpDefault;
      else {bnow = bpFirst;
            while(bnow) if (!strcmp(bnow->thost, val)) break;
                           else bnow = bnow->next;
           }
   if (bnow) {Eroute.Emsg("Config","duplicate protbind definition - ", val);
              return 1;
             }
   thost = strdup(val);

// Now get each protocol to be used (there must be one).
//
   while((val = Config.GetWord()))
        {if (!strcmp(val, "none")) {noprot = 1; break;}
              if (!strcmp(val, "only")) {only = 1; Enforce = true;}
         else if (!strcmp(val, "host")) {phost = 1; anyprot = 1;}
         else if (!PManager.Find(val))
                 {Eroute.Emsg("Config","protbind", val,
                              "protocol not previously defined.");
                  return 1;
                 }
         else if (add2token(Eroute, val, &secbuff, sectlen, PMask))
                 {Eroute.Emsg("Config","Unable to bind protocols to",thost);
                  return 1;
                 } else anyprot = 1;
        }

// Verify that no conflicts arose
//
   if (val && (val = Config.GetWord()))
      {Eroute.Emsg("Config","conflicting protbind:", thost, val);
       return 1;
      }

// Make sure we have some protocols bound to this host
//
   if (!(anyprot || noprot))
      {Eroute.Emsg("Config","no protocols bound to", thost); return 1;}
   DEBUG("XrdSecConfig: Bound "<< thost<< " to "
         << (noprot ? "none" : (phost ? "host" : sectoken)));

// Issue warning if the host protocol was bound to this host but other
// protocols were also bound, making them rather useless.
//
   if (phost && *sectoken)
      {Eroute.Say("Config warning: 'protbind", thost,
                          "host' negates all other bound protocols.");
       *sectoken = '\0';
      }

// Translate "localhost" to our local hostname, if possible.
//
   if (!strcmp("localhost", thost))
      {XrdNetAddr myIPAddr(0);
       free(thost);
       thost = strdup(myIPAddr.Name("localhost"));
      }

// Create new bind object
//
   bnow = new XrdSecProtBind(thost,(noprot ? 0:sectoken),(only ? PMask:0));

// Push the entry onto our bindings
//
   if (isdflt) bpDefault = bnow;
      else {if (bpLast) bpLast->next = bnow;
               else bpFirst = bnow;
            bpLast = bnow;
           }

// All done
//
   return 0;
}

/******************************************************************************/
/*                                 x p r o t                                  */
/******************************************************************************/

/* Function: xprot

   Purpose:  To parse the directive: protocol [<path>] <pid> [ <opts> ]

             <path> is the absolute path where the protocol library resides
             <pid>  is the 1-to-8 character protocol id.
             <opts> are the associated protocol specific options such as:
                    noipcheck         - don't check ip address origin
                    keyfile <kfn>     - the key file associated with protocol
                    args <args>       - associated non-blank arguments
                    Additional arguments may be passed to the protocol using the
                    protargs directive. ALl protargs directives must appear
                    prior to the protocol directive for the given protocol.

   Output: 0 upon success or !0 upon failure.
*/

int XrdSecServer::xprot(XrdOucStream &Config, XrdSysError &Eroute)
{
    XrdSecProtParm *pp, myParms(&Eroute, "protocol");
    char *pap, *val, pid[XrdSecPROTOIDSIZE+1], *args = 0;
    char pathbuff[1024], *path = 0;
    int psize;
    XrdOucErrInfo erp;
    XrdSecPMask_t mymask = 0;

// Get the protocol id
//
   val = Config.GetWord();
   if (val && *val == '/')
      {strlcpy(pathbuff, val, sizeof(pathbuff)); path = pathbuff;
       val = Config.GetWord();
      }
   if (!val || !val[0])
      {Eroute.Emsg("Config","protocol id not specified"); return 1;}

// Verify that we don't have this protocol
//
   if (strlen(val) > XrdSecPROTOIDSIZE)
      {Eroute.Emsg("Config","protocol id too long - ", val); return 1;}

   if (PManager.Find(val))
      {Eroute.Say("Config warning: protocol ",val," previously defined.");
       strcpy(pid, val);
       return add2token(Eroute, pid, &STBuff, STBlen, mymask);
      }

// Add this protocol to the list of protocols that have been defined
//
   char pName[XrdSecPROTOIDSIZE+2];
   *pName = ':';
   strcpy(pName+1, val);
   if (!pidList) pidList = strdup(pName);
      else {std::string pids = pidList;
            pids.append(pName);
            free(pidList);
            pidList = strdup(pids.c_str());
           }

// The builtin host protocol does not accept any parameters. Additionally, the
// host protocol negates any other protocols we may have in the default set.
//
   if (!strcmp("host", val))
      {if (Config.GetWord())
          {Eroute.Emsg("Config", "Builtin host protocol does not accept parms.");
           return 1;
          }
       implauth = true;
       return 0;
      }

// Grab additional parameters that we here and that we have accumulated
//
   strcpy(pid, val);
   while((args = Config.GetWord())) if (!myParms.Cat(args)) return 1;
   if ((pp = myParms.Find(pid, 1)))
      {if ((*myParms.Result(psize) && !myParms.Insert('\n'))
       ||  !myParms.Cat(pp->Result(psize))) return 1;
          else delete pp;
      }

// Load this protocol
//
   pap = myParms.Result(psize);
   if (!PManager.Load(&erp, 's', pid, (psize ? pap : 0), path))
      {if (*(erp.getErrText())) Eroute.Say(erp.getErrText());
       Eroute.Say("Config Failed to load ", pid, " authentication protocol!");
       return 1;
      }

// Add this protocol to the default security token
//
   return add2token(Eroute, pid, &STBuff, STBlen, mymask);
}
  
/******************************************************************************/
/*                                x p p a r m                                 */
/******************************************************************************/

/* Function: xpparm

   Purpose:  To parse the directive: protparm <prot> <args>

             <prot>  is the name of the protocol to which these args apply.
             <args>  are the protocol specific parameters. The remaing tokens
                     on the line will be passed to the protocol at during
                     protocol initialization. Each such line is separated by
                     a new line character.

   Output: 0 upon success or !0 upon failure.
*/

int XrdSecServer::xpparm(XrdOucStream &Config, XrdSysError &Eroute)
{
    XrdSecProtParm *pp;
    char *val, pid[XrdSecPROTOIDSIZE+1];

// Get the protocol name
//
   val = Config.GetWord();
   if (!val || !val[0])
      {Eroute.Emsg("Config","protparm protocol not specified"); return 1;}

// The builtin host protocol does not accept any parameters
//
   if (!strcmp("host", val))
      {Eroute.Emsg("Config", "Builtin host protocol does not accept protparms.");
       return 1;
      }

// Verify that we don't have this protocol
//
   if (strlen(val) > XrdSecPROTOIDSIZE)
      {Eroute.Emsg("Config","protocol id too long - ", val); return 1;}

   if (PManager.Find(val))
      {Eroute.Emsg("Config warning: protparm protocol ",val," already defined.");
       return 0;
      }

   strcpy(pid, val);

// Make sure we have at least one parameter here
//
   if (!(val = Config.GetWord()))
      {Eroute.Emsg("Config","protparm", pid, "parameter not specified");
       return 1;
      }

// Try to find a previous incarnation of this parm
//
   if ((pp = XrdSecProtParm::Find(pid))) {if (!pp->Insert('\n')) return 1;}
      else {pp = new XrdSecProtParm(&Eroute, "protparm");
            pp->setProt(pid);
            pp->Add();
           }

// Grab the options for the protocol. They are pretty much opaque to us here
//
  do {if (!pp->Cat(val)) return 1;} while((val = Config.GetWord()));
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

int XrdSecServer::xtrace(XrdOucStream &Config, XrdSysError &Eroute)
{
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",            TRACE_ALL},
        {"debug",          TRACE_Debug},
        {"auth",           TRACE_Authen},
        {"authentication", TRACE_Authen}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);
    char *val;

    val = Config.GetWord();
    if (!val || !val[0])
       {Eroute.Emsg("Config", "trace option not specified"); return 1;}
    while (val && val[0])
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
                      Eroute.Say("Config warning: ignoring invalid trace option '", val, "'.");
                  }
          val = Config.GetWord();
         }

    SecTrace->What = (SecTrace->What & ~TRACE_Authenxx) | trval;

// Propogate the debug option
//
#ifndef NODEBUG
   if (QTRACE(Debug)) PManager.setDebug(1);
      else            PManager.setDebug(0);
#endif
    return 0;
}

/******************************************************************************/
/*                         M i s c e l l a n e o u s                          */
/******************************************************************************/
/******************************************************************************/
/*                             a d d 2 t o k e n                              */
/******************************************************************************/

int XrdSecServer::add2token(XrdSysError &Eroute, char *pid,
                            char **tokbuff, int &toklen, XrdSecPMask_t &pmask)
{
    int i;
    char *pargs;
    XrdSecPMask_t protnum;

// Find the protocol argument string
//
   if (!(protnum = PManager.Find(pid, &pargs)))
      {Eroute.Emsg("Config","Protocol",pid,"not found after being added!");
       return 1;
      }

// Make sure we have enough room to add
//
   i = 4+strlen(pid)+strlen(pargs);
   if (i >= toklen)
      {Eroute.Emsg("Config","Protocol",pid,"parms exceed overall maximum!");
       return 1;
      }

// Insert protocol specification (we already checked for an overflow)
//
   i = sprintf(*tokbuff, "&P=%s%s%s", pid, (*pargs ? "," : ""), pargs);
   toklen   -= i;
   *tokbuff += i;
   pmask    |= protnum;
   return 0;
}
  
/******************************************************************************/
/*                     P r o t B i n d _ C o m p l e t e                      */
/******************************************************************************/
  
int XrdSecServer::ProtBind_Complete(XrdSysError &Eroute)
{
    EPNAME("ProtBind_Complete")
    XrdOucErrInfo erp;

// Check if we have a default token, create one otherwise
//
   if (!bpDefault)
      {if (!*SToken) {Eroute.Say("Config warning: No protocols defined; "
                                  "only host authentication available.");
                      implauth = true;
                     }
          else if (implauth)
                  {Eroute.Say("Config warning: enabled builtin host "
                   "protocol negates default use of any other protocols.");
                   *SToken = '\0';
                  }
       bpDefault = new XrdSecProtBind(strdup("*"), SToken);
       DEBUG("Default sectoken built: '" <<SToken <<"'");
      }

// Add the host protocol to the set at this point to allow clients to
// actually give use "host" as a protocol id if it's allowed. We do this so
// that the right error message is generated. Otherwise, it ignored.
//
   if (implauth && !PManager.Load(&erp, 's', "host", 0, 0))
      {Eroute.Emsg("Config", erp.getErrText()); return 1;}

// Free up the constructed default sectoken
//
   free(SToken); SToken = STBuff = 0; STBlen = 0;
   return 0;
}
 
/******************************************************************************/
/*                      X r d S e c g e t S e r v i c e                       */
/******************************************************************************/

extern "C"
{
XrdSecService *XrdSecgetService(XrdSysLogger *lp, const char *cfn)
{
   XrdSecServer *SecServer = new XrdSecServer(lp);

// Configure the server object
//
   if (SecServer->Configure(cfn)) return 0;

// Return the server object
//
   return (XrdSecService *)SecServer;
}
}
