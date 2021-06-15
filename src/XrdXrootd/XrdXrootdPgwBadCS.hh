#ifndef __XRDXROOTDPGWBadCS_HH_
#define __XRDXROOTDPGWBadCS_HH_
/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d P g w B a d C S . h h                   */
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

#include "XProtocol/XProtocol.hh"

class XrdXrootdFile;

class XrdXrootdPgwBadCS
{
public:

const char   *boAdd(XrdXrootdFile *fP, kXR_int64 foffs,
                    int dlen=XrdProto::kXR_pgPageSZ);

char         *boInfo(int &boLen);

void          boReset() {boCount = 0;}

              XrdXrootdPgwBadCS(int pid=0) : boCount(0), pathID(pid) {}
             ~XrdXrootdPgwBadCS() {}

private:

ServerResponseBody_pgWrCSE cse;   // cse.dlFirst, cse.dlLast
kXR_int64                  badOffs[XrdProto::kXR_pgMaxEpr];

static
const char      *TraceID;
int              boCount;          // Elements in badOffs
int              pathID;
};
#endif
