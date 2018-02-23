/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d S t a t s . c c                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
 
#include <stdio.h>
  
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSsi/XrdSsiStats.hh"
 
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
XrdSsiStats Stats;
}
using namespace XrdSsi;
  
/******************************************************************************/
/*                           C o n s t r c u t o r                            */
/******************************************************************************/
  
XrdSsiStats::XrdSsiStats()
{

fsP      = 0;

ReqBytes      = 0; // Stats: Number of requests bytes total
ReqMaxsz      = 0; // Stats: Number of requests largest size
RspMDBytes    = 0; // Stats: Number of metada  response bytes
ReqAborts     = 0; // Stats: Number of request aborts
ReqAlerts     = 0; // Stats: Number of request alerts
ReqBound      = 0; // Stats: Number of requests bound
ReqCancels    = 0; // Stats: Number of request Finished()+cancel
ReqCount      = 0; // Stats: Number of requests (total)
ReqFinForce   = 0; // Stats: Number of request Finished()+forced
ReqFinished   = 0; // Stats: Number of request Finished()
ReqGets       = 0; // Stats: Number of requests -> GetRequest()
ReqPrepErrs   = 0; // Stats: Number of request prepare errors
ReqProcs      = 0; // Stats: Number of requests -> ProcessRequest()
ReqRedir      = 0; // Stats: Number of request redirects
ReqRelBuf     = 0; // Stats: Number of request -> RelRequestBuff()
ReqStalls     = 0; // Stats: Number of request stalls
RspBad        = 0; // Stats: Number of invalid responses
RspCallBK     = 0; // Stats: Number of request callbacks
RspData       = 0; // Stats: Number of data    responses
RspErrs       = 0; // Stats: Number of error   responses
RspFile       = 0; // Stats: Number of file    responses
RspReady      = 0; // Stats: Number of ready   responses
RspStrm       = 0; // Stats: Number of stream  responses
RspUnRdy      = 0; // Stats: Number of unready responses
SsiErrs       = 0; // Stats: Number of SSI detected errors
ResAdds       = 0; // Stats: Number of resource additions
ResRems       = 0; // Stats: Number of resource removals
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

int XrdSsiStats::Stats(char *buff, int blen)
{
   static const char statfmt[] = "<stats id=\"ssi\"><err>%d</err>"
   "<req>"
   "<bytes>%lld</bytes><maxsz>%lld</maxsz><cnt>%d</cnt>"
   "<bnd>%d</bnd><rdr>%d</rdr><dly>%d</dly>"
   "<ab>%d</ab><proc>%d</proc><gets>%d</gets>"
   "<relb>%d</relb><al>%d</al><fin>%d</fin>"
   "<can>%d</can><finf>%d</finf><perr>%d</perr>"
   "</req><rsp>"
   "<bad>%d</bad><cbk>%d</cbk><data>%d</data><errs>%d</errs>"
   "<file>%d</file><str>%d</str><rdy>%d</rdy><unr>%d</unr>"
   "<mdb>%lld</mdb"
   "</rsp><res>"
   "<add>%d</add><rem>%d</rem>"
   "</res></stats>";
//                                   1 2 3 4 5 6 7 8
   static const long long LLMax = 0x7fffffffffffffffLL;
   static const int       INMax = 0x7fffffff;
   int len;

// If no buffer, caller wants the maximum size we will generate
//
   if (!buff)
      {char dummy[4096]; // Almost any size will do
       len = snprintf(dummy, sizeof(dummy), statfmt, INMax,
       /*<bytes>*/    LLMax, LLMax, INMax,
       /*<bnd>*/      INMax, INMax, INMax,
       /*<ab>*/       INMax, INMax, INMax,
       /*<relb>*/     INMax, INMax, INMax,
       /*<can>*/      INMax, INMax, INMax,
       /*<bad>*/      INMax, INMax, INMax, INMax,
       /*<file>*/     INMax, INMax, INMax, INMax, LLMax,
       /*<res>*/      INMax, INMax);
       return len + (fsP ? fsP->getStats(0,0) : 0);
      }

// Format our statistics
//
   statsMutex.Lock();
   len = snprintf(buff, blen, statfmt, SsiErrs,
                  ReqBytes,   ReqMaxsz,    ReqCount,
                  ReqBound,   ReqRedir,    ReqStalls,
                  ReqAborts,  ReqProcs,    ReqGets,
                  ReqRelBuf,  ReqAlerts,   ReqFinished,
                  ReqCancels, ReqFinForce, ReqPrepErrs,
                  RspBad,     RspCallBK,   RspData,      RspErrs,
                  RspFile,    RspStrm,     RspReady,     RspUnRdy,
                  RspMDBytes, ResAdds,     ResRems);
   statsMutex.UnLock();

// Now include filesystem statistics and return
//
   if (fsP) len += fsP->getStats(buff+len, blen-len);
   return len;
}
