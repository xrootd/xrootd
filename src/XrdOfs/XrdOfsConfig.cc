/******************************************************************************/
/*                                                                            */
/*                       X r d O f s C o n f i g . c c                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC03-76-SFO0515 with the Deprtment of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOfsConfigCVSID = "$Id$";

/*
   The routines in this file handle ofs() initialization. They get the
   configuration values either from configuration file or XrdOfsconfig.h (in that
   order of precedence).

   These routines are thread-safe if compiled with:
   AIX: -D_THREAD_SAFE
   SUN: -D_REENTRANT
*/
  
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/param.h>

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsConfig.hh"
#include "XrdOfs/XrdOfsTrace.hh"

#include "XrdNet/XrdNetDNS.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTrace.hh"

#include "XrdOdc/XrdOdcFinder.hh"
#include "XrdAcc/XrdAccAuthorize.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

extern XrdOucTrace OfsTrace;

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)   if (!strcmp(x,var)) return m(Config,Eroute);

#define TS_Str(x,m)   if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_PList(x,m)  if (!strcmp(x,var)) \
                          {m.Insert(new XrdOucPList(val,1)); return 0;}

#define TS_Chr(x,m)   if (!strcmp(x,var)) {m = val[0]; return 0;}

#define TS_Bit(x,m,v) if (!strcmp(x,var)) {m |= v; return 0;}

#define Max(x,y) (x > y ? x : y)

#define OFS_Prefix    "ofs."
#define OFS_PrefLen   sizeof(OFS_Prefix)-1

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOfs::Configure(XrdOucError &Eroute) {
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   char *val, *var;
   int  i, cfgFD, retc, NoGo = 0;
   XrdOucStream Config(&Eroute);

// Print warm-up message
//
   Eroute.Emsg("Config", "File system initialization started.");

// Preset all variables with common defaults
//
   Options            = 0;
   if (getenv("XRDDEBUG")) OfsTrace.What = TRACE_MOST | TRACE_debug;

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
           while((var = Config.GetFirstWord()))
                {if (!strncmp(var, OFS_Prefix, OFS_PrefLen))
                    {var += OFS_PrefLen;
                     NoGo |= ConfigXeq(var, Config, Eroute);
                    }
                }

           // Now check if any errors occured during file i/o
           //
           if ((retc = Config.LastError()))
           NoGo = Eroute.Emsg("Config", -retc, "read config file",
                              ConfigFN);
           Config.Close();
          }

// Determine whether we should initialize security
//
   if (Options & XrdOfsAUTHORIZE)
      NoGo |= !(Authorization=XrdAccAuthorizeObject(Eroute.logger(),ConfigFN));

// Check if redirection wanted
//
   if ((val = getenv("XRDREDIRECT")))
      {if (*val == 'R') {i = XrdOfsREDIRRMT; val = (char *)"remote";}
          else if (*val == 'B') {i = XrdOfsREDIRLCL; val = (char *)"local";}
               else i = 0;
      } else i = 0;
   if (getenv("XRDRETARGET")) i |= XrdOfsREDIRTRG;
   if (getenv("XRDREDPROXY")) i |= XrdOfsREDIROXY;
   if (i)
      {if ((Options & (XrdOfsREDIRLCL | XrdOfsREDIRRMT))&& !(i & Options))
          Eroute.Emsg("Config", "Command line redirect options override config "
                                "file;  redirect", val, (char *)"in effect.");
       Options &= ~(XrdOfsREDIRLCL | XrdOfsREDIRRMT);
       Options |= i;
      }
   if (Options & (XrdOfsREDIRECT | XrdOfsREDIROXY)) NoGo |= ConfigRedir(Eroute);

// Turn off forwarding if we are not a remote redirector
//
   if (Options & XrdOfsFWDALL && !(Options & XrdOfsREDIRRMT))
      {Eroute.Emsg("Config", "Forwarding turned off; not a remote redirector");
       Options &= ~(XrdOfsFWDALL);
      }

// All done
//
   val = (NoGo ? (char *)"failed." : (char *)"completed.");
   Eroute.Emsg("Config", "File system initialization",val);
   return NoGo;
}

/******************************************************************************/
/*                        C o n f i g _ D i s p l a y                         */
/******************************************************************************/

#define setBuff(x,y) {strcpy(bp, x); bp += y;}
  
void XrdOfs::Config_Display(XrdOucError &Eroute)
{
     char buff[8192], fwbuff[256], *bp, *cloc, *rdt, *rdu, *rdp;

          if (Options & XrdOfsREDIRLCL) rdt = (char *)"ofs.redirect local\n";
     else if (Options & XrdOfsREDIRRMT) rdt = (char *)"ofs.redirect remote\n";
     else                               rdt = (char *)"";
          if (Options & XrdOfsREDIROXY) rdp = (char *)"ofs.redirect proxy\n";
     else                               rdp = (char *)"";
          if (Options & XrdOfsREDIRTRG) rdu = (char *)"ofs.redirect target\n";
     else                               rdu = (char *)"";

     if (!(Options &  XrdOfsFWDALL)) fwbuff[0] = '\0';
        else {bp = fwbuff;
              setBuff("ofs.forward", 11);
              if (Options & XrdOfsFWDCHMOD) setBuff(" chmod", 6);
              if (Options & XrdOfsFWDMKDIR) setBuff(" mkdir", 6);
              if (Options & XrdOfsFWDMV   ) setBuff(" mv"   , 3);
              if (Options & XrdOfsFWDRM   ) setBuff(" rm"   , 3);
              if (Options & XrdOfsFWDRMDIR) setBuff(" rmdir", 6);
              setBuff("\n", 1);
             }

     if (!ConfigFN || !ConfigFN[0]) cloc = (char *)"Default";
        else cloc = ConfigFN;
     snprintf(buff, sizeof(buff), "%s ofs configuration:\n"
                                  "%s"
                                  "%s"
                                  "%s"
                                  "%s"
                                  "%s"
                                  "ofs.fdscan     %d %d %d\n"
                                  "%s"
                                  "ofs.maxdelay   %d\n"
                                  "ofs.trace      %x",
              cloc, (Options * XrdOfsAUTHORIZE ? "ofs.authorize\n" : ""),
              rdp, rdt, rdu, 
              (Options & XrdOfsFDNOSHARE ? "ofs.fdnoshare\n" : ""),
              FDOpenMax, FDMinIdle, FDMaxIdle, fwbuff, MaxDelay,
              OfsTrace.What);
     Eroute.Say(buff);
     List_VPlist((char *)"ofs.validpath  ", VPlist, Eroute);
}

/******************************************************************************/
/*                     p r i v a t e   f u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                           C o n f i g R e d i r                            */
/******************************************************************************/
  
int XrdOfs::ConfigRedir(XrdOucError &Eroute) 
{
   EPNAME("ConfigRedir")
#ifndef NODEBUG
   const char *tident = "";
#endif
   int i, port, isprime = 1;
   struct sockaddr_in name;
   XrdOdcFinderLCL *myFinder;
   SOCKLEN_t nlen = sizeof(name);

// For remote redirection, we simply do a standard config
//
   if (Options & XrdOfsREDIRRMT)
      {Finder=(XrdOdcFinder *)new XrdOdcFinderRMT(Eroute.logger());
       if (!Finder->Configure(ConfigFN))
          {delete Finder; Finder = 0; return 1;}
      }

// For proxy  redirection, we simply do a standard config
//
   if (Options & XrdOfsREDIROXY)
      {Google=(XrdOdcFinder *)new XrdOdcFinderRMT(Eroute.logger(), 1);
       if (!Google->Configure(ConfigFN))
          {delete Google; Google = 0; return 1;}
      }

// For local or target redirection we need to know the port number
//
//PE   if (Options & (XrdOfsREDIRLCL | XrdOfsREDIRTRG))
//PE      {i = 3;
//PE       while(i < 256 && getsockname(i,(struct sockaddr *)&name, &nlen) < 0) i++;
//PE       if (i >= 256)
//PE          {Eroute.Emsg("Config", "Unable to determine server's port number.");
//PE           return 1;
//PE          }
//PE       port = ntohs(name.sin_port);
//PE       DEBUG("Dynamic port identification... port number=" <<port);
//PE      } else port = -1;

   if (Options & XrdOfsREDIRTRG)
      {char *pp;
       if (!(pp=getenv("XRDPORT")) || !(port=strtol(pp, (char **)NULL, 10)))
          {Eroute.Emsg("Config", "Unable to determine server's port number.");
           return 1;
          }
       Balancer = new XrdOdcFinderTRG(Eroute.logger(),
                                     (Options & XrdOfsREDIRRMT), port);
       if (!Balancer->Configure(ConfigFN)) 
          {delete Balancer; Balancer = 0; return 1;}
      }

// Create a local finder, if need be
//
// if (Options & XrdOfsREDIRLCL)
//    {myFinder = new XrdOdcFinderLCL(Eroute.logger(), port);
//     if (!myFinder->Configure(ConfigFN)) {delete myFinder; return 1;}
//
       // We are either a participator or a redirector, decide which is which
       //
//     if (myFinder->isRedirector()) Finder = (XrdOdcFinder *)myFinder;
//        else isprime = 0;
//     Reporter = myFinder;
//    }

// Create a target finder
//
// if (Options & XrdOfsREDIRTRG)
//    {Balancer = new XrdOdcFinderTRG(Eroute.logger(), isprime, port);
//     if (!Balancer->Configure(ConfigFN))
//        {delete Balancer; Balancer = 0; return 1;}
//    }

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/
  
int XrdOfs::ConfigXeq(char *var, XrdOucStream &Config,
                                 XrdOucError &Eroute)
{
    char *val;

    // Now assign the appropriate global variable
    //
    TS_Bit("authorize",     Options, XrdOfsAUTHORIZE);
    TS_Bit("fdnoshare",     Options, XrdOfsFDNOSHARE);
    TS_Xeq("fdscan",        xfdscan);
    TS_Xeq("forward",       xforward);
    TS_Xeq("locktry",       xlocktry);
    TS_Xeq("maxdelay",      xmaxd);
    TS_Xeq("redirect",      xred);
    TS_Xeq("trace",         xtrace);

    // Get the actual value for simple directives
    //
    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "value not specified for", var); return 1;}

    // Process simple directives
    //
    TS_PList("validpath",   VPlist);

    // No match found, complain.
    //
    Eroute.Emsg("Config", "Warning, unknown directive", var);
    return 0;
}

/******************************************************************************/
/*                                  i s M e                                   */
/******************************************************************************/
  
int XrdOfs::isMe(XrdOucError &eDest, const char *item, char *hval)
{
    struct sockaddr InetAddr[16];
    char *mval;
    int i, j, k, retc;

    if (!strcmp(hval, HostName)) return 1;

    if ((mval = index((const char *)hval, (int)'*')))
       {*mval = '\0'; mval++; 
        k = strlen(HostName); j = strlen(mval); i = strlen(hval);
        if ((i+j) > k
        || strncmp((const char *)HostName,      (const char *)hval,i)
        || strncmp((const char *)(HostName+k-j),(const char *)mval,j)) return 0;
        return 1;
       }

    i = strlen(hval);
    if (hval[i-1] != '+') i = 0;
        else {hval[i-1] = '\0';
              if (!(i = XrdNetDNS::getHostAddr(hval, InetAddr, 16)))
                 {eDest.Emsg("Config",item, hval, (char *)"not found");
                  return 0;
                 }
             }

    while(i--)
         {mval = XrdNetDNS::getHostName(InetAddr[i]);
          retc = !strcmp(mval,HostName) || !strcmp(mval,HostPref);
          free(mval);
          if (retc) return 1;
         }
    return 0;
}

/******************************************************************************/
/*                               x f d s c a n                                */
/******************************************************************************/

/* Function: xfdscan

   Purpose:  To parse the directive: fdscan <numopen> <minidle> <maxidle>

             <numopen> number of fd's that must be open for scan to commence.
             <minidle> minimum number of seconds between scans.
             <maxidle> maximum number of seconds a file can be idle before
                       it is closed.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xfdscan(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    int numf, minidle, maxidle;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config","fdscan numfiles value not specified");return 1;}
      if (XrdOuca2x::a2i(Eroute, "fdscan numfiles", val, &numf, 0)) return 1;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config","fdscan minidle value not specified"); return 1;}
      if (XrdOuca2x::a2tm(Eroute, "fdscan minidle",val, &minidle,0)) return 1;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config","fdscan maxidle value not specified"); return 1;}
      if (XrdOuca2x::a2tm(Eroute,"fdscan maxidle", val, &maxidle, minidle))
         return 1;

      FDOpenMax = numf;
      FDMinIdle = minidle;
      FDMaxIdle = maxidle;
      return 0;
}

/******************************************************************************/
/*                              x f o r w a r d                               */
/******************************************************************************/
  
/* Function: xforward

   Purpose:  To parse the directive: forward <metaops>

             <metaops> list of meta-file operations to forward to manager

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xforward(XrdOucStream &Config, XrdOucError &Eroute)
{
    static struct fwdopts {const char *opname; int opval;} fwopts[] =
       {
        {"all",      XrdOfsFWDALL},
        {"chmod",    XrdOfsFWDCHMOD},
        {"mkdir",    XrdOfsFWDMKDIR},
        {"mv",       XrdOfsFWDMV},
        {"rm",       XrdOfsFWDREMOVE},
        {"rmdir",    XrdOfsFWDREMOVE},
        {"remove",   XrdOfsFWDREMOVE}
       };
    int i, neg, fwval = 0, numopts = sizeof(fwopts)/sizeof(struct fwdopts);
    char *val;

    if (!(val = Config.GetWord()))
       {Eroute.Emsg("Config", "foward option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) fwval = 0;
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, fwopts[i].opname))
                           {if (neg) fwval &= ~fwopts[i].opval;
                               else  fwval |=  fwopts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      Eroute.Emsg("Config", "invalid foward option -", val);
                  }
          val = Config.GetWord();
         }
    Options &= ~XrdOfsFWDALL;
    Options |= fwval;

// All done
//
   return 0;
}

/******************************************************************************/
/*                              x l o c k t r y                               */
/******************************************************************************/
  
/* Function: locktry

   Purpose:  To parse the directive: locktry <times> <wait>

             <times>   number of times to try to get a lock.
             <wait>    number of milliseconds to wait between tries.

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xlocktry(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val;
    int numt, mswt;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config","locktry count not specified"); return 1;}
      if (XrdOuca2x::a2i(Eroute, "locktry count", val, &numt, 0)) return 1;

      if (!(val = Config.GetWord()))
         {Eroute.Emsg("Config","locktry wait interval not specified");return 1;}
      if (XrdOuca2x::a2i(Eroute, "locktry wait",val, &mswt,0)) return 1;

      LockTries = numt;
      LockWait  = mswt;
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

int XrdOfs::xmaxd(XrdOucStream &Config, XrdOucError &Eroute)
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
/*                                  x r e d                                   */
/******************************************************************************/

/* Function: xred

   Purpose:  Parse directive: redirect [local|proxy|remote|target] 
                                              [if] [ <hosts> ]

   Args:     local    - enables this server for dynamic port balancing
             proxy    - enables this server for proxy   load balancing
             remote   - enables this server for dynamic load balancing
             target   - enables this server as a redirection target
             hosts    - list of hostnames for which this directive applies

   Output: 0 upon success or !0 upon failure.
*/

int XrdOfs::xred(XrdOucStream &Config, XrdOucError &Eroute)
{
    char *val, *mode = (char *)"remote";
    int ropt = 0, topt = 0;

    if ((val = Config.GetWord()))
       {     if (!strcmp("local", val)) {ropt = XrdOfsREDIRLCL;
                                         mode = (char *)"local";
                                        }
        else if (!strcmp("proxy",  val)) {ropt = XrdOfsREDIROXY;
                                          mode = (char *)"proxy";
                                         }
        else if (!strcmp("remote", val)) ropt = XrdOfsREDIRRMT;
        else if (!strcmp("target", val)) {topt = XrdOfsREDIRTRG;
                                          mode = (char *)"target";
                                         }
       }

    if (!ropt && !topt) ropt = XrdOfsREDIRRMT;
       else if (val) val = Config.GetWord();

    if (val) {if (!strcmp("if", val) && !(val = Config.GetWord()))
                 {Eroute.Emsg("Config",
                              "Host name missing after 'if' in redirect", mode);
                  return 1;
                 }
              while(val && !isMe(Eroute, "redirect host", val)) 
                    val = Config.GetWord();
              if (!val) {Eroute.Emsg("Config","redirect", mode,
                                    (char *)"ignored; not applicable host.");
                         return 0;
                        }
             }

    if (ropt)
       if (ropt & XrdOfsREDIRLCL)
          Options = (Options & ~(XrdOfsREDIROXY | XrdOfsREDIRRMT)) | ropt;
          else if (ropt) Options = (Options & ~(XrdOfsREDIRLCL))   | ropt;
    Options |= topt;
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

int XrdOfs::xtrace(XrdOucStream &Config, XrdOucError &Eroute)
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
                      Eroute.Emsg("Config", "invalid trace option -", val);
                  }
          val = Config.GetWord();
         }
    OfsTrace.What = trval;

// All done
//
   return 0;
}

/******************************************************************************/
/*                           L i s t _ V P l i s t                            */
/******************************************************************************/
  
void XrdOfs::List_VPlist(char *lname, 
                      XrdOucPListAnchor &plist, XrdOucError &Eroute)
{
     XrdOucPList *fp;

     fp = plist.Next();
     while(fp) {Eroute.Say(lname, fp->Path()); fp = fp->Next();}
}
