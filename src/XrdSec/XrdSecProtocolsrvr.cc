/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l s r v r . c c                  */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdSecProtocolsrvrCVSID = "$Id$";

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucLogger.hh"

#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecProtocolsrvr.hh"
#include "XrdSec/XrdSecTrace.hh"

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
       tpfxlen = tsfxlen = 0;
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

   while(!bp->Match(hname)) bp = bp->next;

   return bp;
}
  
/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/
  
int XrdSecProtBind::Match(const char *hname)
{
    int i;

// If an exact match wanted, return the reult
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
/*                          X r d S e c S e r v e r                           */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSecProtocolsrvr::XrdSecProtocolsrvr(XrdOucLogger *lp) : eDest(0, "sec_")
{

// Set default values
//
   eDest.logger(lp);
   bpFirst     = 0;
   bpLast      = 0;
   STBuff      = (char *)malloc(4096);    // STBuff and STBlen must match!
  *STBuff      = '\0';
   STBlen      = 4096;
   SecTrace    = new XrdOucTrace(&eDest);
   if (getenv("XRDDEBUG") || getenv("XrdSecDEBUG")) SecTrace->What = TRACE_ALL;
   Enforce     = 0;
   implauth    = 1;
}
  
/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/
  
int  XrdSecProtocolsrvr::Authenticate(XrdSecCredentials  *cred,     // In
                                XrdSecParameters  **parms,    // Out
                                XrdSecClientName   &client,   // Out
                                XrdOucErrInfo      *einfo)    // Out
{
   XrdSecProtBind *bp;
   XrdSecProtocol *pp;
   XrdSecPMask_t pnum;
   char *msgv[8];

// If there are no credentials here, complain
//
   if (cred->size < 1 || !(cred->buffer))
      {einfo->setErrInfo(EACCES,(char *)"No authentication credentials supplied.");
       return -1;
      }

// The protocol manager finds the protocol for us. We have preloaded all of
// the protocol we will use, so we better find something that matches.
//
   if (pp = PManager.Find((const char *)cred->buffer, (char **)0, &pnum))
      {if (Enforce)
          {if (!bpFirst || !(bp = bpFirst->Find(client.host))
           ||  !(bp->ValidProts & pnum))
              {msgv[0] = client.host;
               msgv[1] = (char *)" not allowed to authenticate using ";
               msgv[2] = cred->buffer;
               msgv[3] = (char *)" protocol.";
               einfo->setErrInfo(EACCES, msgv, 4);
               return -1;
              }
          }
       return pp->Authenticate(cred, parms, client, einfo);
      }

// Indicate that we don't support whatever the client sent us
//
   msgv[0] = cred->buffer;
   msgv[1] = (char *)" security protocol is not supported.";
   einfo->setErrInfo(EUNATCH, msgv, 2);
   return -1;
}

/******************************************************************************/
/*                              g e t P a r m s                               */
/******************************************************************************/
  
const char *XrdSecProtocolsrvr::getParms(int &size, const char *hname)
{
   const char *epname = "getSecToken";
   XrdSecProtBind *bp;

// Try to find a specific token binding for a host or return default binding
//
   if (!hname) bp = 0;
      else if (bp = bpFirst) while(!bp->Match(hname)) bp = bp->next;

// If we have a binding, return that else return the default
//
   if (!bp) bp = bpLast;
   if (bp->SecToken.buffer) 
      {DEBUG(hname <<" sectoken=" <<bp->SecToken.buffer);
       size = bp->SecToken.size;
       return bp->SecToken.buffer;
      }

   DEBUG(hname <<" sectoken=''");
   size = 0;
   return (const char *)0;
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

#define SEC_Prefix    "sec."
#define SEC_PrefLen   sizeof(SEC_Prefix)-1
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdSecProtocolsrvr::Configure(const char *cfn)
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
{
   int  NoGo;
   char *var;

// Print warm-up message
//
   eDest.Emsg("Config","Authentication system initialization started.");

// Perform initialization
//
   NoGo = ConfigFile(cfn);

// All done
//
   var = (NoGo > 0 ? (char *)"failed." : (char *)"completed.");
   eDest.Emsg("Config", "Authentication system initialization", var);
   return (NoGo > 0);
}

/******************************************************************************/
/*                            C o n f i g F i l e                             */
/******************************************************************************/
  
int XrdSecProtocolsrvr::ConfigFile(const char *ConfigFN)
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   1 - Initialization failed.
            0 - Initialization succeeded.
*/
{
   char *var;
   int  cfgFD, retc, NoGo = 0, recs = 0;
   XrdOucStream Config(&eDest);

// If there is no config file, return with the defaults sets.
//
   if (!ConfigFN || !*ConfigFN)
     {eDest.Emsg("Config", "Authentication configuration file not specified.");
      return 1;
     }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {eDest.Emsg("Config", errno, "opening config file", (char *)ConfigFN);
       return 1;
      }

// Now start reading records until eof.
//
   Config.Attach(cfgFD); Config.Tabs(0);
   while( var = Config.GetFirstWord())
        {if (!strncmp(var, SEC_Prefix, SEC_PrefLen))
            {var += SEC_PrefLen; recs++;
             NoGo |= ConfigXeq(var, Config, eDest);
            }
        }

// Now check if any errors occured during file i/o
//
   if (retc = Config.LastError() )
      NoGo = eDest.Emsg("Config",-retc,"reading config file", (char *)ConfigFN);
      else {char buff[12];
            snprintf(buff, sizeof(buff), "%d", recs);
            eDest.Emsg("Config", buff,
                (char *)" authentication directives processed in ", (char *)ConfigFN);
           }
   Config.Close();

// Determine whether we should initialize security
//
   if (NoGo || ProtBind_Complete(eDest) ) NoGo = 1;

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
  
int XrdSecProtocolsrvr::ConfigXeq(char *var, XrdOucStream &Config, XrdOucError &Eroute)
{

    // Fan out based on the variable
    //
    TS_Xeq("authmode",      xpamode);
    TS_Xeq("protbind",      xpbind);
    TS_Xeq("protocol",      xprot);
    TS_Xeq("trace",         xtrace);

    // No match found, complain.
    //
    Eroute.Emsg("Config", "unknown directive", var, (char *)"ignored.");
    return 0;
}
  
/******************************************************************************/
/*                               x p a m o d e                                */
/******************************************************************************/

/* Function: xpamode

   Purpose:  To parse the directive: authmode {strict | relaxed}

             strict  clients must always go through authentication.
             relaxed clients are allowed to skip authentication and implicitly
                     use host-based identification

   Output: 0 upon success or !0 upon failure.
*/

int XrdSecProtocolsrvr::xpamode(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    int relax = 1;

// Get the argument
//
   val = Config.GetWord();
   if (!val || !val[0])
      {Eroute.Emsg("Config","authmode option not specified"); return 1;}

   if (!strcmp(val, "strict")) relax = 0;
      else if (!strcmp(val, "relaxed")) relax = 1;
           {Eroute.Emsg("Config","Invalid authmode option -", val); return 1;}
   implauth = relax;
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

int XrdSecProtocolsrvr::xpbind(XrdOucStream &Config, XrdOucError &Eroute)
{
    const char *epname = "xpbind";
    char *val, *thost;
    struct XrdSecProtBind *bnow;
    char sectoken[4096], *secbuff = sectoken;
    int  i, only = 0, anyprot = 0, noprot = 0;
    int sectlen = sizeof(sectoken)-1;
    unsigned long PMask = 0;
    *secbuff = '\0';

// Get the template host
//
   val = Config.GetWord();
   if (!val || !val[0])
      {Eroute.Emsg("Config","protbind host not specified"); return 1;}

// Verify that this host has not been bound before
//
   bnow = bpFirst;
   while(bnow)
        {if (!strcmp(bnow->thost, val))
            {Eroute.Emsg("Config","duplicate protbind definition - ", val);
             return 1;
            }
         bnow = bnow->next;
        }
   thost = strdup(val);

// Now get each protocol to be used (there must be one).
//
   while(val = Config.GetWord())
        {if (!strcmp(val, "none")) {noprot = 1; break;}
         if (!strcmp(val, "only")) {only = 1; Enforce = 1;}
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

// Create new bind object
//
   bnow = new XrdSecProtBind(thost,(noprot ? 0:sectoken),(only ? PMask:0));

// Push the entry onto our bindings
//
   if (strcmp("*", thost)) 
      {bnow->next = bpFirst; bpFirst = bnow;
       if (!bpLast) bpLast = bnow;
      } else {
       if (bpLast) bpLast->next = bnow;
          else bpFirst = bnow;
       bpLast = bnow;
      }

// All done
//
   DEBUG("XrdSecConfig: Bound "<< thost<< " to "<< (noprot ? "none" : sectoken));
   return 0;
}

/******************************************************************************/
/*                                 x p r o t                                  */
/******************************************************************************/

/* Function: xprot

   Purpose:  To parse the directive: protocol [<path>] <pid> [ <opts> ]

             <path> is the absolute path where the protocol library resides
             <pid>  is the 1-to-4 character protocol id.
             <opts> are the associated protocol specific options such as:
                    noipcheck         - don't check ip address origin
                    keyfile <kfn>     - the key file associated with protocol
                    args <args>       - associated non-blank arguments

/afs/slac/package/xrd/package/testxrd/bbxroot
   Output: 0 upon success or !0 upon failure.
*/

int XrdSecProtocolsrvr::xprot(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *vp, *val, pid[16], *args = 0;
    char pathbuff[1024], pargs[4096], *pap = 0, *path = 0;
    int alen, pleft = sizeof(pargs)-1;
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
   if (strlen(val) >= XrdSecPROTOIDSIZE)
      {Eroute.Emsg("Config","protocol id too long - ", val); return 1;}
   if (PManager.Find((const char *)val))
      {Eroute.Emsg("Config","protocol",val,(char *)"previously defined."); return 1;}
   strlcpy(pid, val, sizeof(pid));

// Grab the options for the protocol. They are pretty much opaque to us here
//
   while(args = Config.GetWord())
        {alen = strlen(args);
         if (alen+1 > pleft)
            {Eroute.Emsg("Config","Protocol",pid,(char *)"argument string too long");
             return 1;
            }
         if (pap) {*pap = ' '; pap++;}
            else pap = pargs;
         strcpy(pap, args); pap += alen;
        }

// Load this protocol
//
   if (!PManager.Load(&erp, (const char *)path,
                            (const char *)pid,
                            (pap ? (const char *)pargs : (const char *)0), 's'))
      {Eroute.Emsg("Config", erp.getErrText()); return 1;}

// Add this protocol to the default security token
//
   return add2token(Eroute, pid, &STBuff, STBlen, mymask);
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

int XrdSecProtocolsrvr::xtrace(XrdOucStream &Config, XrdOucError &Eroute)
{
    static struct traceopts { char * opname; int opval;} tropts[] =
       {
       (char *)"all",            TRACE_ALL,
       (char *)"debug",          TRACE_Debug,
       (char *)"auth",           TRACE_Authen,
       (char *)"authentication", TRACE_Authen,
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);
    char *val;

    val = Config.GetWord();
    if (!val || !val[0])
       {Eroute.Emsg("Config", "trace option not specified"); return 1;}
    while (val && val[0])
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
                      {Eroute.Emsg("Config", "invalid trace option -", val);
                       return 1;
                      }
                  }
          val = Config.GetWord();
         }

    SecTrace->What = (SecTrace->What & ~TRACE_Authenxx) | trval;

// Propogate the debug option
//
   if (QTRACE(Debug)) PManager.setDebug(1);
      else            PManager.setDebug(0);
    return 0;
}

/******************************************************************************/
/*                         M i s c e l l a n e o u s                          */
/******************************************************************************/
/******************************************************************************/
/*                             a d d 2 t o k e n                              */
/******************************************************************************/

int XrdSecProtocolsrvr::add2token(XrdOucError &Eroute, char *pid,
                            char **tokbuff, int &toklen, XrdSecPMask_t &pmask)
{
    int i, j=0;
    char *pargs, buff[1024];
    unsigned long protnum;

// Find the protocol argument string
//
   if (!PManager.Find(pid, &pargs, &protnum))
      {Eroute.Emsg("Config","Protocol",pid,(char *)"not found after being added!");
       return 1;
      }

// Make sure we have enough room to add
//
   i = 4+strlen(pid)+strlen(pargs);
   if (i >= sizeof(buff) || i >= toklen)
      {Eroute.Emsg("Config","Protocol",pid,(char *)"parms exceed overall maximum!");
       return 1;
      }

// Insert protocol specification (we already checked for an overflow)
//
   toklen -= sprintf(*tokbuff, "&P=%s,%s", pid, pargs);
   pmask |= protnum;
   return 0;
}
  
/******************************************************************************/
/*                     P r o t B i n d _ C o m p l e t e                      */
/******************************************************************************/
  
int XrdSecProtocolsrvr::ProtBind_Complete(XrdOucError &Eroute)
{
    const char *epname = "ProtBind_Complete";
    XrdSecProtBind *bnow;
    char *sectp;
    int NoGo;

// Check if we have a default token, create one otherwise
//
   if (!bpLast || strcmp("*", bpLast->thost))
      {if (*STBuff) sectp = STBuff;
          else {Eroute.Emsg("Config",
                "No protocols defined; only host authentication available.");
                sectp = 0;
               }
       bnow = new XrdSecProtBind(strdup("*"), sectp);
       if (bpLast) bpLast->next = bnow;
          else bpFirst = bnow;
       bpLast = bnow;
       DEBUG("Default sectoken built:" <<(sectp ? sectp : (char *)"''"));
      }

// Free up the construct default sectoken
//
   free(STBuff); STBuff = 0; STBlen = 0;
   return 0;
}
 
/******************************************************************************/
/*              X r d S e c P r o t o c o l s r v r O b j e c t               */
/******************************************************************************/

extern "C"
{
XrdSecProtocol *XrdSecProtocolsrvrObject(XrdOucLogger *lp, const char *cfn)
{
   int NoGo = 1;
   XrdSecProtocolsrvr *sobj;

// Create a server object
//
   if (sobj = new XrdSecProtocolsrvr(lp)) NoGo = sobj->Configure(cfn);

// Check if object was successfully create
//
   if (NoGo && sobj) {delete sobj; sobj = 0;}

// Return result; a null indicates failure.
//
   return (XrdSecProtocol *)sobj;
}
}
