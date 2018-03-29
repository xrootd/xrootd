#ifndef __OUC_IOVEC_H__
#define __OUC_IOVEC_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c I O V e c . h h                         */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
//! XrdOucIOVec
//!
//! The struct defined here is a generic data structure that is used whenever
//! we need to pass a vector of file offsets, lengths, and the corresponding
//! target buffer pointers. It is used by the sfs, ofs, and oss components.
//-----------------------------------------------------------------------------

struct XrdOucIOVec
{
       long long offset;    // Offset into the file.
       int       size;      // Size of I/O to perform.
       int       info;      // Available for arbitrary use
       char     *data;      // Location to read into.
};

// Add backward compatible constructor to XrdOucIOVec
//
struct XrdOucIOVec2 : public XrdOucIOVec
{
       XrdOucIOVec2(char *buff, long long offs, int sz, int inf=0)
                   {XrdOucIOVec::offset = offs;
                    XrdOucIOVec::size   = sz;
                    XrdOucIOVec::info   = inf;
                    XrdOucIOVec::data   = buff;
                   }
};
#endif
