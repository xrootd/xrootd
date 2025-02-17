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
/*
virtual XrdOssDF     *newDir(const char *tident)
                            {return wrapPI.newDir(tident);}
*/
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
