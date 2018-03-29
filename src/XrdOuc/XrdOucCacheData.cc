/******************************************************************************/
/*                                                                            */
/*                    X r d O u c C a c h e D a t a . h h                     */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/* The XrdOucCacheData object defines a remanufactured XrdOucCacheIO object and
   is used to front a XrdOucCacheIO object with an XrdOucCacheReal object.
*/

#include <stdio.h>
#include <string.h>

#include "XrdOuc/XrdOucCacheData.hh"
#include "XrdSys/XrdSysHeaders.hh"

/******************************************************************************/
/*                        X r d O u c C a c h e Z I O                         */
/******************************************************************************/

class XrdOucCacheZIO : public XrdOucCacheIO
{
public:

const char *Path() {return "ZIO";}

int         Read(char *Buff, long long  Off,  int  Len)
                {memset(Buff, 0, Len); return Len;}

int         Sync() {return 0;}

int         Trunc(long long Offset) {return 0;}

int        Write(char *Buff, long long  Off,  int  Len) {return Len;}
};

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOucCacheData::XrdOucCacheData(XrdOucCacheReal *cP, XrdOucCacheIO *ioP,
                                 long long    vn,     int            opts)
                                : pPLock(0), rPLock(0),  wPLock(0),
                                  Cache(cP), ioObj(ioP), VNum(vn)
{
// We need to map the cache options to our local options
//
   isADB = (opts & XrdOucCache::optADB ?    1 : 0);
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
   if (Cache->Options & XrdOucCache::ioMTSafe) pPLopt = rPLopt = xs_Shared;
      else pPLopt = rPLopt = xs_Exclusive;


// Establish serialization handling (only needed for r/w files)
//
   if (Cache->Options & XrdOucCache::Serialized)
      {if (Cache->Options & XrdOucCache::ioMTSafe)
          {if (isRW && prOK) pPLock = wPLock = &rwLock;}
          else if     (prOK) rPLock = pPLock = wPLock = &rwLock;
      } else if (!(Cache->Options & XrdOucCache::ioMTSafe) || isRW)
                rPLock = pPLock = wPLock = &rwLock;
}
  
/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/

XrdOucCacheIO *XrdOucCacheData::Detach()
{
   XrdOucCacheIO *RetVal;

// We must wait for any pre-reads to stop at this point
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
   RetVal = (Cache->Detach(ioObj) ? ioObj : 0);
   DMutex.UnLock();
   rwLock.UnLock(xs_Exclusive);

// Check if we should delete ourselves and if so add our stats to the cache
//
   if (RetVal)
      {Cache->Stats.Add(Statistics);
       if (Cache->Lgs)
          {char sBuff[4096];
           snprintf(sBuff, sizeof(sBuff),
                          "Cache: Stats: %lld Read; %lld Get; %lld Pass; "
                          "%lld Write; %lld Put; %d Hits; %d Miss; "
                          "%lld pead; %d HitsPR; %d MissPR; Path %s\n",
                          Statistics.BytesRead, Statistics.BytesGet,
                          Statistics.BytesPass, Statistics.BytesWrite,
                          Statistics.BytesPut,
                          Statistics.Hits,      Statistics.Miss,
                          Statistics.BytesPead,
                          Statistics.HitsPR,    Statistics.MissPR,
                          ioObj->Path());
           cerr <<sBuff;
          }
       if (isADB) {delete ioObj; RetVal = 0;}
       delete this;
      }
   return RetVal;
}

/******************************************************************************/
/*                               P r e r e a d                                */
/******************************************************************************/

void XrdOucCacheData::Preread()
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
       if (Debug > 1) cerr <<"prD: beg " <<(VNum >>XrdOucCacheReal::Shift) <<' '
                           <<(segEnd-segBeg+1)*SegSize <<'@' <<(segBeg*SegSize)
                           <<" f=" <<int(oVal) <<' ' <<ioObj->Path() <<endl;
       DMutex.UnLock();
       oVal = (oVal == prSUSE ? XrdOucCacheSlot::isSUSE : 0)
            | XrdOucCacheSlot::isNew;
       segBeg |= VNum; segEnd |= VNum;
       do {if ((cBuff = Cache->Get(ioObj, segBeg, rLen, noIO)))
              {if (noIO)  pVal = 0;
                  else   {pVal = oVal; bPead += rLen; prPages++;}
              }
          } while(cBuff && Cache->Ref(cBuff, 0, pVal) && segBeg++ < segEnd);
       if (Debug > 1) cerr <<"PrD: end " <<(VNum >>XrdOucCacheReal::Shift)
                           <<' ' <<prPages <<" pgs " <<bPead <<endl;
       if (bPead)
          {Statistics.Lock();
           Statistics.BytesPead += bPead;
           Statistics.MissPR    += prPages;
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

void XrdOucCacheData::Preread(long long Offs, int rLen, int Opts)
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
   &&  Offs < XrdOucCacheReal::MaxFO && (Offs + rLen) < XrdOucCacheReal::MaxFO) return;
      QueuePR(Offs>>SegShft, rLen, How);
}

/******************************************************************************/

void XrdOucCacheData::Preread(aprParms &Parms)
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

void XrdOucCacheData::QueuePR(long long segBeg, int rLen, int prHow, int isAuto)
{
   XrdSysMutexHelper Monitor(&DMutex);
   long long segCnt, segEnd;
   int i;

// Scuttle everything if we are stopping
//
   if (Debug) cerr <<"prQ: req " <<rLen <<'@' <<(segBeg*SegSize) <<endl;
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
              {if (Debug) cerr <<"pDQ: " <<rLen <<'@' <<(segBeg*SegSize) <<endl;
               prOpt[i] = prSKIP;
              }
           return;
          }

// Return if this is a cancellation request
//
   if (prHow == prSKIP) return;

// At this point check if we need to recalculate stats
//
   if (prAuto && prCalc && Statistics.BytesPead > prCalc)
      {int crPerf;
       Statistics.Lock();
       prCalc = Statistics.BytesPead + Apr.prRecalc;
       crPerf = (Statistics.MissPR?(Statistics.HitsPR*100)/Statistics.MissPR:0);
       Statistics.UnLock();
       if (Debug) cerr <<"PrD: perf " <<crPerf <<"% " <<ioObj->Path() <<endl;
       if (prPerf >= 0)
          {if ( crPerf < Apr.minPerf && prPerf < Apr.minPerf
           &&  (crPerf <= prPerf || crPerf <= prPerf*2))
              {if (Debug) cerr <<"PrD: Disabled for " <<ioObj->Path() <<endl;
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
   if (Debug) cerr <<"prQ: add " <<rLen <<'@' <<(segBeg*SegSize) <<endl;
   if (!prActive) {prActive = prWait; Cache->PreRead(&prReq);}
}
  
/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

int XrdOucCacheData::Read(char *Buff, long long Offs, int rLen)
{
   MrSw EnforceMrSw(rPLock, rPLopt);
   XrdOucCacheStats Now;
   char *cBuff, *Dest = Buff;
   long long segOff, segNum = (Offs >> SegShft);
   int noIO, rAmt, rGot, doPR = prAuto, rLeft = rLen;

// Verify read length and offset
//
   if (rLen <= 0) return 0;
   if (XrdOucCacheReal::MaxFO <  Offs || Offs < 0
   ||  XrdOucCacheReal::MaxFO < (Offs + rLen)) return -EOVERFLOW;

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
   if (Debug > 1) cerr <<"Rdr: " <<rLen <<'@' <<Offs <<" pr=" <<doPR <<endl;

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
                    Dest += rAmt; Offs += rAmt; Now.BytesGet += rGot;
                   }
         if (noIO) {Now.Hits++; if (noIO < 0) Now.HitsPR++;}
            else   {Now.Miss++; Now.BytesRead  += rAmt;}
         if (!(Cache->Ref(cBuff, (isFIS ? rAmt : 0)))) {doPR = 0; break;}
         segNum++; segOff = 0;
         if ((rLeft -= rAmt) <= 0) break;
         rAmt = (rLeft <= SegSize ? rLeft : SegSize);
        }

// Update stats
//
   Statistics.Add(Now);

// See if a preread needs to be done. We will only do this if no errors occured
//
   if (doPR && cBuff)
      {EnforceMrSw.UnLock();
       QueuePR(segNum, rLen, prLRU, 1);
      }

// All done, if we ended fine, return amount read. If there is no page buffer
// then the cache returned the error in the amount present variable.
//
   if (Debug > 1) cerr <<"Rdr: ret " <<(cBuff ? Dest-Buff : rGot) <<" hits "
                       <<Now.Hits <<" pr " <<Now.HitsPR <<endl;
   return (cBuff ? Dest-Buff : rGot);
}

/******************************************************************************/

int XrdOucCacheData::Read(XrdOucCacheStats &Now,
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
   if (Debug > 1) cerr <<"Rdr: " <<rLen <<'@' <<Offs <<" pr=" <<prOK <<endl;

// Optimize here when this is R/O and prereads are disabled. Otherwise, cancel
// any pre-read for this read as we will be bypassing the cache.
//
   if (!isRW && !prOK)
      {if ((rIO = ioObj->Read(Dest, Offs, rLen)) > 0)
          Statistics.Add(Statistics.BytesPass, rLen);
       return rIO;
      } else if (prOK) QueuePR(Offs >> SegShft, rLen, prSKIP);

// Here we try to get as much data from the cache but otherwise we will
// issue the longest read possible.
//
do{if ((cBuff = Cache->Get(0, segNum, rGot, noIO)))
      {if (rPend)
          {if ((rIO = ioObj->Read(Dest, Offs, rPend)) < 0) return rIO;
           Now.BytesPass += rIO; Dest += rIO; Offs += rIO; rPend = 0;
          }
       if (rGot <= segOff + rAmt) rAmt = (rGot <= segOff ? 0 : rGot-segOff);
       if (rAmt) {memcpy(Dest, cBuff+segOff, rAmt);
                  Dest += rAmt; Offs += rAmt; Now.Hits++; Now.BytesGet += rAmt;
                 }
       if (noIO < 0) Now.HitsPR++;
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
       Now.BytesPass += rIO; Dest += rIO;
      }

// Update stats and return read length
//
   if (Debug > 1) cerr <<"Rdr: ret " <<(Dest-Buff) <<" hits " <<Now.Hits
                       <<" pr " <<Now.HitsPR <<endl;
   Statistics.Add(Now);
   return Dest-Buff;
}

/******************************************************************************/
/*                                s e t A P R                                 */
/******************************************************************************/

int XrdOucCacheData::setAPR(aprParms &Dest, aprParms &Src, int pSize)
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

int XrdOucCacheData::Trunc(long long Offs)
{
   MrSw EnforceMrSw(wPLock, xs_Exclusive);

// Verify that we can modify this cache and the trunc offset
//
   if (!isRW) return -EROFS;
   if (Offs > XrdOucCacheReal::MaxFO || Offs < 0) return -EOVERFLOW;

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

int XrdOucCacheData::Write(char *Buff, long long Offs, int wLen)
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
   if (XrdOucCacheReal::MaxFO <  Offs || Offs < 0
   ||  XrdOucCacheReal::MaxFO < (Offs + wLen)) return -EOVERFLOW;

// First step is to write out all the data (write-through for now)
//
   if ((wAmt = ioObj->Write(Buff, Offs, wLen)) != wLen)
      return (wAmt < 0 ? wAmt : -EIO);
   Now.BytesWrite = wLen;

// Get the segment pointer, offset and the initial write amount
//
   segNum = (Offs >> SegShft) | VNum;
   segOff = Offs & OffMask;
   wAmt   = SegSize - segOff;
   if (wAmt > wLen) wAmt = wLen;

// Now update any pages that are actually in the cache
//
do{if (!(cBuff = Cache->Get(0, segNum, rGot, noIO))) Now.Miss++;
      else {memcpy(cBuff+segOff, Src, wAmt);
            Now.BytesPut += wAmt; Now.Hits++;
            if (noIO < 0) Now.HitsPR++;
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
