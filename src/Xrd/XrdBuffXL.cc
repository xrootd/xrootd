/******************************************************************************/
/*                                                                            */
/*                          X r d B u f f X L . c c                           */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>

#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdBuffXL.hh"

/******************************************************************************/
/*                          L o c a l   V a l u e s                           */
/******************************************************************************/
  
namespace
{
static const int maxBuffSz = 1 << 30; //1 GB
static const int iniBuffSz = 1 << (XRD_BUSHIFT+XRD_BUCKETS-1);
static const int minBuffSz = 1 << (XRD_BUSHIFT+XRD_BUCKETS);
static const int minBShift =      (XRD_BUSHIFT+XRD_BUCKETS);
static const int isBigBuff = 0x40000000;
}
 
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdBuffXL::XrdBuffXL() : bucket(0), totalo(0), pagsz(getpagesize()), slots(0),
                         maxsz(1<<(XRD_BUSHIFT+XRD_BUCKETS-1)), totreq(0)
{ }

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

void XrdBuffXL::Init(int maxMSZ)
{
   int lg2, chunksz;

// If this is a duplicate call, delete the previous setup
//
   if (bucket) {delete [] bucket; bucket = 0;}

// Check if this is too small for us
//
   if (maxMSZ <= iniBuffSz) {maxsz = iniBuffSz; return;}

// Check if this is too large for us (1GB limit) and adjust
//
   if (maxMSZ > maxBuffSz) maxMSZ = maxBuffSz;

// Calculate how many buckets we need to have (note we trim this down
//
   chunksz = maxMSZ >> minBShift;
   lg2 = XrdOucUtils::Log2(chunksz);
   chunksz = 1<<(lg2+minBShift);
   if (chunksz < maxMSZ) {lg2++; maxsz = chunksz << 1;}
      else maxsz = chunksz;

// Allocate a bucket array
//
   bucket = new BuckVec[lg2+1];
   slots  = lg2+1;
}
  
/******************************************************************************/
/*                                O b t a i n                                 */
/******************************************************************************/
  
XrdBuffer *XrdBuffXL::Obtain(int sz)
{
   XrdBuffer *bp;
   char *memp;
   int mk, buffSz, bindex = 0;

// Make sure the request is within our limits
//
   if (sz <= 0 || sz > maxsz) return 0;

// Calculate bucket index. This is log2(shifted size) rounded up if need be.
// If the shift results in zero we know the request fits in the slot 0 buffer.
//
   mk = sz >> minBShift;
   if (!mk) buffSz = minBuffSz;
      else {bindex = XrdOucUtils::Log2(mk);
            buffSz = (bindex ? minBuffSz << bindex : minBuffSz);
            if (buffSz < sz) {bindex++; buffSz = buffSz << 1;}
           }
   if (bindex >= slots) return 0;    // Should never happen!

// Obtain a lock on the bucket array and try to give away an existing buffer
//
    slotXL.Lock();
    totreq++;
    bucket[bindex].numreq++;
    if ((bp = bucket[bindex].bnext))
       {bucket[bindex].bnext = bp->next; bucket[bindex].numbuf--;}
    slotXL.UnLock();

// Check if we really allocated a buffer
//
   if (bp) return bp;

// Allocate a chunk of aligned memory
//
   if (posix_memalign((void **)&memp, pagsz, buffSz)) return 0;

// Wrap the memory with a buffer object
//
   if (!(bp = new XrdBuffer(memp, buffSz, bindex|isBigBuff)))
      {free(memp); return 0;}

// Update statistics
//
   slotXL.Lock(); totalo += buffSz; totbuf++; slotXL.UnLock();

// Return the buffer
//
   return bp;
}
 
/******************************************************************************/
/*                                R e c a l c                                 */
/******************************************************************************/
  
int XrdBuffXL::Recalc(int sz)
{
   int buffSz, mk, bindex = 0;

// Make sure the request is within our limits
//
   if (sz <= 0 || sz > maxsz) return 0;

// Calculate bucket size corresponding to the desired size
//
   mk = sz >> minBShift;
   if (!mk) buffSz = minBuffSz;
      else {bindex = XrdOucUtils::Log2(mk);
            buffSz = (bindex ? minBuffSz << bindex : minBuffSz);
            if (buffSz < sz) {bindex++; buffSz = buffSz << 1;}
           }
   if (bindex >= slots) return 0;    // Should never happen!

// All done, return the actual size we would have allocated
//
   return buffSz;
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
void XrdBuffXL::Release(XrdBuffer *bp)
{
   int bindex = bp->bindex & ~isBigBuff;

// Obtain a lock on the bucket array and reclaim the buffer
//
    slotXL.Lock();
    bp->next = bucket[bindex].bnext;
    bucket[bindex].bnext = bp;
    bucket[bindex].numbuf++;
    slotXL.UnLock();
}
 
/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdBuffXL::Stats(char *buff, int blen, int do_sync)
{
    static char statfmt[] = "<xlreqs>%d</xlreqs>"
                "<xlmem>%lld</xlmem><xlbuffs>%d</xlbuffs>";
    int nlen;

// If only size wanted, return it
//
   if (!buff) return sizeof(statfmt) + 16*3;

// Return formatted stats
//
   if (do_sync) slotXL.Lock();
   nlen = snprintf(buff, blen, statfmt, totreq, totalo, totbuf);
   if (do_sync) slotXL.UnLock();
   return nlen;
}

/******************************************************************************/
/*                                  T r i m                                   */
/******************************************************************************/
  
void XrdBuffXL::Trim()
{
  XrdBuffer *bP;
  int n, m;

// Obtain the lock
//
   slotXL.Lock();

// Run through all our slots looking for buffers to release
//
   for (int i = 0; i < slots; i++)
       {if (bucket[i].numbuf > 1 && bucket[i].numbuf > bucket[i].numreq)
           {n = bucket[i].numbuf - bucket[i].numreq;
            m = bucket[i].numbuf/2;
            if (m < n) n = m;
            while(n-- && (bP = bucket[i].bnext))
                 {bucket[i].bnext = bP->next;
                  bucket[i].numbuf--;
                  totalo -= bP->bsize; totbuf--;
                  delete bP;
                 }
            }
        bucket[i].numreq = 0;
       }

// Release the lock
//
   slotXL.UnLock();
}
