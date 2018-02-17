/******************************************************************************/
/*                                                                            */
/*                    X r d S s i R e s p o n d e r . h h                     */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiResponder.hh"
#include "XrdSsi/XrdSsiRRAgent.hh"

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define SSI_VAL_RESPONSE spMutex.Lock();\
                         if (!reqP)\
                            {spMutex.UnLock(); return notActive;}\
                         reqP->rrMutex->Lock();\
                         if (reqP->theRespond != this)\
                            {reqP->rrMutex->UnLock(); spMutex.UnLock();\
                             return notActive;\
                            }\
                         if (reqP->Resp.rType)\
                            {reqP->rrMutex->UnLock(); spMutex.UnLock();\
                             return notPosted;\
                            }

#define SSI_XEQ_RESPONSE if (reqP->onClient)\
                            {XrdSsiRequest *rX = reqP;\
                             reqP->rrMutex->UnLock(); spMutex.UnLock();\
                             return (rX->ProcessResponse(rX->errInfo,rX->Resp)\
                                    ? wasPosted : notActive);\
                            } else {\
                             bool isOK = reqP->ProcessResponse(reqP->errInfo,\
                                                               reqP->Resp);\
                             reqP->rrMutex->UnLock(); spMutex.UnLock();\
                             return (isOK ? wasPosted : notActive);\
                            }
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class XeqUnBind : public XrdSsiResponder
{
public:

virtual void   Finished(      XrdSsiRequest  &rqstR,
                        const XrdSsiRespInfo &rInfo,
                              bool            cancel=false)
                       {XrdSsiRRAgent::Dispose(rqstR);}

               XeqUnBind() {}
              ~XeqUnBind() {}
};
}

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/

namespace
{
XeqUnBind   ForceUnBind;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSsiResponder::XrdSsiResponder()
                : spMutex(XrdSsiMutex::Recursive), reqP(0),
                  rsvd1(0), rsvd2(0), rsvd3(0)
                {}
  
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiResponder::~XrdSsiResponder()
{
// Lock ourselves (unlikely that we need to).
//
   spMutex.Lock();

// If we haven't taken a trip to Finished() then we need todo it here. The
// issue we have is that this object may be deleted before Finished() is called
// which is quite dicey. So, we defer it until Finished() is called. This is
// only an issue server-side as we don't control the finish process.
//
   if (reqP)
      {reqP->rrMutex->Lock();
       if (reqP->theRespond == this)
          {reqP->theRespond = &ForceUnBind;
           reqP->rrMutex->UnLock();
          } else if (reqP->theRespond == 0) // Finish() has been called
                    {reqP->rrMutex->UnLock();
                     reqP->Dispose();
                    }
      }

// All done
//
   spMutex.UnLock();
}

/******************************************************************************/
/*                                 A l e r t                                  */
/******************************************************************************/

void XrdSsiResponder::Alert(XrdSsiRespInfoMsg &aMsg)
{
   XrdSsiMutexMon lck(spMutex);

// If we have a request pointer then forward the alert. Otherwise, deep-six it
//
   if (reqP) reqP->Alert(aMsg);
      else aMsg.RecycleMsg(false);
}
  
/******************************************************************************/
/*                           B i n d R e q u e s t                            */
/******************************************************************************/

void XrdSsiResponder::BindRequest(XrdSsiRequest   &rqstR)
{
   XrdSsiMutexMon lck(spMutex);

// Get the request lock and link the request to this object and vice versa
//
   rqstR.rrMutex->Lock();
   reqP = &rqstR;
   rqstR.theRespond = this;

// Initialize the request object
//
   rqstR.Resp.Init();
   rqstR.errInfo.Clr();

// Notify the request that the bind comleted (this is only used on the
// server to allow a pending finish request to be sent to the responder).
//
   rqstR.BindDone();

// Unlock the request. The responder is unlocked upon return
//
   rqstR.rrMutex->UnLock();
}

/******************************************************************************/
/*                            G e t R e q u e s t                             */
/******************************************************************************/

char *XrdSsiResponder::GetRequest(int &dlen)
{
   XrdSsiMutexMon lck(spMutex);

// If we have a request pointer, forward the call. Otherwise return nothing.
//
   if (reqP) return reqP->GetRequest(dlen);
   dlen = 0;
   return 0;
}

/******************************************************************************/
/*                  R e l e a s e R e q u e s t B u f f e r                   */
/******************************************************************************/
  
void XrdSsiResponder::ReleaseRequestBuffer()
{
   XrdSsiMutexMon lck(spMutex);

// If we have a request, forward the call (note we need to also get the
// the request lock to properly serialize this call).
//
   if (reqP) reqP->ReleaseRequestBuffer();
}

/******************************************************************************/
/*                           S e t M e t a d a t a                            */
/******************************************************************************/
  
XrdSsiResponder::Status XrdSsiResponder::SetMetadata(const char *buff, int blen)
{
   XrdSsiMutexMon lck(spMutex);

// If we don't have a request or the args are invalid, return an error.
//
   if (!reqP || blen < 0 || blen > MaxMetaDataSZ) return notPosted;

// Post the metadata
//
   reqP->rrMutex->Lock();
   reqP->Resp.mdata = buff;
   reqP->Resp.mdlen = blen;
   reqP->rrMutex->UnLock();
   return wasPosted;
}

/******************************************************************************/
/*                        S e t E r r R e s p o n s e                         */
/******************************************************************************/

XrdSsiResponder::Status XrdSsiResponder::SetErrResponse(const char *eMsg,
                                                              int   eNum)
{

// Validate object for a response
//
   SSI_VAL_RESPONSE;

// Set the error response (we have the right locks now)
//
   reqP->errInfo.Set(eMsg, eNum);
   reqP->Resp.eMsg  = reqP->errInfo.Get(reqP->Resp.eNum).c_str();
   reqP->Resp.rType = XrdSsiRespInfo::isError;

// Complete the response
//
   SSI_XEQ_RESPONSE;
}

/******************************************************************************/
/*                           S e t R e s p o n s e                            */
/******************************************************************************/

XrdSsiResponder::Status XrdSsiResponder::SetResponse(const char *buff, int blen)
{

// Validate object for a response
//
   SSI_VAL_RESPONSE;

// Set the response (we have the right locks now)
//
   reqP->Resp.buff  = buff;
   reqP->Resp.blen  = blen;
   reqP->Resp.rType = XrdSsiRespInfo::isData;

// Complete the response
//
   SSI_XEQ_RESPONSE;
}
  
/******************************************************************************/

XrdSsiResponder::Status XrdSsiResponder::SetResponse(long long fsize, int fdnum)
{

// Validate object for a response
//
   SSI_VAL_RESPONSE;

// Set the response (we have the right locks now)
//
   reqP->Resp.fdnum = fdnum;
   reqP->Resp.fsize = fsize;
   reqP->Resp.rType = XrdSsiRespInfo::isFile;

// Complete the response
//
   SSI_XEQ_RESPONSE;
}

/******************************************************************************/

XrdSsiResponder::Status XrdSsiResponder::SetResponse(XrdSsiStream *strmP)
{

// Validate object for a response
//
   SSI_VAL_RESPONSE;

// Set the response (we have the right locks now)
//
   reqP->Resp.eNum  = 0;
   reqP->Resp.strmP = strmP;
   reqP->Resp.rType = XrdSsiRespInfo::isStream;

// Complete the response
//
   SSI_XEQ_RESPONSE;
}

/******************************************************************************/
/*                         U n B i n d R e q u e s t                          */
/******************************************************************************/
  
bool XrdSsiResponder::UnBindRequest()
{
   XrdSsiMutexMon spMon(spMutex);

// If we are not bound to a request, indicate an error.
//
   if (!reqP) return false;

// Lock the request and if Finished() was not called, indicate an error.
//
   reqP->rrMutex->Lock();
   if (reqP->theRespond != 0)
      {reqP->rrMutex->UnLock();
       return false;
      }

// We have a request pointer and Finish() was called; so do the actual unbind.
//
   reqP->rrMutex->UnLock();
   reqP->Dispose();
   reqP = 0;
   return true;
}
