#ifndef __XRDXROOTDAIOPGRW__
#define __XRDXROOTDAIOPGRW__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d A i o P g r w . h h                    */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/uio.h>

#include "XProtocol/XProtocol.hh"
#include "XrdXrootd/XrdXrootdAioBuff.hh"
#include "XrdXrootd/XrdXrootdPgrwAio.hh"

// The XrdXrootdAioPgr object represents a single aio read or write operation.
// One or more of these are allocated by the XrdXrootdPgrwAio to effect
// asynchronous I/O. This object is specific to pgRead requests.

class XrdXrootdFile;
class XrdXrootdAioTask;
class XrdXrootdProtocol;
  
class XrdXrootdAioPgrw : public XrdXrootdAioBuff
{
public:

static
XrdXrootdAioPgrw   *Alloc(XrdXrootdAioTask *arp);

struct   iovec     *iov4Data(int &iovNum) {iovNum = csNum<<1; return &ioVec[1];}

struct   iovec     *iov4Recv(int &iovNum);

struct   iovec     *iov4Send(int &iovNum, int &iovLen, bool cs2net=false);

         bool       noChkSums(bool reset=true)
                             {bool retval = cksVec == 0;
                              if (retval && reset) cksVec = csVec;
                              return retval;
                             }

         void       Recycle() override;

         int        Setup2Recv(off_t offs, int dlen, const char *&eMsg);

         int        Setup2Send(off_t offs, int dlen, const char *&eMsg);

                    XrdXrootdAioPgrw(XrdXrootdAioTask* tP, XrdBuffer *bP);
                   ~XrdXrootdAioPgrw();

static const int    aioSZ = XrdXrootdPgrwAio::aioSZ;
static const int    acsSZ = aioSZ/XrdProto::kXR_pgPageSZ; // 16 checksums

private:

static const char*  TraceID;

int                 csNum;
int                 iovReset;
uint32_t            csVec[acsSZ];
struct iovec        ioVec[acsSZ*2+1];
};
#endif
