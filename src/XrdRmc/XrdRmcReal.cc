/******************************************************************************/
/*                                                                            */
/*                         X r d R m c R e a l . c c                          */
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "XrdRmc/XrdRmcData.hh"
#include "XrdSys/XrdSysHeaders.hh"
  
#ifdef __APPLE__
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

void *XrdRmcRealPRXeq(void *parg)
{   XrdRmcReal *cP = (XrdRmcReal *)parg;
    cP->PreRead();
    return (void *)0;
}
  
XrdRmcReal::XrdRmcReal(int &rc, XrdRmc::Parms &ParmV,
                       XrdOucCacheIO::aprParms *aprP)
                : XrdOucCache("rmc"),
                  Slots(0), Slash(0), Base((char *)MAP_FAILED), Dbg(0), Lgs(0),
                  AZero(0), Attached(0), prFirst(0), prLast(0),
                  prReady(0), prStop(0), prNum(0)
{
   size_t Bytes;
   int n, minPag, isServ = ParmV.Options & XrdRmc::isServer;

// Copy over options
//
   rc = ENOMEM;
   Options = ParmV.Options;
   if (Options & XrdRmc::Debug)    Lgs = Dbg = (Options & XrdRmc::Debug);
   if (Options & XrdRmc::logStats) Lgs = 1;
   minPag = (ParmV.minPages <= 0 ? 256 : ParmV.minPages);

// Establish maximum number of attached files
//
   if (ParmV.MaxFiles <= 0) maxFiles = (isServ ? 16384 : 256);
      else {maxFiles = (ParmV.MaxFiles > 32764 ? 32764 : ParmV.MaxFiles);
            maxFiles = maxFiles/sizeof(int)*sizeof(int);
            if (!maxFiles) maxFiles = 256;
           }

// Adjust segment size to be a power of two and atleast 4k.
//
        if (ParmV.PageSize <= 0) SegSize = 32768;
   else if (!(SegSize = ParmV.PageSize & ~0xfff)) SegSize = 4096;
// else if (SegSize > 16*1024*1024) SegSize = 16*1024*1024;
   SegShft = 0; n = SegSize-1;
   while(n) {SegShft++; n = n >> 1;}
   SegSize = 1<<SegShft;

// The max to cache is also adjusted accrodingly based on segment size.
//
   OffMask = SegSize-1;
   SegCnt = (ParmV.CacheSize > 0 ? ParmV.CacheSize : 104857600)/SegSize;
   if (SegCnt < minPag) SegCnt = minPag;
   if (ParmV.Max2Cache < SegSize) maxCache = SegSize;
      else maxCache = ParmV.Max2Cache/SegSize*SegSize;
   SegFull = (Options & XrdRmc::isServer ? XrdRmcSlot::lenMask : SegSize);

// Allocate the cache plus the cache hash table
//
   Bytes = static_cast<size_t>(SegSize)*SegCnt;
   Base = (char *)mmap(0, Bytes + SegCnt*sizeof(int), PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
   if (Base == MAP_FAILED) {rc = errno; return;}
   Slash = (int *)(Base + Bytes); HNum = SegCnt/2*2-1;

// Now allocate the actual slots. We add additional slots to map files. These
// do not have any memory backing but serve as anchors for memory mappings.
//
   if (!(Slots = new XrdRmcSlot[SegCnt+maxFiles])) return;
   XrdRmcSlot::Init(Slots, SegCnt);

// Set pointers to be able to keep track of CacheIO objects and map them to
// CacheData objects. The hash table will be the first page of slot memory.
//
   hTab = (int *)Base;
   hMax = SegSize/sizeof(int);
   sBeg = sFree = SegCnt;
   sEnd = SegCnt + maxFiles;

// Now iniialize the slots to be used for the CacheIO objects
//
   for (n = sBeg; n < sEnd; n++)
       {Slots[n].Own.Next = Slots[n].Own.Prev = n;
        Slots[n].HLink = n+1;
       }
   Slots[sEnd-1].HLink = 0;

// Setup the pre-readers if pre-read is enabled
//
   if (Options & XrdRmc::canPreRead)
      {pthread_t tid;
       n = (Options & XrdRmc::isServer ? 9 : 3);
       while(n--)
            {if (XrdSysThread::Run(&tid, XrdRmcRealPRXeq, (void *)this,
                                   0, "Prereader")) break;
             prNum++;
            }
       if (aprP && prNum) XrdRmcData::setAPR(aprDefault, *aprP, SegSize);
      }

// All done
//
   rc = 0;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdRmcReal::~XrdRmcReal()
{
// Wait for all attachers to go away
//
   CMutex.Lock();
   if (Attached)
      {XrdSysSemaphore aZero(0);
       AZero = &aZero;
       CMutex.UnLock();
       aZero.Wait();
       CMutex.Lock();
      }

// If any preread threads exist, then stop them now
//
   prMutex.Lock();
   if (prNum)
      {XrdSysSemaphore prDone(0);
       prStop = &prDone;
       prReady.Post();
       prMutex.UnLock();
       prDone.Wait();
       prMutex.Lock();
      }

// Delete the slots
//
   delete Slots; Slots = 0;

// Unmap cache memory and associated hash table
//
   if (Base != MAP_FAILED)
      {munmap(Base, static_cast<size_t>(SegSize)*SegCnt);
       Base = (char *)(MAP_FAILED);
      }

// Release all locks, we are done
//
   prMutex.UnLock();
   CMutex.UnLock();
}

/******************************************************************************/
/*                                A t t a c h                                 */
/******************************************************************************/

XrdOucCacheIO *XrdRmcReal::Attach(XrdOucCacheIO *ioP, int Opts)
{
   static int Inst = 0;
   XrdSysMutexHelper Monitor(CMutex);
   XrdRmcData  *dP;
   int Cnt, Fnum = 0, theOpts = Opts & optRW;

// Check if we are being deleted
//
   if (AZero) {errno = ECANCELED; return ioP;}

// Setup structured/unstructured option
//
   if ((Opts & optFIS) || (Options & XrdRmc::isStructured)) theOpts |= optFIS;

// Get an entry in the filename table.
//
   if (!(Cnt = ioAdd(ioP, Fnum)))
      {errno = EMFILE;
       return ioP;
      }

// If this is the first addition then we need to get a new CacheData object.
// Otherwise, simply reuse the previous cache data object.
//
   if (Cnt != 1) dP = Slots[Fnum].Status.Data;
      else {long long vNum =  static_cast<long long>(Fnum-SegCnt) <<  Shift
                           | (static_cast<long long>(Inst) << (Shift - 16));
            Inst = (Inst+1) & 0xffff;
            if ((dP = new XrdRmcData(this, ioP, vNum, theOpts)))
               {Attached++; Slots[Fnum].Status.Data = dP;}
           }

// Some debugging
//
   if (Dbg) cerr <<"Cache: Attached " <<Cnt <<'/' <<Attached <<' '
                 <<std::hex << Fnum <<std::dec <<' ' <<ioP->Path() <<endl;

// All done
//
   if (!dP) {errno = ENOMEM; return ioP;}
   return (XrdOucCacheIO *)dP;
}
  
/******************************************************************************/
/*                                D e t a c h                                 */
/******************************************************************************/

int XrdRmcReal::Detach(XrdOucCacheIO *ioP)
{
   XrdSysMutexHelper Monitor(CMutex);
   XrdRmcSlot  *sP, *oP;
   int sNum, Fnum, Free = 0, Faults = 0;

// Now we delete this CacheIO from the cache set and see if its still ref'd.
//
   sNum = ioDel(ioP, Fnum);
   if (!sNum || sNum > 1) return 0;

// We will be deleting the CramData object. So, we need to recycle its slots.
//
   oP = &Slots[Fnum];
   while(oP->Own.Next != Fnum)
        {sP = &Slots[oP->Own.Next];
         sP->Owner(Slots);
         if (sP->Contents < 0 || sP->Status.LRU.Next < 0) Faults++;
            else {sP->Hide(Slots, Slash, sP->Contents%HNum);
                  sP->Pull(Slots);
                  sP->unRef(Slots);
                  Free++;
                 }
        }

// Reduce attach count and check if the cache is being deleted
//
   Attached--;
   if (AZero && Attached <= 0) AZero->Post();

// Issue debugging message
//
   if (Dbg) cerr <<"Cache: " <<Attached <<" att; rel " <<Free <<" slots; "
                 <<Faults <<" Faults; " <<std::hex << Fnum <<std::dec <<' '
                 <<ioP->Path() <<endl;

// All done, tell the caller to delete itself
//
   return 1;
}

/******************************************************************************/
/*                                  e M s g                                   */
/******************************************************************************/
  
void XrdRmcReal::eMsg(const char *Path, const char *What, long long xOff,
                       int xAmt, int eCode)
{
   char Buff[128];

   if (Dbg)
      {sprintf(Buff, "Cache: Error %d %s %d bytes at %lld; path=",
                     eCode, What, xAmt, xOff);
       cerr <<Buff <<Path <<endl;
      }
}
  
/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/
  
char *XrdRmcReal::Get(XrdOucCacheIO *ioP, long long lAddr, int &rAmt, int &noIO)
{
   XrdSysMutexHelper Monitor(CMutex);
   XrdRmcSlot::ioQ *Waiter;
   XrdRmcSlot *sP;
   int nUse, Fnum, Slot, segHash = lAddr%HNum;
   char *cBuff;

// See if we have this logical address in the cache. Check if the page is in
// transit and, if so, wait for it to arrive before proceeding.
//
   noIO = 1;
   if (Slash[segHash]
   &&  (Slot = XrdRmcSlot::Find(Slots, lAddr, Slash[segHash])))
      {sP = &Slots[Slot];
       if (sP->Count & XrdRmcSlot::inTrans)
          {XrdSysSemaphore ioSem(0);
           XrdRmcSlot::ioQ ioTrans(sP->Status.waitQ, &ioSem);
           sP->Status.waitQ = &ioTrans;
           if (Dbg > 1) cerr <<"Cache: Wait slot " <<Slot <<endl;
           CMutex.UnLock(); ioSem.Wait(); CMutex.Lock();
           if (sP->Contents != lAddr) {rAmt = -EIO; return 0;}
          } else {
            if (sP->Status.inUse < 0) sP->Status.inUse--;
               else {sP->Pull(Slots); sP->Status.inUse = -1;}
          }
       rAmt = (sP->Count < 0 ? sP->Count & XrdRmcSlot::lenMask : SegSize);
       if (sP->Count & XrdRmcSlot::isNew)
          {noIO = -1; sP->Count &= ~XrdRmcSlot::isNew;}
       if (Dbg > 2) cerr <<"Cache: Hit slot " <<Slot <<" sz " <<rAmt <<" nio "
                         <<noIO <<" uc " <<sP->Status.inUse <<endl;
       return Base+(static_cast<long long>(Slot)*SegSize);
      }

// Page is not here. If no allocation wanted or we cannot obtain a free slot
// return and indicate there is no associated cache page.
//
   if (!ioP || !(Slot = Slots[Slots->Status.LRU.Next].Pull(Slots)))
      {rAmt = -ENOMEM; return 0;}

// Remove ownership over this slot and remove it from the hash table
//
   sP = &Slots[Slot];
   if (sP->Contents >= 0)
      {if (sP->Own.Next != Slot) sP->Owner(Slots);
       sP->Hide(Slots, Slash, sP->Contents%HNum);
      }

// Read the data into the buffer
//
   sP->Count |= XrdRmcSlot::inTrans;
   sP->Status.waitQ = 0;
   CMutex.UnLock();
   cBuff = Base+(static_cast<long long>(Slot)*SegSize);
   rAmt = ioP->Read(cBuff, (lAddr & Strip) << SegShft, SegSize);
   CMutex.Lock();

// Post anybody waiting for this slot. We hold the cache lock which will give us
// time to complete the slot definition before the waiting thread looks at it.
//
   nUse = -1;
   while((Waiter = sP->Status.waitQ))
        {sP->Status.waitQ = sP->Status.waitQ->Next;
         sP->Status.waitQ->ioEnd->Post();
         nUse--;
        }

// If I/O succeeded, reinitialize the slot. Otherwise, return free it up
//
   noIO = 0;
   if (rAmt >= 0)
      {sP->Contents   = lAddr;
       sP->HLink      = Slash[segHash];
       Slash[segHash] = Slot;
       Fnum = (lAddr >> Shift) + SegCnt;
       Slots[Fnum].Owner(Slots, sP);
       sP->Count = (rAmt == SegSize ? SegFull : rAmt|XrdRmcSlot::isShort);
       sP->Status.inUse = nUse;
       if (Dbg > 2) cerr <<"Cache: Miss slot " <<Slot <<" sz "
                         <<(sP->Count & XrdRmcSlot::lenMask) <<endl;
      } else {
       eMsg(ioP->Path(), "reading", (lAddr & Strip) << SegShft, SegSize, rAmt);
       cBuff = 0;
       sP->Contents = -1;
       sP->unRef(Slots);
      }

// Return the associated buffer or zero, as per above
//
   return cBuff;
}

/******************************************************************************/
/*                                 i o A d d                                  */
/******************************************************************************/
  
int XrdRmcReal::ioAdd(XrdOucCacheIO *KeyVal, int &iNum)
{
   int hip, phip, hent = ioEnt(KeyVal);

// Look up the entry. If found, return it.
//
   if ((hip = hTab[hent]) && (hip = ioLookup(phip, hip, KeyVal)))
      {iNum = hip; return ++Slots[hip].Count;}

// Add the entry
//
   if ((hip = sFree))
      {sFree = Slots[sFree].HLink;
       Slots[hip].File(KeyVal, hTab[hent]);
       hTab[hent] = hip;
      }

// Return information to the caller
//
   iNum = hip;
   return (hip ? 1 : 0);
}
  
/******************************************************************************/
/*                                 i o D e l                                  */
/******************************************************************************/
  
int XrdRmcReal::ioDel(XrdOucCacheIO *KeyVal, int &iNum)
{
   int cnt, hip, phip, hent = ioEnt(KeyVal);

// Look up the entry.
//
   if (!(hip = hTab[hent]) || !(hip = ioLookup(phip, hip, KeyVal))) return 0;
   iNum = hip;

// Delete the item, if need be, and return
//
   cnt = --(Slots[hip].Count);
   if (cnt <= 0)
      {if (phip) Slots[phip].HLink = Slots[hip].HLink;
          else   hTab[hent]        = Slots[hip].HLink;
       Slots[hip].HLink = sFree; sFree = hip;
      }
   return (cnt < 0 ? 1 : cnt+1);
}

/******************************************************************************/
/*                               P r e R e a d                                */
/******************************************************************************/
  
void XrdRmcReal::PreRead()
{
   prTask *prP;

// Simply wait and dispatch elements
//
   if (Dbg) cerr <<"Cache: preread thread started; now " <<prNum <<endl;
   while(1)
        {prReady.Wait();
         prMutex.Lock();
         if (prStop) break;
         if ((prP = prFirst))
            {if (!(prFirst = prP->Next)) prLast = 0;
             prMutex.UnLock();
             prP->Data->Preread();
            } else prMutex.UnLock();
        }

// The cache is being deleted, wind down the prereads
//
   prNum--;
   if (prNum > 0) prReady.Post();
      else        prStop->Post();
   if (Dbg) cerr <<"Cache: preread thread exited; left " <<prNum <<endl;
   prMutex.UnLock();
}

/******************************************************************************/

void XrdRmcReal::PreRead(XrdRmcReal::prTask *prReq)
{

// Place this element on the queue
//
   prMutex.Lock();
   if (prLast) {prLast->Next = prReq; prLast = prReq;}
      else      prLast = prFirst = prReq;
   prReq->Next = 0;

// Tell a pre-reader that something is ready
//
   prReady.Post();
   prMutex.UnLock();
}

/******************************************************************************/
/*                                   R e f                                    */
/******************************************************************************/
  
int XrdRmcReal::Ref(char *Addr, int rAmt, int sFlags)
{
    XrdRmcSlot *sP = &Slots[(Addr-Base)>>SegShft];
    int eof = 0;

// Indicate how much data was not yet referenced
//
   CMutex.Lock();
   if (sP->Contents >= 0)
      {if (sP->Count < 0) eof = 1;
       sP->Status.inUse++;
       if (sP->Status.inUse < 0)
          {if (sFlags) sP->Count |= sFlags;
              else if (!eof && (sP->Count -= rAmt) < 0) sP->Count = 0;
          } else {
           if (sFlags) {sP->Count |= sFlags;                 sP->reRef(Slots);}
              else {     if (sP->Count & XrdRmcSlot::isSUSE)
                                                             sP->unRef(Slots);
                    else if (eof || (sP->Count -= rAmt) > 0) sP->reRef(Slots);
                    else   {sP->Count = SegSize/2;           sP->unRef(Slots);}
                   }
          }
      } else eof = 1;

// All done
//
   if (Dbg > 2) cerr <<"Cache: Ref " <<std::hex <<sP->Contents <<std::dec
                     << " slot " <<((Addr-Base)>>SegShft)
                     <<" sz " <<(sP->Count & XrdRmcSlot::lenMask)
                     <<" uc " <<sP->Status.inUse <<endl;
   CMutex.UnLock();
   return !eof;
}

/******************************************************************************/
/*                                 T r u n c                                  */
/******************************************************************************/

void XrdRmcReal::Trunc(XrdOucCacheIO *ioP, long long lAddr)
{
   XrdSysMutexHelper Monitor(CMutex);
   XrdRmcSlot  *sP, *oP;
   int sNum, Free = 0, Left = 0, Fnum = (lAddr >> Shift) + SegCnt;

// We will be truncating CacheData pages. So, we need to recycle those slots.
//
   oP = &Slots[Fnum]; sP = &Slots[oP->Own.Next];
   while(oP != sP)
        {sNum = sP->Own.Next;
         if (sP->Contents < lAddr) Left++;
            else {sP->Owner(Slots);
                  sP->Hide(Slots, Slash, sP->Contents%HNum);
                  sP->Pull(Slots);
                  sP->unRef(Slots);
                  Free++;
                 }
         sP = &Slots[sNum];
        }

// Issue debugging message
//
   if (Dbg) cerr <<"Cache: Trunc " <<Free <<" slots; "
                 <<Left <<" Left; " <<std::hex << Fnum <<std::dec <<' '
                 <<ioP->Path() <<endl;
}
  
/******************************************************************************/
/*                                   U p d                                    */
/******************************************************************************/
  
void XrdRmcReal::Upd(char *Addr, int wLen, int wOff)
{
    XrdRmcSlot *sP = &Slots[(Addr-Base)>>SegShft];

// Check if we extended a short page
//
   CMutex.Lock();
   if (sP->Count < 0)
      {int theLen = sP->Count & XrdRmcSlot::lenMask;
       if (wLen + wOff > theLen)
          sP->Count = (wLen+wOff) | XrdRmcSlot::isShort;
      }

// Adjust the reference counter and if no references, place on the LRU chain
//
   sP->Status.inUse++;
   if (sP->Status.inUse >= 0) sP->reRef(Slots);

// All done
//
   if (Dbg > 2) cerr <<"Cache: Upd " <<std::hex <<sP->Contents <<std::dec
                     << " slot " <<((Addr-Base)>>SegShft)
                     <<" sz " <<(sP->Count & XrdRmcSlot::lenMask)
                     <<" uc " <<sP->Status.inUse <<endl;
   CMutex.UnLock();
}
