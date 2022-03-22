/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n C o n f . h h                    */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <limits>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "XrdNet/XrdNetAddr.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdXrootd/XrdXrootdGSReal.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdTpcMon.hh"

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
struct MonParms
      {char *monDest[2];
       int   monMode[2];
       int   monFlash;
       int   monFlush;
       int   monGBval;
       int   monMBval;
       int   monRBval;
       int   monWWval;
       int   monFbsz;
       int   monIdent;
       int   monRnums;
       int   monFSint;
       int   monFSopt;
       int   monFSion;

       void  Exported() {monDest[0] = monDest[1] = 0;}

             MonParms() : monDest{0,0}, monMode{0,0},  monFlash(0), monFlush(0),
                          monGBval(0),  monMBval(0),   monRBval(0), monWWval(0),
                          monFbsz(0),   monIdent(3600),monRnums(0),
                          monFSint(0),  monFSopt(0),   monFSion(0) {}
            ~MonParms() {if (monDest[0]) free(monDest[0]);
                         if (monDest[1]) free(monDest[1]);
                        }
};

MonParms *MP = 0;

struct XrdXrootdGSReal::GSParms gsObj[] =
       {{"ccm",    0, XROOTD_MON_CCM,   0, -1, XROOTD_MON_GSCCM, 0,
                   XrdXrootdGSReal::fmtBin, XrdXrootdGSReal::hdrNorm},
        {"pfc",    0, XROOTD_MON_PFC,   0, -1, XROOTD_MON_GSPFC, 0,
                   XrdXrootdGSReal::fmtBin, XrdXrootdGSReal::hdrNorm},
        {"TcpMon", 0, XROOTD_MON_TCPMO, 0, -1, XROOTD_MON_GSTCP, 0,
                   XrdXrootdGSReal::fmtBin, XrdXrootdGSReal::hdrNorm},
        {"Tpc",    0, XROOTD_MON_TPC,   0, -1, XROOTD_MON_GSTPC, 0,
                   XrdXrootdGSReal::fmtBin, XrdXrootdGSReal::hdrNorm}
       };
}

/******************************************************************************/
/*                         C o n f i g G S t r e a m                          */
/******************************************************************************/

bool XrdXrootdProtocol::ConfigGStream(XrdOucEnv &myEnv, XrdOucEnv *urEnv)
{
   XrdXrootdGStream *gs;
   static const int numgs=sizeof(gsObj)/sizeof(struct XrdXrootdGSReal::GSParms);
   char vbuff[64];
   bool aOK, gXrd[numgs] = {false, false, true, true};

// For each enabled monitoring provider, allocate a g-stream and put
// its address in our environment.
//
   for (int i = 0; i < numgs; i++)
       {if (gsObj[i].dest || XrdXrootdMonitor::ModeEnabled(gsObj[i].Mode))
           {if (MP && gsObj[i].maxL <= 0) gsObj[i].maxL = MP->monGBval;
            gs = new XrdXrootdGSReal(gsObj[i], aOK);
            if (!aOK) return false;
            snprintf(vbuff, sizeof(vbuff), "%s.gStream*", gsObj[i].pin);
            if (!gXrd[i]) myEnv.PutPtr(vbuff, (void *)gs);
               else if (urEnv) urEnv->PutPtr(vbuff, (void *)gs);
           }
       }

// Configure the TPC monitor if we have a gStream for it
//
   if (urEnv && (gs = (XrdXrootdGStream*)urEnv->GetPtr("Tpc.gStream*")))
      {XrdXrootdTpcMon* tpcMon = new XrdXrootdTpcMon("xroot",eDest.logger(),*gs);
       myEnv.PutPtr("TpcMonitor*", (void*)tpcMon);
      }

// All done
//
   return true;
}

/******************************************************************************/
/*                             C o n f i g M o n                              */
/******************************************************************************/

bool XrdXrootdProtocol::ConfigMon(XrdProtocol_Config *pi, XrdOucEnv &xrootdEnv)
{
   int i, numgs = sizeof(gsObj)/sizeof(struct XrdXrootdGSReal::GSParms);

// Check if anything was configured.
//
   for (i = 0; i < numgs && !gsObj[i].dest; i++);
   if (i < numgs && !MP) MP = new MonParms;
      else if (!MP) return true;

// Set monitor defaults, this has to be done first
//
   XrdXrootdMonitor::Defaults(MP->monMBval, MP->monRBval, MP->monWWval,
                              MP->monFlush, MP->monFlash, MP->monIdent,
                              MP->monRnums, MP->monFbsz,
                              MP->monFSint, MP->monFSopt, MP->monFSion);

// Complete destination dependent setup
//
   XrdXrootdMonitor::Defaults(MP->monDest[0], MP->monMode[0],
                              MP->monDest[1], MP->monMode[1]);

// Initialize monitoring enough to construct gStream objects.
//
   XrdXrootdMonitor::Init(Sched, &eDest, pi->myName, pi->myProg, myInst, Port);

// Config g-stream objects, as needed. This needs to be done before we
// load any plugins but after we initialize phase 1 monitoring.
//
   ConfigGStream(xrootdEnv, pi->theEnv);

// Enable monitoring (it won't do anything if it wasn't enabled)
//
   if (!XrdXrootdMonitor::Init()) return false;

// Cleanup
//
   if (MP->monDest[0]) MP->Exported();
   delete MP;

// All done
//
   return true;
}
  
/******************************************************************************/
/*                                  x m o n                                   */
/******************************************************************************/

/* Function: xmon

   Purpose:  Parse directive: monitor [...] [all] [auth]  [flush [io] <sec>]
                                      [fstat <sec> [lfn] [ops] [ssq] [xfr <n>]
                                      [{fbuff | fbsz} <sz>] [gbuff <sz>]
                                      [ident {<sec>|off}] [mbuff <sz>]
                                      [rbuff <sz>] [rnums <cnt>] [window <sec>]
                                      [dest [Events] <host:port>]

   Events: [ccm] [files] [fstat] [info] [io] [iov] [pfc] [redir] [tcpmon] [user]

         all                enables monitoring for all connections.
         auth               add authentication information to "user".
         flush  [io] <sec>  time (seconds, M, H) between auto flushes. When
                            io is given applies only to i/o events.
         fstat  <sec>       produces an "f" stream for open & close events
                            <sec> specifies the flush interval (also see xfr)
                            lfn    - adds lfn to the open event
                            ops    - adds the ops record when the file is closed
                            ssq    - computes the sum of squares for the ops rec
                            xfr <n>- inserts i/o stats for open files every
                                     <sec>*<n>. Minimum is 1.
         fbsz   <sz>        size of message buffer for file stream monitoring.
         gbuff  <sz>        size of message buffer for g-stream    monitoring.
         ident {<sec>|off}  time (seconds, M, H) between identification records.
                            The keyword "off" turns them off.
         mbuff  <sz>        size of message buffer for event trace monitoring.
         rbuff  <sz>        size of message buffer for redirection monitoring.
         rnums  <cnt>       bumber of redirections monitoring streams.
         window <sec>       time (seconds, M, H) between timing marks.
         dest               specified routing information. Up to two dests
                            may be specified.
         ccm                monitor cache context management
         files              only monitors file open/close events.
         fstats             vectors the "f" stream to the destination
         info               monitors client appid and info requests.
         io                 monitors I/O requests, and files open/close events.
         iov                like I/O but also unwinds vector reads.
         pfc                monitor proxy file cache
         redir              monitors request redirections
         tcpmon             monitors tcp connection closes.
         tpc                Third Party Copy
         user               monitors user login and disconnect events.
         <host:port>        where monitor records are to be sentvia UDP.

   Output: 0 upon success or !0 upon failure. Ignored by master.
*/

int XrdXrootdProtocol::xmon(XrdOucStream &Config)
{
    char  *val = 0;
    long long tempval;
    int  i, xmode=0, *flushDest;
    bool haveWord = true;

// Check if this is a continuation
//
   if ((val = Config.GetWord()) && !strcmp("...", val)) val = Config.GetWord();
      else if (MP) {delete MP; MP = 0;}

// Allocate a new parameter block if we don't have one
//
   if (!MP) MP = new MonParms;

// Make sure we have at least one option here
//
   if (!val)
      {eDest.Emsg("Config", "no monitor parameters specified");
       return 1;
      }

// Process all the options
//
    while(haveWord || (val = Config.GetWord()))
         {haveWord = false;
               if (!strcmp("all",  val)) xmode = XROOTD_MON_ALL;
          else if (!strcmp("auth",  val))
                  MP->monMode[0] = MP->monMode[1] = XROOTD_MON_AUTH;
          else if (!strcmp("flush", val))
                {if ((val = Config.GetWord()) && !strcmp("io", val))
                    {    flushDest = &MP->monFlash; val = Config.GetWord();}
                    else flushDest = &MP->monFlush;
                 if (!val)
                    {eDest.Emsg("Config", "monitor flush value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(eDest,"monitor flush",val,
                                           flushDest,1)) return 1;
                }
          else if (!strcmp("fstat",val))
                  {if (!(val = Config.GetWord()))
                      {eDest.Emsg("Config", "monitor fstat value not specified");
                       return 1;
                      }
                   if (XrdOuca2x::a2tm(eDest,"monitor fstat",val,
                                             &MP->monFSint,0)) return 1;
                   while((val = Config.GetWord()))
                        if (!strcmp("lfn", val)) MP->monFSopt |=  XROOTD_MON_FSLFN;
                   else if (!strcmp("ops", val)) MP->monFSopt |=  XROOTD_MON_FSOPS;
                   else if (!strcmp("ssq", val)) MP->monFSopt |=  XROOTD_MON_FSSSQ;
                   else if (!strcmp("xfr", val))
                           {if (!(val = Config.GetWord()))
                               {eDest.Emsg("Config", "monitor fstat xfr count not specified");
                                return 1;
                               }
                            if (XrdOuca2x::a2i(eDest,"monitor fstat io count",
                                               val, &MP->monFSion,1)) return 1;
                            MP->monFSopt |=  XROOTD_MON_FSXFR;
                           }
                   else {haveWord = true; break;}
                  }
          else if (!strcmp("mbuff", val) || !strcmp("rbuff", val) ||
                   !strcmp("gbuff", val) || !strcmp("fbuff", val) ||
                   !strcmp("fbsz",  val))
                  {char bName[16], bType = *val;
                   snprintf(bName,sizeof(bName),"monitor %s",val);
                   if (!(val = Config.GetWord()))
                      {eDest.Emsg("Config", "value not specified"); return 1;}
                   if (XrdOuca2x::a2sz(eDest,bName,val,&tempval,1024,65535))
                      return 1;
                   int bVal = static_cast<int>(tempval);
                   switch(bType)
                         {case 'f': MP->monFbsz  = bVal; break;
                          case 'g': MP->monGBval = bVal; break;
                          case 'm': MP->monMBval = bVal; break;
                          case 'r': MP->monRBval = bVal; break;
                          default:  break;
                         }
                  }
          else if (!strcmp("ident", val))
                {if (!(val = Config.GetWord()))
                    {eDest.Emsg("Config", "monitor ident value not specified");
                     return 1;
                    }
                 if (!strcmp("off", val)) MP->monIdent = -1;
                    else if (XrdOuca2x::a2tm(eDest,"monitor ident",val,
                                             &MP->monIdent,0)) return 1;
                }
          else if (!strcmp("rnums", val))
                {if (!(val = Config.GetWord()))
                    {eDest.Emsg("Config", "monitor rnums value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2i(eDest,"monitor rnums",val, &MP->monRnums,1,
                                    XrdXrootdMonitor::rdrMax)) return 1;
                }
          else if (!strcmp("window", val))
                {if (!(val = Config.GetWord()))
                    {eDest.Emsg("Config", "monitor window value not specified");
                     return 1;
                    }
                 if (XrdOuca2x::a2tm(eDest,"monitor window",val,
                                           &MP->monWWval,1)) return 1;
                }
          else break;
         }

    if (!val) return 0;

    for (i = 0; i < 2; i++)
        {if (strcmp("dest", val)) break;
         while((val = Config.GetWord()))
                   if (!strcmp("ccm",  val)) MP->monMode[i] |=  XROOTD_MON_CCM;
              else if (!strcmp("files",val)) MP->monMode[i] |=  XROOTD_MON_FILE;
              else if (!strcmp("fstat",val)) MP->monMode[i] |=  XROOTD_MON_FSTA;
              else if (!strcmp("info", val)) MP->monMode[i] |=  XROOTD_MON_INFO;
              else if (!strcmp("io",   val)) MP->monMode[i] |=  XROOTD_MON_IO;
              else if (!strcmp("iov",  val)) MP->monMode[i] |= (XROOTD_MON_IO
                                                               |XROOTD_MON_IOV);
              else if (!strcmp("pfc",  val)) MP->monMode[i] |=  XROOTD_MON_PFC;
              else if (!strcmp("redir",val)) MP->monMode[i] |=  XROOTD_MON_REDR;
              else if (!strcmp("tcpmon",val))MP->monMode[i] |=  XROOTD_MON_TCPMO;
              else if (!strcmp("tpc",   val))MP->monMode[i] |=  XROOTD_MON_TPC;
              else if (!strcmp("user", val)) MP->monMode[i] |=  XROOTD_MON_USER;
              else break;

         if (!val) {eDest.Emsg("Config","monitor dest value not specified");
                    return 1;
                   }
         if (MP->monDest[i]) free(MP->monDest[i]);
         if (!(MP->monDest[i] = xmondest("monitor dest", val))) return 1;
         if (!(val = Config.GetWord())) break;
        }

    if (val)
       {if (!strcmp("dest", val))
           eDest.Emsg("Config", "Warning, a maximum of two dest values allowed.");
           else eDest.Emsg("Config", "Warning, invalid monitor option", val);
       }

// Make sure dests differ
//
   if (MP->monDest[0] && MP->monDest[1]
   &&  !strcmp(MP->monDest[0], MP->monDest[1]))
      {eDest.Emsg("Config", "Warning, monitor dests are identical.");
       MP->monMode[0] |= MP->monMode[1]; MP->monMode[1] = 0;
       free(MP->monDest[1]); MP->monDest[1] = 0;
      }

// Add files option if I/O is enabled
//
   if (MP->monMode[0] & XROOTD_MON_IO) MP->monMode[0] |= XROOTD_MON_FILE;
   if (MP->monMode[1] & XROOTD_MON_IO) MP->monMode[1] |= XROOTD_MON_FILE;

// If ssq was specified, make sure we support IEEE754 floating point
//
#if !defined(__solaris__) || !defined(_IEEE_754)
   if (MP->monFSopt & XROOTD_MON_FSSSQ && !(std::numeric_limits<double>::is_iec559))
      {MP->monFSopt &= ~XROOTD_MON_FSSSQ;
       eDest.Emsg("Config","Warning, 'fstat ssq' ignored; platform does not "
                           "use IEEE754 floating point.");
      }
#endif

// The caller may have deferred setting destinations. If so, don't upset what
// if currently set.
//
   if (MP->monDest[0])
      MP->monMode[0] |= (MP->monMode[0] ? xmode : XROOTD_MON_FILE|xmode);
   if (MP->monDest[1])
      MP->monMode[1] |= (MP->monMode[1] ? xmode : XROOTD_MON_FILE|xmode);
// All done
//
   return 0;
}
  
/******************************************************************************/
/*                              x m o n d e s t                               */
/******************************************************************************/

char *XrdXrootdProtocol::xmondest(const char *what, char *val)
{
   XrdNetAddr netdest;
   const char *eText;
   char netBuff[288];

// Parse the host:port spec
//
   if ((eText = netdest.Set(val)))
      {eDest.Emsg("Config", what, "endpoint is invalid;", eText);
       return 0;
      }

// Reformat it to get full host name
//
   if (!netdest.Format(netBuff, sizeof(netBuff), XrdNetAddrInfo::fmtName))
      {eDest.Emsg("Config", what, "endpoint is unreachable");
       return 0;
      }

// Return a copy
//
   return strdup(netBuff);
}
  
/******************************************************************************/
/*                                x m o n g s                                 */
/******************************************************************************/

/* Function: xmongs

   Purpose:  Parse directive: mongstream <strm> use <opts>

   <strm>:  {all | ccm | pfc | tcpmon | tpc}  [<strm>]

   <opts>:  [flust <t>] [maxlen <l>] [send <fmt> [noident] <host:port>]

   <fmt>    {cgi | json} <hdr> | nohdr

   <hdr>    dflthdr | sitehdr | hosthdr | insthdr | fullhdr

         all                applies options to all gstreams.
         ccm                gstream: cache context management
         pfc                gstream: proxy file cache
         tcpmon             gstream: tcp connection monitoring
         tpc                gstream: Third Party Copy

         noXXX              do not include information.

   Output: 0 upon success or !0 upon failure. Ignored by master.
*/

int XrdXrootdProtocol::xmongs(XrdOucStream &Config)
{
   static const int isFlush = 0;
   static const int isMaxL  = 1;
   static const int isSend  = 2;

   struct gsOpts {const char *opname; int opwhat;} gsopts[] =
         {{"flush",     isFlush},
          {"maxlen",    isMaxL},
          {"send",      isSend}
         };
   int numopts = sizeof(gsopts)/sizeof(struct gsOpts);

   int numgs = sizeof(gsObj)/sizeof(struct XrdXrootdGSReal::GSParms);
   int selAll = XROOTD_MON_CCM | XROOTD_MON_PFC | XROOTD_MON_TCPMO
              | XROOTD_MON_TPC;
   int i, selMon = 0, opt = -1, hdr = -1, fmt = -1, flushVal = -1;
   long long maxlVal = -1;
   char *val, *dest = 0;

// Make sure we have something here
//
   if (!(val = Config.GetWord()))
      {eDest.Emsg("config", "gstream parameters not specified"); return 1;}

// First tokens are the list of streams, at least one must be specified
//
do {if (!strcmp("all", val)) selMon = selAll;
       else {for (i = 0; i < numgs; i++)
                 {if (!strcasecmp(val, gsObj[i].pin))
                     {selMon |= gsObj[i].Mode; break;}
                 }
             if (i >= numgs) break;
            }
   } while((val = Config.GetWord()));

   if (!selMon)
      {eDest.Emsg("config", "gstream name not specified"); return 1;}

// The next token needs to be 'using' if there is is one.
//
   if (val)
      {if (strcmp(val, "use"))
          {eDest.Emsg("config","mongstream expected 'use' not",val); return 1;}
       if (!(val = Config.GetWord()))
          {eDest.Emsg("config","gstream parameters not specified after 'use'");
           return 1;
          }
      } else {
       eDest.Emsg("config","mongstream expected 'use' verb not found");
       return 1;
      }

// Process all the parameters now
//
do{for (i = 0; i < numopts; i++)
       {if (!strcmp(val, gsopts[i].opname))
           {if (!(val =  Config.GetWord()))
               {eDest.Emsg("Config", "gstream", gsopts[i].opname,
                                     "value not specified");
                return 1;
               }
            break;
           }
        }

// Check if we actually found a keyword
//
   if (i >= numopts)
      {eDest.Emsg("config", "invalid gstream parameter", val);
       return 1;
      }

// Handle each specific one
//
   switch(gsopts[i].opwhat)
         {case isFlush:
               if (XrdOuca2x::a2tm(eDest, "gstream flush", val, &flushVal, 0))
                  return 1;
               break;
          case isMaxL:
               if (XrdOuca2x::a2sz(eDest, "gstream maxlen",
                                   val, &maxlVal, 1024, 65535)) return 1;
               break;
          case isSend:
               if (dest) free(dest);
               if (!xmongsend(Config, val, dest, opt, fmt, hdr)) return 1;
               break;
          default: break;
         }

  } while((val = Config.GetWord()));

// Set the values
//
   for (i = 0; i < numgs; i++)
       {if (gsObj[i].Mode & selMon)
           {if (dest)
               {if (gsObj[i].dest) free((void *)gsObj[i].dest);
                gsObj[i].dest = dest;
               }
            if (flushVal >= 0)  gsObj[i].flsT = flushVal;
            if (maxlVal >= 0)   gsObj[i].maxL = maxlVal;
            if (opt >= 0)       gsObj[i].Opt  = opt;
            if (fmt >= 0)       gsObj[i].Fmt  = fmt;
            if (hdr >= 0)       gsObj[i].Hdr  = hdr;
           }
       }

    return 0;
}
  
/******************************************************************************/
/*                              m o n g s e n d                               */
/******************************************************************************/

bool XrdXrootdProtocol::xmongsend(XrdOucStream &Config, char *val, char *&dest,
                                  int &opt, int &fmt, int &hdr)
{
   struct gsFmts  {const char *opname; int opval;} gsfmt[] =
         {
          {"cgi",     XrdXrootdGSReal::fmtCgi},
          {"json",    XrdXrootdGSReal::fmtJson},
          {"nohdr",   XrdXrootdGSReal::fmtNone}
         };
   int numfmts = sizeof(gsfmt)/sizeof(struct gsFmts);

   struct gsHdrs {const char *opname; int opval;} gshdr[] =
         {
          {"dflthdr", XrdXrootdGSReal::hdrNorm},
          {"sitehdr", XrdXrootdGSReal::hdrSite},
          {"hosthdr", XrdXrootdGSReal::hdrHost},
          {"insthdr", XrdXrootdGSReal::hdrInst},
          {"fullhdr", XrdXrootdGSReal::hdrFull}
         };
   int numhdrs = sizeof(gshdr)/sizeof(struct gsHdrs);

   int i;

// First token muxt be the format
//
   for (i = 0; i < numfmts; i++)
       if (!strcmp(val, gsfmt[i].opname))
          {fmt = gsfmt[i].opval; break;}
   if (i >= numfmts)
      {eDest.Emsg("Config","gstream send format is invalid -", val);
       return false;
      }

// The next one is the the optional hdr spec
//
   val = Config.GetWord();
   if (fmt == XrdXrootdGSReal::fmtNone) hdr = XrdXrootdGSReal::hdrNone;
      else if (val)
              {for (i = 0; i < numhdrs; i++)
                   if (!strcmp(val, gshdr[i].opname))
                      {hdr = gshdr[i].opval;
                       val = Config.GetWord();
                       break;
                      }
              }

// The final token can be "noident"
//
   if (val && !strcmp(val, "noident"))
      {opt = XrdXrootdGSReal::optNoID;
       val = Config.GetWord();
      }

// Finally, we must have the host and port
//
   if (!val)
      {eDest.Emsg("Config", "gstream send endpoint not specified");
       return false;
      }

// Get the endpoint
//
   dest = xmondest("gstream send", val);
   return dest != 0;
}
