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

#include <cstdint>
#include <vector>
#include <sys/types.h>

class XrdOucPgrwUtils
{
public:

//------------------------------------------------------------------------------
//! Compute a CRC32C checksums for a pgRead/pgWrite request.
//!
//! @param  data   Pointer to the data whose checksum it to be computed.
//! @param  offs   The offset at which the read or write occurs.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector to hold individual page checksums. The
//!                raw vector must be sized as computed by csNum(). When
//!                passed an std::vector, it is done automatically.
//!
//! @return Each element of csval holds the checksum for the associated page.
//------------------------------------------------------------------------------

static void csCalc(const char* data, off_t offs, size_t count,
                   uint32_t* csval);

static void csCalc(const char* data, off_t offs, size_t count,
                   std::vector<uint32_t> &csvec);

//------------------------------------------------------------------------------
//! Compute the required size of a checksum vector based on offset & length
//| applying the pgRead/pgWrite requirements.
//!
//! @param  offs   The offset at which the read or write occurs.
//! @param  count  The number of bytes read or to write.
//!
//! @return The number of checksums that are needed.
//------------------------------------------------------------------------------

static int   csNum(off_t offs, int count);

//------------------------------------------------------------------------------
//! Compute the required size of a checksum vector based on offset & length
//| applying the pgRead/pgWrite requirements and return the length of the first
//! and last segments required in the iov vector.
//!
//! @param  offs   The offset at which the read or write occurs.
//! @param  count  The number of bytes read or to write excluding checksums.
//! @param  fLen   The number of bytes needed in iov[0].iov_length
//! @param  lLen   The number of bytes needed in iov[csnum-1].iov_length
//!
//! @return The number of checksums that are needed.
//------------------------------------------------------------------------------

static int   csNum(off_t offs, int count, int &fLen, int &lLen);

//------------------------------------------------------------------------------
//! Verify CRC32C checksums for a pgWrite request.
//!
//! @param  dInfo  Reference to the data information used or state control.
//! @param  bado   The offset in error when return false.
//! @param  badc   The length of erroneous data at bado.
//!
//! @return true if all the checksums match. Otherwise, false is returned with
//!         bado and badc set and dInfo is updated so that the next call with
//!         the same dInfo will verify the remaing data. To avoid an unneeded
//!         call first check if dInfo.count is positive.
//------------------------------------------------------------------------------

struct dataInfo
      {const char*     data;   //!< Pointer to data buffer
       const uint32_t* csval;  //!< Pointer to vector of checksums
       off_t           offs;   //!< Offset associated with data
       int             count;  //!< Number of bytes to check

       dataInfo(const char* dP, const uint32_t* cP, off_t o, int n)
               : data(dP), csval(cP), offs(o), count(n) {}
      };

static bool  csVer(dataInfo &dInfo, off_t &bado, int &badc);

//------------------------------------------------------------------------------
//! Compute the layout for an iovec that receives network bytes applying
//| pgRead/pgWrite requirements.
//!
//! @param  layout Reference to the layout parameter (see below).
//! @param  offs   recvLayout: Offset at which the subsequent write occurs.
//!                sendLayout: Offset at which the preceding  read  occurs.
//! @param  dlen   recvLayout: Nmber of sock bytes to receive with    checksums.
//! @param  dlen   sendLayout: Nmber of file bytes to read    without checksums.
//! @param  bsz    The size of the buffer exclusive of any checksums and must
//!                be a multiple of 4096 (one page). If it's <= 0 then then the
//!                layout is computed as if bsz could fully accomodate the dlen.
//!
//! @return The number of checksums that are needed. If the result is zero then
//!         the supplied offset/dlen violates requirements amd eWhy holds reason.
//!
//! @note The iovec layout assumes iov[0] reads the checksum and iov[1] reads
//!       only the data where the last such pair is iov[csnum*-2],iov[csnum*-1].
//! @note dataLen can be used to adjust the next offset for filesystem I/O while
//!       sockLen is the total number of network bytes to receive or send.
//------------------------------------------------------------------------------

struct Layout
{
off_t       bOffset; //!< Buffer offset to apply iov[1].iov_base
int         dataLen; //!< Total number of filesys bytes the iovec will handle
int         sockLen; //!< Total number of network bytes the iovec will handle
int         fLen;    //!< Length to use for iov[1].iov_len
int         lLen;    //!< Length to use for iov[csnum*2-1].iov_len)
const char *eWhy;    //!< Reason for failure when zero is returned
};

static int  recvLayout(Layout &layout, off_t offs, int dlen, int bsz=0);

static int  sendLayout(Layout &layout, off_t offs, int dlen, int bsz=0);

             XrdOucPgrwUtils() {}
            ~XrdOucPgrwUtils() {}

private:
};
#endif
