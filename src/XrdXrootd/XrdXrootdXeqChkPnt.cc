/******************************************************************************/
/*                                                                            */
/*                 X r d X r o o t d X e q C h k P n t . c c                  */
/*                                                                            */
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

#include "XProtocol/XProtocol.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdWVInfo.hh"
#include "XrdXrootd/XrdXrootdXeq.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

/******************************************************************************/
/*                             d o _ C h k P n t                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_ChkPnt()
{
   static const char *ckpName[] = {"begin","commit","query","rollback","xeq"};

// Keep statistics
//
   SI->Bump(SI->miscCnt);

// The kXR_ckpXeq is far to complicated to process here so we do it elsewhere.
//
   if (Request.chkpoint.opcode == kXR_ckpXeq) return do_ChkPntXeq();

// Validate the filehandle
//
   XrdXrootdFHandle fh(Request.chkpoint.fhandle);
   struct iov ckpVec;
   int rc;

   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "chkpoint does not refer to an open file");

// Handle each subcode
//
   switch(Request.chkpoint.opcode)
         {case kXR_ckpBegin:
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpCreate);
               break;
          case kXR_ckpCommit:
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpDelete);
               break;
          case kXR_ckpQuery:
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpQuery,&ckpVec,1);
               if (!rc)
                  {ServerResponseBody_ChkPoint ckpQResp;
                   ckpQResp.maxCkpSize = htonl(ckpVec.size);
                   ckpQResp.useCkpSize =
                                  htonl(static_cast<uint32_t>(ckpVec.offset));
                   return Response.Send(&ckpQResp, sizeof(ckpQResp));
                  }
               break;
          case kXR_ckpRollback:
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpRestore);
               break;
          default: return Response.Send(kXR_ArgInvalid,
                                        "chkpoint subcode is invalid");
         };

// Do some tracing
//
   TRACEP(FS, "fh=" <<fh.handle <<" chkpnt " <<ckpName[Request.chkpoint.opcode]
                    <<" rc=" <<rc);

// Check for error and invalid return codes from checkpoint note that writev's
// aren't flushed, we simply close the connection to get rid of pending data.
//
   if (SFS_OK != rc)
      {if (rc != SFS_ERROR)
          {char eBuff[128];
           snprintf(eBuff, sizeof(eBuff), "chkpoint %s returned invalid rc=%d!",
                    ckpName[Request.chkpoint.opcode], rc);
           eDest.Emsg("Xeq", eBuff);
           IO.File->XrdSfsp->error.setErrInfo(ENODEV, "logic error");
          }
       return fsError(SFS_ERROR, 0, IO.File->XrdSfsp->error, 0, 0);
      }

// Respond that all went well
//
   return Response.Send();
}
  
/******************************************************************************/
/*                          d o _ C h k P n t X e q                           */
/******************************************************************************/
  
int XrdXrootdProtocol::do_ChkPntXeq()
{
   static const int sidSZ = sizeof(Request.header.streamid);
   int rc;

// If this is the first pass, check that streamid's match and setup the
// request to be that of the chkpnt request. Note that kXR_writev requires
// an additional fetch of data which may cause re-entry as pass2.
//
   if (Request.header.requestid == kXR_chkpoint)
      {ClientRequestHdr *Subject = (ClientRequestHdr *)(argp->buff);
       if (memcmp(Request.header.streamid, Subject->streamid, sidSZ))
          {Response.Send(kXR_ArgInvalid, "Request streamid missmatch");
           return -1;
          }
       if (Request.header.dlen != sizeof(Request.header))
          {Response.Send(kXR_ArgInvalid, "Request length invalid");
           return -1;
          }

       memcpy(Request.header.body, Subject->body, sizeof(Request.header.body));
       Request.header.requestid = ntohs(Subject->requestid);
       Request.header.dlen      = ntohl(Subject->dlen);

       if (Request.header.requestid == kXR_chkpoint
       || (Request.header.requestid == kXR_truncate && Request.header.dlen))
          {Response.Send(kXR_ArgInvalid,"chkpoint request is invalid");
           return -1;
          }

       if (Request.header.requestid == kXR_writev)
          {if (!Request.header.dlen) return Response.Send();
           if (Request.header.dlen > XrdProto::maxWvecln)
              {Response.Send(kXR_ArgTooLong,"chkpoint write vector is too long");
               return -1;
              }
           if ( Request.header.dlen > argp->bsize)
              {BPool->Release(argp);
               if (!(argp = BPool->Obtain(Request.header.dlen)))
                  {Response.Send(kXR_NoMemory,
                                 "Insufficient memory for chkpoint request");
                   return -1;
                  }
               hcNow = hcPrev; halfBSize = argp->bsize >> 1;
              }
           if ((rc = getData("arg", argp->buff, Request.header.dlen)))
              {Resume = &XrdXrootdProtocol::do_ChkPntXeq; return rc;}
          }
      }

// Prepare to process the actual request
//
   const char      *xeqOp;
   struct iov       ckpVec;
   XrdXrootdFHandle fh;
   kXR_unt16        reqID;

   reqID = Request.header.requestid;
   Request.header.requestid = kXR_chkpoint;

// Obtain the filehandle that we should check
//
   switch(reqID)
         {case kXR_pgwrite:
               xeqOp = "pgwrite";
               fh.Set(Request.pgwrite.fhandle);
               break;
          case kXR_truncate:
               xeqOp = "trunc";
               fh.Set(Request.truncate.fhandle);
               break;
          case kXR_write:
               xeqOp = "write";
               fh.Set(Request.write.fhandle);
               break;
          case kXR_writev:
               xeqOp = "writev";
               if ((rc = do_WriteV())) return rc;
               if (!wvInfo) return 0;
               fh.handle = wvInfo->curFH;
               for (int i = 0; i < wvInfo->vEnd; i++)
                   if (wvInfo->ioVec[i].info != fh.handle)
                      {free(wvInfo); wvInfo = 0;
                       Response.Send(kXR_Unsupported,
                                "multi-file chkpoint writev not supported");
                       return -1;
                      }
               break;
          default: return Response.Send(kXR_ArgInvalid,
                                        "chkpoint request is invalid");
           }

// Make sure we have the target file
//
   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      {rc = Response.Send(kXR_FileNotOpen,
                          "chkpoint does not refer to an open file");
       if (reqID != kXR_truncate)
          return Link->setEtext("chkpnt xeq write protocol violation");
       return rc;
      }

// If this is a packaged request, create a checkpoint
//

// Now perform the action
//
   switch(reqID)
         {case kXR_pgwrite:
               ckpVec.size = Request.header.dlen;
                             n2hll(Request.pgwrite.offset, ckpVec.offset);
               ckpVec.info = 0;
               ckpVec.data = 0;
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpWrite,&ckpVec,1);
               if (!rc) return do_PgWrite();
               break;
          case kXR_truncate:
               n2hll(Request.write.offset, ckpVec.offset);
               ckpVec.info = 0;
               ckpVec.data = 0;
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpTrunc,&ckpVec,1);
               if (!rc) return do_Truncate();
               break;
          case kXR_write:
               ckpVec.size = Request.header.dlen;
                             n2hll(Request.write.offset, ckpVec.offset);
               ckpVec.info = 0;
               ckpVec.data = 0;
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpWrite,&ckpVec,1);
               if (!rc) return do_Write();
               break;
          default: // kXR_writev
               rc = IO.File->XrdSfsp->checkpoint(XrdSfsFile::cpWrite,
                                     (iov *)wvInfo->ioVec, wvInfo->vEnd);
               if (!rc)
                  {for (int i = 0; i < wvInfo->vEnd; i++)
                       wvInfo->ioVec[i].info = fh.handle;
                   return do_WriteVec();
                  }
               break;
         }

// Do some tracing
//
   TRACEP(FS, "fh=" <<fh.handle <<" chkpnt " <<xeqOp <<" rc=" <<rc);

// Check for error and invalid return codes from checkpoint note that writev's
// aren't flushed, we simply close the connection to get rid of pending data.
//
   if (SFS_OK != rc)
      {if (rc != SFS_ERROR)
          {char eBuff[128];
           snprintf(eBuff, sizeof(eBuff),
                    "chkpoint xeq %s returned invalid rc=%d!", xeqOp, rc);
           eDest.Emsg("Xeq", eBuff);
           IO.File->XrdSfsp->error.setErrInfo(ENODEV, "logic error");
          }
       if (reqID == kXR_pgwrite)
          {IO.EInfo[0] = SFS_ERROR; IO.EInfo[0] = 0;
           return do_WriteNone(static_cast<int>(Request.pgwrite.pathid));
          }
       if (reqID == kXR_write)
          {IO.EInfo[0] = SFS_ERROR; IO.EInfo[0] = 0;
           return do_WriteNone(static_cast<int>(Request.write.pathid));
          }
       rc = fsError(SFS_ERROR, 0, IO.File->XrdSfsp->error, 0, 0);
       return (reqID != kXR_truncate ? -1 : rc);
      }

// Respond that all went well
//
   return Response.Send();
}
