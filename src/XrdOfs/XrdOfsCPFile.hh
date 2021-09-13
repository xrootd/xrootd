#ifndef __XRDOFSCPFILE_HH__
#define __XRDOFSCPFILE_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d O f s C P F i l e . h h                        */
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

#include <cstdint>
#include <ctime>
  
struct stat;
class  XrdOucIOVec;

class XrdOfsCPFile
{
public:

//-----------------------------------------------------------------------------
//! Append data to the checkpoint. Appends should be followed by Sync().
//!
//! @param  data    - Pointer to the data to be recorded.
//! @param  offset  - Offset in the source file where data came from.
//! @param  dlen    - Length of the data.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

int            Append(const char *data, off_t offset, int dlen);

//-----------------------------------------------------------------------------
//! Create a checkpoint
//!
//! @param  srcFN   - Pointer to the name of the source file being checkpointed.
//! @param  Stat    - Reference to source file stat information.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

int            Create(const char *lfn, struct stat &Stat);

//-----------------------------------------------------------------------------
//! Destroy a checkpoint
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

int            Destroy();

//-----------------------------------------------------------------------------
//! Place checkpoint file in error state.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

int            ErrState();

//-----------------------------------------------------------------------------
//! Get the checkpoint file name.
//!
//! @return pointer to the checkpoint file name. The pointer will be "???" if
//!         there is no checkpoint file established.
//-----------------------------------------------------------------------------

const char    *FName(bool trim=false);

//-----------------------------------------------------------------------------
//! Check if checpoint is established.
//!
//! @return True if checkpoint has been established and false otherwise.
//-----------------------------------------------------------------------------

bool           isActive() {return ckpFN != 0;}

//-----------------------------------------------------------------------------
//! Reserve space for a subsequent checkpoint.
//!
//! @param  dlen - the number of bytes that will be writen.
//! @param  nseg - the number of segements that will be written.
//!
//! @return True upon success and false otherwise
//-----------------------------------------------------------------------------

       bool    Reserve(int dlen, int nseg);

//-----------------------------------------------------------------------------
//! Get the file restore information from a checkpoint file.
//!
//! @param  rinfo   - Reference to the rpInfo object.
//! @param  ewhy    - Pointer to text explaining the error encountered.
//!
//! @return 0 upon success with the rpInfo object filled out. Otherwise,
//!           -ENODATA  File is empty (not committed) -ENODATA is returned.
//!           -ENOMEM   Insufficient memory to read the file.
//!           -errno    The file is corrupted, -errno indicates problem. If
//!           On error, if the source is known, rinfo.srcLFN will have the path.
//-----------------------------------------------------------------------------

class rInfo
     {public:
      friend class XrdOfsCPFile;
      const char  *srcLFN;     //!< Pointer to the source filename
      int64_t      fSize;      //!< Original size of the source file
      time_t       mTime;      //!< Original modification time of the source
      XrdOucIOVec *DataVec;    //!< A vector of data that must be written back
      int          DataNum;    //!< Number of elements in DataVec (may be 0)
      int          DataLen;    //!< Number of bytes to write back (may be 0)
                   rInfo();
                  ~rInfo();
      private:
      void        *rBuff;
     };

       int     RestoreInfo(rInfo &rinfo, const char *&ewhy);

//-----------------------------------------------------------------------------
//! Commit data to media.
//!
//! @return 0 upon success and -errno upon failure.
//-----------------------------------------------------------------------------

       int     Sync();

//-----------------------------------------------------------------------------
//! Get the name of the source file associated with a checkpoint file.
//!
//! @param  ckpfn   - Pointer to the name of the checkpoint file to use.
//!
//! @return pointer to either the source file name or reason it's not valid.
//-----------------------------------------------------------------------------

static char   *Target(const char *ckpfn);

//-----------------------------------------------------------------------------
//! Get the curent size of the checkpoint file.
//!
//! @param  nseg - the number of future segments to account for.
//!
//! @return The size of the file in bytes.
//-----------------------------------------------------------------------------

       int     Used(int nseg=0);

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  ckpfn   - Pointer to the name of the checkpoint file to use. When
//!                   supplied, creates are prohibited.
//-----------------------------------------------------------------------------

               XrdOfsCPFile(const char *cfn=0);

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

              ~XrdOfsCPFile();

private:
static char *genCkpPath();
static int   getSrcLfn(const char *cFN, rInfo &rinfo, int fd, int rc);

char *ckpFN;
int   ckpFD;
int   ckpDLen;
int   ckpSize;
};
#endif
