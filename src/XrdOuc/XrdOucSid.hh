#ifndef __XRDOUCSID_HH_
#define __XRDOUCSID_HH_
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c S i d . h h                           */
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

#include "XrdSys/XrdSysPthread.hh"

//-----------------------------------------------------------------------------
//! This class implements a fast bit vector based stream ID generator. When
//! stream ID's are generated on a connection basis, each connection can
//! allocate a local SID object sizing the bit vector to match the expected
//! number of simultaneous stream ID's in order to save memory. In order to
//! accomodate overflows, the local object can be initialized with a global SID
//! object that be used to obtain stream ID's when the local object runs out.
//! See the constructor description for the details.
//-----------------------------------------------------------------------------

class XrdOucSid
{
public:

//-----------------------------------------------------------------------------
//! The type to pass to Obtain(). Simply cast the char[2] to (theSid *).
//-----------------------------------------------------------------------------

union theSid {short sidS; char sidC[2];};

//-----------------------------------------------------------------------------
//! Obtain a stream ID. When not needed use Release() to recycle them.
//!
//! @param  sidP   -> the place where the new stream ID is to be placed.
//!
//! @return true      a stream ID was allocated.
//!
//! @return false     no more stream ID's are available. The sidP target was
//!                   not altered (i.e. does not contain a stream ID).
//-----------------------------------------------------------------------------

bool  Obtain(theSid *sidP);

//-----------------------------------------------------------------------------
//! Release a stream ID using the stream ID value.
//!
//! @param  sidP   -> to the stream ID cast as a theSid pointer.
//!
//! @return true      the stream ID was released and may be reassigned.
//!
//! @return false     the stream ID is invalid. No action performed.
//-----------------------------------------------------------------------------

bool  Release(theSid *sidP);

//-----------------------------------------------------------------------------
//! Unassign all stream ID's as if the object were newly allocated. Any global
//! stream ID object associated with this object is *not* reset.
//-----------------------------------------------------------------------------

void  Reset();

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  numSid    the maximum number of stream ID's that this object can
//!                   allocate. The value is rounded up to the next multiple of
//!                   eight. Every 8 ID's requires one byte of memory. Normally,
//!                   stream ID's range from 0 to 32767, though technically can
//!                   go to 64K-1 but are then represented as a negative short.
//! @param  mtproof   When true (the default) the object obtains a lock before
//!                   doing any operations. When false, it does not and must
//!                   be protected, if need be, by some other serialization.
//! @param  glblSid   a pointer to another SID object that can be used to
//!                   obtain stream ID's should this object run out of them.
//!                   Typically, this is a global pool of stream ID's.
//-----------------------------------------------------------------------------

      XrdOucSid(int numSid=256, bool mtproof=true, XrdOucSid *glblSid=0);

//-----------------------------------------------------------------------------
//! Destructor. Only this object is destroyed. The global object, if any, is
//!             not alteered in any way.
//-----------------------------------------------------------------------------

     ~XrdOucSid();

private:

XrdSysMutex    sidMutex;
XrdOucSid     *globalSid;
unsigned char *sidVec;
int            sidFree;
int            sidSize;
int            sidMax;
bool           sidLock;
};
#endif
