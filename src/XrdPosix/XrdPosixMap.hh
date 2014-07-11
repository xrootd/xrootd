#ifndef __XRDPOSIXMAP_H__
#define __XRDPOSIXMAP_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x M a p . h h                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/

#include <stdint.h>

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

class XrdPosixMap
{
public:

static mode_t              Flags2Mode(dev_t *rdv, uint32_t flags);

static XrdCl::Access::Mode Mode2Access(mode_t mode);

static int                 Result(const XrdCl::XRootDStatus &Status);

static void                SetDebug(bool dbg) {Debug = dbg;}

       XrdPosixMap() {}
      ~XrdPosixMap() {}

private:
static int  mapCode(int rc);
static int  mapError(int rc);
static bool Debug;
};
#endif
