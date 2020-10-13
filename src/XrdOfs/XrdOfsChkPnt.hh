#ifndef __XRDOFSCHKPNT_HH__
#define __XRDOFSCHKPNT_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d O f s C h k P n t . h h                        */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOfs/XrdOfsCPFile.hh"
#include "XrdOuc/XrdOucChkPnt.hh"

//-----------------------------------------------------------------------------
//! The XrdOfsChkPnt class defines an implementation of file checkpoints.
//-----------------------------------------------------------------------------

struct iov;
class  XrdOssDF;

class XrdOfsChkPnt : public XrdOucChkPnt
{
public:

//-----------------------------------------------------------------------------
//! Create a checkpoint.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

        int  Create();

//-----------------------------------------------------------------------------
//! Delete a checkpoint.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

        int  Delete();

//-----------------------------------------------------------------------------
//! Indicate that the checkpointing is finished. Any outstanding checkpoint
//! should be delete and the object should delete itself if necessary.
//-----------------------------------------------------------------------------

        void Finished() {delete this;}

//-----------------------------------------------------------------------------
//! Query checkpoint limits.
//!
//! @param  range   - reference to where limits are placed.
//!                   range.length - holds maximum checkpoint length allowed.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

        int  Query(struct iov &range);

//-----------------------------------------------------------------------------
//! Restore a checkpoint.
//!
//! @param  readok  - When not nil and an error occurs readok is set true
//!                   if read access is still allowed; otherwise no access
//!                   should be allowed.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

        int  Restore(bool *readok=0);

//-----------------------------------------------------------------------------
//! Truncate a file to a specific size.
//!
//! @param  range   - reference to the file truncate size in offset.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

        int  Truncate(struct iov *&range);

//-----------------------------------------------------------------------------
//! Write data to a checkpointed file.
//!
//! @param  range   - reference to the file pieces to write.
//! @param  rnum    - number of elements in "range".
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

        int  Write(struct iov *&range, int rnum);

//-----------------------------------------------------------------------------
//! Constructor and destructor.
//!
//! @param  ossfl   - reference to the Oss File Object for the source file.
//! @param  lfn     - pointer to the source file path.
//! @param  ckpfn   - pointer to optional prexisting checkpoint file.
//-----------------------------------------------------------------------------

             XrdOfsChkPnt(XrdOssDF &ossfl, const char *lfn, const char *ckpfn=0)
                         : lFN(lfn), cpFile(ckpfn), ossFile(ossfl), fSize(0),
                           cpUsed(0) {}

virtual     ~XrdOfsChkPnt() {}

private:

int          Failed(const char *opn, int rc, bool *readok);

const char   *lFN;
XrdOfsCPFile  cpFile;
XrdOssDF     &ossFile;
int64_t       fSize;
int           cpUsed;
};
#endif
