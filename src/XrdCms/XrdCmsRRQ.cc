/******************************************************************************/
/*                                                                            */
/*                          X r d C m s R R Q . c c                           */
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

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsRRQ.hh"
#include "XrdCms/XrdCmsRTable.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTimer.hh"
#include <stdio.h>

using namespace XrdCms;

// Note: Debugging statements have been commented out. This is time critical
//       code and debugging may only be enabled in standalone testing as the
//       delays introduced by DEBUG() will usually cause timeout failures.
  
/******************************************************************************/
/*       G l o b a l   O b j e c t s   &   S t a t i c   M e m b e r s        */
/******************************************************************************/
  
XrdCmsRRQ             XrdCms::RRQ;

XrdSysMutex           XrdCmsRRQSlot::myMutex;
XrdCmsRRQSlot        *XrdCmsRRQSlot::freeSlot = 0;
short                 XrdCmsRRQSlot::initSlot = 0;

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
  
void *XrdCmsRRQ_StartTimeOut(void *parg) {return RRQ.TimeOut();}

void *XrdCmsRRQ_StartRespond(void *parg) {return RRQ.Respond();}

/******************************************************************************/
/*               X r d C m s R R Q   C l a s s   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
short XrdCmsRRQ::Add(short Snum, XrdCmsRRQInfo *Info)
{
// EPNAME("RRQ Add");
   XrdCmsRRQSlot *sp;

// Obtain a slot and fill it in
//
   if (!(sp = XrdCmsRRQSlot::Alloc(Info))) return 0;
// DEBUG("adding slot " <<sp->slotNum);

// If a slot number given, check if it's the right slot and it is still queued.
// If so, piggy-back this request to existing one and make a fast exit
//
   myMutex.Lock(); Stats.Add2Q++;
   if (Snum && Slot[Snum].Info.Key == Info->Key && Slot[Snum].Expire)
      {if (Info->isLU)
          {sp->LkUp = Slot[Snum].LkUp;
           Slot[Snum].LkUp = sp;
          } else {
           sp->Cont = Slot[Snum].Cont;
           Slot[Snum].Cont = sp;
          }
       Stats.PBack++;
       myMutex.UnLock();
       return Snum;
      }

// Queue this slot to the pending response queue and tell the timeout scheduler
//
   sp->Expire = myClock+1;
   if (waitQ.Singleton()) isWaiting.Post();
   waitQ.Prev()->Insert(&sp->Link);
   myMutex.UnLock();
   return sp->slotNum;
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdCmsRRQ::Del(short Snum, const void *Key)
{
     Ready(Snum, Key, 0, 0);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdCmsRRQ::Init(int Tint, int Tdly)
{
   int rc;
   pthread_t tid;

// Set values
//
   if (Tint) Tslice = Tint;
   if (Tdly) Tdelay = Tdly;
   memset(&Stats, 0, sizeof(Stats));

// Fill out the response structure
//
   dataResp.Hdr.streamid = 0;
   dataResp.Hdr.rrCode   = kYR_data;
   dataResp.Hdr.modifier = 0;
   dataResp.Hdr.datalen  = 0;
   dataResp.Val          = 0;

// Fill out the data i/o vector
//
   data_iov[0].iov_base = (char *)&dataResp;
   data_iov[0].iov_len  = sizeof(dataResp);
   data_iov[1].iov_base = databuff;;

// Fill out the response structure
//
   redrResp.Hdr.streamid = 0;
   redrResp.Hdr.rrCode   = kYR_redirect;
   redrResp.Hdr.modifier = 0;
   redrResp.Hdr.datalen  = 0;
   redrResp.Val          = 0;

// Fill out the redirect i/o vector
//
   redr_iov[0].iov_base = (char *)&redrResp;
   redr_iov[0].iov_len  = sizeof(redrResp);
   redr_iov[1].iov_base = hostbuff;;

// Fill out the wait info
//
   waitResp.Hdr.streamid = 0;
   waitResp.Hdr.rrCode   = kYR_wait;
   waitResp.Hdr.modifier = 0;
   waitResp.Hdr.datalen  = htons(static_cast<unsigned short>(sizeof(waitResp.Val)));
   waitResp.Val          = htonl(Tdelay);

// Start the responder thread
//
   if ((rc = XrdSysThread::Run(&tid, XrdCmsRRQ_StartRespond, (void *)0,
                               0, "Request Responder")))
      {Say.Emsg("Config", rc, "create request responder thread");
       return 1;
      }

// Start the timeout thread
//
   if ((rc = XrdSysThread::Run(&tid, XrdCmsRRQ_StartTimeOut, (void *)0,
                               0, "Request Timeout")))
      {Say.Emsg("Config", rc, "create request timeout thread");
       return 1;
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*                                 R e a d y                                  */
/******************************************************************************/
  
int XrdCmsRRQ::Ready(int Snum, const void *Key, SMask_t mask1, SMask_t mask2)
{
// EPNAME("RRQ Ready");
   XrdCmsRRQSlot *sp;

// Check if it's the right slot and it is still queued.
//
   myMutex.Lock();
   sp = &Slot[Snum];
   if (sp->Info.Key != Key || !sp->Expire)
      {myMutex.UnLock();
//     DEBUG("slot " <<Snum <<" no longer valid");
       return 1;
      }

// Update the arguments. The first is the running node mask and the second is
// a fixed differentiation mask. Accumulate the 1st but replace the 2nd.
//
   sp->Arg1 |= mask1; sp->Arg2 = mask2;
   Stats.Resp++;

// Check if we should still hold on to this slot because the number of actual
// responders is less than the number needed.
//
   if (sp->Info.actR < sp->Info.minR)
      {sp->Info.actR++; Stats.Multi++;
       myMutex.UnLock();
       return 0;
      }

// Move the element from the waiting queue to the ready queue
//
   sp->Link.Remove();
   if (readyQ.Singleton()) isReady.Post();
   readyQ.Prev()->Insert(&sp->Link);
   myMutex.UnLock();
// DEBUG("readied slot " <<Snum <<" mask " <<mask);
   return 1;
}

/******************************************************************************/
/*                               R e s p o n d                                */
/******************************************************************************/
  
void *XrdCmsRRQ::Respond()
{
// EPNAME("RRQ Respond");
   XrdCmsRRQSlot *sp;

// In an endless loop, process all ready elements
//
   do {isReady.Wait();     // DEBUG("responder awoken");
   do {myMutex.Lock();
       Stats.rdFast += rdFast; Stats.rdSlow += rdSlow;
       Stats.luFast += luFast; Stats.luSlow += luSlow;
       if (readyQ.Singleton()) {myMutex.UnLock(); break;}
       sp = readyQ.Next()->Item(); sp->Link.Remove(); sp->Expire = 0;
       myMutex.UnLock();

    // A locate request can be pggy-backed on a select request and vice-versa
    // We separate the two queues here as each has a different response.
    //
       if (sp->Info.isLU)
          {if (sp->Cont)
              {sp->Cont->Arg1 = sp->Arg1;
               sendRedResp(sp->Cont);
              }
           sendLocResp(sp);
          } else {
           if (sp->LkUp)
              {sp->LkUp->Arg1 = sp->Arg1; sp->LkUp->Arg2 = sp->Arg2;
               sendLocResp(sp->LkUp);
              }
           sendRedResp(sp);
          }
       sp->Recycle();
      } while(1);
      } while(1);

// Keep the compiler happy
//
   return (void *)0;
}

/******************************************************************************/
/*                           s e n d L o c R e s p                            */
/******************************************************************************/
  
void XrdCmsRRQ::sendLocResp(XrdCmsRRQSlot *lP)
{
   static const int ovhd = sizeof(kXR_unt32);
   XrdCmsSelected *sP;
   XrdCmsNode *nP;
   XrdCmsCluster::CmsLSOpts lsopts;
   int bytes;
   bool oksel;

// Send a delay if we timed out
//
   if (!(lP->Arg1))
      {sendLwtResp(lP);
       return;
      }

// Get the list of servers that have this file. If none found, then force the
// client to wait as this should never happen and the long path is called for.
// ASAP responses always respond in with IPv6 addresses or mapped IPv4 ones.
//
   lsopts = static_cast<XrdCmsCluster::CmsLSOpts>(lP->Info.lsLU);
   if (!(sP = Cluster.List(lP->Arg1, lsopts, oksel))
   || (!(bytes = XrdCmsNode::do_LocFmt(databuff,sP,lP->Arg2,lP->Info.rwVec))))
      {sendLwtResp(lP);
       return;
      }

// Complete the I/O vector
//
   bytes++;
   data_iov[1].iov_len  = bytes;
   bytes += ovhd;
   dataResp.Hdr.datalen = htons(static_cast<unsigned short>(bytes));
   bytes += sizeof(dataResp.Hdr);

// Send the reply to each waiting redirector
//
   RTable.Lock();
   do {if ((nP = RTable.Find(lP->Info.Rnum, lP->Info.Rinst)))
          {dataResp.Hdr.streamid = lP->Info.ID;
           nP->Send(data_iov, iov_cnt, bytes);
          }
       luFast++;
      } while((lP = lP->LkUp));
   RTable.UnLock();
}

/******************************************************************************/
/*                           s e n d L w t R e s p                            */
/******************************************************************************/
  
void XrdCmsRRQ::sendLwtResp(XrdCmsRRQSlot *rP)
{
// EPNAME("sendLwtResp");
   XrdCmsNode *nP;

// For each request, find the redirector and ask it to send a wait
//
   RTable.Lock();
do{if ((nP = RTable.Find(rP->Info.Rnum, rP->Info.Rinst)))
      {waitResp.Hdr.streamid = rP->Info.ID; luSlow++;
       nP->Send((char *)&waitResp, sizeof(waitResp));
//     DEBUG("Redirect delay " <<nP->Name() <<' ' <<Tdelay);
      }
//    else {DEBUG("redirector " <<Info->Rnum <<'.' <<Info->Rinst <<"not found");}
  } while((rP = rP->LkUp));
   RTable.UnLock();
}
  
/******************************************************************************/
/*                           s e n d R e d R e s p                            */
/******************************************************************************/
  
void XrdCmsRRQ::sendRedResp(XrdCmsRRQSlot *rP)
{
// EPNAME("sendRedResp");
   static const int ovhd = sizeof(kXR_unt32);
   XrdCmsNode *nP;
   int doredir, port, hlen;

// Determine where the client should be redirected
//
   if ((doredir = (rP->Arg1 && Cluster.Select(rP->Arg1, port, hostbuff, hlen,
                                              rP->Info.isRW, rP->Info.actR,
                                              rP->Info.ifOP))))
      {redrResp.Val = htonl(port);
       redrResp.Hdr.datalen = htons(static_cast<unsigned short>(hlen+ovhd));
       redr_iov[1].iov_len  = hlen;
       hlen += ovhd + sizeof(redrResp.Hdr);
      }

// For each request, find the redirector and ask it to send the message
//
   RTable.Lock();
do{if ((nP = RTable.Find(rP->Info.Rnum, rP->Info.Rinst)))
      {if (doredir){redrResp.Hdr.streamid = rP->Info.ID; rdFast++;
                    nP->Send(redr_iov, iov_cnt, hlen);
//                  DEBUG("Fast redirect " <<nP->Name() <<" -> " <<hostbuff);
                   }
              else {waitResp.Hdr.streamid = rP->Info.ID; rdSlow++;
                    nP->Send((char *)&waitResp, sizeof(waitResp));
//                  DEBUG("Redirect delay " <<nP->Name() <<' ' <<Tdelay);
                   }
      } 
//    else {DEBUG("redirector " <<Info->Rnum <<'.' <<Info->Rinst <<"not found");}
  } while((rP = rP->Cont));
   RTable.UnLock();
}

/******************************************************************************/
/*                               T i m e O u t                                */
/******************************************************************************/
  
void *XrdCmsRRQ::TimeOut()
{
// EPNAME("RRQ TimeOut");
   XrdCmsRRQSlot *sp;

// We measure millisecond intervals to timeout waiting requests. We used to zero
// out arg1/2 to force expiration, but they would be zero anyway if no responses
// occurred. Now with qdn we need to leave them alone as we may have defered
// a fast dispatch because we were waiting for more than one responder.
//
   while(1)
        {isWaiting.Wait();
         myMutex.Lock();
         while(1)
              {myClock++;
               myMutex.UnLock();
               XrdSysTimer::Wait(Tslice);
               myMutex.Lock();
               while((sp=waitQ.Next()->Item()) && sp->Expire < myClock)
                    {sp->Link.Remove();
                     if (readyQ.Singleton()) isReady.Post();
//                   sp->Arg1 = 0; sp->Arg2 = 0;
//                   DEBUG("expired slot " <<sp->slotNum);
                     readyQ.Prev()->Insert(&sp->Link);
                    }
               if (waitQ.Singleton()) break;
              }
         myMutex.UnLock();
        }

// Keep the compiler happy
//
   return (void *)0;
}

/******************************************************************************/
/*           X r d C m s R R Q S l o t   C l a s s   M e t h o d s            */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdCmsRRQSlot::XrdCmsRRQSlot() : Link(this)
{

   slotNum  = initSlot++;
   if (slotNum)
      {Cont     = freeSlot;
       freeSlot = this;
      } else Cont = 0;
   Arg1 = Arg2 = 0;
   Info.Key = 0;
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdCmsRRQSlot *XrdCmsRRQSlot::Alloc(XrdCmsRRQInfo *theInfo)
{
   XrdCmsRRQSlot *sp;

   myMutex.Lock();
   if ((sp = freeSlot))
      {sp->Info = *theInfo;
       freeSlot = sp->Cont;
       sp->Cont = 0;
       sp->LkUp = 0;
       sp->Arg1 = 0;
       sp->Arg2 = 0;
      }
   myMutex.UnLock();
   return sp;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdCmsRRQSlot::Recycle()
{
   XrdCmsRRQSlot *sp, *np;

   myMutex.Lock();
   if (!Link.Singleton()) Link.Remove();

// Remove items in the lookup chain first
//
   np = LkUp;
   while((sp = np))
        {np           = sp->LkUp;
         sp->Cont     = freeSlot;
         freeSlot     = sp;
         sp->Info.Key = 0;
        }

// Now remove items in the select chain
//
   np = Cont;
   while((sp = np))
        {np           = sp->Cont;
         sp->Cont     = freeSlot;
         freeSlot     = sp;
         sp->Info.Key = 0;
        }

// Now put this item in the free chain
//
   Info.Key = 0;
   Cont     = freeSlot;
   freeSlot = this;
   myMutex.UnLock();
}
