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
#include <sys/uio.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XProtocol/XProtocol.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXeq.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdOucTrace *XrdXrootdTrace;
  
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

// Perform a sanity check on the arguments
//
   if (myIOLen <= 0 || myIOLen & (XrdSys::PageMask))
      return Response.Send(kXR_ArgInvalid, "Read length is invalid");
   if (myOffset & (XrdSys::PageMask))
      return Response.Send(kXR_ArgInvalid, "Read offset is invalid");

// Find the file object
//
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "read does not refer to an open file");

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
   TRACEP(FS, pathID <<" fh=" <<fh.handle <<" read " <<myIOLen <<'@' <<myOffset);

// If we are monitoring, insert a read entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_rd(myFile->Stats.FileID, Request.pgread.rlen,
                                                  Request.pgread.offset);

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
   static const int maxPGRD = maxCSSZ*XrdSys::PageSize; // 2,093,056 usually
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

// Compute the quantum. Make sure it is a multiple of the page size.
//
   Quantum = (maxPGRD > maxBuffsz ? maxBuffsz : maxPGRD);
   if (myIOLen < Quantum) Quantum = myIOLen;
   Quantum &= ~XrdSys::PageMask;
   if (!Quantum) return Response.Send(kXR_NoMemory, "Insufficient memory.");

// Make sure we have a large enough buffer
//
   if (!argp || Quantum < halfBSize || Quantum > argp->bsize)
      {if ((rc = getBuff(1, Quantum)) <= 0) return rc;}
      else if (hcNow < hcNext) hcNow++;
   buff = argp->buff;

// Set flags, as needed
//
   if (myFlags & XrdProto::kXR_pgRetry) pgrOpts |= XrdSfsFile::Verify;

// Preinitialize the header
//
   pgrResp.rsp.bdy.requestid = kXR_pgread - kXR_1stRequest;
   pgrResp.rsp.bdy.resptype  = XrdProto::kXR_PartialResult;;
   memset(pgrResp.rsp.bdy.reserved, 0, sizeof(pgrResp.rsp.bdy.reserved));

// Preinitialize the io vector for checksums and data (leave 1st element free).
//
   uint32_t *csVP = csVec;
   int items = (Quantum / XrdSys::PageSize);
   int i = 1, n = items * 2;
   while(i <= n)
       {iov[i  ].iov_base = csVP++;
        iov[i++].iov_len  = sizeof(uint32_t);
        iov[i  ].iov_base = buff;
        iov[i++].iov_len  = XrdSys::PageSize;
        buff += XrdSys::PageSize;
       }
   buff = argp->buff;

// Now read all of the data. For statistics, we need to record the orignal
// amount of the request even if we really do not get to read that much!
//
   myFile->Stats.rdOps(myIOLen);
   do {if ((xframt = sfsP->pgRead(myOffset, buff, Quantum, csVec, pgrOpts)) <= 0)
          break;
       pgrResp.ofs = htonll(myOffset);
       items = xframt / XrdSys::PageSize;

       if (xframt < Quantum || xframt == myIOLen)
          {pgrResp.rsp.bdy.resptype = XrdProto::kXR_FinalResult;
           int pfrag = xframt & XrdSys::PageMask;
           if (pfrag) {items++; iov[items*2].iov_len = pfrag;}
           myIOLen = 0;
          } else {
           myIOLen -= xframt; myOffset += xframt;
           if (myIOLen < Quantum) Quantum = myIOLen;
          }

       dlen  = xframt + (items * sizeof(uint32_t));
       pgrResp.rsp.bdy.dlen = htonl(dlen);
       if ((rc = Response.Send(pgrResp.rsp, infoLen, iov, items*2+1, dlen)) < 0)
          return rc;
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
   return Response.Send(kXR_Unsupported, "pgWrite not supported.");
}

/******************************************************************************/
/*                              d o _ P g W I O                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_PgWIO()
{
   return 0;
}
