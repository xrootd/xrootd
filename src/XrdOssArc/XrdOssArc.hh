#ifndef _XRDOSSARC_H
#define _XRDOSSARC_H
/******************************************************************************/
/*                                                                            */
/*                          X r d O s s A r c . h h                           */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include "XrdOssArc/XrdOssArcDir.hh"
#include "XrdOssArc/XrdOssArcFile.hh"

class XrdOucEnv;

class XrdOssArc : public XrdOssWrapper
{
public:

//-----------------------------------------------------------------------------
//! Obtain a new director object to be used for future directory requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to a possibly wrapped XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------

virtual XrdOssDF *newDir(const char *tident) override
                        {return new XrdOssArcDir(tident,wrapPI.newDir(tident));}

//-----------------------------------------------------------------------------
//! Obtain a new file object to be used for a future file requests.
//!
//! @param  tident - The trace identifier.
//!
//! @return pointer- Pointer to a possibly wrapped XrdOssDF object.
//! @return nil    - Insufficient memory to allocate an object.
//-----------------------------------------------------------------------------

virtual XrdOssDF *newFile(const char *tident) override
                         {return new XrdOssArcFile(tident,wrapPI.newFile(tident));}

//-----------------------------------------------------------------------------
//! Change file mode settings.
//!
//! @param  path   - Pointer to the path of the file in question.
//! @param  mode   - The new file mode setting.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Chmod(const char * path, mode_t mode, XrdOucEnv *envP=0)
                       override;

//-----------------------------------------------------------------------------
//! Create file.
//!
//! @param  tid    - Pointer to the trace identifier.
//! @param  path   - Pointer to the path of the file to create.
//! @param  mode   - The new file mode setting.
//! @param  env    - Reference to environmental information.
//! @param  opts   - Create options:
//!                  XRDOSS_mkpath - create dir path if it does not exist.
//!                  XRDOSS_new    - the file must not already exist.
//!                  oflags<<8     - open flags shifted 8 bits to the left/
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Create(const char* tid, const char* path, mode_t mode,
                         XrdOucEnv& env, int opts=0) override;

//-----------------------------------------------------------------------------
//! Return storage system features.
//!
//! @return Storage system features (see XRDOSS_HASxxx flags).
//-----------------------------------------------------------------------------

virtual uint64_t  Features() override;

//-----------------------------------------------------------------------------
//! Execute a special storage system operation.
//!
//! @param  cmd    - The operation to be performed:
//!                  XRDOSS_FSCTLFA - Perform proxy file attribute operation
//! @param  alen   - Length of data pointed to by args.
//! @param  args   - Data sent with request, zero if alen is zero.
//! @param  resp   - Where the response is to be set, if any.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       FSctl(int cmd, int alen, const char *args, char **resp=0)
                       override;

//-----------------------------------------------------------------------------
//! Obtain detailed error message text for the immediately preceeding error
//! returned by any method in this class.
//!
//! @param  eText  - Where the message text is to be returned.
//!
//! @return True if message text is available, false otherwise.
//!
//! @note This method should be called using the same thread that encountered
//!       the error; otherwise, missleading error text may be returned.
//! @note Upon return, the internal error message text is cleared.
//-----------------------------------------------------------------------------

virtual bool    getErrMsg(std::string& eText) override;

//-----------------------------------------------------------------------------
//! Translate logical name to physical name V1 (deprecated).
//!
//! @param  Path   - Path in whose information is wanted.
//! @param  buff   - Pointer to the buffer to hold the new path.
//! @param  blen   - Length of the buffer.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Lfn2Pfn(const char *Path, char *buff, int blen) override;

//-----------------------------------------------------------------------------
//! Translate logical name to physical name V2.
//!
//! @param  Path   - Path in whose information is wanted.
//! @param  buff   - Pointer to the buffer to hold the new path.
//! @param  blen   - Length of the buffer.
//! @param  rc     - Place where failure return code is to be returned:
//!                  -errno or -osserr (see XrdOssError.hh).
//!
//! @return Pointer to the translated path upon success or nil on failure.
//-----------------------------------------------------------------------------
virtual
const char       *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
                         override;

//-----------------------------------------------------------------------------
//! Create a directory.
//!
//! @param  path   - Pointer to the path of the directory to be created.
//! @param  mode   - The directory mode setting.
//! @param  mkpath - When true the path is created if it does not exist.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Mkdir(const char *path, mode_t mode, int mkpath=0,
                        XrdOucEnv  *envP=0) override;

//-----------------------------------------------------------------------------
//! Remove a directory.
//!
//! @param  path   - Pointer to the path of the directory to be removed.
//! @param  Opts   - The processing options:
//!                  XRDOSS_Online   - only remove online copy
//!                  XRDOSS_isPFN    - path is already translated.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *envP=0)
                        override;

//-----------------------------------------------------------------------------
//! Rename a file or directory.
//!
//! @param  oPath   - Pointer to the path to be renamed.
//! @param  nPath   - Pointer to the path oPath is to have.
//! @param  oEnvP   - Environmental information for oPath.
//! @param  nEnvP   - Environmental information for nPath.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Rename(const char *oPath, const char *nPath,
                         XrdOucEnv  *oEnvP=0, XrdOucEnv *nEnvP=0) override;


//-----------------------------------------------------------------------------
//! Return state information on a file or directory.
//!
//! @param  path   - Pointer to the path in question.
//! @param  buff   - Pointer to the structure where info it to be returned.
//! @param  opts   - Options:
//!                  XRDOSS_preop    - this is a stat prior to open.
//!                  XRDOSS_resonly  - only look for resident files.
//!                  XRDOSS_updtatm  - update file access time.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Stat(const char *path, struct stat *buff,
                       int opts=0, XrdOucEnv *envP=0) override;

//-----------------------------------------------------------------------------
//! Truncate a file.
//!
//! @param  path   - Pointer to the path of the file to be truncated.
//! @param  fsize  - The size that the file is to have.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Truncate(const char *path, unsigned long long fsize,
                           XrdOucEnv *envP=0) override;

//-----------------------------------------------------------------------------
//! Remove a file.
//!
//! @param  path   - Pointer to the path of the file to be removed.
//! @param  Opts   - Options:
//!                  XRDOSS_isMIG  - this is a migratable path.
//!                  XRDOSS_isPFN  - do not apply name2name to path.
//!                  XRDOSS_Online - remove only the online copy.
//! @param  envP   - Pointer to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *envP=0)
                         override;

                  XrdOssArc(XrdOss& ossref) :XrdOssWrapper(ossref) {}

virtual          ~XrdOssArc() {}

// This is used to initialize the object from an extern "C" function.
//
int               Init(const char*, const char *parms, XrdOucEnv* envP);
};
#endif
