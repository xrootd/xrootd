#ifndef __SSI_STATS_H__
#define __SSI_STATS_H__
/******************************************************************************/
/*                        X r d S s i S t a t s . h h                         */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucStats.hh"

class XrdSfsFileSystem;
class XrdStats;

class XrdSsiStats : public XrdOucStats
{
public:
long long        ReqBytes;     // Stats: Number of requests bytes total
long long        ReqMaxsz;     // Stats: Number of requests largest size
long long        RspMDBytes;   // Stats: Number of metada  response bytes
int              ReqAborts;    // Stats: Number of request aborts
int              ReqAlerts;    // Stats: Number of request alerts
int              ReqBound;     // Stats: Number of requests bound
int              ReqCancels;   // Stats: Number of request Finished()+cancel
int              ReqCount;     // Stats: Number of requests (total)
int              ReqFinForce;  // Stats: Number of request Finished()+forced
int              ReqFinished;  // Stats: Number of request Finished()
int              ReqGets;      // Stats: Number of requests -> GetRequest()
int              ReqPrepErrs;  // Stats: Number of request prepare errors
int              ReqProcs;     // Stats: Number of requests -> ProcessRequest()
int              ReqRedir;     // Stats: Number of request redirects
int              ReqRelBuf;    // Stats: Number of request -> RelRequestBuff()
int              ReqStalls;    // Stats: Number of request stalls
int              RspBad;       // Stats: Number of invalid responses
int              RspCallBK;    // Stats: Number of request callbacks
int              RspData;      // Stats: Number of data    responses
int              RspErrs;      // Stats: Number of error   responses
int              RspFile;      // Stats: Number of file    responses
int              RspReady;     // Stats: Number of ready   responses
int              RspStrm;      // Stats: Number of stream  responses
int              RspUnRdy;     // Stats: Number of unready responses
int              SsiErrs;      // Stats: Number of SSI detected errors
int              ResAdds;      // Stats: Number of resource additions
int              ResRems;      // Stats: Number of resource removals

void             setFS(XrdSfsFileSystem *fsp) {fsP = fsp;}

int              Stats(char *buff, int blen);

                 XrdSsiStats();
                ~XrdSsiStats() {}
private:

XrdSfsFileSystem *fsP;
};
#endif
