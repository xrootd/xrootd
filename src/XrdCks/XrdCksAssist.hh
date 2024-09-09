#ifndef __XRDCKSASSIST_HH__
#define __XRDCKSASSIST_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d C k s A s s i s t . h h                        */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>
#include <vector>

#include <time.h>

//------------------------------------------------------------------------------
//! This header file defines linkages to various XRootD checksum assistants.
//! The functions described here are located in libXrdUtils.so.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Generate the extended attribute data for a particular checksum  that can
//! be used to set the corresponding checksum attribute variable.
//!
//! @param  cstype    A null terminated string holding the checksum type
//!                   (e.g. "adler32", "md5", "sha2", etc).
//! @param  csval     A null terminated string holding the corresonding
//!                   checksum value. This must be an ASCII hex representation
//!                   of the value and must be of appropriate length.
//! @param  mtime     The subject file's modification time.
//!
//! @return A vector of bytes that should be usedto set the attribute variable.
//!         If the size of the vector is zero, then the supplied parameters
//!         were incorrect and valid data cannot be generated; errno is:
//!         EINVAL       - csval length is incorrect for checksum type or
//!                        contains a non-hex digit.
//!         ENAMETOOLONG - checksum type is too long.
//!         EOVERFLOW    - csval could not be represented in the data.
//------------------------------------------------------------------------------

extern std::vector<char> XrdCksAttrData(const char *cstype,
                                        const char *csval, time_t mtime);

//------------------------------------------------------------------------------
//! Generate the extended attribute variable name for a particular checksum.
//!
//! @param  cstype    A null terminated string holding the checksum type
//!                   (e.g. "adler32", "md5", "sha2", etc).
//! @param  nspfx     Is the namespace prefix to add to the variable name.
//!                   By default no prefix os used. Certain platforms and/or
//!                   filesystems require that user attributes start with a
//!                   particular prefix (e.g. Linux requires 'user.') others
//!                   do not. If your are going to use the variable name to get
//!                   or set an attribute you should specify any required
//!                   prefix. If specified and it does not end with a dot, a
//!                   dot is automatically added to the nspfx.
//!
//! @return A string holding the variable name that should be used to get or
//!         set the extended attribute holding the correspnding checksum. If
//!         a null string is returned, the variable could not be generated;
//!         errno is set to:
//!         ENAMETOOLONG - checksum type is too long.
//------------------------------------------------------------------------------

extern std::string XrdCksAttrName(const char *cstype, const char *nspfx="");

//------------------------------------------------------------------------------
//! Extract th checksum value from checksum extended attribute data.
//!
//! @param  cstype    A null terminated string holding the checksum type
//!                   (e.g. "adler32", "md5", "sha2", etc).
//! @param  csbuff    A pointer to a buffer hlding the checksum data.
//! @param  csblen    The length of the checksum data (i.e. the length of the
//!                   retrieved extended attribute).
//!
//! @return A string holding the ASCII hexstring correspoding to the checksum
//!         value. If a null string is returned then the checksum data was
//!         invalid or did not correspond to the specified checksum type, the
//!         errno is set to:
//!         EINVAL       - the checksum length in csbuff is incorrect.
//!         EMSGSIZE     - csblen was not the expected value.
//!         ENOENT       - the specified cstype did not match the one in csbuff.
//!         EOVERFLOW    - checksum value could not be generated from csbuff.
//------------------------------------------------------------------------------

extern std::string XrdCksAttrValue(const char *cstype,
                                   const char *csbuff, int csblen);
#endif
