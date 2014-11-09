/******************************************************************************/
/*                                                                            */
/*                         X r d C m s T a l k . c c                          */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "XProtocol/YProtocol.hh"

#include "Xrd/XrdLink.hh"
#include "XrdCms/XrdCmsTalk.hh"

using namespace XrdCms;

/******************************************************************************/
/*                                A t t e n d                                 */
/******************************************************************************/

const char *XrdCmsTalk::Attend(XrdLink *Link, XrdCms::CmsRRHdr &Hdr,
                               char    *buff, int blen,
                               int     &rlen, int TimeOut)
{

// First obtain the complete header
//
   if (Link->Recv((char *)&Hdr, sizeof(Hdr), TimeOut) != sizeof(Hdr))
      return "header not sent";

// Decode the length and make sure it fits in the buffer
//
   rlen = static_cast<int>(ntohs(Hdr.datalen));
   if (rlen > blen) return "data too long";

// Get the actual data
//
   if (Link->Recv(buff,rlen,TimeOut) != rlen) return "data not received";

// All done
//
   return 0;
}

/******************************************************************************/
/*                              C o m p l a i n                               */
/******************************************************************************/

int XrdCmsTalk::Complain(XrdLink *Link, int ecode, const char *msg)
{
   static const int xNum   = 2;

   struct iovec Liov[xNum];
   int mlen = strlen(msg)+1;
   CmsResponse LEResp={{0, kYR_error, 0, 0}, htonl((unsigned int)ecode)};

// Fill out header and iovector
//
   LEResp.Hdr.datalen = htons(static_cast<kXR_unt16>(mlen+sizeof(LEResp.Val)));
   Liov[0].iov_base = (char *)&LEResp;
   Liov[0].iov_len  = sizeof(LEResp);
   Liov[1].iov_base = (char *)msg;
   Liov[1].iov_len  = mlen;

// Send off the data
//
   Link->Send(Liov, xNum);
   return 0;
}
  
/******************************************************************************/
/*                               R e q u e s t                                */
/******************************************************************************/
  
const char *XrdCmsTalk::Request(XrdLink *Link, XrdCms::CmsRRHdr &Hdr,
                                char    *buff, int blen)
{
   struct iovec ioV[2] = {{(char *)&Hdr, sizeof(Hdr)},
                          {(char *)buff, (size_t)blen}};

   Hdr.datalen = htons(static_cast<unsigned short>(blen));

// Send the actual data
//
   if (Link->Send(ioV, 2) < 0) return "request not sent";
   return 0;
}

/******************************************************************************/
/*                               R e s p o n d                                */
/******************************************************************************/

const char *XrdCmsTalk::Respond(XrdLink *Link, XrdCms::CmsRspCode rcode,
                                char    *buff, int                blen)
{
   static const unsigned short ovhd = sizeof(kXR_unt32);
   CmsResponse Resp = {{0, (kXR_char)rcode, 0,
                        htons(static_cast<unsigned short>(blen+ovhd))}, 0};
   struct iovec ioV[2] = {{(char *)&Resp, sizeof(Resp)},
                          {         buff, (size_t)blen}};

// Send the actual data
//
   if (Link->Send(ioV, 2) < 0) return "response not sent";
   return 0;
}
