#ifndef __XRDOUCCRC_HH__
#define __XRDOUCCRC_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c C R C . h h                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stddef.h>
#include <stdint.h>

class XrdOucCRC
{
public:

static unsigned int CRC32(const unsigned char *rec, int reclen);

static void Calc32C(const void* data,  size_t count,
                      uint32_t* csvec, size_t pgsz);

static bool Ver32C( const void* data,  size_t count,
                      uint32_t* csvec, size_t pgsz, int &pgErr);

                    XrdOucCRC() {}
                   ~XrdOucCRC() {}

private:

static unsigned int crctable[256];
};
#endif
