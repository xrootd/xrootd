/******************************************************************************/
/*                                                                            */
/*                        X r d C m s L o g i n . c c                         */
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

#include <netinet/in.h>

#include "XProtocol/YProtocol.hh"

#include "Xrd/XrdLink.hh"

#include "XrdCms/XrdCmsBlackList.hh"
#include "XrdCms/XrdCmsLogin.hh"
#include "XrdCms/XrdCmsParser.hh"
#include "XrdCms/XrdCmsTalk.hh"
#include "XrdCms/XrdCmsSecurity.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdOuc/XrdOucPup.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

using namespace XrdCms;

/******************************************************************************/
/* Public:                         A d m i t                                  */
/******************************************************************************/
  
int XrdCmsLogin::Admit(XrdLink *Link, CmsLoginData &Data)
{
   CmsRRHdr      myHdr;
   CmsLoginData  myData;
   const char   *eText, *Token;
            int  myDlen, Toksz;

// Get complete request
//
   if ((eText = XrdCmsTalk::Attend(Link, myHdr, myBuff, myBlen, myDlen)))
      return Emsg(Link, eText, 0);

// Check if this node is blacklisted
//
   if (!(Data.Mode & CmsLoginData::kYR_director)
   &&  XrdCmsBlackList::Present(Link->AddrInfo()->Name()))
      return SendErrorBL(Link);

// If we need to do authentication, do so now
//
   if ((Token = XrdCmsSecurity::getToken(Toksz, Link->AddrInfo()))
   &&  !XrdCmsSecurity::Authenticate(Link, Token, Toksz)) return 0;

// Fiddle with the login data structures
//
   Data.SID = Data.Paths = Data.ifList = Data.envCGI = 0;
   memset(&myData, 0, sizeof(myData));
   myData.Mode     = Data.Mode;
   myData.HoldTime = Data.HoldTime;
   myData.Version  = Data.Version = kYR_Version;

// Decode the data pointers ans grab the login data
//
   if (!Parser.Parse(&Data, myBuff, myBuff+myDlen)) 
      return Emsg(Link, "invalid login data", 0);

// Send off login reply
//
   return (sendData(Link, myData) ? 0 : 1);
}

/******************************************************************************/
/* Private:                         E m s g                                   */
/******************************************************************************/

int XrdCmsLogin::Emsg(XrdLink *Link, const char *msg, int ecode)
{
   Say.Emsg("Login", Link->Name(), "login failed;", msg);
   return ecode;
}
  
/******************************************************************************/
/* Public:                         L o g i n                                  */
/******************************************************************************/
  
int XrdCmsLogin::Login(XrdLink *Link, CmsLoginData &Data, int timeout)
{
   CmsRRHdr LIHdr;
   char WorkBuff[4096], *hList, *wP = WorkBuff;
   int n, dataLen;

// Send the data
//
   if (sendData(Link, Data)) return kYR_EINVAL;

// Get the response.
//
   if ((n = Link->RecvAll((char *)&LIHdr, sizeof(LIHdr), timeout)) < 0)
      return Emsg(Link, (n == -ETIMEDOUT ? "timed out" : "rejected"));

// Receive and decode the response. We apparently have protocol version 2.
//
   if ((dataLen = static_cast<int>(ntohs(LIHdr.datalen))))
      {if (dataLen > (int)sizeof(WorkBuff)) 
          return Emsg(Link, "login reply too long");
       if (Link->RecvAll(WorkBuff, dataLen, timeout) < 0)
          return Emsg(Link, "login receive error");
      }

// Check if we are being asked to identify ourselves
//
   if (LIHdr.rrCode == kYR_xauth)
      {if (!XrdCmsSecurity::Identify(Link, LIHdr, WorkBuff, sizeof(WorkBuff)))
          return kYR_EINVAL;
       dataLen = static_cast<int>(ntohs(LIHdr.datalen));
       if (dataLen > (int)sizeof(WorkBuff))
          return Emsg(Link, "login reply too long");
      }

// The response can also be a login redirect (i.e., a try request).
//
   if (!(Data.Mode & CmsLoginData::kYR_director)
   &&  LIHdr.rrCode == kYR_try)
      {if (!XrdOucPup::Unpack(&wP, wP+dataLen, &hList, n))
          return Emsg(Link, "malformed try host data");
       Data.Paths = (kXR_char *)strdup(n ? hList : "");
       return kYR_redirect;
      }

// Process error reply
//
   if (LIHdr.rrCode == kYR_error)
      {unsigned int eRC;
       if (dataLen < (int)sizeof(kXR_unt32)+8)
          return Emsg(Link, "invalid error reply");
       Emsg(Link, WorkBuff+sizeof(kXR_unt32));
       memcpy(&eRC, WorkBuff, sizeof(eRC));
       eRC = ntohl(eRC);
       return (eRC == kYR_EPERM ? -1 : kYR_EINVAL);
      }

// Process normal reply
//
   if (LIHdr.rrCode != kYR_login
   || !Parser.Parse(&Data, WorkBuff, WorkBuff+dataLen))
      return Emsg(Link, "invalid login response");
   return 0;
}

/******************************************************************************/
/* Private:                     s e n d D a t a                               */
/******************************************************************************/
  
int XrdCmsLogin::sendData(XrdLink *Link, CmsLoginData &Data)
{
   static const int xNum   = 18;

   int          n, iovcnt, iovnum;
   char         Work[xNum*12];
   struct iovec Liov[xNum];
   CmsRRHdr     Resp={0, kYR_login, 0, 0};

// Pack the response (ignore the auth token for now)
//
   if (!(iovcnt=Parser.Pack(kYR_login,&Liov[1],&Liov[xNum],(char *)&Data,Work)))
      return Emsg(Link, "too much login data");

// Complete I/O vector
//
   Resp.datalen = Data.Size;
   Liov[0].iov_base = (char *)&Resp;
   Liov[0].iov_len  = sizeof(Resp);

// Send off the data *break it up to IOV_MAX chunks, mostly for Solaris)
//
   n = 0; iovcnt++;
   if (iovcnt <= IOV_MAX) Link->Send(Liov, iovcnt);
      else while(iovcnt > 0)
                {iovnum = (iovcnt > IOV_MAX ? IOV_MAX : iovcnt);
                 Link->Send(&Liov[n], iovnum);
                 n += IOV_MAX; iovcnt -= IOV_MAX;
                }

// Return success
//
   return 0;
}

/******************************************************************************/
/* Private:                  S e n d E r r o r B L                            */
/******************************************************************************/
  
int XrdCmsLogin::SendErrorBL(XrdLink *Link)
{
  struct {CmsResponse rInfo;
          char        rText[512];
         }            rData;
  const char *hName = Link->AddrInfo()->Name("???");
  int n;

// Format the message
//
   n = snprintf(rData.rText, sizeof(rData.rText), "%s is blacklisted.", hName)
     + sizeof(rData.rInfo.Val) + 1;

// Construct response
//
   rData.rInfo.Hdr.streamid = 0;
   rData.rInfo.Hdr.rrCode   = kYR_error;
   rData.rInfo.Hdr.modifier = 0;
   rData.rInfo.Hdr.datalen  = htons(n);
   rData.rInfo.Val          = htonl(static_cast<unsigned int>(kYR_EPERM));

// Send off the data
//
   Link->Send((const char *)&rData, n + sizeof(CmsRRHdr));
   return Emsg(Link, "blacklisted", 0);
}
