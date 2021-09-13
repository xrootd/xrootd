#ifndef __XRDXROOTDPGWFOB_HH_
#define __XRDXROOTDPGWFOB_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d P g w F o b . h h                     */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <set>
#include <cstring>

#include "XProtocol/XProtocol.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdOucString;
class XrdXrootdFile;

class XrdXrootdPgwFob
{
public:


bool             addOffs(kXR_int64 foffs, int dlen)
                        {XrdSysMutexHelper mHelp(fobMutex);
                         foffs = foffs << XrdProto::kXR_pgPageBL;
                         if (dlen < XrdProto::kXR_pgPageSZ) foffs |= dlen;
                         badOffs.insert(foffs);
                         numErrs++;
                         return badOffs.size() <= XrdProto::kXR_pgMaxEos;
                        }

bool             delOffs(kXR_int64 foffs, int dlen)
                        {XrdSysMutexHelper mHelp(fobMutex);
                         foffs = foffs << XrdProto::kXR_pgPageBL;
                         if (dlen < XrdProto::kXR_pgPageSZ) foffs |= dlen;
                         numFixd++;
                         return badOffs.erase(foffs) != 0;
                        }

bool             hasOffs(kXR_int64 foffs, int dlen)
                        {XrdSysMutexHelper mHelp(fobMutex);
                         foffs = foffs << XrdProto::kXR_pgPageBL;
                         if (dlen < XrdProto::kXR_pgPageSZ) foffs |= dlen;
                         return badOffs.find(foffs) != badOffs.end();
                        }

int              numOffs(int *errs=0, int *fixs=0)
                        {XrdSysMutexHelper mHelp(fobMutex);
                         if (errs) *errs = numErrs;
                         if (fixs) *fixs = numFixd;
                         return badOffs.size();
                        }

                 XrdXrootdPgwFob(XrdXrootdFile *fP)
                                : fileP(fP), numErrs(0), numFixd(0) {}

                ~XrdXrootdPgwFob();

private:

XrdXrootdFile      *fileP;
XrdSysMutex         fobMutex;
std::set<kXR_int64> badOffs;           // Uncorrected offsets
int                 numErrs;
int                 numFixd;
};
#endif
