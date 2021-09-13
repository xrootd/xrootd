#ifndef __NET_BUFF__
#define __NET_BUFF__
/******************************************************************************/
/*                                                                            */
/*                       X r d N e t B u f f e r . h h                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>

#include "XrdOuc/XrdOucChain.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         X r d N e t B u f f e r Q                          */
/******************************************************************************/

class XrdNetBuffer;
  
class XrdNetBufferQ
{
public:

       XrdNetBuffer  *Alloc();

inline int            BuffSize(void) {return size;}

       void           Recycle(XrdNetBuffer *bp);

       void           Set(int maxb);

       XrdNetBufferQ(int bsz, int maxb=16);
      ~XrdNetBufferQ();

       int                       alignit;
       XrdSysMutex               BuffList;
       XrdOucStack<XrdNetBuffer> BuffStack;
       int                       maxbuff;
       int                       numbuff;
       int                       size;
};

/******************************************************************************/
/*                          X r d N e t B u f f e r                           */
/******************************************************************************/

class XrdNetBuffer
{
friend class XrdNetBufferQ;

public:
       char         *data;
       int           dlen;

inline int           BuffSize(void) {return BuffQ->BuffSize();}

       void          Recycle(void)  {BuffQ->Recycle(this);}

      XrdNetBuffer(XrdNetBufferQ *bq);
     ~XrdNetBuffer() {if (data) free(data);}

private:

      XrdOucQSItem<XrdNetBuffer> BuffLink;
      XrdNetBufferQ             *BuffQ;
};
#endif
