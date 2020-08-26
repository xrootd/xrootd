#ifndef __XRDXROOTDGSTREAM_HH_
#define __XRDXROOTDGSTREAM_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t G S t r e a m . h h                     */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>

//-----------------------------------------------------------------------------
//! This class implements a generic reporter for the XRootD monitoring stream,
//! also known as the G-Stream. It is passed to various plugins when monitoring
//! is enabled to allow plugins to add monitoring information into the G-Stream.
//-----------------------------------------------------------------------------

class XrdXrootdGSReal;

class XrdXrootdGStream
{
public:

//-----------------------------------------------------------------------------
//! Flush any pending monitoring messages to the data collector. Also, see
//! the related SetAutoFlush() method.
//-----------------------------------------------------------------------------

void      Flush();

//-----------------------------------------------------------------------------
//! Obtain a dictionary ID to map a string to an integer ID. The mapping is
//! automatically sent to the monitor collector for future use using the
//! 'd' or 'i' mapping identifier.
//!
//! @param  text   -> the null terminated string to be assigned an ID. The
//!                   text must be less than or equal to 1024 characters.
//! @param isPath     when true the text specified a file system path and
//!                   identified as a XROOTD_MON_MAPPATH item. Otherwise,
//!                   it is identified as a XROOTD_MON_MAPINFO item.
//!
//! @return the integer identifier assigned to the string information. The
//!         returned value is in network byte order!
//-----------------------------------------------------------------------------

uint32_t  GetDictID(const char *text, bool isPath=false);

//-----------------------------------------------------------------------------
//! Check if payload is fronted by a header.
//!
//! @return true      payload is fronted by a header.
//! @return false     payload is not fronted by a header. GetDictID() mappings
//!                   are not sent and must be manually included inline.
//-----------------------------------------------------------------------------

bool      HasHdr();

//-----------------------------------------------------------------------------
//! Insert information into the G-Stream.
//!
//! @param  data   -> to null-terminated text to be included in the G-Stream.
//! @param  dlen      the length of the text *including* the null character.
//!                   Requires that (8 <= dlen <= MaxDataLen); defined below.
//!
//! @return true      data included.
//! @return false     data rejected; invalid length or is not null terminated.
//-----------------------------------------------------------------------------

bool      Insert(const char *data, int dlen);

//-----------------------------------------------------------------------------
//! Insert information into the G-Stream using the data placed in the buffer
//! space obtained by a previous call to Reserve(). Upon return, this object
//! is unlocked.
//!
//! @param  dlen      the number of bytes actually present in the buffer. The
//!                   text must end with a null byte and dlen must be
//!                   8 <= dlen <= dlen used in the previous Reserve() call.
//!
//! @return true      data included.
//! @return false     data rejected; invalid length or no buffer outstanding.
//-----------------------------------------------------------------------------

bool      Insert(int dlen);

//-----------------------------------------------------------------------------
//! Obtain a buffer space for information. This object is locked and no other
//! thread can insert information until the buffer is inserted using Insert().
//!
//! @param  dlen      the number of bytes required to be available for use.
//!                   Requires that (8 <= dlen <= MaxDataLen); defined below.
//!
//! @return !0        pointer to a dlen sized buffer.
//! @return =0        invalid length specified or a buffer is outstanding.
//-----------------------------------------------------------------------------

char     *Reserve(int dlen);

//-----------------------------------------------------------------------------
//! Set autoflush time interval (or disable it). Disabling autoflush may be
//! useful when data is periodically generated at a low rate and manual
//! flushing would produce more consistent results.
//!
//! @param  afsec     Number of seconds between autoflushing. A zero or
//!                   negative value disables autoflush. The default is
//!                   600 seconds (i.e. 10 minutes) subject to what is
//!                   specified via the xrootd.monitor flush directive.
//!                   Positive values less that 60 are considered to be 60.
//!
//! @return The previous auto-flush setting.
//-----------------------------------------------------------------------------

int       SetAutoFlush(int afsec);

//-----------------------------------------------------------------------------
//! Get the amount of buffer space remaining.
//!
//! @return The maximum number of bytes that can be inserted at time of call.
//-----------------------------------------------------------------------------

int       Space();

//-----------------------------------------------------------------------------
//! The larest amount of data that can be inserted in a single call to GStream.
//-----------------------------------------------------------------------------
static
const int MaxDataLen = 65280;

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  gsRef     refrence to the G-Stream implementation.
//-----------------------------------------------------------------------------

          XrdXrootdGStream(XrdXrootdGSReal &gsRef) : gStream(gsRef) {}

protected:

//-----------------------------------------------------------------------------
//! Destructor. This stream should never be directly deleted.
//-----------------------------------------------------------------------------

         ~XrdXrootdGStream() {}

private:

XrdXrootdGSReal &gStream;
};
#endif
