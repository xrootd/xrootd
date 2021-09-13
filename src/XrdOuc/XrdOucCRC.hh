#ifndef __XRDOUCCRC_HH__
#define __XRDOUCCRC_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c C R C . h h                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stddef.h>
#include <cstdint>

#include "XrdSys/XrdSysPageSize.hh"

class XrdOucCRC
{
public:

//------------------------------------------------------------------------------
//! Compute a CRC32 checksum.
//!
//! @note This is a historical method as it uses the very slow CRC32 algoritm.
//        It is now better to use the CRC32C hardware assisted methods.
//!
//! @param  data   Pointer to the data whose checksum it to be computed.
//! @param  count  The number of bytes pointed to by data.
//!
//! @return The CRC32 checksum.
//------------------------------------------------------------------------------

static uint32_t CRC32(const unsigned char *data, int count);

//------------------------------------------------------------------------------
//! Compute a CRC32C checksum using hardware assist if available.
//!
//! @param  data   Pointer to the data whose checksum it to be computed.
//! @param  count  The number of bytes pointed to by data.
//! @param  prevcs The previous checksum value. The initial checksum of
//!                checksum sequence should be zero, the default.
//!
//! @return The CRC32C checksum.
//------------------------------------------------------------------------------

static uint32_t Calc32C(const void* data, size_t count, uint32_t prevcs=0);

//------------------------------------------------------------------------------
//! Compute a CRC32C page checksums using hardware assist if available.
//!
//! @param  data   Pointer to the data whose checksum it to be computed.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector to hold individual page checksums. The
//!                vector must be sized:
//!                (count/XrdSys::PageSize + (count%XrdSys::PageSize != 0)).
//!
//! @return Each element of csval holds the checksum for the associated page.
//------------------------------------------------------------------------------

static void Calc32C(const void* data, size_t count, uint32_t* csval);

//------------------------------------------------------------------------------
//! Verify a CRC32C checksum using hardware assist if available.
//!
//! @param  data   Pointer to the data whose checksum it to be verified.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  The expected checksum.
//! @param  csbad  If csbad is not nil, the computed checksum is returned.
//!
//! @return True if the expected checksum equals the actual checksum;
//!         otherwise, false is returned.
//------------------------------------------------------------------------------

static bool Ver32C(const void*    data,  size_t    count,
                   const uint32_t csval, uint32_t* csbad=0);

//------------------------------------------------------------------------------
//! Verify a CRC32C page checksums using hardware assist if available.
//!
//! @param  data   Pointer to the data whose checksum it to be verified.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector of expected page checksums. The
//!                vector must be sized:
//!                (count/XrdSys::PageSize + (count%XrdSys::PageSize != 0)).
//! @param  valcs  Where the computed checksum is returned for the page
//!                whose verification failed; otherwise it is untouched.
//!
//! @return -1 if all the checksums match. Otherwise, the non-negative index
//!         into csval whose checksum does not match.
//------------------------------------------------------------------------------

static int  Ver32C(const void*     data,  size_t    count,
                   const uint32_t* csval, uint32_t& valcs);

//------------------------------------------------------------------------------
//! Verify a CRC32C page checksums using hardware assist if available.
//!
//! @param  data   Pointer to the data whose checksum it to be verified.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector of expected page checksums. The
//!                vector must be sized (count/PageSize+(count%PageSize != 0)).
//! @param  valok  Pointer to a vector of the same size as csval to hold
//!                the results of the comparison (true matches, o/w false).
//!
//! @return True if all the checksums match with each element of valok set to
//!         true. Otherwise, false is returned and false is set in valok for
//!         each page that did not match the expected checksum.
//------------------------------------------------------------------------------

static bool Ver32C(const void*     data,  size_t count,
                   const uint32_t* csval, bool*  valok);

//------------------------------------------------------------------------------
//! Verify a CRC32C page checksums using hardware assist if available.
//!
//! @param  data   Pointer to the data whose checksum it to be verified.
//! @param  count  The number of bytes pointed to by data.
//! @param  csval  Pointer to a vector of expected page checksums. The
//!                vector must be sized (count/PageSize+(count%PageSize != 0)).
//! @param  valcs  Pointer to a vector of the same size as csval to hold
//!                the computed checksum.
//!
//! @return True if all the checksums match; false otherwise.
//------------------------------------------------------------------------------

static bool Ver32C(const void*     data,  size_t    count,
                   const uint32_t* csval, uint32_t* valcs);

                    XrdOucCRC() {}
                   ~XrdOucCRC() {}

private:

static unsigned int crctable[256];
};
#endif
