/******************************************************************************/
/*                                                                            */
/*                          X r d B u f f e r . c c                           */
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

#include <ctime>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>

#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdBuffXL.hh"
#include "Xrd/XrdTrace.hh"

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
void *XrdReshaper(void *pp)
{
     XrdBuffManager *bmp = (XrdBuffManager *)pp;
     bmp->Reshape();
     return (void *)0;
}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

const char *XrdBuffManager::TraceID = "BuffManager";

namespace
{
static const int minBuffSz = 1 << XRD_BUSHIFT;
}

namespace XrdGlobal
{
       XrdBuffXL   xlBuff;
extern XrdSysError Log;
}

using namespace XrdGlobal;
 
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdBuffManager::XrdBuffManager(int minrst) :
                   slots(XRD_BUCKETS),
                   shift(XRD_BUSHIFT),
                   pagsz(getpagesize()),
                   maxsz(1<<(XRD_BUSHIFT+XRD_BUCKETS-1)),
                   Reshaper(0, "buff reshaper")
{

// Clear everything to zero
//
   totbuf   = 0;
   totreq   = 0;
   totalo   = 0;
   totadj   = 0;
#ifdef _SC_PHYS_PAGES
   maxalo   = static_cast<long long>(pagsz)/8
              * static_cast<long long>(sysconf(_SC_PHYS_PAGES));
#else
   maxalo = 0x7ffffff;
#endif
   rsinprog = 0;
   minrsw   = minrst;
   memset(static_cast<void *>(bucket), 0, sizeof(bucket));
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdBuffManager::~XrdBuffManager()
{
   XrdBuffer *bP;

   for (int i = 0; i < XRD_BUCKETS; i++)
       {while((bP = bucket[i].bnext))
             {bucket[i].bnext = bP->next;
              delete bP;
             }
        bucket[i].numbuf = 0;
       }
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

void XrdBuffManager::Init()
{
   pthread_t tid;
   int rc;

// Start the reshaper thread
//
   if ((rc = XrdSysThread::Run(&tid, XrdReshaper, static_cast<void *>(this), 0,
                          "Buffer Manager reshaper")))
      Log.Emsg("BuffManager", rc, "create reshaper thread");
}
  
/******************************************************************************/
/*                                O b t a i n                                 */
/******************************************************************************/
  
XrdBuffer *XrdBuffManager::Obtain(int sz)
{
   XrdBuffer *bp;
   char *memp;
   int mk, pk, bindex;

// Make sure the request is within our limits
//
   if (sz <= 0)     return 0;
   if (sz >  maxsz) return xlBuff.Obtain(sz);

// Calculate bucket index
//
   mk = sz >> shift;
   bindex = XrdOucUtils::Log2(mk);
   mk = minBuffSz << bindex;
   if (mk < sz) {bindex++; mk = mk << 1;}
   if (bindex >= slots) return 0;    // Should never happen!

// Obtain a lock on the bucket array and try to give away an existing buffer
//
    Reshaper.Lock();
    totreq++;
    bucket[bindex].numreq++;
    if ((bp = bucket[bindex].bnext))
       {bucket[bindex].bnext = bp->next; bucket[bindex].numbuf--;}
    Reshaper.UnLock();

// Check if we really allocated a buffer
//
   if (bp) return bp;

// Allocate a chunk of aligned memory
//
   pk = (mk < pagsz ? mk : pagsz);
   if (posix_memalign((void **)&memp, pk, mk)) return 0;

// Wrap the memory with a buffer object
//
   if (!(bp = new XrdBuffer(memp, mk, bindex))) {free(memp); return 0;}

// Update statistics
//
    Reshaper.Lock();
    totbuf++;
    if ((totalo += mk) > maxalo && !rsinprog)
       {rsinprog = 1; Reshaper.Signal();}
    Reshaper.UnLock();
    return bp;
}
 
/******************************************************************************/
/*                                R e c a l c                                 */
/******************************************************************************/
  
int XrdBuffManager::Recalc(int sz)
{
   int mk, bindex;

// Make sure the request is within our limits
//
   if (sz <= 0)     return 0;
   if (sz >  maxsz) return xlBuff.Recalc(sz);

// Calculate bucket index
//
   mk = sz >> shift;
   bindex = XrdOucUtils::Log2(mk);
   mk = minBuffSz << bindex;
   if (mk < sz) {bindex++; mk = mk << 1;}
   if (bindex >= slots) return 0;    // Should never happen!

// All done, return the actual size we would have allocated
//
   return mk;
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
void XrdBuffManager::Release(XrdBuffer *bp)
{
   int bindex = bp->bindex;

// Check if we should release this via the big buffer object
//
   if (bindex >= slots) {xlBuff.Release(bp); return;}

// Obtain a lock on the bucket array and reclaim the buffer
//
    Reshaper.Lock();
    bp->next = bucket[bp->bindex].bnext;
    bucket[bp->bindex].bnext = bp;
    bucket[bindex].numbuf++;
    Reshaper.UnLock();
}
 
/******************************************************************************/
/*                               R e s h a p e                                */
/******************************************************************************/
  
void XrdBuffManager::Reshape()
{
int i, bufprof[XRD_BUCKETS], numfreed;
time_t delta, lastshape = time(0);
long long memslot, memhave, memtarget = (long long)(.80*(float)maxalo);
XrdSysTimer Timer;
float requests, buffers;
XrdBuffer *bp;

// This is an endless loop to periodically reshape the buffer pool
//
while(1)
     {Reshaper.Lock();
      while(Reshaper.Wait(minrsw) && totalo <= maxalo)
           {TRACE(MEM, "Reshaper has " <<(totalo>>10) <<"K; target " <<(memtarget>>10) <<"K");}
      if ((delta = (time(0) - lastshape)) < minrsw) 
         {Reshaper.UnLock();
          Timer.Wait((minrsw-delta)*1000);
          Reshaper.Lock();
         }

      // We have the lock so compute the request profile
      //
      if (totreq > slots)
         {requests = (float)totreq;
          buffers  = (float)totbuf;
          for (i = 0; i < slots; i++)
              {bufprof[i] = (int)(buffers*(((float)bucket[i].numreq)/requests));
               bucket[i].numreq = 0;
              }
          totreq = 0; memhave = totalo;
         } else memhave = 0;
      Reshaper.UnLock();

      // Reshape the buffer pool to agree with the request profile
      //
      memslot = maxsz; numfreed = 0;
      for (i = slots-1; i >= 0 && memhave > memtarget; i--)
          {Reshaper.Lock();
           while(bucket[i].numbuf > bufprof[i])
                if ((bp = bucket[i].bnext))
                   {bucket[i].bnext = bp->next;
                    delete bp;
                    bucket[i].numbuf--; numfreed++;
                    memhave -= memslot; totalo  -= memslot;
                    totbuf--;
                   } else {bucket[i].numbuf = 0; break;}
           Reshaper.UnLock();
           memslot = memslot>>1;
          }

       // All done
       //
       totadj += numfreed;
       TRACE(MEM, "Pool reshaped; " <<numfreed <<" freed; have " <<(memhave>>10) <<"K; target " <<(memtarget>>10) <<"K");
       lastshape = time(0);
       rsinprog = 0;    // No need to lock, we're the only ones now setting it

       xlBuff.Trim();   // Trim big buffers
      }
}
 
/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
void XrdBuffManager::Set(int maxmem, int minw)
{

// Obtain a lock and set the values
//
   Reshaper.Lock();
   if (maxmem > 0) maxalo = (long long)maxmem;
   if (minw   > 0) minrsw = minw;
   Reshaper.UnLock();
}
 
/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdBuffManager::Stats(char *buff, int blen, int do_sync)
{
    static char statfmt[] = "<stats id=\"buff\"><reqs>%d</reqs>"
                "<mem>%lld</mem><buffs>%d</buffs><adj>%d</adj>%s</stats>";
    char xlStats[1024];
    int nlen;

// If only size wanted, return it
//
   if (!buff) return sizeof(statfmt) + 16*4 + xlBuff.Stats(0,0);

// Return formatted stats
//
   if (do_sync) Reshaper.Lock();
   xlBuff.Stats(xlStats, sizeof(xlStats), do_sync);
   nlen = snprintf(buff,blen,statfmt,totreq,totalo,totbuf,totadj,xlStats);
   if (do_sync) Reshaper.UnLock();
   return nlen;
}
