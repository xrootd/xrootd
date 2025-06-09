#ifndef _XRDOSSARCDIR_H
#define _XRDOSSARCDIR_H
/******************************************************************************/
/*                                                                            */
/*                       X r d O s s A r c D i r . h h                        */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdOssArcDir : public XrdOssWrapDF
{
public:

/******************************************************************************/
/*            D i r e c t o r y   O r i e n t e d   M e t h o d s             */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! Close a directory or file.
//!
//! @param  retsz     If not nil, where the size of the file is to be returned.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Close(long long *retsz=0) override;

//-----------------------------------------------------------------------------
//! Obtain detailed error message text for the immediately preceeding 
//! directory or file error (see also XrdOss::getErrMsg()).
//!
//! @param  eText  - Where the message text is to be returned.
//!
//! @return True if message text is available, false otherwise.
//!
//! @note This method should be called using the same thread that encountered
//!       the error; otherwise, missleading error text may be returned.
//! @note Upon return, the internal error message text is cleared.
//-----------------------------------------------------------------------------

bool    getErrMsg(std::string& eText) override;

//-----------------------------------------------------------------------------
//! Open a directory.
//!
//! @param  path   - Pointer to the path of the directory to be opened.
//! @param  env    - Reference to environmental information.
//!
//! @return 0 upon success or -errno or -osserr (see XrdOssError.hh).
//-----------------------------------------------------------------------------

int     Opendir(const char *path, XrdOucEnv &env) override;


                XrdOssArcDir(const char* tident, XrdOssDF* df)
                            : XrdOssWrapDF(*df), ossDF(df) {}

virtual        ~XrdOssArcDir();

private:
XrdOssDF*         ossDF;          // Underlying dir/file object
XrdOssArcZipFile* zFile =  0;
};
#endif
