#ifndef __SFS_XIO_H__
#define __SFS_XIO_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d S f s X i o . h h                           */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! XrdSfsXio.hh
//!
//! This class is used to allow file I/O interfaces to perform exchange buffer
//! I/O in order to minimize data copying. When this feature is enabled, the
//! XrdSfsInterface::setXio() method is called on a newly created XrdSfsFile
//! object. Ideally, all oustanding buffers should be be released when the file
//! is closed. Alternatively, the XrdSfsXio::Reclaim() method may be used
//! at any time when it is convenient to do so. For best performance, use
//! XrdSfsXio::Swap() as it provides memory locality and is kind to the cache.
//! Buffer swapping is only supported for common file write operations.
//-----------------------------------------------------------------------------

//typedef void* XrdSfsXioHandle;
typedef class XrdBuffer* XrdSfsXioHandle;

class XrdSfsXioImpl;

/******************************************************************************/
/*                       C l a s s   X r d S f s X i o                        */
/******************************************************************************/
  
class XrdSfsXio
{
public:

//-----------------------------------------------------------------------------
//! Get the address and size of the buffer associated with a handle.
//!
//! @param theHand  - The handle associated with the buffer.
//! @param buffsz   - If not nil, the size of the buffer is returned. The
//!                   size will always be >= to the original data length.
//!
//! @return A pointer to the buffer.
//-----------------------------------------------------------------------------

static char      *Buffer(XrdSfsXioHandle theHand, int *buffsz=0);

//-----------------------------------------------------------------------------
//! Claim ownership of the current buffer if it is memory effecient to do so.
//!
//! @param  curBuff - The address of the current buffer. It must match the
//!                   the buffer that was most recently passed to the caller.
//! @param  datasz  - Number of useful bytes in the buffer (i.e. write size).
//! @param  minasz  - Minimum buffer size that would be allocated to copy data.
//!
//! @return !0        The buffer handle of the current buffer is returned along
//!                   with ownership rights.
//! @return =0        Too much memory would be wasted by transferring ownership
//!                   (errno == 0) or an error ocurred (errno != 0). When an
//!                   error see Swap() below for possible types of errors.
//-----------------------------------------------------------------------------
virtual
XrdSfsXioHandle   Claim(const char *curBuff, int datasz, int minasz) = 0;

//-----------------------------------------------------------------------------
//! Return a buffer previously gotten from a Claim() or Swap() call.
//!
//! @param theHand  - The handle associated with the buffer.
//-----------------------------------------------------------------------------

static void       Reclaim(XrdSfsXioHandle theHand);

//-----------------------------------------------------------------------------
//! Swap the current I/O buffer
//!
//! @param  curBuff - The address of the current buffer. It must match the
//!                   the buffer that was most recently passed to the caller.
//! @param  oldHand - The handle associated with a buffer returned by a
//!                   previous call to Swap(). A value of zero indicates that
//!                   the caller is taking control of the buffer but has no
//!                   replacement buffer.
//! @return !0        The buffer handle of the current buffer is returned along
//!                   with ownership rights. If oldHand was not nil, the
//!                   caller's ownership of the associated buffer is reclaimed.
//! @return =0        An error occurred and nothing has changed; errno holds
//!                   the reason for the error. Typically,
//!                   EINVAL  - curBuff doe not match current buffer.
//!                   ENOBUFS - not enough memory to give up buffer.
//!                   ENOTSUP - unsupported context for call.
//-----------------------------------------------------------------------------
virtual
XrdSfsXioHandle   Swap(const char *curBuff, XrdSfsXioHandle oldHand=0) = 0;

//-----------------------------------------------------------------------------
//! Constructor and destructor
//!
//! @param xioimpl    Reference to static method implementations.
//-----------------------------------------------------------------------------

             XrdSfsXio(XrdSfsXioImpl &xioimpl);

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

virtual     ~XrdSfsXio() {}
};
#endif
