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
//! I/O in order to minimize data copying. To enable the use of this feature,
//! this object must be passed newFile() when creating a file object. This
//! object is associated with a file. When the file is closed, all outstanding
//! buffers should be released prior to returning from Close(). It is possible
//! to return buffers associated with one file via another file when SetXio()
//! is called. Buffer swapping is only supported for write operations.
//-----------------------------------------------------------------------------

class XrdSfsXio
{
public:

//-----------------------------------------------------------------------------
//! Values return by Release() and Swap().
//-----------------------------------------------------------------------------

enum XioStatus {allOK = 0,   //!< Successful completion
                BadBuff,     //!<      Failed, curBuff is bad.
                BadHandle,   //!<      Failed, oHandle is bad
                NotWrite,    //!< Swap failed, not from a write() call
                TooMany      //!< Swap failed, too many buffs outstanding
               };

//-----------------------------------------------------------------------------
//! Release a buffer that was previously given to the caller via Swap().
//!
//! @param  buff    - The address of the buffer associated with bHandle.
//! @param  bHandle - The buffer handle returned by a previous Swap() call.
//-----------------------------------------------------------------------------

virtual XioStatus Release(const char *buff, const void *bHandle) = 0;

//-----------------------------------------------------------------------------
//! Swap the current I/O buffer
//!
//! @param  curBuff - The address of the current buffer. It must match the
//!                   the buffer that was most recently passed to the caller.
//! @param  cHandle - Where the handle associated with curBuff is to be placed.
//! @param  oldBuff - The buffer that is to replace the current buffer and is
//!                   associated with oHandle returned by a previous Swap().
//!                   This is ignored if oHandle is set to NoHandle (see below).
//! @param  oHandle - The handle associated with oldBuff as returned by a
//!                   previous call to Swap(). A value of zero indicates that
//!                   the caller is taking control of the buffer but has no
//!                   replacement buffer. In this case, oldBuff is immaterial.
//! @return !allOK    One or more arguments or context is invalid. Nothing was
//!                   swapped. The returned value describes the problem. The
//!                   cHandle has been set to zero.
//! @return =allOK    The handle associated with curBuff has been placed in
//!                   cHandle. This handle along with curBuff must be used in
//!                   a future Release() or Swap() call.
//-----------------------------------------------------------------------------

virtual XioStatus Swap(const char * curBuff,
                       const void *&cHandle,
                       const char * oldBuff=0,
                       const void * oHandle=0
                      ) = 0;

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

             XrdSfsXio() {}
virtual     ~XrdSfsXio() {}
};
#endif
