#ifndef __OOUC_PINPATH_HH__
#define __OOUC_PINPATH_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c P i n P a t h . h h                       */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
//! XrdOucPinPath
//!
//! This function performs name versioning for shared library plug-ins. It is
//! a public header and may be used by third parties to adhere to the plugin
//! naming conventions.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//! Obtain the primary path to be used to load a plugin.
//!
//! @param  piPath  Pointer to the original (i.e. specified) plugin path.
//! @param  noAltP  Upon return it is set to true or false.
//!                 TRUE:  only the returned primary path may be used to load
//!                        the plugin (i.e. no alternate path allowed).
//!                 FALSE: the plugin should first be loaded using the returned
//!                        primary path and if that fails, the original path
//!                        (i.e. piPath) should be used to load the plugin.
//! @param  buff    Pointer to a buffer that will hold the primary path.
//! @param  blen    The size of the buffer.
//!
//! @return success The length of the primary path in buff.
//! @return failure Zero (buffer is too small) but eqName is still set.
//-----------------------------------------------------------------------------

extern int XrdOucPinPath(const char *piPath, bool &noAltP, char *buff, int blen);

#endif
