#ifndef __POSIX_CALLBACK_HH__
#define __POSIX_CALLBACK_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d P o s i x C a l l B a c k . h h                    */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucCache.hh"

//-----------------------------------------------------------------------------
//! @brief An abstract class to define a callback for Open() call.
//!
//! This abstract class defines the callback interface for Open() calls.
//! When passed, the request is done in the background. When a callback
//! object is supplied, the method *always* return -1. However, if started
//! successfully, the method sets errno to EINPROGRESS. Otherwise, errno
//! contain the reason the request immediately failed. Upon completion, the
//! the callback's Compete() method is invoked.  The Result parameter contains
//! what the method would have returned if it were executed synchronously:
//! for succsessful Open() it is a non-negative file descriptor but on failure
//! -1 with errno indicating why Open() failed. The caller is responsible for
//! deleting the callback object after it has been invoked. Callbacks are
//! executed in a separate thread.
//-----------------------------------------------------------------------------

class XrdPosixCallBack
{
public:

virtual void Complete(int Result) = 0;

             XrdPosixCallBack() {}
virtual     ~XrdPosixCallBack() {}
};

//-----------------------------------------------------------------------------
//! @brief An abstract class to define a callback for file I/O requests.
//!
//! This abstract class defines the callback interface for Fsync(), Pread(),
//! Pwrite(), and VRead(). Async I/O is not supported for Read(), Readv(),
//! Write(), and Writev() as these update the file offset associated with the
//! file descriptor and cannot be made async safe. All results are return via
//! the callback object. For immediate errors, the callback is invoked on the
//! calling thread. Any locks held by the calling thread that are obtained by
//! the callback must be recursive locks in order to avoid a deadlock.
//-----------------------------------------------------------------------------

class XrdPosixFile;

class XrdPosixCallBackIO : public XrdOucCacheIOCB
{
public:
friend class XrdPosixExtra;
friend class XrdPosixXrootd;

virtual void Complete(ssize_t Result) = 0;

             XrdPosixCallBackIO() : theFile(0) {}
virtual     ~XrdPosixCallBackIO() {}

private:

void         Done(int result);

XrdPosixFile *theFile;
};
#endif
