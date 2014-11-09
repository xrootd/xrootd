/******************************************************************************/
/*                                                                            */
/*                       X r d O u c B u f f e r . c c                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <stdlib.h>
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <malloc.h>
#endif

#include "XrdNet/XrdNetBuffer.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                 X r d N e t B u f f e r Q   M e t h o d s                  */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetBufferQ::XrdNetBufferQ(int bsz, int maxb)
{
   size    = bsz;
   alignit = (size < sysconf(_SC_PAGESIZE)
                   ? size : sysconf(_SC_PAGESIZE));
   maxbuff = maxb; 
   numbuff = 0;
}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdNetBufferQ::~XrdNetBufferQ()
{
   XrdNetBuffer *bp;

   while((bp = BuffStack.Pop())) delete bp;
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdNetBuffer *XrdNetBufferQ::Alloc()
{
  XrdNetBuffer *bp;

// Lock the data area
//
   BuffList.Lock();

// Either return a new buffer or an old one
//
   if ((bp = BuffStack.Pop())) numbuff--;
      else if ((bp = new XrdNetBuffer(this))
           &&  !(bp->data = (char *)memalign(alignit, size)))
              {delete bp; bp = 0;}

// Unlock the data area
//
   BuffList.UnLock();

// Return the buffer
//
   return bp;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdNetBufferQ::Recycle(XrdNetBuffer *bp)
{

// Check if we have enough objects, if so, delete ourselves and return
//
   if (numbuff >= maxbuff) {delete bp; return;}
   bp->dlen = 0;

// Add the buffer to the recycle list
//
   BuffList.Lock();
   BuffStack.Push(&bp->BuffLink);
   numbuff++;
   BuffList.UnLock();
   return;
}
 
/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
void XrdNetBufferQ::Set(int maxb)
{

// Lock the data area, set max buffers, unlock and return
//
   BuffList.Lock();
   maxbuff = maxb;
   BuffList.UnLock();
   return;
}
  
/******************************************************************************/
/*                  X r d N e t B u f f e r   M e t h o d s                   */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetBuffer::XrdNetBuffer(XrdNetBufferQ *bq) : BuffLink(this)
{
   BuffQ= bq;
   data = 0;
   dlen = 0; 
}
