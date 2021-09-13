#ifndef __OUC_BUFF__
#define __OUC_BUFF__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c B u f f e r . h h                        */
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

#include <cstdlib>

#include "XrdOuc/XrdOucChain.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        X r d O u c B u f f P o o l                         */
/******************************************************************************/

class XrdOucBuffer;
  
//-----------------------------------------------------------------------------
//! These classes allow for buffer management to minimize data copying. They
//! are typically used in conjunction with the XrdOucErrInfo class. The
//! XrdOucBuffPool class defines a pool of buffers and one such object must
//! exist for each buffer pool (there can be many such pools). This object
//! manufactures XrdOucBuffer objects. You can also create XrdOucBuffers
//! without using a buffer pool (i.e. one time buffers). See the XrdOucBuffer
//! constructor for details on how to do this and the associated caveats.
//-----------------------------------------------------------------------------

class XrdOucBuffPool
{
friend class XrdOucBuffer;
public:

//-----------------------------------------------------------------------------
//! Allocate a buffer object.
//!
//! @param  sz    - the desired size. It os rounded up to be a multiple of
//!                 incBsz but cannot exceed maxBsz.
//!
//! @return !0    - pointer to usable buffer object of suitable size.
//! @return =0    - insufficient memort ro allocate a buffer.
//-----------------------------------------------------------------------------

       XrdOucBuffer *Alloc(int sz);

//-----------------------------------------------------------------------------
//! Obtain the maximum size a buffer can have.
//!
//! @return The maximum size a buffer can be.
//-----------------------------------------------------------------------------

inline int          MaxSize() const {return maxBsz;}

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  minsz - the minimum size a buffer can have. If it is smaller than
//!                 1024 it is set to 1024. The minsz is also adjusted to be
//!                 equal to the closest smaller value of 1024*(2**n) (i.e. 1K,
//!                 2k, 4K, etc). If it's greater than 16MB, it is set to 16MB.
//! @param  maxsz - the maximum size a buffer can have and must be >= minsz.
//!                 If it's >minsz it is rounded up to the next minsz increment.
//!                 Buffer sizes are always allocated in minsz increments.
//! @param  minh  - the minimum number of buffers that should be held in
//!                 reserve when a buffer is recycled.
//! @param  maxh  - the maximum number of buffers that should be held in
//!                 reserve when a buffer is recycled. The value applies to the
//!                 smallest buffer size and is progessively reduced as the
//!                 buffer size increases. If maxh < minh it is set to minh.
//! @param  rate  - specifies how quickly the hold vale is to be reduced as
//!                 buffer sizes increase. A rate of 0 specifies a purely linear
//!                 decrease. Higher values logrithmically decrease the hold.
//-----------------------------------------------------------------------------

       XrdOucBuffPool(int minsz=4096, int  maxsz=65536,
                      int minh=1,     int  maxh=16,
                      int rate=1);

//-----------------------------------------------------------------------------
//! Destructor - You must not destroy this object prior to recycling all
//!              oustanding buffers allocated out of this pool.
//-----------------------------------------------------------------------------

      ~XrdOucBuffPool() {delete [] bSlot;}

private:
static int            alignit;

struct BuffSlot
      {XrdSysMutex    SlotMutex;
       XrdOucBuffer  *buffFree;
       int            size;
       short          numbuff;
       short          maxbuff;

       void           Recycle(XrdOucBuffer *bP);

                      BuffSlot() : buffFree(0), size(0),
                                   numbuff(0),  maxbuff(0) {}
                     ~BuffSlot();
      };

BuffSlot *bSlot;
int       incBsz;
int       shfBsz;
int       rndBsz;
int       maxBsz;
int       slots;
};

/******************************************************************************/
/*                          X r d O u c B u f f e r                           */
/******************************************************************************/

class XrdOucBuffer
{
friend class XrdOucBuffPool;

public:

//-----------------------------------------------------------------------------
//! Get the pointer to the buffer.
//!
//! @return pointer to the buffer.
//-----------------------------------------------------------------------------

inline char         *Buffer() const {return data;}

//-----------------------------------------------------------------------------
//! Get the size of the buffer.
//!
//! @return size of the buffer.
//-----------------------------------------------------------------------------

inline int           BuffSize() const {return size;}

//-----------------------------------------------------------------------------
//! Produce a clone of this buffer.
//!
//! @param  trim     - when true the memory buffer is trimmed to be of
//!                    sufficient size to hold the actual data. Otherwise, the
//!                    cloned memory buffer is of the same length.
//!
//! @return !0       - pointer to the cloned buffer.
//!         =0       - insufficient memory to clone the buffer.
//-----------------------------------------------------------------------------

      XrdOucBuffer  *Clone(bool trim=true);

//-----------------------------------------------------------------------------
//! Get a pointer to the data in the buffer.
//!
//! @return pointer to the data.
//-----------------------------------------------------------------------------

inline char         *Data() const {return data+doff;}

//-----------------------------------------------------------------------------
//! Get a pointer to the data in the buffer and the length of the data.
//!
//! @param  dataL - place where the length is to be stored.
//!
//! @return pointer to the data with dataL holding its length.
//-----------------------------------------------------------------------------

inline char         *Data(int &dataL) const {dataL = dlen; return data+doff;}

//-----------------------------------------------------------------------------
//! Get the data length.
//!
//! @return The data length.
//-----------------------------------------------------------------------------

inline int           DataLen() {return dlen;}

//-----------------------------------------------------------------------------
//! Highjack the buffer contents and reinitialize the original buffer.
//!
//! @param  xsz   - the desired size to be given to the highjacked buffer. If
//!                 zero, the current size is used. Same size resictions apply
//!                 as for buffer pool Alloc(), above.
//!
//! @return !0    - pointer to a usable buffer object which is identical to the
//!                 original buffer. The original buffer was reallocated with
//!                 the specified size.
//! @return =0    - insufficient memory to allocate a buffer.
//-----------------------------------------------------------------------------

       XrdOucBuffer *Highjack(int bPsz=0);

//-----------------------------------------------------------------------------
//! Recycle the buffer. The buffer may be reused in the future.
//-----------------------------------------------------------------------------

inline void         Recycle()  {buffPool->bSlot[slot].Recycle(this);}

//-----------------------------------------------------------------------------
//! Resize the buffer.
//!
//! @param  newsz - the size that the buffer is to have. The same restrictions
//!                 apply as for buffer pool Alloc(), above.
//!
//! @return true  - buffer has been reallocated.
//! @return false - insufficient memoy to reallocated the buffer.
//-----------------------------------------------------------------------------

       bool         Resize(int newsz);

//-----------------------------------------------------------------------------
//! Set the data length of offset.
//!
//! @param  dataL - the length of the data.
//! @param  dataO - the offset of the data in the buffer.
//-----------------------------------------------------------------------------

inline void         SetLen(int dataL, int dataO=0) {dlen = dataL; doff = dataO;}

//-----------------------------------------------------------------------------
//! Public constructor. You can create one-time buffers not associated with a
//! buffer pool via new to associated your own storage area that will be
//! freed when the buffer is recycled. This may be handy to pass along such a
//! buffer to XrdOucErrInfo in a pinch. A one-time buffer is restricted and
//! the Clone(), Highjack() and Resize() methods will always fail. However,
//! all the other methods will work in the expected way.
//!
//! @param  buff  - pointer to a storage area obtained via posix_memalign()
//!                 and it will be released via free().
//! @param  blen  - the size of the buffer as well as the data length.
//!                 Use SetLen() to set a new data length if it differs.
//-----------------------------------------------------------------------------

      XrdOucBuffer(char *buff, int blen);

private:
      XrdOucBuffer(XrdOucBuffPool *pP, int snum)
                  : data(0), dlen(0), doff(0), size(pP->bSlot[snum].size),
                    slot(snum), buffPool(pP) {}

      XrdOucBuffer()
                  : data(0), dlen(0), doff(0), size(0), slot(0), buffPool(0) {}

     ~XrdOucBuffer() {if (data) free(data);}

      char           *data;
      int             dlen;
      int             doff;
      int             size;
      int             slot;
union{XrdOucBuffer   *buffNext;
      XrdOucBuffPool *buffPool;
     };
};
#endif
