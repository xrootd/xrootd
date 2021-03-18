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

#include <ctype.h>
#include <stdio.h>
#include <sys/uio.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XProtocol/XProtocol.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdPgwCtl.hh"
#include "XrdXrootd/XrdXrootdPgwFob.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXeq.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdOucTrace *XrdXrootdTrace;

namespace
{
static const int pgPageSize = XrdProto::kXR_pgPageSZ;
static const int pgPageMask = XrdProto::kXR_pgPageSZ-1;
static const int pgUnitSize = XrdProto::kXR_pgUnitSZ;
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
   myIOLen = ntohl(Request.pgread.rlen);
             n2hll(Request.pgread.offset, myOffset);

// Perform a sanity check on the length
//
   if (myIOLen <= 0)
      return Response.Send(kXR_ArgInvalid, "Read length is invalid");

// Find the file object
//
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "pgread does not refer to an open file");

// Now handle the optional pathid and reqflags arguments.
//
   myFlags = 0;
   if (!Request.header.dlen) pathID = 0;
      else {ClientPgReadReqArgs *rargs=(ClientPgReadReqArgs *)(argp->buff);
            pathID = (rargs->pathid == XrdProto::kXR_AnyPath)
                   ? getPathID(true) : static_cast<int>(rargs->pathid);
            if (Request.header.dlen > 1)
               myFlags = static_cast<unsigned short>(rargs->reqflags);
           }

// Trace this
//
   TRACEP(FS, pathID <<" fh="<<fh.handle<<" pgread "<<myIOLen<<'@'<<myOffset);

// If we are monitoring, insert a read entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_rd(myFile->Stats.FileID, Request.pgread.rlen,
                                                  Request.pgread.offset);

// Do statistics. They will not always be accurate because we may not be
// able to fully complete the I/O from the file. Note that we also count
// the checksums which a questionable practice.
//
   myFile->Stats.pgrOps(myIOLen, (myFlags & XrdProto::kXR_pgRetry) != 0);

// See if an alternate path is required, offload the read
//
   if (pathID) return do_Offload(pathID, false, true);

// Now do the read on the main path
//
   return do_PgRIO();
}

/******************************************************************************/
/*                              d o _ P g R I O                               */
/******************************************************************************/

// myFile   = file to be read
// myOffset = Offset at which to read
// myIOLen  = Number of bytes to read from file and write to socket

int XrdXrootdProtocol::do_PgRIO()
{
// We restrict the maximum transfer size to generate no more than 1023 iovec
// elements where the first is used for the header.
//
   static const int maxCSSZ = 1022;
   static const int maxPGRD = maxCSSZ*pgPageSize; // 2,093,056 usually
   static const int maxIOVZ = maxCSSZ*2+1;
   static const int infoLen = sizeof(kXR_int64);

   struct pgReadResponse
         {ServerResponseStatus rsp;
          kXR_int64            ofs;
         } pgrResp;

   char *buff;
   XrdSfsFile *sfsP = myFile->XrdSfsp;
   uint64_t pgrOpts = XrdSfsFile::NetOrder;
   int dlen, rc, xframt, Quantum;
   uint32_t csVec[maxCSSZ];
   struct iovec iov[maxIOVZ];

// Set flags, as needed
//
   if (myFlags & XrdProto::kXR_pgRetry) pgrOpts |= XrdSfsFile::Verify;

// Preinitialize the header
//
   pgrResp.rsp.bdy.requestid = kXR_pgread - kXR_1stRequest;
   pgrResp.rsp.bdy.resptype  = XrdProto::kXR_PartialResult;;
   memset(pgrResp.rsp.bdy.reserved, 0, sizeof(pgrResp.rsp.bdy.reserved));

// Calculate the total pages in the read request. Note that the first and
// last pages may require short reads if they are not fully aligned.
//
   int pFrag, pgOff, rPages, rLen = myIOLen;
   rPages = XrdOucPgrwUtils::csNum(myOffset, myIOLen) * pgPageSize;

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
   if ((pgOff = myOffset & pgPageMask))
      {rLen = pgPageSize - pgOff;
       buff = argp->buff + pgOff;
       iov[2].iov_base = buff;
       iov[2].iov_len  = rLen;
       rLen += Quantum - pgPageSize;
      } else {
       rLen = Quantum;
       buff = argp->buff;
      }
   if (myIOLen < rLen) rLen = myIOLen;

// Now read all of the data. For each read we must recacalculate the number
// of iovec elements that we will use to send the data as fewer bytes may have
// been read. In fact, no bytes may have been read.
//
   do {if ((xframt = sfsP->pgRead(myOffset, buff, rLen, csVec, pgrOpts)) <= 0)
          break;

       if (xframt <= pgPageSize)
          {if (xframt > (int)iov[2].iov_len)
              {pFrag = xframt - iov[2].iov_len;
               items = 2;
              } else {
               iov[2].iov_len = xframt;
               pFrag = 0;
               items = 1;
              }
          } else {
           n = xframt - iov[2].iov_len;
           pFrag = n & pgPageMask;
           items = 1 + n/pgPageSize + (pFrag != 0);
          }

       if (pFrag) iov[items*2].iov_len = pFrag;

       if (xframt < rLen || xframt == myIOLen)
          {pgrResp.rsp.bdy.resptype = XrdProto::kXR_FinalResult;
           myIOLen = 0;
          } else {
           myIOLen -= xframt; myOffset += xframt;
           rLen = (myIOLen < Quantum ? myIOLen : Quantum);
          }

       pgrResp.ofs = htonll(myOffset);
       dlen  = xframt + (items * sizeof(uint32_t));
       if ((rc = Response.Send(pgrResp.rsp, infoLen, iov, items*2+1, dlen)) < 0)
          return rc;

       if (pgOff)
          {iov[2].iov_base = argp->buff;
           iov[2].iov_len  = pgPageSize;
           pgOff = 0;
          }
       if (pFrag) iov[items*2+1].iov_len = pgPageSize;

      } while(myIOLen > 0);

// Determine why we ended here
//
   if (xframt < 0) return fsError(xframt, 0, sfsP->error, 0, 0);

// Return no bytes if we were tricked into sending a partial result
//
   if (pgrResp.rsp.bdy.resptype != XrdProto::kXR_FinalResult)
      {pgrResp.rsp.bdy.resptype = XrdProto::kXR_FinalResult;
       pgrResp.rsp.bdy.dlen     = 0;
       pgrResp.ofs              = htonll(myOffset);
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
   myIOLen = Request.pgwrite.dlen;
             n2hll(Request.pgwrite.offset, myOffset);
   pathID   = Request.pgwrite.pathid;
   myFlags  = static_cast<unsigned short>(Request.pgwrite.reqflags);

// Perform a sanity check on the length.
//
   if (myIOLen <= (int)sizeof(kXR_unt32))
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
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      {myFile = 0;
       return do_WriteNone(pathID);
      }

// If the file object does not have a pgWrite object, allocate one.
//
   if (myFile->pgwFob == 0) myFile->pgwFob = new XrdXrootdPgwFob(myFile);

// We now need to allocate a control object for the wanted stream
//
   if (!myFile->pgwFob->ctlVec[pathID])
      myFile->pgwFob->ctlVec[pathID] = new XrdXrootdPgwCtl(myFile->ID, pathID);
      else myFile->pgwFob->ctlVec[pathID]->Suspend(false);

// Trace this
//
   TRACEP(PGWR, pathID<<" pgwrite "
          <<(myFlags & XrdProto::kXR_pgRetry ? "retry " : "")
          <<myIOLen<<'@'<<myOffset <<" fn=" <<myFile->FileKey);


// Do statistics. They will not always be accurate because we may not be
// able to fully complete the I/O to the file. Note that we also count
// the checksums which a questionable practice.
//
   myFile->Stats.pgwOps(myIOLen, (myFlags & XrdProto::kXR_pgRetry) != 0);

// If we are monitoring, insert a write entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_wr(myFile->Stats.FileID, Request.pgwrite.dlen,
                                                  Request.pgwrite.offset);

// See if an alternate path is required, offload the write
//
   if (pathID) return do_Offload(pathID, true, true);

// Now do the write on the main path
//
   return do_PgWIO();
}

/******************************************************************************/
/*                              d o _ P g W I O                               */
/******************************************************************************/
  
// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket
// myFlags  = Flags associated with request

int XrdXrootdProtocol::do_PgWIO()
{
   struct iovec    *ioV;
   XrdSfsFile      *sfsP = myFile->XrdSfsp;
   XrdXrootdPgwCtl *pgwCtl;
   const char      *eMsg;
   char            *buff;
   kXR_unt32       *csVec;
   int n, rc, Quantum, iovLen, iovNum, csNum;

// Verify that we still have a control area
//
   if (!myFile->pgwFob || !(pgwCtl = myFile->pgwFob->ctlVec[PathID]))
      return do_WriteNone(PathID, kXR_Impossible, "pgwrite logic error 1");

// If this is the first entry then perform one-time initialization. This is
// also the only time we can see a retry request, so handle that first.
//
   if (!pgwCtl->Suspend())
      {if (myFlags & XrdProto::kXR_pgRetry && !do_PgWIORetry(rc)) return rc;
       if (!do_PgWIOSetup(pgwCtl)) return -1;
      }

// Either complete the current frame or start a new one. When we start a new
// one, the I/O will not return unless all of the data was successfully read.
// Hence, we update the length outstanding.
//
do{if (!pgwCtl->Suspend(false))
      {if (!(ioV = pgwCtl->FrameInfo(iovNum, iovLen))) break;
       myIOLen -= iovLen;
       rc = getData(&XrdXrootdProtocol::do_PgWIO, "pgwrite", ioV, iovNum);
       if (rc)
          {if (rc > 0) pgwCtl->Suspend(true);
           return rc;
          }
      }

// We have now all the data, get checksum and data information
//
   if (!(csVec = pgwCtl->FrameInfo(csNum, buff, Quantum, argp)))
      return do_WriteNone(PathID, kXR_Impossible, "pgwrite logic error 2");

// Convert checksums to host byte order
//
  for (int i = 0; i < csNum; i++) csVec[i] = ntohl(csVec[i]);

// Verify the checksums
//
  char   *data = buff;
  ssize_t bado, offs = myOffset;
   size_t badc;
  int     k = 0, dlen = Quantum;
  n = 0;

  do {if ((k = XrdOucPgrwUtils::csVer(data,offs,dlen,&csVec[n],bado,badc)))
         {if ((eMsg = pgwCtl->boAdd(myFile, bado, badc)))
             return do_WriteNone(PathID, kXR_TooManyErrs, eMsg);
          n += k;
          data = pgwCtl->FrameLeft(n, dlen);
          offs = bado + badc;
         } else {
          if (myFlags & XrdProto::kXR_pgRetry)
             myFile->pgwFob->delOffs(myOffset, dlen);
         }

     } while(k && dlen);

// Write the data out. The callee is responsible for unaligned writes!
//
   if ((rc = sfsP->pgWrite(myOffset, buff, Quantum, csVec)) <= 0)
      {myEInfo[0] = rc; myEInfo[1] = 0;
       return do_WriteNone();
      }

// Update offset, length and advance to next frame
//
   myOffset += Quantum;

  } while(pgwCtl->Advance());


// Return final result
//
   buff = pgwCtl->boInfo(n);
   return Response.Send(pgwCtl->resp, sizeof(pgwCtl->info), buff, n);
}
  
/******************************************************************************/
/*                         d o _ P g W I O R e t r y                          */
/******************************************************************************/

// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket
// myFlags  = Flags associated with request

bool XrdXrootdProtocol::do_PgWIORetry(int &rc)
{
   static const int csLen = sizeof(kXR_unt32);
   bool isBad;

// Make sure the write does not cross a page bounday. For unaligned writes we
// can compute the exact length that we need. Otherwise, it can't be bigger
// than a unit's worth of data. Not precise but usually good enough.
//
   if (myOffset & pgPageMask)
      {int n = pgPageSize - (myOffset & pgPageMask);
       isBad = myIOLen > (n + csLen);
      } else isBad = myIOLen > pgUnitSize;

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
   if (!myFile->pgwFob->hasOffs(myOffset, myIOLen - csLen))
      {char buff[64];
       snprintf(buff, sizeof(buff), "retry %d@%lld", myIOLen-csLen, myOffset);
       eDest.Emsg("pgwRetry", buff, "not in error; fn=", myFile->FileKey);
       myFlags &= ~XrdProto::kXR_pgRetry;
      }

// We can proceed with this write now.
//
   return true;
}

/******************************************************************************/
/*                         d o _ P g w I O S e t u p                          */
/******************************************************************************/

// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket
// myFlags  = Flags associated with request

bool XrdXrootdProtocol::do_PgWIOSetup(XrdXrootdPgwCtl *pgwCtl)
{
   const char *eMsg;
   int Quantum;

// Compute the minimum (4K) or maximum buffer size we will use.
//
   if (myIOLen < XrdXrootdPgwCtl::maxBSize/2)
      Quantum = (myIOLen < pgPageSize ? pgPageSize : myIOLen);
      else Quantum = XrdXrootdPgwCtl::maxBSize;

// Make sure we have a large enough buffer
//
   if (!argp || Quantum < halfBSize || argp->bsize < Quantum
   ||   argp->bsize > XrdXrootdPgwCtl::maxBSize)
      {if (getBuff(0, Quantum) <= 0) return -1;}
      else if (hcNow < hcNext) hcNow++;

// Do the setup. If it fails yhen either the client sent an incorrect stream
// of the header was corrupted. In either case, it doesn't matter as we can't
// depend on the information to clear the stream. So, we close the connection.
//
   if ((eMsg = pgwCtl->Setup(argp, myOffset, myIOLen)))
      {Response.Send(kXR_ArgInvalid, eMsg);
       Link->setEtext("pgwrite protocol violation");
       return false;
      }
   return true;
}
