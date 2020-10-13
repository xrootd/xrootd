#ifndef __XRDOUCCHKPNT_HH__
#define __XRDOUCCHKPNT_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c C h k P n t . h h                        */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! The XrdOucChkPnt class defines a virtual interface to perform file
//! checkpoints in a uniform way, irrespective of the underlying implementation.
//-----------------------------------------------------------------------------

struct iov;

class XrdOucChkPnt
{
public:

//-----------------------------------------------------------------------------
//! Create a checkpoint.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

virtual int  Create() = 0;

//-----------------------------------------------------------------------------
//! Delete a checkpoint.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

virtual int  Delete() = 0;

//-----------------------------------------------------------------------------
//! Indicate that the checkpointing is finished. Any outstanding checkpoint
//! should be delete and the object should delete itself if necessary.
//-----------------------------------------------------------------------------

virtual void Finished() = 0;

//-----------------------------------------------------------------------------
//! Query checkpoint limits.
//!
//! @param  range   - reference to where limits are placed.
//!                   range.length - holds maximum checkpoint length allowed.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

virtual int  Query(struct iov &range) = 0;

//-----------------------------------------------------------------------------
//! Restore a checkpoint.
//!
//! @param  readok  - When not nil and an error occurs readok is set true
//!                   if read access is still allowed; otherwise no access
//!                   should be allowed.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

virtual int  Restore(bool *readok=0) = 0;

//-----------------------------------------------------------------------------
//! Truncate a file to a specific size.
//!
//! @param  range   - reference to the file truncate size in offset.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

virtual int  Truncate(struct iov *&range) = 0;

//-----------------------------------------------------------------------------
//! Write data to a checkpointed file.
//!
//! @param  range   - reference to the file pieces to write.
//! @param  rnum    - number of elements in "range".
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

virtual int  Write(struct iov *&range, int rnum) = 0;

//-----------------------------------------------------------------------------
//! Constructor and destructor.
//-----------------------------------------------------------------------------

             XrdOucChkPnt() {}

virtual     ~XrdOucChkPnt() {} // Use Finished() not delete!
};
#endif
