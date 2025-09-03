#ifndef __XRDPOSIXEXTRA_H__
#define __XRDPOSIXEXTRA_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x E x t r a . h h                       */
/*                                                                            */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/

#include <cstdint>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/types.h>

#include "XrdOuc/XrdOucCache.hh"

class XrdPosixCallBackIO;

//-----------------------------------------------------------------------------
//! Extended POSIX interface to XRootD.
//-----------------------------------------------------------------------------

class XrdPosixExtra
{
public:

//-----------------------------------------------------------------------------
//! Perform file oriented control operation (i.e. a query).
//!
//! @param  fildes  - Posix file descriptor of associated file.
//! @param  opc     - The requested operation (e.g. QFinfo).
//! @param  args    - The arguments (this is not necessarily a URL)
//! @param  resp    - Where the result is to be placed.
//!
//! @return >= 0    - Success, resp holds the response data.
//! @return  < 0      errno hold reason for failure.
//!
//! @note This call is routed via the cache if file is cache enabled using
//!       the associated CacheIO object returned by the cache attach call..
//-----------------------------------------------------------------------------

static int      Fctl(int fildes, XrdOucCacheOp::Code opc,
                     const std::string& args, std::string& resp);

//-----------------------------------------------------------------------------
//! Perform file system oriented control operation (i.e. a query).
//!
//! @param  opc     - The requested operation (e.g. QFSinfo).
//! @param  args    - The arguments (this should be a URL).
//! @param  resp    - Where the result is to be placed.
//! @param  viaCache- False -> Bypass calling any cache using URL in args.
//!                   True  -> Use Cache object API if cache enabled.
//!                            Otherwise, assume viaCache is false;
//!
//! @return >= 0    - Success, resp holds the response data.
//! @return  < 0      errno hold reason for failure.
//-----------------------------------------------------------------------------

static int      FSctl(XrdOucCacheOp::Code opc,
                      const std::string& args, std::string& resp,
                      bool viaCache=false);

//-----------------------------------------------------------------------------
//! Read file pages into a buffer and return corresponding checksums.
//!
//! @param  fildes  - File descriptor
//! @param  buffer  - pointer to buffer where the bytes are to be placed.
//! @param  offset  - The offset where the read is to start.
//! @param  rdlen   - The number of bytes to read.
//! @param  csvec   - A vector to be filled with the corresponding CRC32C
//!                   checksums for each page or page segment, if available.
//! @param  opts    - Options as noted.
//! @param  cbp     - Async version: return is made via callback.
//!
//! @return >= 0       Sync: Number of bytes that placed in buffer upon success;
//!                   Async: Always returns 0.
//! @return  < 0      errno hold reason for failure.
//-----------------------------------------------------------------------------

static const uint64_t forceCS = 0x0000000000000001ULL; // Gen chksum if missing

static ssize_t pgRead (int fildes, void* buffer, off_t offset, size_t rdlen,
                       std::vector<uint32_t> &csvec, uint64_t opts=0,
                       XrdPosixCallBackIO *cbp=0);

//-----------------------------------------------------------------------------
//! Write file pages into a file with corresponding checksums.
//!
//! @param  fildes  - File descriptor
//! @param  buffer  - pointer to buffer containing the bytes to write.
//! @param  offset  - The offset where the write is to start.
//! @param  wrlen   - The number of bytes to write.
//!                   be the last write to the file at or above the offset.
//! @param  csvec   - A vector which contains the corresponding CRC32 checksum
//!                   for each page or page segment. If size is 0, then
//!                   checksums are calculated. If not zero, the size must
//!                   equal the required number of checksums for offset/wrlen.
//! @param  opts    - Options as noted.
//! @param  cbp     - When supplied, return is made via callback.
//!
//! @return >= 0       Sync: The number of bytes written upon success.
//!                   Async: Always returns 0.
//! @return  < 0      errno hold reason for failure.
//-----------------------------------------------------------------------------

static ssize_t pgWrite(int fildes, void* buffer, off_t offset, size_t wrlen,
                       std::vector<uint32_t> &csvec, uint64_t opts=0,
                       XrdPosixCallBackIO *cbp=0);

               XrdPosixExtra() {}
              ~XrdPosixExtra() {}

};
#endif
