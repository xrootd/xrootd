#ifndef __XRDCMSRRQ_HH__
#define __XRDCMSRRQ_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d C m s R R Q . h h                           */
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

#include <sys/uio.h>

#include "XProtocol/XPtypes.hh"
#include "XProtocol/YProtocol.hh"

#include "XrdCms/XrdCmsTypes.hh"
#include "XrdOuc/XrdOucDLlist.hh"
#include "XrdSys/XrdSysPthread.hh"
  
/******************************************************************************/
/*                         X r d C m s R R Q I n f o                          */
/******************************************************************************/
  
class XrdCmsRRQInfo
{
public:
void     *Key;     // Key link, which is the cache line address
kXR_unt32 ID;      // Response link, which is the request ID
int       Rinst;   // Redirector instance
short     Rnum;    // Redirector number (RTable slot number)
char      isRW;    // True if r/w access wanted
char      isLU;    // True if locate response wanted
char      minR;    // Minimum number of responses for fast redispatch
char      actR;    // Actual  number of responses
char      lsLU;    // Lookup options
char      ifOP;    // XrdNetIF::ifType to return (cast as char)
SMask_t   rwVec;   // R/W servers for corresponding path (if isLU is true)

        XrdCmsRRQInfo() : isLU(0), ifOP(0) {}
        XrdCmsRRQInfo(int rinst, short rnum, kXR_unt32 id, int minQ=0)
                        : Key(0), ID(id), Rinst(rinst), Rnum(rnum),
                          isRW(0), isLU(0), minR(minQ), actR(0), lsLU(0), ifOP(0),
                          rwVec(0) {}
       ~XrdCmsRRQInfo() {}
};

/******************************************************************************/
/*                         X r d C m s R R Q S l o t                          */
/******************************************************************************/
  
class XrdCmsRRQSlot
{
friend class XrdCmsRRQ;

static XrdCmsRRQSlot *Alloc(XrdCmsRRQInfo *Info);

       void           Recycle();

       XrdCmsRRQSlot();
      ~XrdCmsRRQSlot() {}

private:

static   XrdSysMutex                 myMutex;
static   XrdCmsRRQSlot              *freeSlot;
static   short                       initSlot;

         XrdOucDLlist<XrdCmsRRQSlot> Link;
         XrdCmsRRQSlot              *Cont;
         XrdCmsRRQSlot              *LkUp;
         XrdCmsRRQInfo               Info;
         SMask_t                     Arg1;
         SMask_t                     Arg2;
unsigned int                         Expire;
         int                         slotNum;
};

/******************************************************************************/
/*                             X r d C m s R R Q                              */
/******************************************************************************/
  
class XrdCmsRRQ
{
public:

short Add(short Snum, XrdCmsRRQInfo *ip);

void  Del(short Snum, const void *Key);

int   Init(int Tint=0, int Tdly=0);

int   Ready(int Snum, const void *Key, SMask_t mask1, SMask_t mask2);

void *Respond();

struct Info
      {
        Info(): Add2Q(0), PBack(0), Resp(0), Multi(0), luFast(0), luSlow(0),
                rdFast(0), rdSlow(0) {}
       long long Add2Q;    // Number added to queue
       long long PBack;    // Number that we could piggy-back
       long long Resp;     // Number of reponses for a waiting request
       long long Multi;    // Number of multiple response fielded
       long long luFast;   // Fast lookups
       long long luSlow;   // Slow lookups
       long long rdFast;   // Fast redirects
       long long rdSlow;   // Slow redirects
      };

void  Statistics(Info &Data) {myMutex.Lock(); Data = Stats; myMutex.UnLock();}

void *TimeOut();

      XrdCmsRRQ() : isWaiting(0), isReady(0),
                    luFast(0),    luSlow(0),  rdFast(0), rdSlow(0),
                    Tslice(178),  Tdelay(5),  myClock(0) {}
     ~XrdCmsRRQ() {}

private:

void sendLocResp(XrdCmsRRQSlot *lP);
void sendLwtResp(XrdCmsRRQSlot *rP);
void sendRedResp(XrdCmsRRQSlot *rP);
static const int numSlots = 1024;

         XrdSysMutex                   myMutex;
         XrdSysSemaphore               isWaiting;
         XrdSysSemaphore               isReady;
         XrdCmsRRQSlot                 Slot[numSlots];
         XrdOucDLlist<XrdCmsRRQSlot>   waitQ;
         XrdOucDLlist<XrdCmsRRQSlot>   readyQ;  // Redirect/Locate ready queue
static   const int                     iov_cnt = 2;
         struct iovec                  data_iov[iov_cnt];
         struct iovec                  redr_iov[iov_cnt];
         XrdCms::CmsResponse           dataResp;
         XrdCms::CmsResponse           redrResp;
         XrdCms::CmsResponse           waitResp;
union   {char                          hostbuff[288];
         char                          databuff[XrdCms::CmsLocateRequest::RHLen
                                               *STMax];
        };
         Info                          Stats;
         int                           luFast;
         int                           luSlow;
         int                           rdFast;
         int                           rdSlow;
         int                           Tslice;
         int                           Tdelay;
unsigned int                           myClock;
};

namespace XrdCms
{
extern    XrdCmsRRQ RRQ;
}
#endif
