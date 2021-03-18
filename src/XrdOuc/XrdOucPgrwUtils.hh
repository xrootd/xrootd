#ifndef __XRDOUCPGRWUTILS_HH__
#define __XRDOUCPGRWUTILS_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c P g r w U t i l s . h h                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdint.h>
#include <sys/types.h>

class XrdOucPgrwUtils
{
public:

//------------------------------------------------------------------------------
//! Compute a CRC32C checksums for a pgRead request.
//!
//! @param  data   Pointer to the data whose checksum it to be computed.
//! @param  offs   The offset at which the read or write occurs.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector to hold individual page checksums. The
//!                vector must be sized as computed by csNum().
//! @param  asnbo  Return checksum values in network bytes order.
//!
//! @return Each element of csval holds the checksum for the associated page.
//------------------------------------------------------------------------------

static void csCalc(const char* data, ssize_t offs, size_t count,
                   uint32_t* csval);

//------------------------------------------------------------------------------
//! Compute the required size of a checksum vector based on offset & length
//| applying the pgRead/pgWrite requirements.
//!
//! @param  offs   The offset at which the read or write occurs.
//! @param  count  The number of bytes read or to write.
//!
//! @return The number of checksums that are needed.
//------------------------------------------------------------------------------

static int   csNum(ssize_t offs, size_t count);

//------------------------------------------------------------------------------
//! Verify CRC32C checksums for a pgWrite request.
//!
//! @param  data   Pointer to the data whose checksum it to be verified.
//! @param  offs   The offset at which the read or write occurs.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector of expected page checksums. The
//!                vector must be sized as returned by csNum().
//! @param  bado   The offset in error when return > 0.
//! @param  badc   The length of erroneous data at bado.
//!
//! @return 0 if all the checksums match. Otherwise, the index+1 in the checksum
//!           vector containing the bad checksum (i.e. resumption point).
//------------------------------------------------------------------------------

static int   csVer(const char*     data,  ssize_t  offs, size_t  count,
                   const uint32_t* csval, ssize_t &bado, size_t &badc);

             XrdOucPgrwUtils() {}
            ~XrdOucPgrwUtils() {}

private:
};
#endif
