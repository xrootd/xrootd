#ifndef __XRDOUCEXPORT_HH_
#define __XRDOUCEXPORT_HH_
/******************************************************************************/
/*                                                                            */
/*                       X r d O u c E x p o r t . h h                        */
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

#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdOuc/XrdOucStream.hh"
  
/******************************************************************************/
/*                           F l a g   V a l u e s                            */
/******************************************************************************/
  
//                          0 1 2 3 4 5 6 7
#define XRDEXP_READONLY   0x0000000000000001LL
#define XRDEXP_FORCERO    0x0000000000000002LL
#define XRDEXP_ROW_X      0x0000000300000000LL
#define XRDEXP_NOTRW      0x0000000000000003LL
#define XRDEXP_NODREAD    0x0000000000000004LL
#define XRDEXP_DREAD_X    0x0000000400000000LL
#define XRDEXP_RCREATE    0x0000000000000008LL
#define XRDEXP_RCREATE_X  0x0000000800000000LL
#define XRDEXP_NOCHECK    0x0000000000000010LL
#define XRDEXP_CHECK_X    0x0000001000000000LL
#define XRDEXP_STAGE      0x0000000000000020LL
#define XRDEXP_STAGE_X    0x0000002000000000LL
#define XRDEXP_MIG        0x0000000000000400LL
#define XRDEXP_MIG_X      0x0000040000000000LL
#define XRDEXP_MMAP       0x0000000000000800LL
#define XRDEXP_MMAP_X     0x0000080000000000LL
#define XRDEXP_MLOK       0x0000000000001000LL
#define XRDEXP_MLOK_X     0x0000100000000000LL
#define XRDEXP_MKEEP      0x0000000000002000LL
#define XRDEXP_MKEEP_X    0x0000200000000000LL
#define XRDEXP_PURGE      0x0000000000004000LL
#define XRDEXP_PURGE_X    0x0000400000000000LL
#define XRDEXP_NOXATTR    0x0000000000008000LL
#define XRDEXP_NOXATTR_X  0x0000800000000000LL
#define XRDEXP_INPLACE    0x0000000000010000LL
#define XRDEXP_INPLACE_X  0x0001000000000000LL
#define XRDEXP_MWMODE     0x0000000000020000LL
#define XRDEXP_MWMODE_X   0x0002000000000000LL
#define XRDEXP_PFCACHE    0x0000000000040000LL
#define XRDEXP_PFCACHE_X  0x0004000000000000LL
#define XRDEXP_LOCAL      0x0000000000080000LL
#define XRDEXP_LOCAL_X    0x0008000000000000LL
#define XRDEXP_GLBLRO     0x0000000000100000LL
#define XRDEXP_GLBLRO_X   0x0018000000000000LL
#define XRDEXP_STAGEMM    0x0000000000200020LL
//                        0x0020000000000000LL
//                        0x0040000000400000LL
//                        0x0080000000800000LL
#define XRDEXP_AVAILABLE  0xff000000ff000000LL
#define XRDEXP_MASKSHIFT  32
#define XRDEXP_SETTINGS   0x00000000ffffffffLL

#define XRDEXP_MEMAP      0x0000000000003800LL
#define XRDEXP_REMOTE     0x0000000000000420LL
#define XRDEXP_MIGPRG     0x0000000000004400LL

#define XRDEXP_ROOTDIR    0x0000000001000000LL
#define XRDEXP_NOFICL     0x0000000002000000LL
#define XRDEXP_NOFICL_X   0x0200000000000000LL


// The following options are prescreened elsewhere
//
#define XRDEXP_NOLK       0LL
#define XRDEXP_NOLK_X     0LL

/******************************************************************************/
/*                       C l a s s   X r d E x p o r t                        */
/******************************************************************************/

class XrdOucPListAnchor;
  
class XrdOucExport
{
public:

static unsigned long long ParseDefs(XrdOucStream &Config, XrdSysError &Eroute,
                                    unsigned long long Flags);

static XrdOucPList       *ParsePath(XrdOucStream &Config, XrdSysError &Eroute,
                                    XrdOucPListAnchor &Export,
                                    unsigned long long Defopts);

                   XrdOucExport() {}
                  ~XrdOucExport() {}

private:
};
#endif
