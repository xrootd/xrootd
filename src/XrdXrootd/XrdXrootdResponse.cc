/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d R e s p o n s e . c c                   */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
 
#include <netinet/in.h>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <sys/types.h>

#include "Xrd/XrdLinkCtl.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdXrootd/XrdXrootdResponse.hh"
#define TRACELINK Link
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdTransit.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdSysTrace  XrdXrootdTrace;

const char *XrdXrootdResponse::TraceID = "Response";

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/


namespace
{
const char *sName[] = {"final ", "partial ", "progress "};
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/

int XrdXrootdResponse::Send()
{
    static kXR_unt16 isOK = static_cast<kXR_unt16>(htons(kXR_ok));

    TRACES(RSP, "sending OK");

    if (Bridge)
       {if (Bridge->Send(kXR_ok, 0, 0, 0) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    Resp.status = isOK;
    Resp.dlen   = 0;

    if (Link->Send((char *)&Resp, sizeof(Resp)) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(const char *msg)
{
    static kXR_unt16 isOK = static_cast<kXR_unt16>(htons(kXR_ok));

    TRACES(RSP, "sending OK: " <<msg);

    RespIO[1].iov_base = (caddr_t)msg;
    RespIO[1].iov_len  = strlen(msg)+1;

    if (Bridge)
       {if (Bridge->Send(kXR_ok,&RespIO[1],1,RespIO[1].iov_len) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    Resp.status        = isOK;
    Resp.dlen          = static_cast<kXR_int32>(htonl(RespIO[1].iov_len));

    if (Link->Send(RespIO, 2, sizeof(Resp) + RespIO[1].iov_len) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XResponseType rcode, void *data, int dlen)
{

    TRACES(RSP, "sending " <<dlen <<" data bytes; status=" <<rcode);

    RespIO[1].iov_base = (caddr_t)data;
    RespIO[1].iov_len  = dlen;

    if (Bridge)
       {if (Bridge->Send(rcode, &RespIO[1], 1, dlen) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    Resp.status        = static_cast<kXR_unt16>(htons(rcode));
    Resp.dlen          = static_cast<kXR_int32>(htonl(dlen));

    if (Link->Send(RespIO, 2, sizeof(Resp) + dlen) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XResponseType rcode,
                            struct iovec *IOResp,int iornum, int iolen)
{
    int i, dlen = 0;

    if (iolen < 0) for (i = 1; i < iornum; i++) dlen += IOResp[i].iov_len;
       else dlen = iolen;
    TRACES(RSP, "sending " <<dlen <<" data bytes; status=" <<rcode);

    if (Bridge)
       {if (Bridge->Send(rcode, &IOResp[1], iornum-1, dlen) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    IOResp[0].iov_base = RespIO[0].iov_base;
    IOResp[0].iov_len  = RespIO[0].iov_len;
    Resp.status        = static_cast<kXR_unt16>(htons(rcode));
    Resp.dlen          = static_cast<kXR_int32>(htonl(dlen));

    if (Link->Send(IOResp, iornum, sizeof(Resp) + dlen) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XResponseType rcode, int info,
                            const char   *data,  int dsz)
{
    kXR_int32 xbuf = static_cast<kXR_int32>(htonl(info));
    int dlen;

    RespIO[1].iov_base = (caddr_t)(&xbuf);
    RespIO[1].iov_len  = sizeof(xbuf);
    RespIO[2].iov_base = (caddr_t)data;
    RespIO[2].iov_len  = dlen = (dsz < 0 ? strlen(data) : dsz);

    TRACES(RSP,"sending " <<(sizeof(xbuf)+dlen) <<" data bytes; status=" <<rcode);

    if (Bridge)
       {if (Bridge->Send(rcode, &RespIO[1], 2, dlen) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    Resp.status        = static_cast<kXR_unt16>(htons(rcode));
    Resp.dlen          = static_cast<kXR_int32>(htonl((dlen+sizeof(xbuf))));

    if (Link->Send(RespIO, 3, sizeof(Resp) + dlen + sizeof(xbuf)) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(void *data, int dlen)
{
    static kXR_unt16 isOK = static_cast<kXR_unt16>(htons(kXR_ok));

    TRACES(RSP, "sending " <<dlen <<" data bytes");

    RespIO[1].iov_base = (caddr_t)data;
    RespIO[1].iov_len  = dlen;

    if (Bridge)
       {if (Bridge->Send(kXR_ok, &RespIO[1], 1, dlen) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    Resp.status        = isOK;
    Resp.dlen          = static_cast<kXR_int32>(htonl(dlen));

    if (Link->Send(RespIO, 2, sizeof(Resp) + dlen) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(struct iovec *IOResp, int iornum, int iolen)
{
    static kXR_unt16 isOK = static_cast<kXR_unt16>(htons(kXR_ok));
    int dlen = 0;

    if (iolen < 0) for (int i = 1; i < iornum; i++) dlen += IOResp[i].iov_len;
       else dlen = iolen;
    TRACES(RSP, "sending " <<dlen <<" data bytes; status=0");


    if (Bridge)
       {if (Bridge->Send(kXR_ok, &IOResp[1], iornum-1, dlen) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    IOResp[0].iov_base = RespIO[0].iov_base;
    IOResp[0].iov_len  = RespIO[0].iov_len;
    Resp.status        = isOK;
    Resp.dlen          = static_cast<kXR_int32>(htonl(dlen));

    if (Link->Send(IOResp, iornum, sizeof(Resp) + dlen) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XErrorCode ecode, const char *msg)
{
    int dlen;
    kXR_int32 erc = static_cast<kXR_int32>(htonl(ecode));

    TRACES(EMSG, "sending err " <<ecode <<": " <<msg);

    RespIO[1].iov_base = (char *)&erc;
    RespIO[1].iov_len  = sizeof(erc);
    RespIO[2].iov_base = (caddr_t)msg;
    RespIO[2].iov_len  = strlen(msg)+1;
                dlen   = sizeof(erc) + RespIO[2].iov_len;

    if (Bridge)
       {if (Bridge->Send(kXR_error, &RespIO[1], 2, dlen) >= 0) return 0;
        return Link->setEtext("send failure");
       }

    Resp.status        = static_cast<kXR_unt16>(htons(kXR_error));
    Resp.dlen          = static_cast<kXR_int32>(htonl(dlen));

    if (Link->Send(RespIO, 3, sizeof(Resp) + dlen) < 0)
       return Link->setEtext("send failure");
    return 0;
}
 
/******************************************************************************/

int XrdXrootdResponse::Send(int fdnum, long long offset, int dlen)
{
   static kXR_unt16 isOK = static_cast<kXR_unt16>(htons(kXR_ok));
   XrdLink::sfVec myVec[2];

   TRACES(RSP, "sendfile " <<dlen <<" data bytes");

   if (Bridge)
      {if (Bridge->Send(offset, dlen, fdnum) >= 0) return 0;
       return Link->setEtext("send failure");
      }

// We are only called should sendfile be enabled for this response
//
   Resp.status = isOK;
   Resp.dlen   = static_cast<kXR_int32>(htonl(dlen));

// Fill out the sendfile vector
//
   myVec[0].buffer = (char *)&Resp;
   myVec[0].sendsz = sizeof(Resp);
   myVec[0].fdnum  = -1;
   myVec[1].offset = static_cast<off_t>(offset);
   myVec[1].sendsz = dlen;
   myVec[1].fdnum  = fdnum;

// Send off the request
//
    if (Link->Send(myVec, 2) < 0)
       return Link->setEtext("sendfile failure");
    return 0;
}
 
/******************************************************************************/

int XrdXrootdResponse::Send(XrdOucSFVec *sfvec, int sfvnum, int dlen)
{
   static kXR_unt16 isOK = static_cast<kXR_unt16>(htons(kXR_ok));

   TRACES(RSP, "sendfile " <<dlen <<" data bytes");

   if (Bridge)
      {if (Bridge->Send(sfvec, sfvnum, dlen) >= 0) return 0;
       return Link->setEtext("send failure");
      }

// We are only called should sendfile be enabled for this response
//
   Resp.status = isOK;
   Resp.dlen   = static_cast<kXR_int32>(htonl(dlen));
   sfvec[0].buffer = (char *)&Resp;
   sfvec[0].sendsz = sizeof(Resp);
   sfvec[0].fdnum  = -1;

// Send off the request
//
    if (Link->Send(sfvec, sfvnum) < 0)
       return Link->setEtext("sendfile failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(ServerResponseStatus &srs, int iLen)
{

// Fill out the status structure and send this off
//
    if (Link->Send((char *)&srs, srsComplete(srs, iLen)) < 0)
       return Link->setEtext("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(ServerResponseStatus &srs, int iLen,
                            void *data, int dlen)
{
   int rc;

// Send off the appropriate response
//
    if (!dlen) rc = Link->Send((char *)&srs, srsComplete(srs, iLen));
       else {struct iovec srsIOV[2];
             srsIOV[0].iov_base = &srs;
             srsIOV[0].iov_len  = srsComplete(srs, iLen, dlen);
             srsIOV[1].iov_base = (caddr_t)data;
             srsIOV[1].iov_len  = dlen;
             rc = Link->Send(srsIOV, 2, srsIOV[0].iov_len + dlen);
            }

// Finish up
//
   if (rc < 0) return Link->setEtext("send failure");
   return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(ServerResponseStatus &srs, int iLen,
                            struct iovec *IOResp, int iornum, int iolen)
{
   int dlen = 0;

// If we need to compute the amount of data we are sending, do so now.
//
   if (iolen < 0) for (int i = 1; i < iornum; i++) dlen += IOResp[i].iov_len;
      else dlen = iolen;

// Fill out the status structure
//
   int rspLen = srsComplete(srs, iLen, dlen);

// Complete the iovec for the send

   IOResp[0].iov_base = &srs;
   IOResp[0].iov_len  = rspLen;

// Send the data off
//
   if (Link->Send(IOResp, iornum, rspLen + dlen) < 0)
      return Link->setEtext("send failure");
   return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XrdXrootdReqID &ReqID, 
                            XResponseType   Status,
                            struct iovec   *IOResp, 
                            int             iornum, 
                            int             iolen)
{
   static const kXR_unt16 Xattn = static_cast<kXR_unt16>(htons(kXR_attn));
   static const kXR_int32 Xarsp = static_cast<kXR_int32>(htonl(kXR_asynresp));

// We would have used struct ServerResponseBody_Attn_asynresp but the silly
// imbedded 4096 char array causes grief when computing lengths.
//
   struct {ServerResponseHeader atnHdr;
           kXR_int32            act;
           kXR_int32            rsvd;  // Same as char[4]
           ServerResponseHeader theHdr;
          } asynResp;

   static const int sfxLen = sizeof(asynResp) - sizeof(asynResp.atnHdr);

   XrdLink           *Link;
   unsigned char      theSID[2];
   int                theFD, rc, ioxlen = iolen;
   unsigned int       theInst;

// Fill out the header with constant information
//
   asynResp.atnHdr.streamid[0] = '\0';
   asynResp.atnHdr.streamid[1] = '\0';
   asynResp.atnHdr.status      = Xattn;
   asynResp.act                = Xarsp;
   asynResp.rsvd               = 0;

// Complete the io vector to send this response
//
   IOResp[0].iov_base = (char *)&asynResp;
   IOResp[0].iov_len  = sizeof(asynResp);           // 0

// Insert the status code
//
    asynResp.theHdr.status = static_cast<kXR_unt16>(htons(Status));

// We now insert the length of the delayed response and the full response
//
   asynResp.theHdr.dlen = static_cast<kXR_int32>(htonl(iolen));
   iolen += sfxLen;
   asynResp.atnHdr.dlen = static_cast<kXR_int32>(htonl(iolen));
   iolen += sizeof(ServerResponseHeader);

// Decode the destination
//
   ReqID.getID(theSID, theFD, theInst);

// Map the destination to an endpoint, and send the response
//
   if ((Link = XrdLinkCtl::fd2link(theFD, theInst)))
      {Link->setRef(1);
       if (Link->isInstance(theInst))
          {if (Link->hasBridge())
              rc = XrdXrootdTransit::Attn(Link, (short *)theSID, int(Status),
                                          &IOResp[1], iornum-1, ioxlen);
             else {asynResp.theHdr.streamid[0] = theSID[0];
                   asynResp.theHdr.streamid[1] = theSID[1];
                   rc = Link->Send(IOResp, iornum, iolen);
                  }
          } else rc = -1;
       Link->setRef(-1);
       return (rc < 0 ? -1 : 0);
      }
   return -1;
}
  
/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/

void XrdXrootdResponse::Set(unsigned char *stream)
{
   static char hv[] = "0123456789abcdef";
   char *outbuff;
   int i;

   Resp.streamid[0] = stream[0];
   Resp.streamid[1] = stream[1];

   if (TRACING((TRACE_REQ|TRACE_RSP)))
      {outbuff = trsid;
       for (i = 0; i < (int)sizeof(Resp.streamid); i++)
           {*outbuff++ = hv[(stream[i] >> 4) & 0x0f];
            *outbuff++ = hv[ stream[i]       & 0x0f];
            }
       *outbuff++ = ' '; *outbuff = '\0';
      }
}

/******************************************************************************/
/* Private:                  s r s C o m p l e t e                            */
/******************************************************************************/
  
int XrdXrootdResponse::srsComplete(ServerResponseStatus &srs,
                                   int iLen, int dlen)
{
   static const int csSZ = sizeof(kXR_unt32);
   static const int bdSZ = sizeof(ServerResponseBody_Status);

   const unsigned char *body;
   kXR_unt32 crc32c;

// Do some tracing if so requested
//
   TRACES(RSP, "sending " <<sName[srs.bdy.resptype]
          <<iLen <<" info and " <<dlen <<" data bytes");

// Fill out the header
//
   srs.hdr.streamid[0] = Resp.streamid[0];
   srs.hdr.streamid[1] = Resp.streamid[1];
   srs.hdr.status      = htons(kXR_status);
   srs.hdr.dlen        = htonl(bdSZ+iLen);

// Complete the status body
//
   srs.bdy.streamID[0] = Resp.streamid[0];
   srs.bdy.streamID[1] = Resp.streamid[1];
   srs.bdy.dlen        = htonl(dlen);

// Finally, compute the crc for the body
//
   body   = ((const unsigned char *)&srs.bdy.crc32c)+csSZ;
   crc32c = XrdOucCRC::Calc32C(body, bdSZ-csSZ+iLen);
   srs.bdy.crc32c = htonl(crc32c);

// Return the total amount of bytes to send for the header
//
   return sizeof(ServerResponseStatus) + iLen;
}
