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
//! is closed. Alternatively, the XrdSfsXioHandle::Recycle() method may be used
//! at any time when it is convenient to do so. For best performance, use
//! XrdSfsXio::Swap() as it provides memory locality and is kind to the cache.
//! Buffer swapping is only supported for file write operations.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//! The XrdSfsXioHandle class describes a handle to a buffer returned by
//! XrdSfsXio::Swap().
//-----------------------------------------------------------------------------

class XrdSfsXioHandle
{
public:

//-----------------------------------------------------------------------------
//! Obtain te address and, optionally, the length of the associated buffer.
//!
//! @param  blen  When not null will hold the length of the buffer. This is
//!               not to be confused with the length of data in the buffer!
//!
//! @return Pointer to the buffer.
//-----------------------------------------------------------------------------

virtual char *Buffer(int **blen=0) = 0;

//-----------------------------------------------------------------------------
//! Recycle a buffer that was previously given to the caller via
//! XrdSfsXio::Swap(). Use it when future swaps will no longer be requested.
//-----------------------------------------------------------------------------

virtual void  Recycle() = 0;

              XrdSfsXioHandle() {}
virtual      ~XrdSfsXioHandle() {}
};

/******************************************************************************/
/*                       C l a s s   X r d S f s X i o                        */
/******************************************************************************/
  
class XrdSfsXio
{
public:

//-----------------------------------------------------------------------------
//! Values return by Swap().
//-----------------------------------------------------------------------------

enum XioStatus {allOK = 0,   //!< Successful completion
                BadBuff,     //!< Swap failed, curBuff is bad.
                BadHandle,   //!< Swap failed, oHandle is bad
                NotWrite,    //!< Swap failed, not from a write() call
                TooMany      //!< Swap failed, too many buffs outstanding
               };

//-----------------------------------------------------------------------------
//! Swap the current I/O buffer
//!
//! @param  curBuff - The address of the current buffer. It must match the
//!                   the buffer that was most recently passed to the caller.
//! @param  curHand - Where the handle associated with curBuff is to be placed.
//! @param  oldHand - The handle associated with a buffer returned by a
//!                   previous call to Swap(). A value of zero indicates that
//!                   the caller is taking control of the buffer but has no
//!                   replacement buffer.
//! @return !allOK    One or more arguments or context is invalid. Nothing was
//!                   swapped. The returned value describes the problem. The
//!                   curHand has been set to zero.
//! @return =allOK    The handle associated with curBuff has been placed in
//!                   curHand. This handle must be used in a future Swap() call.
//-----------------------------------------------------------------------------

virtual XioStatus Swap(const char      * curBuff,
                       XrdSfsXioHandle *&curHand,
                       XrdSfsXioHandle * oldHand=0
                      ) = 0;

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

             XrdSfsXio() {}
virtual     ~XrdSfsXio() {}
};
#endif
