/******************************************************************************/
/*                                                                            */
/*                       X r d O u c B u f f e r . c c                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#ifndef WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#include <cstdlib>

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
int   XrdOucBuffPool::alignit = sysconf(_SC_PAGESIZE);

/******************************************************************************/
/*                X r d O u c B u f f P o o l   M e t h o d s                 */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucBuffPool::XrdOucBuffPool(int minsz, int maxsz,
                               int minh,  int maxh, int rate)
{
   int keep, pct, i, n = 0;

// Adjust the minsz
//
   while(minsz > 1024*(1<<n)) n++;
   if (n > 14) n = 14;
      else if (n && minsz < 1024*(1<<n)) n--;
   incBsz = 1024*(1<<n);
   shfBsz = 10 + n;
   rndBsz = incBsz - 1;
   if (maxh < 0) maxh = 0;
   if (minh < 0) minh = 0;
   if (maxh < minh) maxh = minh;
   if (rate < 0) rate = 0;

// Round up the maxsz and make it a multiple of 4k
//
   if (!(slots = maxsz / incBsz))  slots = 1;
      else if (maxsz % incBsz) slots++;
   maxBsz = slots << shfBsz;

// Allocate a slot vector for this
//
   bSlot = new BuffSlot[(unsigned int)slots];

// Complete initializing the slot vector
//
   n = incBsz;
   for (i = 0; i < slots; i++)
       {bSlot[i].size = n; n += incBsz;
        pct = (slots - i + 1)*100/slots;
        if (pct >= 100) keep = maxh;
           else {keep = ((maxh * pct) + 55)/100 - i*rate;
                 if (keep > maxh) keep = maxh;
                    else if (keep < minh) keep = minh;
                }
        bSlot[i].maxbuff = keep;
       }
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdOucBuffer *XrdOucBuffPool::Alloc(int bsz)
{
  XrdOucBuffPool::BuffSlot *sP;
  XrdOucBuffer  *bP;
  int snum;

// Compute buffer slot
//
   snum = (bsz <= incBsz ? 0 : (bsz + rndBsz) >> shfBsz);
   if (snum >= slots) return 0;
   sP = &bSlot[snum];

// Lock the data area
//
   sP->SlotMutex.Lock();

// Either return a new buffer or an old one
//
   if ((bP = sP->buffFree))
      {sP->buffFree = bP->buffNext;
       bP->buffPool = this;
       sP->numbuff--;
      } else {
       if ((bP = new XrdOucBuffer(this, snum)))
          {int mema;
           if (sP->size >= alignit) mema = alignit;
              else if (sP->size > 2048) mema = 4096;
                      else if (sP->size > 1024) mema = 2048;
                              else mema = 1024;
           if (posix_memalign((void **)&(bP->data), mema, sP->size))
              {delete bP; bP = 0;}
          }
      }

// Unlock the data area
//
   sP->SlotMutex.UnLock();

// Return the buffer
//
   return bP;
}
  
/******************************************************************************/
/*      X r d O u c B u f f P o o l : : B u f f S l o t   M e t h o d s       */
/******************************************************************************/
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOucBuffPool::BuffSlot::~BuffSlot()
{
   XrdOucBuffer *bP;

   while((bP = buffFree)) {buffFree = buffFree->buffNext; delete bP;}
}
  
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdOucBuffPool::BuffSlot::Recycle(XrdOucBuffer *bP)
{

// Check if we have enough objects, if so, delete ourselves and return
//
   if (numbuff >= maxbuff) {delete bP; return;}
   bP->dlen = 0;
   bP->doff = 0;

// Add the buffer to the recycle list
//
   SlotMutex.Lock();
   bP->buffNext = buffFree;
   buffFree     = bP;
   numbuff++;
   SlotMutex.UnLock();
   return;
}
  
/******************************************************************************/
/*                  X r d O u c B u f f e r   M e t h o d s                   */
/******************************************************************************/
/******************************************************************************/
/*                    P u b l i c   C o n s t r u c t o r                     */
/******************************************************************************/
  
XrdOucBuffer::XrdOucBuffer(char *buff, int blen)
{
   static XrdOucBuffPool nullPool(0, 0, 0, 0, 0);

// Initialize the one time buffer
//
   data = buff;
   dlen = blen;
   doff = 0;
   size = blen;
   slot = 0;
   buffPool = &nullPool;
};

/******************************************************************************/
/*                                 C l o n e                                  */
/******************************************************************************/

XrdOucBuffer *XrdOucBuffer::Clone(bool trim)
{
   XrdOucBuffer *newbP;
   int newsz;

// Compute the size of the new buffer
//
   newsz = (trim ? doff+dlen : size);

// Allocate a new buffer
//
   if (!(newbP = buffPool->Alloc(newsz))) return 0;

// Copy the data and the information
//
   newbP->dlen = dlen;
   newbP->doff = doff;
   memcpy(newbP->data, data, dlen+doff);
   return newbP;
}

/******************************************************************************/
/*                              H i g h j a c k                               */
/******************************************************************************/

XrdOucBuffer *XrdOucBuffer::Highjack(int xsz)
{
   XrdOucBuffer tempBuff, *newbP;

// Adjust the size to revert highjacked buffer
//
   if (xsz <= 0) xsz = size;

// Allocate a new buffer
//
   if (!(newbP = buffPool->Alloc(xsz))) return 0;

// Swap information
//
   tempBuff = *this;
   *this    = *newbP;
   *newbP   = tempBuff;
   tempBuff.data = 0;
   return newbP;
}
  
/******************************************************************************/
/*                                R e s i z e                                 */
/******************************************************************************/

bool XrdOucBuffer::Resize(int newsz)
{

// If the new size differs from the old size, reallocate by simply highjacking
// the buffer and releasing the newly acquired one.
//
   if (newsz != size)
      {XrdOucBuffer *newbP;
       if (!(newbP = Highjack(newsz))) return false;
       newbP->Recycle();
      }
   return true;
}
