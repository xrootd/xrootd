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

#include <cstdlib>
#include <cstring>

#include "XrdSsi/XrdSsiRequest.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiResponder.hh class provides a request processing object a way to
//! respond to a request. It is a companion (friend) class of XrdSsiRequest.
//! This class is only practically meaningful server-side.
//!
//! Any class that needs to post a response, release a request buffer or post
//! stream data to the request object should inherit this class and use its
//! methods to get access to the request object. Typically, a task class that
//! is created by XrdSsiService::Execute() to handle the request would inherit
//! this class so it can respond. The object that wants to post a response must
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
//!
//! Once Finished() is called you must call UnBindRequest() when you are
//! actually through referencing the request object. While that is true most
//! of the time, it may not be so when an async cancellation occurs (i.e.
//! you may need to defer release of the request object). Note that should
//! you delete this object before calling UnBindRequest(), the responder
//! object is forcibly unbound from the request.
//-----------------------------------------------------------------------------

class XrdSsiStream;

class XrdSsiResponder
{
public:
friend class XrdSsiRequest;
friend class XrdSsiRRAgent;

//-----------------------------------------------------------------------------
//! The maximum amount of metadata+data (i.e. the sum of two blen arguments in
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

        void    BindRequest(XrdSsiRequest   &rqstR);

//-----------------------------------------------------------------------------
//! Unbind this responder from the request object it is bound to. Upon return
//! ownership of the associated request object reverts back to the creator of
//! the object who is responsible for deleting or recycling the request object.
//! UnBindRequest() is also called when the responder object is deleted.
//!
//! @return true  Request successfully unbound.
//!         false UnBindRequest already called or called prior to Finish().
//-----------------------------------------------------------------------------

        bool    UnBindRequest();

protected:

//-----------------------------------------------------------------------------
//! Send an alert message to the request. This is a convenience method that
//! avoids race conditions with Finished() so it is safe to use in all cases.
//! This is a server-side call. The service is responsible for creating a
//! RespInfoMsg object containing the message and supplying a RecycleMsg() method.
//!
//! @param  aMsg  reference to the message to be sent.
//-----------------------------------------------------------------------------

        void   Alert(XrdSsiRespInfoMsg &aMsg);

//-----------------------------------------------------------------------------
//! Notify the responder that a request either completed or was canceled. This
//! allows the responder to release any resources given to the request object
//! (e.g. data response buffer or a stream). This method is invoked when
//! XrdSsiRequest::Finished() is called by the client.
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
//! Note: This method is called with the object's recursive mutex unlocked!
//!
//! @param  dlen  holds the length of the request after the call.
//!
//! @return =0    No request data available, dlen has been set to zero.
//! @return !0    Pointer to the buffer holding the request, dlen has the length
//-----------------------------------------------------------------------------

        char  *GetRequest(int &dlen);

//-----------------------------------------------------------------------------
//! Release the request buffer of the request bound to this object. This method
//! duplicates the protected method of the same name in XrdSsiRequest and exists
//! here for calling safety and consistency relative to the responder.
//-----------------------------------------------------------------------------

        void   ReleaseRequestBuffer();

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

       Status  SetMetadata(const char *buff, int blen);

//-----------------------------------------------------------------------------
//! Set an error response for a request.
//!
//! @param  eMsg  the message describing the error. The message is copied to
//!               private storage.
//! @param  eNum  the errno associated with the error.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

       Status  SetErrResponse(const char *eMsg, int eNum);

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

       Status  SetResponse(const char *buff, int blen);

//-----------------------------------------------------------------------------
//! Set a file containing data as the response.
//!
//! @param  fsize the size of the file containing the response.
//! @param  fdnum the file descriptor of the open file.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

       Status  SetResponse(long long fsize, int fdnum);

//-----------------------------------------------------------------------------
//! Set a stream object that is to provide data as the response.
//!
//! @param  strmP pointer to stream object that is to be used to supply response
//!               data. See XrdSsiStream for more details.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

       Status  SetResponse(XrdSsiStream *strmP);

//-----------------------------------------------------------------------------
//! This class is meant to be inherited by an object that will actually posts
//! responses.
//-----------------------------------------------------------------------------

               XrdSsiResponder();

//-----------------------------------------------------------------------------
//! Destructor is protected. You cannot use delete on a responder object, as it
//! is meant to be inherited by a class and not separately instantiated.
//-----------------------------------------------------------------------------

protected:

virtual       ~XrdSsiResponder();

private:

// The spMutex protects the reqP pointer. It is a hiearchical mutex in that it
// may be obtained prior to obtaining the mutex protecting the request without
// fear of a deadlock (the reverse is not possible). If reqP is zero then
// this responder is not bound to a request.
//
XrdSsiMutex    spMutex;
XrdSsiRequest *reqP;
long long      rsvd1; // Reserved fields for extensions with ABI compliance
long long      rsvd2;
long long      rsvd3;
};
#endif
