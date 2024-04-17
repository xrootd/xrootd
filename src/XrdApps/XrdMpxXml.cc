/******************************************************************************/
/*                                                                            */
/*                          X r d M p x X m l . c c                           */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>
#include <map>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <ctime>

#include "XrdApps/XrdMpxXml.hh"
#include "XrdOuc/XrdOucTokenizer.hh"

using namespace std;

/******************************************************************************/
/*                      v n M a p   D e f i n i t i o n                       */
/******************************************************************************/

namespace
{
struct vCmp
      {bool operator()(const char *a, const char *b)
                      const {return strcmp(a,b) < 0;}
      };

std::map<const char *, const char *, vCmp> vnMap {

{"src",             "Server location:"},
{"tod",             "~Statistics:"},
{"tos",             "~Server started: "},
{"pgm",             "Server program: "},
{"ins",             "Server instance:"},
{"pid",             "Server process: "},
{"site",            "Server sitename: "},
{"ver",             "Server version: "},
{"info.host",       "Host name:"},
{"info.port",       "Port:"},
{"info.name",       "Instance name:"},
{"buff.reqs",       "Buffer requests:"},
{"buff.mem",        "Buffer bytes:"},
{"buff.buffs",      "Buffer count:"},
{"buff.adj",        "Buffer adjustments:"},
{"buff.xlreqs",     "Buffer XL requests:"},
{"buff.xlmem",      "Buffer XL bytes:"},
{"buff.xlbuffs",    "Buffer XL count:"},
{"link.num",        "Current connections:"},
{"link.maxn",       "Maximum connections:"},
{"link.tot",        "Overall connections:"},
{"link.in",         "Bytes received:"},
{"link.out",        "Bytes sent:"},
{"link.ctime",      "Total connect seconds:"},
{"link.tmo",        "Read request timeouts:"},
{"link.stall",      "Number of partial reads:"},
{"link.sfps",       "Number of partial sends:"},
{"poll.att",        "Poll sockets:"},
{"poll.en",         "Poll enables:"},
{"poll.ev",         "Poll events: "},
{"poll.int",        "Poll events unsolicited:"},
{"proc.usr.s",      "Seconds user time:"},
{"proc.usr.u",      "Micros  user time:"},
{"proc.sys.s",      "Seconds sys  time:"},
{"proc.sys.u",      "Micros  sys  time:"},
{"xrootd.num",      "XRootD protocol loads:"},
{"xrootd.ops.open", "XRootD opens:"},
{"xrootd.ops.rf",   "XRootD cache refreshes:"},
{"xrootd.ops.rd",   "XRootD reads:"},
{"xrootd.ops.pr",   "XRootD preads:"},
{"xrootd.ops.rv",   "XRootD readv's:"},
{"xrootd.ops.rs",   "XRootD readv segments:"},
{"xrootd.ops.wr",   "XRootD writes:"},
{"xrootd.ops.sync", "XRootD syncs:"},
{"xrootd.ops.getf", "XRootD getfiles:"},
{"xrootd.ops.putf", "XRootD putfiles:"},
{"xrootd.ops.misc", "XRootD misc requests:"},
{"xrootd.sig.ok",   "XRootD ok  signatures:"},
{"xrootd.sig.bad",  "XRootD bad signatures:"},
{"xrootd.sig.ign",  "XRootD ign signatures:"},
{"xrootd.aio.num",  "XRootD aio requests:"},
{"xrootd.aio.max",  "XRootD aio max requests:"},
{"xrootd.aio.rej",  "XRootD aio rejections:"},
{"xrootd.err",      "XRootD request failures:"},
{"xrootd.rdr",      "XRootD request redirects:"},
{"xrootd.dly",      "XRootD request delays:"},
{"xrootd.lgn.num",  "XRootD login total count:"},
{"xrootd.lgn.af",   "XRootD login auths bad:  "},
{"xrootd.lgn.au",   "XRootD login auths good: "},
{"xrootd.lgn.ua",   "XRootD login auths none: "},
{"ofs.role",        "Server role:"},
{"ofs.opr",         "Ofs reads:"},
{"ofs.opw",         "Ofs writes:"},
{"ofs.opp",         "POSC files now open:"},
{"ofs.ups",         "POSC files deleted:"},
{"ofs.han",         "Ofs handles:"},
{"ofs.rdr",         "Ofs redirects:"},
{"ofs.bxq",         "Ofs background tasks:"},
{"ofs.rep",         "Ofs callbacks:"},
{"ofs.err",         "Ofs errors:"},
{"ofs.dly",         "Ofs delays:"},
{"ofs.sok",         "Ofs ok  events:"},
{"ofs.ser",         "Ofs bad events:"},
{"ofs.tpc.grnt",    "TPC grants:"},
{"ofs.tpc.deny",    "TPC denials:"},
{"ofs.tpc.err",     "TPC errors:"},
{"ofs.tpc.exp",     "TPC expires:"},
{"oss.paths",       "Oss exports:"},
{"oss.space",       "Oss space:"},
{"sched.jobs",      "Tasks scheduled: "},
{"sched.inq",       "Tasks now queued:"},
{"sched.maxinq",    "Max tasks queued:"},
{"sched.threads",   "Threads in pool:"},
{"sched.idle",      "Threads idling: "},
{"sched.tcr",       "Threads created:"},
{"sched.tde",       "Threads deleted:"},
{"sched.tlimr",     "Threads unavail:"},
{"sgen.as",         "Unsynchronized stats:"},
{"sgen.et",         "Mills to collect stats:"},
{"sgen.toe",        "~Time when stats collected:"},
{"ssi.err",         "SSI errors:"},
{"ssi.req.bytes",   "Request total bytes:"},
{"ssi.req.maxsz",   "Request largest size:"},
{"ssi.req.ab",      "Request aborts:"},
{"ssi.req.al",      "Request alerts:"},
{"ssi.req.bnd",     "Requests now bound:"},
{"ssi.req.can",     "Requests cancelled:"},
{"ssi.req.cnt",     "Request total count:"},
{"ssi.req.fin",     "Requests finished:"},
{"ssi.req.finf",    "Requests forced off:"},
{"ssi.req.gets",    "Request retrieved:"},
{"ssi.req.perr",    "Request prep errors:"},
{"ssi.req.proc",    "Requests started:"},
{"ssi.req.rdr",     "Requests redirected:"},
{"ssi.req.relb",    "Request buff releases:"},
{"ssi.req.dly",     "Requests delayed:"},
{"ssi.rsp.bad",     "Response violations:"},
{"ssi.rsp.cbk",     "Response callbacks:"},
{"ssi.rsp.data",    "Responses as data:"},
{"ssi.rsp.errs",    "Responses as errors:"},
{"ssi.rsp.file",    "Responses as files:"},
{"ssi.rsp.rdy",     "Responses without delay:"},
{"ssi.rsp.str",     "Responses as streams:"},
{"ssi.rsp.unr",     "Responses with delay:"},
{"ssi.rsp.mdb",     "Response metadata bytes:"},
{"ssi.res.add",     "Resources added:"},
{"ssi.res.rem",     "Resources removed:"}
};
}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                             X r d M p x V a r                              */
/******************************************************************************/
  
class  XrdMpxVar
{
public:

      int   Pop(const char *vName);

      int   Push(const char *vName);

      void  Reset() {vEnd = vBuff; vNum = -1; *vBuff = 0;}

const char *Var() {return vBuff;}

            XrdMpxVar(bool dbg=false)
                     : vFence(vBuff + sizeof(vBuff) - 1), Debug(dbg) {Reset();}
           ~XrdMpxVar() {}

private:
static const int   vMax = 15;
             char *vEnd, *vFence, *vStack[vMax+1], vBuff[1024];
             int   vNum;
             bool  Debug;
};

/******************************************************************************/
/*                        X r d M p x V a r : : P o p                         */
/******************************************************************************/
  
int XrdMpxVar::Pop(const char *vName)
{
    if (Debug) std::cerr <<"Pop:  " <<(vName ? vName : "") <<"; var=" <<vBuff <<std::endl;
    if (vNum < 0 || (vName && strcmp(vStack[vNum], vName))) return 0;
    vEnd = vStack[vNum]-1; *vEnd = '\0'; vNum--;
    return 1;
}

/******************************************************************************/
/*                       X r d M p x V a r : : P u s h                        */
/******************************************************************************/
  
int XrdMpxVar::Push(const char *vName)
{
   int n = strlen(vName);

   if (Debug) std::cerr <<"Push: " <<vName <<"; var=" <<vBuff <<std::endl;
   if (vNum >= vMax) return 0;
   if (vNum >= 0) *vEnd++ = '.';
      else         vEnd = vBuff;
   if (vEnd+n+1 >= vFence) return 0;
   strcpy(vEnd, vName);
   vStack[++vNum] = vEnd;
   vEnd += n;
   return 1;
}

/******************************************************************************/
/*                     X r d M p x X m l : : F o r m a t                      */
/******************************************************************************/
  
int XrdMpxXml::Format(const char *Host, char *ibuff, char *obuff)
{
   static const char *Hdr0 = "<statistics ";
   static const int   H0Len = strlen(Hdr0);

   XrdMpxVar       xVar(Debug);
   XrdOucTokenizer Data(ibuff);
   VarInfo vHead[] = {{"tod", 0}, {"ver", 0}, {"src", 0}, {"tos", 0},
                      {"pgm", 0}, {"ins", 0}, {"pid", 0}, {0, 0}};
   VarInfo vStat[] = {{"id",  0}, {0, 0}};
   VarInfo vTail[] = {{"toe", 0}, {0, 0}};
   char *lP = ibuff, *oP = obuff, *tP, *vP;
   int i, rc;

// Insert a newline for the first '>'
//
   if (!(lP = (char *)index(lP, '>')))
      return xmlErr("Invalid xml stream: ", ibuff);
   *lP++ = '\n';

// Now make the input tokenizable
//
   while(*lP)
        {if (*lP == '>' || (*lP == '<' && *(lP+1) == '/')) *lP = ' ';
         lP++;
        }

// The first token better be '<statistics'
//
   if (!(lP = Data.GetLine()) || strncmp(Hdr0, lP, H0Len))
      return xmlErr("Stream does not start with '<statistics'.");
   Data.GetToken(); getVars(Data, vHead);

// Output the vars in the headers as 'stats..var'
//
   for (i = 0; vHead[i].Name; i++)
       {if (vHead[i].Data) oP = Add(oP, vHead[i].Name, vHead[i].Data);}

// Add in the host name, if supplied
//
   if (Host) oP = Add(oP, "host", Host);

// Get the remainder
//
   if (!Data.GetLine()) return xmlErr("Null xml stream after header.");

// The following segment reads all of the "stats" entries
//
   while((tP = Data.GetToken()) && strcmp(tP, "/statistics"))
        {     if (*tP == '/')
                 {if (!xVar.Pop(strcmp("/stats", tP) ? tP+1 : 0))
                     return xmlErr(tP, "invalid end for ", xVar.Var());
                 }
         else if (*tP == '<')
                 {if (strcmp("<stats", tP)) rc = xVar.Push(tP+1);
                     else {getVars(Data, vStat);
                           rc = (vStat[0].Data ? xVar.Push(vStat[0].Data)
                                               : xVar.Push(tP+1));
                          }
                  if (!rc) return xmlErr("Nesting too deep for ", xVar.Var());
                 }
         else    {if ((vP = index(tP, '<'))) *vP = '\0';
                  if (*tP == '"')
                     {i = strlen(tP)-1;
                      if (*(tP+i) == '"') {*(tP+i) = '\0'; i = 1;}
                     } else i = 0;
                  oP = Add(oP, xVar.Var(), tP+i);
                  if (vP) {*vP = '<';
                           if (vP != tP) memset(tP, ' ', vP - tP);
                           Data.RetToken();
                          }
                 }
        }
   if (!tP) return xmlErr("Missing '</statistics>' in xml stream.");
   getVars(Data, vTail);
   if (vTail[0].Data) oP = Add(oP, vTail[0].Name, vTail[0].Data);
   if (*(oP-1) == '&') oP--;
   *oP++ = '\n';
   return oP - obuff;
}

/******************************************************************************/
/*                        X r d M p x X m l : : A d d                         */
/******************************************************************************/
  
char *XrdMpxXml::Add(char *Buff, const char *Var, const char *Val)
{
   char tmBuff[256];

   if (noZed && !strcmp("0", Val)) return Buff;

   if (doV2T)
      {std::map<const char *, const char *, vCmp>::iterator it;
       it = vnMap.find(Var);
       if (it != vnMap.end())
          {Var = it->second;
           if (*Var == '~')
              {time_t tod = atoi(Val);
               Var++;
               if (tod)
                  {struct tm *tInfo = localtime(&tod);
                   strftime(tmBuff, sizeof(tmBuff), "%a %F %T", tInfo);
                   Val = tmBuff;
                  }
              }
          }
      }

   strcpy(Buff, Var); Buff += strlen(Var);
   *Buff++ = vSep;
   strcpy(Buff, Val); Buff += strlen(Val);
   *Buff++ = vSfx;
   return Buff;
}

/******************************************************************************/
/*                                                                            */
/*                    X r d M p x X m l : : g e t V a r s                     */
/*                                                                            */
/******************************************************************************/
  
void XrdMpxXml::getVars(XrdOucTokenizer &Data, VarInfo Var[])
{
   char *tVar, *tVal;
   int i;

// Initialize the data pointers to null
//
   i = 0;
   while(Var[i].Name) Var[i++].Data = 0;

// Get all of the variables/values and return where possible
//
   while((tVar = Data.GetToken()) && *tVar != '<' && *tVar != '/')
        {if (!(tVal = (char *)index(tVar, '='))) continue;
         *tVal++ = '\0';
         if (*tVal == '"')
            {tVal++, i = strlen(tVal);
             if (*(tVal+i-1) == '"') *(tVal+i-1) = '\0';
            }
         i = 0;
         while(Var[i].Name)
              {if (!strcmp(Var[i].Name, tVar)) {Var[i].Data = tVal; break;}
                  else i++;
              }
        }
   if (tVar && (*tVar == '<' || *tVar == '/')) Data.RetToken();
}

/******************************************************************************/
/*                     X r d M p x X m l : : x m l E r r                      */
/******************************************************************************/
  
int XrdMpxXml::xmlErr(const char *t1, const char *t2, const char *t3)
{
   std::cerr <<"XrdMpxXml: " <<t1 <<' ' <<t2 <<' ' <<t3 <<std::endl;
   return 0;
}
