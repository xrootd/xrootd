/******************************************************************************/
/*                                                                            */
/*                       X r d O u c B u f f e r . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//          $Id$

const char *XrdOucBufferCVSID = "$Id$";

#include <malloc.h>
#include <unistd.h>

#include "Experiment/Experiment.hh"
#include "XrdOuc/XrdOucBuffer.hh"

/******************************************************************************/
/*                 G l o b a l   I n i t i a l i z a t i o n                  */
/******************************************************************************/

XrdOucMutex               XrdOucBuffer::BuffList;
XrdOucStack<XrdOucBuffer> XrdOucBuffer::BuffStack;
int                     XrdOucBuffer::size    = 4096;
int                     XrdOucBuffer::alignit = (size < getpagesize() ? size :
                                                        getpagesize());
int                     XrdOucBuffer::maxbuff = 16;
int                     XrdOucBuffer::numbuff = 0;
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucBuffer::XrdOucBuffer() : BuffLink(this)
{
   data = (char *)memalign(alignit, size);
   dlen = 0; 
   dpnt = 0;
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdOucBuffer *XrdOucBuffer::Alloc()
{
  XrdOucBuffer *bp;

// Lock the data area
//
   BuffList.Lock();

// Either return a new buffer or an old one
//
   if (!(bp = BuffStack.Pop())) bp = new XrdOucBuffer();
      else numbuff--;

// Unlock the data area
//
   BuffList.UnLock();

// Return the buffer
//
   return bp;
}
 
/******************************************************************************/
/*                                 T o k e n                                  */
/******************************************************************************/
  
char *XrdOucBuffer::Token(char **rest)
{
   char *tp;

// Set up the buffer pointers if this is the first call
//
   if (!dpnt) 
      {dpnt = data; 
       if (dlen >= size) data[size-1] = '\0';
          else data[dlen] = '\0';
      }

// Check if we are at the end of the buffer
//
   if (!*dpnt) 
      {if (rest) *rest = (char *)"";
       return (char *)0;
      }

// Skip leading blanks
//
   while(!*dpnt && ' ' == *dpnt) dpnt++;

// Skip non-blank chars
//
   tp = dpnt;
   while(*dpnt && ' ' != *dpnt) dpnt++;
   if (*dpnt) {*dpnt = '\0'; dpnt++;}

// Return rmainder of the line
//
   if (rest)
      {while(!*dpnt && ' ' == *dpnt) dpnt++;
       *rest = dpnt;
      }

// Return the pointer to the token
//
   return tp;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdOucBuffer::Recycle()
{

// Check if we have enough objects, if so, delete ourselves and return
//
   if (numbuff >= maxbuff) {delete this; return;}
   dlen = 0; dpnt = 0;

// Add the buffer to the recycle list
//
   BuffList.Lock();
   BuffStack.Push(&BuffLink);
   numbuff++;
   BuffList.UnLock();
   return;
}
 
/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
void XrdOucBuffer::Set(int maxb)
{

// Lock the data area, set max buffers, unlock and return
//
   BuffList.Lock();
   maxbuff = maxb;
   BuffList.UnLock();
   return;
}
