#ifndef _XRDOSSARCFILE_H
#define _XRDOSSARCFILE_H
/******************************************************************************/
/*                                                                            */
/*                      X r d O s s A r c F i l e . h h                       */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOss/XrdOssWrapper.hh"

class  XrdOssArcZipFile;
class  XrdOucEnv;
struct stat;

class XrdOssArcFile : public XrdOssWrapDF
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
//! Return state information for this file.
//!
//! @param  buf    - Pointer to the structure where info it to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Fstat(struct stat* buf) override;

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
//! Preread file blocks into the file system cache.
//!
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to pre-read.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

ssize_t Read(off_t offset, size_t size) override {return 0;}

//-----------------------------------------------------------------------------
//! Read file bytes into a buffer.
//!
//! @param  buffer  - pointer to buffer where the bytes are to be placed.
//! @param  offset  - The offset where the read is to start.
//! @param  size    - The number of bytes to read.
//!
//! @return >= 0      The number of bytes that placed in buffer.
//! @return  < 0      -errno or -osserr upon failure (see XrdOssError.hh).
//-----------------------------------------------------------------------------

ssize_t Read(void* buffer, off_t offset, size_t size) override;

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

                XrdOssArcFile(const char* tident, XrdOssDF* df)
                             : XrdOssWrapDF(*df), ossDF(df) {}

virtual        ~XrdOssArcFile();

private:
XrdOssDF*         ossDF;          // Underlying dir/file object
XrdOssArcZipFile* zFile =  0;
int               theFD = -1;
};
#endif
