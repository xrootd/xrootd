#ifndef _XRDOFSCKSFILE_H
#define _XRDOFSCKSFILE_H
/******************************************************************************/
/*                                                                            */
/*                      X r d O f s C k s F i l e . h h                       */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <map>

#include "XrdOss/XrdOssWrapper.hh"
#include "XrdSys/XrdSysPthread.hh"

class  XrdCks;
class  XrdCksCalc;
class  XrdOucEnv;
class  XrdSfsAio;
class  XrdSysLogger;

class XrdOfsCksFile : public XrdOssWrapDF
{
public:

//-----------------------------------------------------------------------------
//! Close file.
//!
//! @param  retsz     If not nil, where the size of the file is to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Close(long long* retsz=0) override;

//-----------------------------------------------------------------------------
//! Set the size of the associated file.
//!
//! @param  flen   - The new size of the file.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Ftruncate(unsigned long long flen) override;

//-----------------------------------------------------------------------------
// Perform one time initialization
//
// @param cp - Pointer to checksum manager.
// @param ep - Pointer to the environment.
//-----------------------------------------------------------------------------

static
void    Init(XrdCks* cp, XrdOucEnv* ep);

//-----------------------------------------------------------------------------
//! Open a file.
//!
//! @param  path   - Pointer to the path of the file to be opened.
//! @param  Oflag  - Standard open flags.
//! @param  Mode   - File open mode (ignored unless creating a file).
//! @param  env    - Reference to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Open(const char* path, int Oflag, mode_t Mode, XrdOucEnv& env)
             override;

//-----------------------------------------------------------------------------
//! Write file pages into a file with corresponding checksums.
//!
//! @param  buffer  - pointer to buffer containing the bytes to write.
//! @param  offset  - The offset where the write is to start. It must be
//!                   page aligned.
//! @param  wrlen   - The number of bytes to write. If amount is not an
//!                   integral number of XrdSys::PageSize bytes, then this must
//!                   be the last write to the file at or above the offset.
//! @param  csvec   - A vector which contains the corresponding CRC32 checksum
//!                   for each page. It must be size to
//!                   wrlen/XrdSys::PageSize + (wrlen%XrdSys::PageSize != 0)
//! @param  opts    - Processing options (see above).
//!
//! @return >= 0      The number of bytes written upon success.
//!                   or -errno or -osserr upon failure. (see XrdOssError.hh).
//! @return  < 0      -errno or -osserr upon failure. (see XrdOssError.hh).
//-----------------------------------------------------------------------------

ssize_t pgWrite(void* buffer, off_t offset, size_t wrlen,
                        uint32_t* csvec, uint64_t opts) override;

//-----------------------------------------------------------------------------
//! Write file pages and checksums using asynchronous I/O.
//!
//! @param  aioparm - Pointer to async I/O object controlling the I/O.
//! @param  opts    - Processing options (see above).
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     pgWrite(XrdSfsAio* aioparm, uint64_t opts) override;

//-----------------------------------------------------------------------------
// Indicate if a checksum object can be used in real-time.
//
// @param  cP  - pointer to checksum object.
//
// @return True if object can be used by this object, false otherwise.
//-----------------------------------------------------------------------------

static
bool    Viable(XrdCksCalc* cP);

//-----------------------------------------------------------------------------
//! Write file bytes from a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes reside.
//! @param  offset  - The offset where the write is to start.
//! @param  size    - The number of bytes to write.
//!
//! @return >= 0      The number of bytes that were written.
//! @return <  0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

ssize_t Write(const void* buffer, off_t offset, size_t size) override;

//-----------------------------------------------------------------------------
//! Write file bytes using asynchronous I/O.
//!
//! @param  aiop    - Pointer to async I/O object controlling the I/O.
//!
//! @return 0 upon if request started success or -errno or -osserr
//!         (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Write(XrdSfsAio* aiop) override;

//-----------------------------------------------------------------------------
//! Write file bytes as directed by the write vector.
//!
//! @param  writeV    pointer to the array of write requests.
//! @param  wrvcnt    the number of elements in writeV.
//!
//! @return >=0       The numbe of bytes read.
//! @return < 0       -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

ssize_t WriteV(XrdOucIOVec *writeV, int wrvcnt) override;

         XrdOfsCksFile(const char* tid, const char* path,
                       XrdOssDF*   df,  XrdCksCalc* cP);

virtual ~XrdOfsCksFile();

private:

const char* RTC_CB32(const void* inBuff, off_t inoff, int inLen);
const char* RTE_CB32(char* eBuff, int eBLen);

const char* (XrdOfsCksFile::*ProcessRTC)(const void*, off_t, int);
const char* (XrdOfsCksFile::*ProcessRTE)(char*, int);

XrdSysMutex cksMtx;
const char* tident;
const char* fPath;          // Valid throughout object lifetime
XrdOssDF*   ossDF;          // Underlying dir/file object
XrdCksCalc* calcP;
const char* cksName;
XrdCksCalc* altcP;
off_t       nextOff;

struct      inSeg
           {off_t       segBeg;
            int         segLen;
            uint32_t    segCks; // adler32 or crc...

            inSeg(off_t newOff, int newLen, uint32_t newCks)
                    : segBeg(newOff), segLen(newLen), segCks(newCks) {}
           ~inSeg() {}
           };

std::map<off_t, inSeg> segMap;

bool        Dirty;
};
#endif
