/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R e q u e s t . c c                       */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "XrdSsi/XrdSsiPacer.hh"
#include "XrdSsi/XrdSsiRespInfo.hh"
#include "XrdSsi/XrdSsiResponder.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiStream.hh"
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
namespace XrdSsi
{
XrdSsiMutex ubMutex(XrdSsiMutex::Recursive);
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSsiRequest::XrdSsiRequest(const char *reqid, uint16_t tmo)
                            : reqID(reqid), rrMutex(&XrdSsi::ubMutex),
                              theRespond(0), rsvd1(0), epNode(0),
                              detTTL(0), tOut(0), onClient(true), rsvd2(0) {}
  
/******************************************************************************/
/* Private:                     C l e a n U p                                */
/******************************************************************************/
  
void XrdSsiRequest::CleanUp()
{
// Reinitialize the this object in case it is being reused. While we don't
// really need to get a lock, we do so just in case there is a coding error.
//
   rrMutex->Lock();  
   Resp.Init();
   errInfo.Clr();
   epNode = 0;
   XrdSsiMutex *mP = rrMutex;
   rrMutex = &XrdSsi::ubMutex;
   mP->UnLock();
}

/******************************************************************************/
/* Private:                     C o p y D a t a                               */
/******************************************************************************/
  
bool XrdSsiRequest::CopyData(char *buff, int blen)
{
   bool last;

// Make sure the buffer length is valid
//
   if (blen <= 0)
      {errInfo.Set("Buffer length invalid", EINVAL);
       return false;
      }

// Check if we have any data here
//
   rrMutex->Lock();
   if (Resp.blen > 0)
      {if (Resp.blen > blen) last = false;
          else {blen = Resp.blen; last = true;}
       memcpy(buff, Resp.buff, blen);
       Resp.buff += blen; Resp.blen -= blen;
      } else {blen = 0; last = true;}
   rrMutex->UnLock();

// Invoke the callback
//
   ProcessResponseData(errInfo, buff, blen, last);
   return true;
}

/******************************************************************************/
/*                              F i n i s h e d                               */
/******************************************************************************/

bool XrdSsiRequest::Finished(bool cancel)
{
   XrdSsiResponder *respP;

// Obtain the responder
//
   rrMutex->Lock();
   respP = theRespond;
   theRespond = 0;
   rrMutex->UnLock();

// Tell any responder we are finished (we might not have one)
//
   if (respP) respP->Finished(*this, Resp, cancel);

// We are done. The object will be reiniialized when UnBindRequest() is
// called which will call UnBind() in this object. Since the timing is not
// known we can't touch anthing in this object at this point.
// Return false if there was no responder associated with this request.
//
   return respP != 0;
}

/******************************************************************************/
/*                           G e t E n d P o i n t                            */
/******************************************************************************/
  
std::string XrdSsiRequest::GetEndPoint()
{
   XrdSsiMutexMon lck(rrMutex);
   std::string epName(epNode ? epNode : "");
   return epName;
}
  
/******************************************************************************/
/*                           G e t M e t a d a t a                            */
/******************************************************************************/

const char *XrdSsiRequest::GetMetadata(int &dlen)
{
   XrdSsiMutexMon lck(rrMutex);
   if ((dlen = Resp.mdlen)) return Resp.mdata;
   return 0;
}
  
/******************************************************************************/
/*                       G e t R e s p o n s e D a t a                        */
/******************************************************************************/
  
void XrdSsiRequest::GetResponseData(char *buff, int  blen)
{
   XrdSsiMutexMon mHelper(rrMutex);

// If this is really a stream then just call the stream object to get the data.
// In the degenrate case, it's actually a data response, then we must copy it.
//
        if (Resp.rType == XrdSsiRespInfo::isStream)
           {if (Resp.strmP->SetBuff(errInfo, buff, blen)) return;}
   else if (Resp.rType == XrdSsiRespInfo::isData)
           {if (CopyData(buff, blen)) return;}
   else    errInfo.Set("Not a stream", ENODATA);

// If we got here then an error occured during the setup, reflect the error
// via the callback (in the future we will schedule a new thread).
//
   ProcessResponseData(errInfo, buff, -1, true);
}

/******************************************************************************/
/*                  R e l e a s e R e q u e s t B u f f e r                   */
/******************************************************************************/
  
void XrdSsiRequest::ReleaseRequestBuffer()
{
   XrdSsiMutexMon lck(rrMutex);
   RelRequestBuffer();
}

/******************************************************************************/
/*                   R e s t a r t D a t a R e s p o n s e                    */
/******************************************************************************/
  
XrdSsiRequest::RDR_Info XrdSsiRequest::RestartDataResponse
                                      (XrdSsiRequest::RDR_How  rhow,
                                       const char             *reqid
                                      )
{
   RDR_Info rInfo;

   XrdSsiPacer::Run(rInfo, rhow, reqid);
   return rInfo;
}
