/******************************************************************************/
/*                                                                            */
/*                         X r d R m c D a t a . c c                          */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <cstring>

#include "XrdRmc/XrdRmc.hh"
#include "XrdRmc/XrdRmcData.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdRmcData::XrdRmcData(XrdRmcReal *cP, XrdOucCacheIO *ioP,
                       long long   vn, int            opts)
                      : pPLock(0), rPLock(0),  wPLock(0),
                        Cache(cP), ioObj(ioP), VNum(vn)
{
// We need to map the cache options to our local options
//
   isFIS = (opts & XrdOucCache::optFIS ?    1 : 0);
   isRW  = (opts & XrdOucCache::optRW  ? okRW : 0);

// Copy some values from the cache to our local area for convenience
//
   SegShft  = Cache->SegShft;
   OffMask  = Cache->OffMask;
   SegSize  = Cache->SegSize;
   maxCache = Cache->maxCache;
   Debug    = Cache->Dbg;

// Initialize the pre-read area
//
   memset(prRR,  -1, sizeof(prRR) );
   memset(prBeg, -1, sizeof(prBeg));
   memset(prEnd, -1, sizeof(prEnd));
   memset(prOpt,  0, sizeof(prOpt));

   prNSS      =-1;
   prRRNow    = 0;
   prStop     = 0;
   prNext     = prFree = 0;
   prActive   = 0;
   prOK       = (Cache->prNum ? 1 : 0);
   prReq.Data = this;
   prAuto     = (prOK ? setAPR(Apr, Cache->aprDefault, SegSize) : 0);
   prPerf     = 0;
   prCalc     = Apr.prRecalc;

// Establish serialization options
//
   if (Cache->Options & XrdRmc::ioMTSafe) pPLopt = rPLopt = xs_Shared;
      else pPLopt = rPLopt = xs_Exclusive;

// Establish serialization handling (only needed for r/w files)
//
   if (Cache->Options & XrdRmc::Serialized)
      {if (Cache->Options & XrdRmc::ioMTSafe)
          {if (isRW && prOK) pPLock = wPLock = &rwLock;}
          else if     (prOK) rPLock = pPLock = wPLock = &rwLock;
      } else if (!(Cache->Options & XrdRmc::ioMTSafe) || isRW)
                rPLock = pPLock = wPLock = &rwLock;
}
  
/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/

bool XrdRmcData::Detach(XrdOucCacheIOCD &iocd)
{
   int delOK;

// We must wait for any pre-reads to stop at this point. TO DO: We really
// should run this in a sperate thread and use the callback mechanism.
//
   DMutex.Lock();
   if (prActive)
      {XrdSysSemaphore prDone(0);
       prStop = &prDone;
       DMutex.UnLock();
       prDone.Wait();
       DMutex.Lock();
      }

// Get exclusive control
//
   rwLock.Lock(xs_Exclusive);

// We can now detach ourselves from the cache
//
   delOK = Cache->Detach(ioObj);
   DMutex.UnLock();
   rwLock.UnLock(xs_Exclusive);

// Check if we should delete ourselves and if so add our stats to the cache
//
   if (delOK)
      {Cache->Statistics.Add(Statistics);
       if (Cache->Lgs)
          {char sBuff[4096];
           snprintf(sBuff, sizeof(sBuff),
                          "Cache: Stats: %lld Read; %lld Get; %lld Pass; "
                          "%lld Write; %lld Put; %lld Hits; %lld Miss; "
                          "%lld pead; %lld HitsPR; %lld MissPR; Path %s\n",
                          Statistics.X.BytesRead, Statistics.X.BytesGet,
                          Statistics.X.BytesPass, Statistics.X.BytesWrite,
                          Statistics.X.BytesPut,
                          Statistics.X.Hits,      Statistics.X.Miss,
                          Statistics.X.BytesPead,
                          Statistics.X.HitsPR,    Statistics.X.MissPR,
                          ioObj->Path());
           std::cerr <<sBuff;
          }
       delete this;
       return true;
      }
// TO DO: We should issue a message here as this will cause a memory leak
//        as we won't try to do the detavh again.
//
   return false;
}

/******************************************************************************/
/*                               P r e r e a d                                */
/******************************************************************************/

void XrdRmcData::Preread()
{
   MrSw EnforceMrSw(pPLock, pPLopt);
   long long segBeg, segEnd;
   int       oVal, pVal = 0, rLen, noIO, bPead = 0, prPages = 0;
   char *cBuff;

// Check if we are stopping, if so, ignore this request
//
   DMutex.Lock();
   if (prStop)
      {prActive = 0;
       prStop->Post();
       DMutex.UnLock();
       return;
      }

// Do the next pre-read in the queue (it's possible another may get in)
//
do{if ((oVal = prOpt[prNext]))
      {segBeg = prBeg[prNext]; segEnd = prEnd[prNext];
       prOpt[prNext++] = 0;
       if (prNext >= prMax) prNext = 0;
       if (oVal == prSKIP) continue;
       prActive = prRun;
       if (Debug > 1) std::cerr <<"prD: beg " <<(VNum >>XrdRmcReal::Shift) <<' '
                           <<(segEnd-segBeg+1)*SegSize <<'@' <<(segBeg*SegSize)
                           <<" f=" <<int(oVal) <<' ' <<ioObj->Path() <<std::endl;
       DMutex.UnLock();
       oVal = (oVal == prSUSE ? XrdRmcSlot::isSUSE : 0) | XrdRmcSlot::isNew;
       segBeg |= VNum; segEnd |= VNum;
       do {if ((cBuff = Cache->Get(ioObj, segBeg, rLen, noIO)))
              {if (noIO)  pVal = 0;
                  else   {pVal = oVal; bPead += rLen; prPages++;}
              }
          } while(cBuff && Cache->Ref(cBuff, 0, pVal) && segBeg++ < segEnd);
       if (Debug > 1) std::cerr <<"PrD: end " <<(VNum >>XrdRmcReal::Shift)
                           <<' ' <<prPages <<" pgs " <<bPead <<std::endl;
       if (bPead)
          {Statistics.Lock();
           Statistics.X.BytesPead += bPead;
           Statistics.X.MissPR    += prPages;
           Statistics.UnLock();
          }
       DMutex.Lock();
      }
   } while(oVal);

// See if we should schedule the next preread or stop
//
   if (prStop)
      {prActive = 0;
       prStop->Post();
      } else if (prOpt[prNext])
                {prActive = prWait;
                 Cache->PreRead(&prReq);
                } else prActive = 0;

// All done here
//
   DMutex.UnLock();
}

/******************************************************************************/

void XrdRmcData::Preread(long long Offs, int rLen, int Opts)
{
   int How;

// Determine how to place the pages. We do this via assignment to avoid a gcc
// bug that doesn't optimize out static const int's except via assignment.
//
   if (Opts & SingleUse) How = prSUSE;
      else How = prLRU;

// Verify that this preread will succeed then schedule it if so
//
   if (prOK && rLen > 0 && Offs > 0
   &&  Offs < XrdRmcReal::MaxFO && (Offs + rLen) < XrdRmcReal::MaxFO) return;
      QueuePR(Offs>>SegShft, rLen, How);
}

/******************************************************************************/

void XrdRmcData::Preread(aprParms &Parms)
{

// Establish the new feature set if prereads are enabled
//
   if (prOK)
      {DMutex.Lock();
       prAuto = setAPR(Apr, Parms, SegSize);
       DMutex.UnLock();
      }
}

/******************************************************************************/
/*                               Q u e u e P R                                */
/******************************************************************************/

void XrdRmcData::QueuePR(long long segBeg, int rLen, int prHow, int isAuto)
{
   XrdSysMutexHelper Monitor(&DMutex);
   long long segCnt, segEnd;
   int i;

// Scuttle everything if we are stopping
//
   if (Debug) std::cerr <<"prQ: req " <<rLen <<'@' <<(segBeg*SegSize) <<std::endl;
   if (prStop) return;

// Verify that this offset is not in the table of recent offsets
//
   for (i = 0; i < prRRMax; i++) if (prRR[i] == segBeg) return;

// Compute number of pages to preread. If none, we are done.
//
   segCnt = rLen/SegSize + ((rLen & OffMask) != 0);
   if (prHow == prLRU)
      {if (segCnt < Apr.minPages) segCnt = Apr.minPages;
       if (!segCnt) return;
      }

// Compute last segment to read
//
   segEnd = segBeg + segCnt - 1;

// Run through the preread queue and check if we have this block scheduled or
// we completed the block in the recent past. We do not catch overlapping
// prereads, they will need to go through the standard fault mechanism).
//
   for (i = 0; i < prMax; i++)
       if (segBeg == prBeg[i] || (segBeg >  prBeg[i] && segEnd <= prEnd[i]))
          {if (prHow == prSKIP)
              {if (Debug) std::cerr <<"pDQ: " <<rLen <<'@' <<(segBeg*SegSize) <<std::endl;
               prOpt[i] = prSKIP;
              }
           return;
          }

// Return if this is a cancellation request
//
   if (prHow == prSKIP) return;

// At this point check if we need to recalculate stats
//
   if (prAuto && prCalc && Statistics.X.BytesPead > prCalc)
      {int crPerf;
       Statistics.Lock();
       prCalc = Statistics.X.BytesPead + Apr.prRecalc;
       crPerf = (Statistics.X.MissPR?
                    (Statistics.X.HitsPR*100)/Statistics.X.MissPR : 0);
       Statistics.UnLock();
       if (Debug) std::cerr <<"PrD: perf " <<crPerf <<"% " <<ioObj->Path() <<std::endl;
       if (prPerf >= 0)
          {if ( crPerf < Apr.minPerf && prPerf < Apr.minPerf
           &&  (crPerf <= prPerf || crPerf <= prPerf*2))
              {if (Debug) std::cerr <<"PrD: Disabled for " <<ioObj->Path() <<std::endl;
               prAuto = 0;
               if (isAuto) return;
              }
          }
        prPerf = crPerf;
       }

// Add this read to the queue
//
   if (prFree == prNext && prOpt[prNext]) prNext = (prNext+1)%prMax;
   prBeg[prFree]   = segBeg; prEnd[prFree] = segEnd;
   prOpt[prFree++] = prHow;
   if (prFree >= prMax) prFree = 0;

// If nothing pending then activate a preread
//
   if (Debug) std::cerr <<"prQ: add " <<rLen <<'@' <<(segBeg*SegSize) <<std::endl;
   if (!prActive) {prActive = prWait; Cache->PreRead(&prReq);}
}
  
/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

int XrdRmcData::Read(char *Buff, long long Offs, int rLen)
{
   MrSw EnforceMrSw(rPLock, rPLopt);
   XrdOucCacheStats Now;
   char *cBuff, *Dest = Buff;
   long long segOff, segNum = (Offs >> SegShft);
   int noIO, rAmt, rGot, doPR = prAuto, rLeft = rLen;

// Verify read length and offset
//
   if (rLen <= 0) return 0;
   if (XrdRmcReal::MaxFO <  Offs || Offs < 0
   ||  XrdRmcReal::MaxFO < (Offs + rLen)) return -EOVERFLOW;

// Check for preread request and Determine how to place the pages.
//
   if (!Buff)
      {int How;
       if (rLen > maxCache) How = prSUSE;
          else How = prLRU;
       QueuePR(segNum, rLen, How);
       return 0;
      }

// Ignore caching it if it's too large. Use alternate read algorithm.
//
   if (rLen > maxCache) return Read(Now, Buff, Offs, rLen);

// We check now whether or not we will try to do a preread later. This is
// advisory at this point so we don't need to obtain any locks to do this.
//
   if (doPR)
      {if (rLen >= Apr.Trigger) doPR = 0;
          else for (noIO = 0; noIO < prRRMax; noIO++)
                   if (prRR[noIO] == segNum) {doPR = 0; break;}
       if (doPR)
          {DMutex.Lock();
           prRR[prRRNow] = segNum;
           prRRNow = (prRRNow+1)%prRRMax;
           DMutex.UnLock();
          }
      }
   if (Debug > 1) std::cerr <<"Rdr: " <<rLen <<'@' <<Offs <<" pr=" <<doPR <<std::endl;

// Get the segment pointer, offset and the initial read amount
//
   segNum|= VNum;
   segOff = Offs & OffMask;
   rAmt   = SegSize - segOff;
   if (rAmt > rLen) rAmt = rLen;

// Now fault the pages in
//
   while((cBuff = Cache->Get(ioObj, segNum, rGot, noIO)))
        {if (rGot <= segOff + rAmt) rAmt = (rGot <= segOff ? 0 : rGot-segOff);
         if (rAmt) {memcpy(Dest, cBuff+segOff, rAmt);
                    Dest += rAmt; Offs += rAmt; Now.X.BytesGet += rGot;
                   }
         if (noIO) {Now.X.Hits++; if (noIO < 0) Now.X.HitsPR++;}
            else   {Now.X.Miss++; Now.X.BytesRead  += rAmt;}
         if (!(Cache->Ref(cBuff, (isFIS ? rAmt : 0)))) {doPR = 0; break;}
         segNum++; segOff = 0;
         if ((rLeft -= rAmt) <= 0) break;
         rAmt = (rLeft <= SegSize ? rLeft : SegSize);
        }

// Update stats
//
   Statistics.Add(Now);

// See if a preread needs to be done. We will only do this if no errors occurred
//
   if (doPR && cBuff)
      {EnforceMrSw.UnLock();
       QueuePR(segNum, rLen, prLRU, 1);
      }

// All done, if we ended fine, return amount read. If there is no page buffer
// then the cache returned the error in the amount present variable.
//
   if (Debug > 1) std::cerr <<"Rdr: ret " <<(cBuff ? Dest-Buff : rGot) <<" hits "
                       <<Now.X.Hits <<" pr " <<Now.X.HitsPR <<std::endl;
   return (cBuff ? Dest-Buff : rGot);
}

/******************************************************************************/

int XrdRmcData::Read(XrdOucCacheStats &Now,
                      char *Buff, long long  Offs, int rLen)
{
   char *cBuff, *Dest = Buff;
   long long segOff, segNum;
   int noIO, rAmt, rGot, rIO, rPend = 0, rLeft = rLen;

// Get the segment pointer, offset and the initial read amount
//
   segNum = (Offs >> SegShft) | VNum;
   segOff = Offs & OffMask;
   rAmt   = SegSize - segOff;
   if (rAmt > rLen) rAmt = rLen;
   if (Debug > 1) std::cerr <<"Rdr: " <<rLen <<'@' <<Offs <<" pr=" <<prOK <<std::endl;

// Optimize here when this is R/O and prereads are disabled. Otherwise, cancel
// any pre-read for this read as we will be bypassing the cache.
//
   if (!isRW && !prOK)
      {if ((rIO = ioObj->Read(Dest, Offs, rLen)) > 0)
          Statistics.Add(Statistics.X.BytesPass, rLen);
       return rIO;
      } else if (prOK) QueuePR(Offs >> SegShft, rLen, prSKIP);

// Here we try to get as much data from the cache but otherwise we will
// issue the longest read possible.
//
do{if ((cBuff = Cache->Get(0, segNum, rGot, noIO)))
      {if (rPend)
          {if ((rIO = ioObj->Read(Dest, Offs, rPend)) < 0) return rIO;
           Now.X.BytesPass += rIO; Dest += rIO; Offs += rIO; rPend = 0;
          }
       if (rGot <= segOff + rAmt) rAmt = (rGot <= segOff ? 0 : rGot-segOff);
       if (rAmt) {memcpy(Dest, cBuff+segOff, rAmt);
                  Dest += rAmt; Offs += rAmt; Now.X.Hits++;
                  Now.X.BytesGet += rAmt;
                 }
       if (noIO < 0) Now.X.HitsPR++;
       if (!(Cache->Ref(cBuff, (isFIS ? rAmt : 0)))) break;
      } else rPend += rAmt;

   if ((rLeft -= rAmt) <= 0) break;
   rAmt = (rLeft <= SegSize ? rLeft : SegSize);
   segNum++; segOff = 0;
  } while(1);

// Finish any outstanding read
//
   if (rPend)
      {if ((rIO = ioObj->Read(Dest, Offs, rPend)) < 0) return rIO;
       Now.X.BytesPass += rIO; Dest += rIO;
      }

// Update stats and return read length
//
   if (Debug > 1) std::cerr <<"Rdr: ret " <<(Dest-Buff) <<" hits " <<Now.X.Hits
                       <<" pr " <<Now.X.HitsPR <<std::endl;
   Statistics.Add(Now);
   return Dest-Buff;
}

/******************************************************************************/
/*                                s e t A P R                                 */
/******************************************************************************/

int XrdRmcData::setAPR(aprParms &Dest, aprParms &Src, int pSize)
{

// Copy over the information
//
   Dest = Src;

// Fix it up as needed
//
   if (Dest.Trigger  <= 0) Dest.Trigger  = (Dest.minPages ? pSize + 1: 0);
   if (Dest.prRecalc <= 0) Dest.prRecalc = (Dest.prRecalc ? 52428800 : 0);
   if (Dest.minPages <  0) Dest.minPages = 0;
   if (Dest.minPerf  <  0) Dest.minPerf  = 0;
   if (Dest.minPerf  >100) Dest.minPerf  = 100;


// Indicate whether anything can be preread
//
   return (Dest.minPages > 0 && Dest.Trigger > 1);
}

/******************************************************************************/
/*                                 T r u n c                                  */
/******************************************************************************/

int XrdRmcData::Trunc(long long Offs)
{
   MrSw EnforceMrSw(wPLock, xs_Exclusive);

// Verify that we can modify this cache and the trunc offset
//
   if (!isRW) return -EROFS;
   if (Offs > XrdRmcReal::MaxFO || Offs < 0) return -EOVERFLOW;

// Get the segment pointer and truncate pages from the cache
//
   Cache->Trunc(ioObj, (Offs >> SegShft) | VNum);

// Now just return the result of actually doing the truncate
//
   return ioObj->Trunc(Offs);
}
  
/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/

int XrdRmcData::Write(char *Buff, long long Offs, int wLen)
{
   MrSw EnforceMrSw(wPLock, xs_Exclusive);
   XrdOucCacheStats Now;
   char *cBuff, *Src = Buff;
   long long segOff, segNum;
   int noIO, wAmt, rGot, wLeft = wLen;

// Verify write length, ability, and buffer
//
   if (wLen <= 0) return 0;
   if (!isRW)     return -EROFS;
   if (!Buff)     return -EINVAL;
   if (XrdRmcReal::MaxFO <  Offs || Offs < 0
   ||  XrdRmcReal::MaxFO < (Offs + wLen)) return -EOVERFLOW;

// First step is to write out all the data (write-through for now)
//
   if ((wAmt = ioObj->Write(Buff, Offs, wLen)) != wLen)
      return (wAmt < 0 ? wAmt : -EIO);
   Now.X.BytesWrite = wLen;

// Get the segment pointer, offset and the initial write amount
//
   segNum = (Offs >> SegShft) | VNum;
   segOff = Offs & OffMask;
   wAmt   = SegSize - segOff;
   if (wAmt > wLen) wAmt = wLen;

// Now update any pages that are actually in the cache
//
do{if (!(cBuff = Cache->Get(0, segNum, rGot, noIO))) Now.X.Miss++;
      else {memcpy(cBuff+segOff, Src, wAmt);
            Now.X.BytesPut += wAmt; Now.X.Hits++;
            if (noIO < 0) Now.X.HitsPR++;
            Cache->Upd(cBuff, wAmt, segOff);
           }
   Src += wAmt;
   if ((wLeft -= wAmt) <= 0) break;
   wAmt = (wLeft <= SegSize ? wLeft : SegSize);
   segNum++; segOff = 0;
  } while(1);

// Update stats and return how much we wrote
//
   Statistics.Add(Now);
   return wLen;
}
