/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d P g w B a d C S . c c                   */
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

#include <cstring>
#include <arpa/inet.h>

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdPgwBadCS.hh"
#include "XrdXrootd/XrdXrootdPgwFob.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"

#define TRACELINK fP

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
const char *XrdXrootdPgwBadCS::TraceID = "pgwBadCS";

/******************************************************************************/
/*                                 b o A d d                                  */
/******************************************************************************/

const char *XrdXrootdPgwBadCS::boAdd(XrdXrootdFile *fP,
                                     kXR_int64 foffs, int dlen)
{

// Do some tracing
//
   TRACEI(PGCS, pathID <<" csErr "<<dlen<<'@'<<foffs<<" inreq="<<boCount+1
                <<" infile=" <<fP->pgwFob->numOffs()+1<<" fn="<<fP->FileKey);

// If this is the first offset, record the length as first and last.
// Othewrise just update the last length.
//
   if (!boCount) cse.dlFirst = cse.dlLast = htons(dlen);
      else cse.dlLast = htons(dlen);

// Add offset to the vector to be returned to client for corrections.
//
   if (boCount+1 >= XrdProto::kXR_pgMaxEpr)
      return "Too many checksum errors in request";
   badOffs[boCount++] = htonll(foffs);

// Add offset in the set of uncorrected offsets
//
   if (!fP->pgwFob->addOffs(foffs, dlen))
      return "Too many uncorrected checksum errors in file";

// Success!
//
   return 0;
}
  
/******************************************************************************/
/*                                b o I n f o                                 */
/******************************************************************************/
  
char *XrdXrootdPgwBadCS::boInfo(int &boLen)
{
   static const int crcSZ = sizeof(uint32_t);

// If no bad offsets are present, indicate so.
//
   if (!boCount)
      {boLen = 0;
       return 0;
      }

// Return the additional data
//
   boLen = sizeof(cse) + (boCount * sizeof(kXR_int64));
   cse.cseCRC = htonl(XrdOucCRC::Calc32C(((char *)&cse)+crcSZ, boLen-crcSZ));
   return (char *)&cse;
}
