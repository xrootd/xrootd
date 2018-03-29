#ifndef __SFS_DIO_H__
#define __SFS_DIO_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d S f s D i o . h h                           */
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

#include <sys/types.h>
#include <unistd.h>

#include "XrdOuc/XrdOucSFVec.hh"


//-----------------------------------------------------------------------------
//! XrdSfsDio.hh
//!
//! This class is used to define specialized I/O interfaces that can be
//! provided to the underlying filesystem. This object is normally passed via
//! the read() call should fctl() indicate this interface is to be used.
//-----------------------------------------------------------------------------

class XrdSfsDio
{
public:

//-----------------------------------------------------------------------------
//! Send data to a client using the sendfile() system interface.
//!
//! @param  fildes - The file descriptor to use to effect a sendfile() for
//!                  all of the requested data. The original offset and
//!                  length are used relative to this file descriptor.
//!
//! @return >0     - data has been sent in a previous call. This is indicative
//!                  of a logic error in SendData() as only one call is allowed.
//! @return =0     - data has been sent.
//! @return <0     - A fatal transmission error occurred. SendData() should
//!                  return SFS_ERROR to force the connection to be closed.
//-----------------------------------------------------------------------------

virtual int SendFile(int fildes) = 0;

//-----------------------------------------------------------------------------
//! Send data to a client using the sendfile() system interface.
//!
//! @param  sfvec  - One or more XrdOucSFVec elements describing what should be
//!                  transferred. The first element of the vector *must* be
//!                  available for use by the interface for proper framing.
//!                  That is, start filling in elements at sfvec[1] and sfvnum
//!                  should be the count of elements filled in plus 1.
//! @param  sfvnum - total number of elements in sfvec and includes the first
//!                  unused element. There is a maximum number of elements
//!                  that the vector may have; defined inside XrdOucSFVec.
//!
//! @return >0     - either data has been sent in a previous call or the total
//!                  amount of data in sfvec is greater than the original
//!                  request. This is indicative of a SendData() logic error.
//! @return =0     - data has been sent.
//! @return <0     - A fatal transmission error occurred. SendData() should
//!                  return SFS_ERROR to force the connection to be closed.
//-----------------------------------------------------------------------------

virtual int  SendFile(XrdOucSFVec *sfvec, int sfvnum) = 0;

//-----------------------------------------------------------------------------
//! Change the file descriptor setting and, consequently, interface processing.
//!
//! @param  fildes - The file descriptor to use in the future, as follows:
//!                  <  0 - Disable sendfile and always use read().
//!                  >= 0 - Enable  sendfile and always use sendfile() w/o
//!                         invoking this interface (i.e. fast path).
//-----------------------------------------------------------------------------

virtual void SetFD(int fildes) = 0;

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

             XrdSfsDio() {}
virtual     ~XrdSfsDio() {}
};
#endif
