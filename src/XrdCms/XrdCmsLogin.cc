/******************************************************************************/
/*                                                                            */
/*                        X r d C m s L o g i n . c c                         */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

#include <netinet/in.h>

#include "XProtocol/YProtocol.hh"

#include "Xrd/XrdLink.hh"

#include "XrdCms/XrdCmsLogin.hh"
#include "XrdCms/XrdCmsParser.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdOuc/XrdOucPup.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"

using namespace XrdCms;

/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/
  
int       XrdCmsLogin::TimeOut = 1000;

/******************************************************************************/
/* Public:                         A d m i t                                  */
/******************************************************************************/
  
int XrdCmsLogin::Admit(XrdLink *Link, CmsLoginData &Data)
{
   CmsRRHdr      myHdr;
   CmsLoginData  myData;
            int  myDlen;

// First obtain the complete header
//
   if (Link->Recv((char *)&myHdr, sizeof(myHdr), TimeOut) != sizeof(myHdr))
      return sendError(Link, "login header not sent");

// Decode the length and get the rest of the data. Login data will never
// exceeds 1k bytes (well, not at the moment anyway)
//
   myDlen = static_cast<int>(ntohs(myHdr.datalen));
   if (myDlen > myBlen) return sendError(Link, "login data too long");
   if (Link->Recv(myBuff,myDlen,TimeOut) != myDlen)
      return sendError(Link, "login data not sent");

// Fiddle with the login data structures
//
   Data.SID = Data.Paths = Data.Auth = 0;
   memset(&myData, 0, sizeof(myData));
   myData.Mode     = Data.Mode;
   myData.HoldTime = Data.HoldTime;
   myData.Version  = Data.Version = kYR_Version;

// Decode the data pointers ans grab the login data
//
   if (!Parser.Parse(&Data, myBuff, myBuff+myDlen)) 
      return sendError(Link, "invalid login data");
   myData.Auth = Data.Auth;

// Do authentication, send login reply, and receive final authentication reply
//
   return Authenticate(Link, myData);
}

/******************************************************************************/
/* Private:                 A u t h e n t i c a t e                           */
/******************************************************************************/
  
int XrdCmsLogin::Authenticate(XrdLink *Link, CmsLoginData &Data)
{

// At this point, we would decrypt the authtoken and generate a new token
//

// Send off login reply
//
   return sendData(Link, Data);

// Receive final authentication reply

// If we sent a token, then the sender must send back the token. Since we know
// we didn't send one, we don't receive one here. Eventually we will.

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
  
int XrdCmsLogin::Login(XrdLink *Link, CmsLoginData &Data)
{
   CmsRRHdr LIHdr;
   char WorkBuff[4096], *hList, *wP = WorkBuff;
   int n, dataLen;

// Generate an authenticatioon token
//
   Data.Auth = 0;

// Send the data
//
   if (sendData(Link, Data)) return kYR_EINVAL;

// Get the response. If we get an immediate close, then we must try protocol
// version 1 as the login message was likely rejected. We can switch
// to the old protocol but only if this is a redirector login.
//
   if (Link->RecvAll((char *)&LIHdr, sizeof(LIHdr)) < 0)
      if (Data.Mode & CmsLoginData::kYR_director) return kYR_ENETUNREACH;
         else return Emsg(Link, "login rejected");

// Receive and decode the response. We apparently have protocol version 2.
//
   if ((dataLen = static_cast<int>(ntohs(LIHdr.datalen))))
      {if (dataLen > (int)sizeof(WorkBuff)) 
          return Emsg(Link, "login reply too long");
       if (Link->RecvAll(WorkBuff, dataLen) < 0)
          return Emsg(Link, "login receive error");
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
      return (dataLen < (int)sizeof(kXR_unt32)+8
             ? Emsg(Link, "invalid error reply")
             : Emsg(Link, WorkBuff+sizeof(kXR_unt32)));

// Process normal reply
//
   if (LIHdr.rrCode != kYR_login
   || !Parser.Parse(&Data, WorkBuff, WorkBuff+dataLen))
      return Emsg(Link, "invalid login response");

// At this point we would validate the authentication token, generate a new
// one and send it using a kYR_data response. For now, skip it.
//

// All done
//
   return 0;
}

/******************************************************************************/
/* Private:                     s e n d D a t a                               */
/******************************************************************************/
  
int XrdCmsLogin::sendData(XrdLink *Link, CmsLoginData &Data)
{
   static const int xNum   = 16;

   int          iovcnt;
   char         Work[xNum*12];
   struct iovec Liov[xNum];
   CmsRRHdr     LIHdr={0, kYR_login, 0, 0};

// Pack the response (ignore the auth token for now)
//
   if (!(iovcnt=Parser.Pack(kYR_login,&Liov[1],&Liov[xNum],(char *)&Data,Work)))
      return Emsg(Link, "too much login reply data");

// Complete I/O vector
//
   LIHdr.datalen = Data.Size;
   Liov[0].iov_base = (char *)&LIHdr;
   Liov[0].iov_len  = sizeof(LIHdr);

// Send off the data
//
   Link->Send(Liov, iovcnt+1);

// Return success
//
   return 0;
}

/******************************************************************************/
/* Private:                    s e n d E r r o r                              */
/******************************************************************************/
  
int XrdCmsLogin::sendError(XrdLink *Link, const char *msg, int ecode)
{
   static const int xNum   = 2;

   struct iovec Liov[xNum];
   int mlen = strlen(msg)+1;
   CmsRdrResponse LEResp={{0, kYR_error, 0, 0}, htonl(ecode)};

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

// Return the error text
//
   return Emsg(Link, msg, ecode);
}
