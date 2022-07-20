/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d X e q P g r w . c c                    */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cctype>
#include <cstdio>
#include <sys/uio.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XProtocol/XProtocol.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdXrootd/XrdXrootdAioFob.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdPgrwAio.hh"
#include "XrdXrootd/XrdXrootdPgwCtl.hh"
#include "XrdXrootd/XrdXrootdPgwFob.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXeq.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

namespace
{
static const int pgPageSize = XrdProto::kXR_pgPageSZ;
static const int pgPageMask = XrdProto::kXR_pgPageSZ-1;
static const int pgUnitSize = XrdProto::kXR_pgUnitSZ;
}

namespace
{
static const int pgAioMin = XrdXrootdPgrwAio::aioSZ
                          + XrdXrootdPgrwAio::aioSZ*8/10; // 1.8 of aiosz
static const int pgAioHalf= XrdXrootdPgrwAio::aioSZ/2;
}
  
/******************************************************************************/
/*                            d o _ P g C l o s e                             */
/******************************************************************************/
  
bool XrdXrootdProtocol::do_PgClose(XrdXrootdFile *fP, int &rc)
{
    XrdXrootdPgwFob *fobP = fP->pgwFob;
    int numErrs, numFixes, numLeft;

// Make sure we have a fob
//
   if (!fobP) return true;

// Obtain the checksum status of this file and update statistics
//
   numLeft = fobP->numOffs(&numErrs, &numFixes);
   fP->Stats.pgUpdt(numErrs, numFixes, numLeft);

// If there are uncorrected checksum, indicate failure. These will be logged
// when the fob is deleted later on.
//
   if (numLeft)
      {char ebuff[128];
       snprintf(ebuff,sizeof(ebuff),"%d uncorrected checksum errors",numLeft);
       rc = Response.Send(kXR_ChkSumErr, ebuff);
       return false;
      }

// All is well
//
   return true;
}
  
/******************************************************************************/
/*                             d o _ P g R e a d                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_PgRead()
{
   int pathID;
   XrdXrootdFHandle fh(Request.pgread.fhandle);
   numReads++;

// Unmarshall the data
//
   IO.IOLen = ntohl(Request.pgread.rlen);
              n2hll(Request.pgread.offset, IO.Offset);

// Perform a sanity check on the length
//
   if (IO.IOLen <= 0)
      return Response.Send(kXR_ArgInvalid, "Read length is invalid");

// Find the file object
//
   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "pgread does not refer to an open file");

// Now handle the optional pathid and reqflags arguments.
//
   IO.Flags = 0;
   if (!Request.header.dlen) pathID = 0;
      else {ClientPgReadReqArgs *rargs=(ClientPgReadReqArgs *)(argp->buff);
            pathID = static_cast<int>(rargs->pathid);
            if (Request.header.dlen > 1)
               IO.Flags = static_cast<unsigned short>(rargs->reqflags);
           }

// Trace this
//
   TRACEP(FSIO,pathID<<" pgread "<<IO.IOLen<<'@'<<IO.Offset
                     <<" fn=" <<IO.File->FileKey);

// If we are monitoring, insert a read entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_rd(IO.File->Stats.FileID, Request.pgread.rlen,
                                                   Request.pgread.offset);

// Do statistics. They will not always be accurate because we may not be
// able to fully complete the I/O from the file. Note that we also count
// the checksums which a questionable practice.
//
   IO.File->Stats.pgrOps(IO.IOLen, (IO.Flags & XrdProto::kXR_pgRetry) != 0);

// Use synchronous reads unless async I/O is allowed, the read size is
// sufficient, and there are not too many async operations in flight.
//
   if (IO.File->AsyncMode && IO.IOLen >= pgAioMin
   &&  IO.Offset+IO.IOLen <= IO.File->Stats.fSize+pgAioHalf
   &&  linkAioReq < as_maxperlnk && srvrAioOps < as_maxpersrv
   &&  !(IO.Flags & XrdProto::kXR_pgRetry))
        {XrdXrootdProtocol *pP;
         XrdXrootdPgrwAio  *aioP;
         int rc;

         if (!pathID) pP = this;
            else {if (!(pP = VerifyStream(rc, pathID, false))) return rc;
                  if (pP->linkAioReq >= as_maxperlnk) pP = 0;
                 }

         if (pP && (aioP = XrdXrootdPgrwAio::Alloc(pP, pP->Response, IO.File)))
            {if (!IO.File->aioFob) IO.File->aioFob = new XrdXrootdAioFob;
             aioP->Read(IO.Offset, IO.IOLen);
             return 0;
            }
         SI->AsyncRej++;
        }

// See if an alternate path is required, offload the read
//
   if (pathID) return do_Offload(&XrdXrootdProtocol::do_PgRIO, pathID);

// Now do the read on the main path
//
   return do_PgRIO();
}

/******************************************************************************/
/*                              d o _ P g R I O                               */
/******************************************************************************/

// IO.File   = file to be read
// IO.Offset = Offset at which to read
// IO.IOLen  = Number of bytes to read from file and write to socket

int XrdXrootdProtocol::do_PgRIO()
{
// We restrict the maximum transfer size to generate no more than 1023 iovec
// elements where the first is used for the header.
//
   static const int maxCSSZ = 1022;
// static const int maxCSSZ = 32;
   static const int maxPGRD = maxCSSZ*pgPageSize; // 2,093,056 usually
   static const int maxIOVZ = maxCSSZ*2+1;
   static const int infoLen = sizeof(kXR_int64);

   struct pgReadResponse
         {ServerResponseStatus rsp;
          kXR_int64            ofs;
         } pgrResp;

   char *buff;
   XrdSfsFile *sfsP = IO.File->XrdSfsp;
   uint64_t pgrOpts = 0;
   int dlen, fLen, lLen, rc, xframt, Quantum;
   uint32_t csVec[maxCSSZ];
   struct iovec iov[maxIOVZ];

// Set flags, as needed
//
   if (IO.Flags & XrdProto::kXR_pgRetry) pgrOpts |= XrdSfsFile::Verify;

// Preinitialize the header
//
   pgrResp.rsp.bdy.requestid = kXR_pgread - kXR_1stRequest;
   pgrResp.rsp.bdy.resptype  = XrdProto::kXR_PartialResult;;
   memset(pgrResp.rsp.bdy.reserved, 0, sizeof(pgrResp.rsp.bdy.reserved));

// Calculate the total pages in the read request. Note that the first and
// last pages may require short reads if they are not fully aligned.
//
   int pgOff, rPages, rLen = IO.IOLen;
   rPages = XrdOucPgrwUtils::csNum(IO.Offset, IO.IOLen) * pgPageSize;

// Compute the quantum.
//
   Quantum = (maxPGRD > maxBuffsz ? maxBuffsz : maxPGRD);
   if (rPages < Quantum) Quantum = rPages;

// Make sure we have a large enough buffer. We may need to adjust it downward
// due to reallocation rounding.
//
   if (!argp || Quantum < halfBSize || Quantum > argp->bsize)
      {if ((rc = getBuff(1, Quantum)) <= 0) return rc;}
      else if (hcNow < hcNext) hcNow++;
   if (argp->bsize > maxPGRD) Quantum = maxPGRD;

// Compute the number of iovec elements we need plus one for the header. The
// Quantum is gauranteed to be a multiple of pagesize now. Verify that this
// calculation was indeed correct to avoid overwriting the stack.
//
   int items = 1 + (Quantum / pgPageSize);
   if (items > maxCSSZ+1)
      return Response.Send(kXR_Impossible, "pgread logic error 1");

// Preinitialize the io vector for checksums and data (leave 1st element free).
//
   uint32_t *csVP = csVec;
   buff = argp->buff;
   int i = 1, n = items * 2;
   while(i <= n)
       {iov[i  ].iov_base = csVP++;
        iov[i++].iov_len  = sizeof(uint32_t);
        iov[i  ].iov_base = buff;
        iov[i++].iov_len  = pgPageSize;
        buff += pgPageSize;
       }

// If this is an unaligned read, offset the unaligned segment in the buffer
// so that remaining pages are page-aligned. It will be reset when needed.
// We also calculate the actual length of the first read.
//
   if ((pgOff = IO.Offset & pgPageMask))
      {rLen = pgPageSize - pgOff;
       buff = argp->buff + pgOff;
       iov[2].iov_base = buff;
       iov[2].iov_len  = rLen;
       rLen += Quantum - pgPageSize;
      } else {
       rLen = Quantum;
       buff = argp->buff;
      }
   if (IO.IOLen < rLen) rLen = IO.IOLen;

// Now read all of the data. For each read we must recacalculate the number
// of iovec elements that we will use to send the data as fewer bytes may have
// been read. In fact, no bytes may have been read.
//
   long long ioOffset = IO.Offset;
   do {if ((xframt = sfsP->pgRead(IO.Offset, buff, rLen, csVec, pgrOpts)) <= 0)
          break;

       items = XrdOucPgrwUtils::csNum(IO.Offset, xframt, fLen, lLen);
       iov[2].iov_len = fLen;
       if (items > 1) iov[items<<1].iov_len = lLen;

       if (xframt < rLen || xframt == IO.IOLen)
          {pgrResp.rsp.bdy.resptype = XrdProto::kXR_FinalResult;
           IO.IOLen = 0;
          } else {
           IO.IOLen -= xframt; IO.Offset += xframt;
           rLen = (IO.IOLen < Quantum ? IO.IOLen : Quantum);
          }

       for (int i = 0; i < items; i++) csVec[i] = htonl(csVec[i]);

       pgrResp.ofs = htonll(ioOffset);
//     char trBuff[512];
//     snprintf(trBuff, sizeof(trBuff), "Xeq PGR: %d@%lld (%lld)\n",
//              xframt, ioOffset, ioOffset>>12);
//     std::cerr<<trBuff<<std::flush;
       dlen  = xframt + (items * sizeof(uint32_t));
       if ((rc = Response.Send(pgrResp.rsp, infoLen, iov, items*2+1, dlen)) < 0)
          return rc;

       if (pgOff)
          {iov[2].iov_base = argp->buff;
           iov[2].iov_len  = pgPageSize;
           buff = argp->buff;
           pgOff = 0;
          }

       ioOffset = IO.Offset;
      } while(IO.IOLen > 0);

// Determine why we ended here
//
   if (xframt < 0) return fsError(xframt, 0, sfsP->error, 0, 0);

// Return no bytes if we were tricked into sending a partial result
//
   if (pgrResp.rsp.bdy.resptype != XrdProto::kXR_FinalResult)
      {pgrResp.rsp.bdy.resptype = XrdProto::kXR_FinalResult;
       pgrResp.rsp.bdy.dlen     = 0;
       pgrResp.ofs              = htonll(IO.Offset);
       return Response.Send(pgrResp.rsp, infoLen);
      }
   return 0;
}

/******************************************************************************/
/*                            d o _ P g W r i t e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_PgWrite()
{
   XrdXrootdFHandle fh(Request.pgwrite.fhandle);
   int pathID;
   numWrites++;

// Unmarshall the data
//
   IO.IOLen = Request.pgwrite.dlen;
             n2hll(Request.pgwrite.offset, IO.Offset);
   pathID   = Request.pgwrite.pathid;
   IO.Flags  = static_cast<unsigned short>(Request.pgwrite.reqflags);

// Perform a sanity check on the length.
//
   if (IO.IOLen <= (int)sizeof(kXR_unt32))
      {Response.Send(kXR_ArgInvalid, "pgwrite length is invalid");
       return Link->setEtext("pgwrite protocol violation");
      }

// Validate pathid, at least as much as we need to. If it's wrong then we
// don't know where the data is and we just let it go.
//
   if (pathID && (pathID >= maxStreams || !Stream[pathID]))
      return Response.Send(kXR_ArgInvalid, "invalid path ID");

// Find the file object
//
   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      {IO.File = 0;
       return do_WriteNone(pathID);
      }

// If the file object does not have a pgWrite object, allocate one.
//
   if (IO.File->pgwFob == 0) IO.File->pgwFob = new XrdXrootdPgwFob(IO.File);

// Trace this
//
   TRACEP(FSIO, pathID<<" pgwrite "
          <<(IO.Flags & XrdProto::kXR_pgRetry ? "retry " : "")
          <<IO.IOLen<<'@'<<IO.Offset<<" fn=" <<IO.File->FileKey);

// Do statistics. They will not always be accurate because we may not be
// able to fully complete the I/O to the file. Note that we also count
// the checksums which a questionable practice.
//
   IO.File->Stats.pgwOps(IO.IOLen, (IO.Flags & XrdProto::kXR_pgRetry) != 0);

// If we are monitoring, insert a write entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_wr(IO.File->Stats.FileID, Request.pgwrite.dlen,
                                                   Request.pgwrite.offset);

// See if an alternate path is required, offload the write
//
   if (pathID) return do_Offload(&XrdXrootdProtocol::do_PgWIO, pathID);

// Now do the write on the main path
//
   return do_PgWIO(true);
}

/******************************************************************************/
/*                             d o _ P g W A I O                              */
/******************************************************************************/
  
// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket
// IO.Flags  = Flags associated with request

bool XrdXrootdProtocol::do_PgWAIO(int &rc)
{
   XrdXrootdPgrwAio  *aioP;

// Make sure the client is fast enough to do this
//
   if (myStalls >= as_maxstalls)
      {SI->AsyncRej++;
       myStalls--;
       return false;
      }

// Allocate an aio request object
//
   if (!(aioP = XrdXrootdPgrwAio::Alloc(this, Response, IO.File, pgwCtl)))
      {SI->AsyncRej++;
       return false;
      }

// Issue the write request
//
   rc = aioP->Write(IO.Offset, IO.IOLen);
   return true;
}
  
/******************************************************************************/
/*                              d o _ P g W I O                               */
/******************************************************************************/
  
// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket
// IO.Flags  = Flags associated with request

int XrdXrootdProtocol::do_PgWIO() {return do_PgWIO(true);}

int XrdXrootdProtocol::do_PgWIO(bool isFresh)
{
   struct iovec    *ioV;
   XrdSfsFile      *sfsP = IO.File->XrdSfsp;
   const char      *eMsg;
   char            *buff;
   kXR_unt32       *csVec;
   int n, rc, Quantum, iovLen, iovNum, csNum;
   bool isRetry = (IO.Flags & XrdProto::kXR_pgRetry) != 0;

// Verify that we still have a control area and allocate a control object
// if we do not have one already. The object stays around until disconnect.
//
   if (!IO.File->pgwFob)
      return do_WriteNone(PathID, kXR_Impossible, "pgwrite logic error 1");
   if (!pgwCtl) pgwCtl = new XrdXrootdPgwCtl(PathID);

// If this is the first entry then check if the request is eligible for async
// I/O or if this is a retry request which, of course, is not eligible.
//
   if (isFresh)
      {if (IO.File->AsyncMode && IO.IOLen >= pgAioMin
       &&  linkAioReq < as_maxperlnk && srvrAioOps < as_maxpersrv
       &&  !isRetry && do_PgWAIO(rc)) return rc;
       if (isRetry && !do_PgWIORetry(rc)) return rc;
       if (!do_PgWIOSetup(pgwCtl)) return -1;
      }

// Either complete the current frame or start a new one. When we start a new
// one, the I/O will not return unless all of the data was successfully read.
// Hence, we update the length outstanding.
//
do{if (isFresh)
      {if (!(ioV = pgwCtl->FrameInfo(iovNum, iovLen))) break;
       IO.IOLen -= iovLen;
       if ((rc = getData(this, "pgwrite", ioV, iovNum))) return rc;
      }

// We now have all the data, get checksum and data information
//
   if (!(csVec = pgwCtl->FrameInfo(csNum, buff, Quantum, argp)))
      return do_WriteNone(PathID, kXR_Impossible, "pgwrite logic error 2");

// Convert checksums to host byte order
//
  for (int i = 0; i < csNum; i++) csVec[i] = ntohl(csVec[i]);

// Verify the checksums
//
  XrdOucPgrwUtils::dataInfo dInfo(buff, csVec, IO.Offset, Quantum);
  off_t bado;
  int   badc;
  bool aOK = true;

  while(dInfo.count > 0 && !XrdOucPgrwUtils::csVer(dInfo, bado, badc))
       {if ((eMsg = pgwCtl->boAdd(IO.File, bado, badc)))
           return do_WriteNone(PathID, kXR_TooManyErrs, eMsg);
        aOK = false;
       }

// Write the data out. The callee is responsible for unaligned writes!
//
   if ((rc = sfsP->pgWrite(IO.Offset, buff, Quantum, csVec)) <= 0)
      {IO.EInfo[0] = rc; IO.EInfo[1] = 0;
       return do_WriteNone();
      }

// If this was a successful retry write, remove corrrected offset
//
   if (aOK && IO.Flags & XrdProto::kXR_pgRetry)
      IO.File->pgwFob->delOffs(IO.Offset, Quantum);

// Update offset and advance to next frame
//
   IO.Offset += Quantum;
   isFresh = true;

  } while(pgwCtl->Advance());


// Return final result
//
   buff = pgwCtl->boInfo(n);
   return Response.Send(pgwCtl->resp, sizeof(pgwCtl->info), buff, n);
}
  
/******************************************************************************/
/*                         d o _ P g W I O R e t r y                          */
/******************************************************************************/

// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket
// IO.Flags  = Flags associated with request

bool XrdXrootdProtocol::do_PgWIORetry(int &rc)
{
   static const int csLen = sizeof(kXR_unt32);
   bool isBad;

// Make sure the write does not cross a page bounday. For unaligned writes we
// can compute the exact length that we need. Otherwise, it can't be bigger
// than a unit's worth of data. Not precise but usually good enough.
//
   if (IO.Offset & pgPageMask)
      {int n = pgPageSize - (IO.Offset & pgPageMask);
       isBad = IO.IOLen > (n + csLen);
      } else isBad = IO.IOLen > pgUnitSize;

// Deep six the write if it violates retry rules.
//
   if (isBad)
      {rc = do_WriteNone(PathID, kXR_ArgInvalid,
                         "pgwrite retry of more than one page not allowed");
       return false;
      }

// Make sure that the offset is registered, if it is not, treat this as a
// regular write as this may have been a resend during write recovery.
//
   if (!IO.File->pgwFob->hasOffs(IO.Offset, IO.IOLen - csLen))
      {char buff[64];
       snprintf(buff, sizeof(buff), "retry %d@%lld", IO.IOLen-csLen, IO.Offset);
       eDest.Emsg("pgwRetry", buff, "not in error; fn=", IO.File->FileKey);
       IO.Flags &= ~XrdProto::kXR_pgRetry;
      }

// We can proceed with this write now.
//
   return true;
}

/******************************************************************************/
/*                         d o _ P g w I O S e t u p                          */
/******************************************************************************/

// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket
// IO.Flags  = Flags associated with request

bool XrdXrootdProtocol::do_PgWIOSetup(XrdXrootdPgwCtl *pgwCtl)
{
   const char *eMsg;
   int Quantum;

// Compute the minimum (4K) or maximum buffer size we will use.
//
   if (IO.IOLen < XrdXrootdPgwCtl::maxBSize/2)
      Quantum = (IO.IOLen < pgPageSize ? pgPageSize : IO.IOLen);
      else Quantum = XrdXrootdPgwCtl::maxBSize;

// Make sure we have a large enough buffer
//
   if (!argp || Quantum < halfBSize || argp->bsize < Quantum
   ||   argp->bsize > XrdXrootdPgwCtl::maxBSize)
      {if (getBuff(0, Quantum) <= 0) return -1;}
      else if (hcNow < hcNext) hcNow++;

// Do the setup. If it fails then either the client sent an incorrect stream
// of the header was corrupted. In either case, it doesn't matter as we can't
// depend on the information to clear the stream. So, we close the connection.
//
   if ((eMsg = pgwCtl->Setup(argp, IO.Offset, IO.IOLen)))
      {Response.Send(kXR_ArgInvalid, eMsg);
       Link->setEtext("pgwrite protocol violation");
       return false;
      }
   return true;
}
