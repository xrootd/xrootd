/******************************************************************************/
/*                                                                            */
/*                        X r d S s i A l e r t . c c                         */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstddef>
#include <cstring>
#include <sys/uio.h>

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSsi/XrdSsiAlert.hh"
#include "XrdSsi/XrdSsiRRInfo.hh"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
XrdSysMutex      XrdSsiAlert::aMutex;
XrdSsiAlert     *XrdSsiAlert::free = 0;
int              XrdSsiAlert::fNum = 0;
int              XrdSsiAlert::fMax = XrdSsiAlert::fmaxDflt;

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdSsiAlert *XrdSsiAlert::Alloc(XrdSsiRespInfoMsg &aMsg)
{
   XrdSsiAlert *aP;

// Obtain a lock
//
   aMutex.Lock();

// Allocate via stack or a new call
//
   if (!(aP = free)) aP = new XrdSsiAlert();
      else {free = aP->next; fNum--;}

// Unlock mutex
//
   aMutex.UnLock();

// Fill out object and return it
//
   aP->next    = 0;
   aP->theMsg  = &aMsg;
   return aP;
}

/******************************************************************************/
/*                                  D o n e                                   */
/******************************************************************************/

// Gets invoked only after query() on wtresp signal was sent
  
void XrdSsiAlert::Done(int &retc, XrdOucErrInfo *eiP, const char *name)
{

// This is an async callback so we need to delete our errinfo object.
//
   delete eiP;

// Simply recycle this object.
//
   Recycle();
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdSsiAlert::Recycle()
{

// Issue callback to release the message if we have one
//
   if (theMsg) theMsg->RecycleMsg();

// Place object on the queue unless we have too many
//
   aMutex.Lock();
   if (fNum >= fMax) delete this;
      else {next = free; free = this; fNum++;}
   aMutex.UnLock();
}

/******************************************************************************/
/*                               S e t I n f o                                */
/******************************************************************************/
  
int XrdSsiAlert::SetInfo(XrdOucErrInfo &eInfo, char *aMsg, int aLen)
{
   static const int aIovSz = 3;
   struct AlrtResp {struct iovec ioV[aIovSz]; XrdSsiRRInfoAttn aHdr;};

   AlrtResp *alrtResp;
   char *mBuff, *aData;
   int n;

// We will be constructing the response in the message buffer. This is
// gauranteed to be big enough for our purposes so no need to check the size.
//
   mBuff = eInfo.getMsgBuff(n);

// Initialize the response
//
   alrtResp = (AlrtResp *)mBuff;
   memset(alrtResp, 0, sizeof(AlrtResp));
   alrtResp->aHdr.pfxLen = htons(sizeof(XrdSsiRRInfoAttn));

// Fill out iovec to point to our header
//
// alrtResp->ioV[0].iov_len  = sizeof(XrdSsiRRInfoAttn) + msgBlen;
   alrtResp->ioV[1].iov_base = mBuff+offsetof(struct AlrtResp, aHdr);
   alrtResp->ioV[1].iov_len  = sizeof(XrdSsiRRInfoAttn);

// Fill out the iovec for the alert data
//
   aData = theMsg->GetMsg(n);
   alrtResp->ioV[2].iov_base = aData;
   alrtResp->ioV[2].iov_len  = n;
   alrtResp->aHdr.mdLen = htonl(n);
   alrtResp->aHdr.tag = XrdSsiRRInfoAttn::alrtResp;

// Return up to 8 bytes of alert data for debugging purposes
//
   if (aMsg) memcpy(aMsg, aData, (n < (int)sizeof(aMsg) ? n : 8));

// Setup to have metadata actually sent to the requestor
//
   eInfo.setErrCode(aIovSz);
   return n;
}
