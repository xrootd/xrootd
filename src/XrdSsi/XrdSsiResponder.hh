#ifndef __XRDSSIRESPONDER_HH__
#define __XRDSSIRESPONDER_HH__
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

#include <stdlib.h>
#include <string.h>

#include "XrdSsi/XrdSsiRequest.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiResponder.hh class provides a request processing object a way to
//! respond to a request. It is a companion (friend) class of XrdSsiRequest.
//! This class is only practically meaningful server-side.
//!
//! Any class that needs to post a response, release a request buffer or post
//! stream data to the request object should inherit this class and use its
//! methods to get access to the request object. Typically, a task class that
//! is created by XrdSsiService::Execute() to handle te request would inherit
//! this class so it can respond. The object that wantsto post a response must
//! first call BindRequest() to establish the request-responder association.
//!
//! When the XrdSsiResponder::SetResponse() method is called to post a response
//! the request object's ProcessResponse() method is called. Ownership of the
//! request object does not revert back to the object's creator until the
//! XrdSsiRequest::Finished() method returns. That method first calls
//! XrdSsiResponder::Finished() to break the request-responder association and
//! reclaim any response data buffer or stream resource before it gives up
//! control of the request object. This means you must provide an implementation
//! To the Finished() method defined here.
//-----------------------------------------------------------------------------

#define SSI_VAL_RESPONSE(rX) rrMutex->Lock();\
                             XrdSsiRequest *rX = reqP;\
                             if (!rX)\
                                {rrMutex->UnLock(); return notActive;}\
                             reqP = 0;\
                             if (rX->theRespond != this)\
                                {rrMutex->UnLock(); return notActive;}

#define SSI_XEQ_RESPONSE(rX) rrMutex->UnLock();\
                             return (rX->ProcessResponse(rX->errInfo,rX->Resp)\
                                    ? wasPosted : notActive)

class XrdSsiStream;

class XrdSsiResponder
{
public:
friend class XrdSsiRequest;

//-----------------------------------------------------------------------------
//! The maximum amount of metedata+data (i.e. the sum of two blen arguments in
//! SetMetadata() and and SetResponse(const char *buff, int blen), respectively)
//! that may be directly sent to the client without the SSI framework converting
//! the data buffer response into a stream response.
//-----------------------------------------------------------------------------

static const int MaxDirectXfr = 2097152; //< Max (metadata+data) direct xfr

//-----------------------------------------------------------------------------
//! Take ownership of a request object by binding the request object to a
//! responder object. This method must be called by the responder before
//! posting any responses.
//!
//! @param  rqstR reference to the request object.
//-----------------------------------------------------------------------------

inline  void    BindRequest(XrdSsiRequest   &rqstR)
                           {XrdSsiMutexMon(rqstR.rrMutex);
                            rqstR.theRespond = this;
                            reqP    = &rqstR;
                            rrMutex = rqstR.rrMutex;
                            rqstR.Resp.Init();
                            rqstR.errInfo.Clr();
                            rqstR.BindDone();
                           }

protected:

//-----------------------------------------------------------------------------
//! Send an alert message to the request. This is a convenience method that
//! avoids race condistions with Finished() so it is safe to use in all cases.
//! This is a server-side call. The service is responsible for creating a
//! RespInfoMsg object containing the message and supplying a RecycleMsg() method.
//!
//! @param  aMsg  reference to the message to be sent.
//-----------------------------------------------------------------------------

inline  void   Alert(XrdSsiRespInfoMsg &aMsg)
                    {XrdSsiMutexMon(rrMutex);
                     if (reqP) reqP->Alert(aMsg);
                        else aMsg.RecycleMsg(false);
                    }

//-----------------------------------------------------------------------------
//! Notify the responder that a request either completed or was canceled. This
//! allows the responder to release any resources given to the request object
//! (e.g. data response buffer or a stream). Upon return the object is owned by
//! the request object's creator who is responsible for releaasing or recycling
//! the object. This method is automatically invoked by XrdSsiRequest::Finish().
//!
//! @param  rqstP  reference to the object describing the request.
//! @param  rInfo  reference to the object describing the response.
//! @param  cancel False -> the request/response interaction completed.
//!                True  -> the request/response interaction aborted because
//!                         of an error or the client requested that the
//!                         request be canceled.
//-----------------------------------------------------------------------------

virtual void   Finished(      XrdSsiRequest  &rqstR,
                        const XrdSsiRespInfo &rInfo,
                              bool            cancel=false) = 0;

//-----------------------------------------------------------------------------
//! Obtain the request data sent by a client.
//!
//! Note: This method may be called with the object's recursive mutex unlocked!
//!
//! @param  dlen  holds the length of the request after the call.
//!
//! @return =0    No request data available, dlen has been set to zero.
//! @return !0    Pointer to the buffer holding the request, dlen has the length
//-----------------------------------------------------------------------------

inline  char  *GetRequest(int &dlen) {XrdSsiMutexMon(rrMutex);
                                      if (reqP) return reqP->GetRequest(dlen);
                                      dlen = 0; return 0;
                                     }

//-----------------------------------------------------------------------------
//! Release the request buffer of the request bound to this object. This method
//! duplicates the protected method of the same name in XrdSsiRequest and exists
//! here for calling safety and consistency relative to the responder.
//-----------------------------------------------------------------------------

inline  void   ReleaseRequestBuffer() {XrdSsiMutexMon(rrMutex);
                                       if (reqP) reqP->RelRequestBuffer();
                                      }

//-----------------------------------------------------------------------------
//! The following enums are returned by SetMetadata() and SetResponse() to
//!  indicate ending status.
//-----------------------------------------------------------------------------

enum Status {wasPosted=0, //!< Success: The response was successfully posted
             notPosted,   //!< Failure: A request was not bound to this object
                          //!<          or a response has already been posted
                          //!<          or the metadata length was invalid
             notActive    //!< Failure: Request is no longer active
            };

//-----------------------------------------------------------------------------
//! Set a pointer to metadata to be sent out-of-band ahead of the response.
//!
//! @param  buff  pointer to a buffer holding the metadata. The buffer must
//!               remain valid until XrdSsiResponder::Finished() is called.
//! @param  blen  the length of the metadata in buff that is to be sent. It must
//!               in the range 0 <= blen <= MaxMetaDataSZ.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

static const int MaxMetaDataSZ = 2097152; //!< 2MB metadata limit

inline Status  SetMetadata(const char *buff, int blen)
                          {XrdSsiMutexMon(rrMutex);
                           if (!reqP || blen < 0 || blen > MaxMetaDataSZ)
                              return notPosted;
                           reqP->Resp.mdata = buff; reqP->Resp.mdlen = blen;
                           return wasPosted;
                          }

//-----------------------------------------------------------------------------
//! Set an error response for a request.
//!
//! @param  eMsg  the message describing the error. The message is copied to
//!               private storage.
//! @param  eNum  the errno associated with the error.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

inline Status  SetErrResponse(const char *eMsg, int eNum)
                          {SSI_VAL_RESPONSE(rP);
                           rP->errInfo.Set(eMsg, eNum);
                           rP->Resp.eMsg  = rP->errInfo.Get(rP->Resp.eNum).c_str();
                           rP->Resp.rType = XrdSsiRespInfo::isError;
                           SSI_XEQ_RESPONSE(rP);
                          }

//-----------------------------------------------------------------------------
//! Set a nil response for a request (used for sending only metadata).
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

inline Status  SetNilResponse() {return SetResponse((const char *)0,0);}

//-----------------------------------------------------------------------------
//! Set a memory buffer containing data as the request response.
//!
//! @param  buff  pointer to a buffer holding the response. The buffer must
//!               remain valid until XrdSsiResponder::Finished() is called.
//! @param  blen  the length of the response in buff that is to be sent.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

inline Status  SetResponse(const char *buff, int blen)
                          {SSI_VAL_RESPONSE(rP);
                           rP->Resp.buff  = buff; rP->Resp.blen  = blen;
                           rP->Resp.rType = XrdSsiRespInfo::isData;
                           SSI_XEQ_RESPONSE(rP);
                          }

//-----------------------------------------------------------------------------
//! Set a file containing data as the response.
//!
//! @param  fsize the size of the file containing the response.
//! @param  fdnum the file descriptor of the open file.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

inline Status  SetResponse(long long fsize, int fdnum)
                          {SSI_VAL_RESPONSE(rP);
                           rP->Resp.fdnum = fdnum;
                           rP->Resp.fsize = fsize;
                           rP->Resp.rType = XrdSsiRespInfo::isFile;
                           SSI_XEQ_RESPONSE(rP);
                          }

//-----------------------------------------------------------------------------
//! Set a stream object that is to provide data as the response.
//!
//! @param  strmP pointer to stream object that is to be used to supply response
//!               data. See XrdSsiStream for more details.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

inline Status  SetResponse(XrdSsiStream *strmP)
                          {SSI_VAL_RESPONSE(rP);
                           rP->Resp.eNum  = 0;
                           rP->Resp.strmP = strmP;
                           rP->Resp.rType = XrdSsiRespInfo::isStream;
                           SSI_XEQ_RESPONSE(rP);
                          }


//-----------------------------------------------------------------------------
//! This class is meant to be inherited by an object that will actually posts
//! responses.
//-----------------------------------------------------------------------------

               XrdSsiResponder() : reqP(0) {}

//-----------------------------------------------------------------------------
//! Destructor is protected. You cannot use delete on a responder object, as it
//! is meant to be inherited by a class and not separately instantiated.
//-----------------------------------------------------------------------------

protected:

virtual       ~XrdSsiResponder() {}

private:
inline void    Unbind() {rrMutex->Lock(); reqP = 0; rrMutex->UnLock();}

XrdSsiMutex   *rrMutex;
XrdSsiRequest *reqP;
};
#endif
