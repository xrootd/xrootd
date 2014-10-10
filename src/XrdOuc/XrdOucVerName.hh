#ifndef __OOUC_VERNAME_HH__
#define __OOUC_VERNAME_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c V e r N a m e . h h                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! XrdOucVerName
//!
//! This class performs name versioning for shared library plug-ins.
//-----------------------------------------------------------------------------

class XrdOucVerName
{
public:

//-----------------------------------------------------------------------------
//! Version a plug-in library path.
//!
//! @param  piVers  Pointer to the version string to be used.
//! @param  piPath  Pointer to the original path to the plug-in.
//! @param  noFBK   Upon return is set to true if the versioned name has no
//!                 fallback name and must be loaded with the resulting path.
//! @param  buff    Pointer to abuffer that will hold the resulting path.
//! @param  blen    The size of the buffer.
//!
//! @return success The length of the reulting path in buff withe eqName set.
//! @return failure Zero (buffer is too small) but eqName is still set.
//-----------------------------------------------------------------------------

static int Version(const char *piVers, const char *piPath, bool &noFBK,
                         char *buff,         int   blen);
};
#endif
