#ifndef __OUC_SFVEC_H__
#define __OUC_SFVEC_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c S F V e c . h h                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <unistd.h>

//-----------------------------------------------------------------------------
//! XrdOucSFVec
//!
//! The struct defined here is a generic data structure that is used whenever
//! we need to pass a vector of file offsets, lengths, and the corresponding
//! target buffer pointers to effect a sendfile() call. It is used by the
//! xrd, sfs, ofs., and oss components.
//-----------------------------------------------------------------------------

struct XrdOucSFVec {union {char *buffer;    //!< ->Data if fdnum < 0
                           off_t offset;    //!< File offset of data otherwise
                          };
                    int   sendsz;           //!< Length of data at offset
                    int   fdnum;            //!< File descriptor for data

                    enum {sfMax = 16};      //!< Maximum number of elements
                   };
#endif
