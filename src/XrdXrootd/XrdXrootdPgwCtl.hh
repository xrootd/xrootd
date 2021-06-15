#ifndef __XRDXROOTDPGWCTL_HH_
#define __XRDXROOTDPGWCTL_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d P g w C t l . h h                     */
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

#include <sys/uio.h>

#include "XProtocol/XProtocol.hh"
#include "Xrd/XrdBuffer.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdXrootd/XrdXrootdPgwBadCS.hh"

class XrdXrootdFile;

class XrdXrootdPgwCtl : public XrdXrootdPgwBadCS
{
public:

static const int crcSZ    = sizeof(kXR_unt32);
static const int maxBSize = 1048576;   // 1MB maximum buffer size
static const int maxIOVN  = maxBSize/XrdProto::kXR_pgPageSZ*2;

ServerResponseStatus       resp;
ServerResponseBody_pgWrite info;  // info.offset

bool          Advance();

struct iovec *FrameInfo(int &iovn, int &rdlen)
                       {rdlen = iovLen;
                        return ((iovn = iovNum) ? ioVec : 0);
                       }

kXR_unt32    *FrameInfo(int &csNum, char *&buff, int &datalen, XrdBuffer *bP)
                      {if (bP->buff != dataBuff || bP->bsize != dataBLen
                       ||  !iovNum) return 0;
                       csNum   = iovNum>>1;
                       buff    = (char *)ioVec[1].iov_base;
                       datalen = iovLen - (crcSZ * csNum);
                       return csVec;
                      }

char         *FrameLeft(int k, int &dlen)
                       {k *= 2;
                        if (k >= iovNum) {dlen = 0; return 0;}
                        char *buff = (char *)ioVec[k+1].iov_base;
                        if (!k) dlen = iovLen - (iovNum/2*crcSZ);
                           else {int n = (iovNum - k)/2;
                                 dlen = ((n-1)*XrdProto::kXR_pgPageSZ)
                                      + ioVec[k+1].iov_len;
                                }
                        return buff;
                       }

const char   *Setup(XrdBuffer *buffP, kXR_int64 fOffs, int totlen);


     XrdXrootdPgwCtl(int pid);
    ~XrdXrootdPgwCtl() {}

private:

static
const char      *TraceID;
char            *dataBuff;         // Pointer to data buffer
int              dataBLen;         // Actual length of buffer
int              iovNum;           // Number of elements in use
int              lenLeft;          // Number of bytes left to read

int              iovRem;           // Number of elements remaining to do
int              iovLen;           // Length of data read by the ioVec
int              endLen;           // Length of last segment if it is short
int              fixSRD;           // ioVec[fixSRD] has short read
kXR_unt32        csVec[maxIOVN/2]; // Checksums received
struct iovec     ioVec[maxIOVN];   // Read vector
};

#endif
