/******************************************************************************/
/*                                                                            */
/*                     x r o o t d _ R e s p o n s e . C                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//      $Id$

const char *XrdXrootdResponseCVSID = "$Id$";
 
#include "Experiment/Experiment.hh"

#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <string.h>

#include "Xrd/XrdLink.hh"
#include "XrdXrootd/XrdXrootdResponse.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOucTrace *XrdXrootdTrace;

const char *XrdXrootdResponse::TraceID = "Response";

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define TRACELINK Link

#define DISCARD_LINK(y) (Link->setEtext(y), Link->Close(), -1)
  
/******************************************************************************/
/*                                  P u s h                                   */
/******************************************************************************/

int XrdXrootdResponse::Push(void *data, int dlen)
{
    kXR_int32 DLen = htonl((kXR_int32)dlen);
    RespIO[1].iov_base = (caddr_t)&DLen;
    RespIO[1].iov_len  = sizeof(dlen);
    RespIO[2].iov_base = (caddr_t)data;
    RespIO[2].iov_len  = dlen;

    if (Link->Send(&RespIO[1], 2, sizeof(kXR_int32) + dlen) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

int XrdXrootdResponse::Push()
{
    static int null = 0;
    if (Link->Send((char *)&null, sizeof(kXR_int32)) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/

int XrdXrootdResponse::Send()
{

    Resp.status = htons((kXR_int16)kXR_ok);
    Resp.dlen   = 0;
    TRACES(RSP, "sending OK");

    if (Link->Send((char *)&Resp, sizeof(Resp)) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(const char *msg)
{

    Resp.status        = htons((kXR_int16)kXR_ok);
    RespIO[1].iov_base = (caddr_t)msg;
    RespIO[1].iov_len  = strlen(msg)+1;
    Resp.dlen          = htonl((kXR_int32)RespIO[1].iov_len);
    TRACES(RSP, "sending OK " <<msg);

    if (Link->Send(RespIO, 2, sizeof(Resp) + RespIO[1].iov_len) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XResponseType rcode, void *data, int dlen)
{

    Resp.status        = htons((kXR_int16)rcode);
    RespIO[1].iov_base = (caddr_t)data;
    RespIO[1].iov_len  = dlen;
    Resp.dlen          = htonl((kXR_int32)dlen);
    TRACES(RSP, "sending " <<dlen <<" data bytes; rc=" <<rcode);

    if (Link->Send(RespIO, 2, sizeof(Resp) + dlen) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XResponseType rcode, int info, char *data)
{
    kXR_int32 longbuf = htonl((kXR_int32)info);
    int dlen;

    Resp.status        = htons((kXR_int16)rcode);
    RespIO[1].iov_base = (caddr_t)(&longbuf);
    RespIO[1].iov_len  = sizeof(longbuf);
    RespIO[2].iov_base = (caddr_t)data;
    RespIO[2].iov_len  = dlen = strlen(data);
    Resp.dlen          = htonl((kXR_int32)(dlen+sizeof(longbuf)));

    if (Link->Send(RespIO, 3, sizeof(Resp) + dlen) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(void *data, int dlen)
{

    Resp.status        = htons((kXR_int16)kXR_ok);
    RespIO[1].iov_base = (caddr_t)data;
    RespIO[1].iov_len  = dlen;
    Resp.dlen          = htonl((kXR_int32)dlen);
    TRACES(RSP, "sending " <<dlen <<" data bytes; rc=0");

    if (Link->Send(RespIO, 2, sizeof(Resp) + dlen) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(struct iovec *IOResp, int iornum, int iolen)
{
    int i, dlen = 0;

    if (iolen < 0) for (i = 1; i < iornum; i++) dlen += IOResp[i].iov_len;
       else dlen = iolen;

    Resp.status        = htons((kXR_int16)kXR_ok);
    IOResp[0].iov_base = RespIO[0].iov_base;
    IOResp[0].iov_len  = RespIO[0].iov_len;
    Resp.dlen          = htonl((kXR_int32)dlen);
    TRACES(RSP, "sending " <<dlen <<" data bytes; rc=0");

    if (Link->Send(IOResp, iornum, sizeof(Resp) + dlen) < 0)
       return DISCARD_LINK("send failure");
    return 0;
}

/******************************************************************************/

int XrdXrootdResponse::Send(XErrorCode ecode, const char *msg)
{
    long dlen;
    kXR_int32 erc = htonl((kXR_int32)ecode);

    Resp.status        = htons((kXR_int16)kXR_error);
    RespIO[1].iov_base = (char *)&erc;
    RespIO[1].iov_len  = sizeof(erc);
    RespIO[2].iov_base = (caddr_t)msg;
    RespIO[2].iov_len  = strlen(msg)+1;
                dlen   = sizeof(erc) + RespIO[2].iov_len;
    Resp.dlen          = htonl((kXR_int32)dlen);
    TRACES(EMSG, "sending err " <<ecode <<": " <<msg);

    if (Link->Send(RespIO, 3, sizeof(Resp) + dlen) < 0)
       return DISCARD_LINK("send failure");
    return 0;
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

   if (TRACING(TRACE_REQ|TRACE_RSP))
      {outbuff = trsid;
       for (i = 0; i < sizeof(Resp.streamid); i++)
           {*outbuff++ = hv[(stream[i] >> 4) & 0x0f];
            *outbuff++ = hv[ stream[i]       & 0x0f];
            }
       *outbuff++ = ' '; *outbuff = '\0';
      }
}
