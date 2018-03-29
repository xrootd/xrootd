#ifndef __XRDSSISTREAM_HH__
#define __XRDSSISTREAM_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d S s i S t r e a m . h h                        */
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

#include <errno.h>

#include "XrdSsi/XrdSsiErrInfo.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiStream class describes an object capable of providing data for a
//! response in real time. A pointer to such an object may be used to set this
//! response mode via XrdSsiResponder::SetResponse(). Two kinds of streams exist:
//!
//! Active  the stream supplies the buffer that contains the response data.
//!         The buffer is recycled via Buffer::Recycle() once the response data
//!         is sent. Active streams are supported only server-side.
//! Passive the stream requires a buffer to be passed to it where response data
//!         will be placed. Only passive streams are created on the client-side.
//!         Passive streams can also work in asynchronous mode. However, async
//!         mode is never used server-side but may be requested client-side.
//!
//! The type of stream must be declared at the time the stream is created. You
//! must supply an implementation for the associated stream type.
//-----------------------------------------------------------------------------

class XrdSsiStream
{
public:

//-----------------------------------------------------------------------------
//! The Buffer object is returned by active streams as they supply the buffer
//! holding the requested data. Once the buffer is no longer needed it must be
//! recycled by calling Recycle().
//-----------------------------------------------------------------------------

class Buffer
{
public:
virtual void    Recycle() = 0;     //!> Call to recycle the buffer when finished

char           *data;              //!> -> Buffer containing the data
Buffer         *next;              //!> For chaining by buffer receiver

                Buffer(char *dp=0) : data(dp), next(0) {}
virtual        ~Buffer() {}
};

//-----------------------------------------------------------------------------
//! Synchronously obtain data from an active stream (server-side only).
//!
//! @param  eRef The object to receive any error description.
//! @param  dlen  input:  the optimal amount of data wanted (this is a hint)
//!               output: the actual amount of data returned in the buffer.
//! @param  last  input:  should be set to false.
//!               output: if true it indicates that no more data remains to be
//!                       returned either for this call or on the next call.
//!
//! @return =0    No more data remains or an error occurred:
//!               last = true:  No more data remains.
//!               last = false: A fatal error occurred, eRef has the reason.
//! @return !0    Pointer to the Buffer object that contains a pointer to the
//!               the data (see below). The buffer must be returned to the
//!               stream using Buffer::Recycle(). The next member is usable.
//-----------------------------------------------------------------------------

virtual Buffer *GetBuff(XrdSsiErrInfo &eRef, int &dlen, bool &last)
                   {eRef.Set("Not an active stream", EOPNOTSUPP); return 0;}

//-----------------------------------------------------------------------------
//! Asynchronously obtain data from a passive stream (client-side only).
//!
//! @param  eRef  reference to where error information is to be placed for
//!               encountered before during the stream initiation. When data is
//!               ready for processing, the ProcessResponseData() callback is
//!               called on the request associated with this stream.
//!               Also see XrdSsiRequest::GetResponseData() helper method.
//! @param  buff  pointer to the buffer to receive the data. The buffer must
//!               remain valid until ProcessResponse() is called.
//! @param  blen  the length of the buffer (i.e. maximum that can be returned).
//!
//! @return true  The stream has been successfully scheduled to return the data.
//! @return false The stream could not be scheduled; eRef holds the reason.
//-----------------------------------------------------------------------------

virtual bool    SetBuff(XrdSsiErrInfo &eRef, char *buff, int  blen)
                   {eRef.Set("Not a passive stream", EOPNOTSUPP); return false;}

//-----------------------------------------------------------------------------
//! Synchronously obtain data from a passive stream (client- or server-side).
//!
//! @param  eRef  The object to receive any error description.
//! @param  buff  pointer to the buffer to receive the data.
//!               request object is notified that the operation completed.
//! @param  blen  the length of the buffer (i.e. maximum that can be returned).
//! @param  last  input:  should be set to false.
//!               output: if true it indicates that no more data remains to be
//!                       returned either for this call or on the next call.
//!
//! @return >0    The number of bytes placed in buff.
//! @return =0    No more data remains and the stream becomes invalid.
//! @return <0    Fatal error occurred; eRef holds the reason.
//-----------------------------------------------------------------------------

virtual int     SetBuff(XrdSsiErrInfo &eRef, char *buff, int blen, bool &last)
                   {eRef.Set("Not a passive stream", EOPNOTSUPP); return 0;}

//-----------------------------------------------------------------------------
//! Stream type descriptor:
//!
//! isActive  - Active  stream that supplies it own buffers with data.
//!             GetBuff() & RetBuff() must be used.
//!
//! isPassive - Passive stream that provides data via a supplied buffer.
//!             SetBuff() must be used.
//-----------------------------------------------------------------------------

        enum    StreamType {isActive = 0, isPassive};

//-----------------------------------------------------------------------------
//! Get the stream type descriptor.
//!
//! @return The stream type, isActive or isPassive.
//-----------------------------------------------------------------------------

StreamType      Type() {return SType;}

                XrdSsiStream(StreamType stype) : SType(stype) {}

virtual        ~XrdSsiStream() {}

protected:

const StreamType SType;
};
#endif
