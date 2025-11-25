#ifndef __OUC_CLONESEG_H__
#define __OUC_CLONESEG_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d O u c C l o n e S e g . h h                      */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! XrdOucCloneSeg
//!
//! The struct defined here is a generic data structure that is used whenever
//! we need to pass a vector of file segments that need to be cloned into
//! anothe file. Generally, this vector is transformed as it passes through.
//-----------------------------------------------------------------------------

class XrdSfsFile;
class XrdOfsFile;
class XrdOssDF;

struct XrdOucCloneSeg
{
union {XrdSfsFile* sfsFile; // Pointer to source file at Sfs level
       XrdOfsFile* ofsFile; // Pointer to source file at Ofs level
       XrdOssDF*   ossFile; // Pointer to source file at Oss level
       int64_t     unxFD;   // The source file descriptor
      } src;
      uint64_t     srcOffs; // The source offset
      uint64_t     srcLen;  // The source length
      uint64_t     dstOffs;  // The dest   offset
};
#endif
