#ifndef __XRDSSIREQUEST_HH__
#define __XRDSSIREQUEST_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R e q u e s t . h h                       */
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

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiRespInfo.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiRequest class describes a client request and is used to effect a
//! response to the request via a companion object described by XrdSsiResponder.
//! Client-Side: Use this object to encapsulate your request and hand it off
//!              to XrdSsiService::Execute() either use GetResponseData() or
//!              the actual response structure to get the response data once the
//!              ProcessResponse() callback is invoked.
//!
//! Server-side: XrdSsiService::ProcessRequest() is called with this object.
//!              Use the XrdSsiResponder object to post a response.
//!
//! In either case, the client must invoke XrdSsiRequest::Finished() after the
//! client-server exchange is complete in order to revert ownership of this
//! object to the object's creator to allow it to be deleted or reused.
//!
//! This is an abstract class and several methods need to be implemented:
//!
//! Alert()               Optional, allows receiving of server alerts.
//! GetRequest()          Mandatory to supply the buffer holding the request
//!                       along with its length.
//! RelRequestBuffer()    Optional, allows memory optimization.
//! ProcessResponse()     Initial response: Mandatory
//! ProcessResponseData() Data    response: Mandatory only if response data is
//!                                         asynchronously received.
//!
//! All callbacks are invoked with no locks outstanding unless otherwise noted.
//-----------------------------------------------------------------------------

class XrdSsiResponder;

class XrdSsiRequest
{
public:
friend class XrdSsiResponder;
friend class XrdSsiRRAgent;

//-----------------------------------------------------------------------------
//! Indicate that request processing has been finished. This method calls
//! XrdSsiResponder::Finished() on the associated responder object.
//!
//! Note: This method locks the object's recursive mutex.
//!
//! @param  cancel False -> the request/response sequence completed normally.
//!                True  -> the request/response sequence aborted because of an
//!                         error or the client cancelled the request.
//!
//! @return true   Finish accepted. Request object may be reclaimed.
//! @return false  Finish cannot be accepted because this request object is
//!                not bound to a responder. This indicates a logic error.
//-----------------------------------------------------------------------------

        bool    Finished(bool cancel=false);

//-----------------------------------------------------------------------------
//! Obtain the detached request time to live value. If the value is non-zero,
//! the request is detached. Otherwise, it is an attached request and requires a
//! live TCP connection during it execution.
//!
//! @return The detached time to live value in seconds.
//-----------------------------------------------------------------------------

inline uint32_t GetDetachTTL() {return detTTL;}

//-----------------------------------------------------------------------------
//! Obtain the endpoint host name.
//!
//! @return A string containing the endpoint host name. If a null string is
//!         returned, the endpoint has not yet been determined. Generally, the
//!         endpoint is available on the first callback to this object.
//-----------------------------------------------------------------------------

std::string     GetEndPoint();

//-----------------------------------------------------------------------------
//! Obtain the metadata associated with a response.
//!
//!
//! Note: This method locks the object's recursive mutex.
//!
//! @param  dlen  holds the length of the metadata after the call.
//!
//! @return =0    No metadata available, dlen has been set to zero.
//! @return !0    Pointer to the buffer holding the metadata, dlen has the length
//-----------------------------------------------------------------------------

const char     *GetMetadata(int &dlen);

//-----------------------------------------------------------------------------
//! Obtain the request data sent by a client.
//!
//! This method is duplicated in XrdSsiResponder to allow calling consistency.
//!
//! @param  dlen  holds the length of the request after the call.
//!
//! @return =0    No request data available, dlen has been set to zero.
//! @return !0    Pointer to the buffer holding the request, dlen has the length
//-----------------------------------------------------------------------------

virtual char   *GetRequest(int &dlen) = 0;

//-----------------------------------------------------------------------------
//! Get the request ID established at object creation time.
//!
//! @return Pointer to the request ID or nil if there is none.
//-----------------------------------------------------------------------------

inline
const   char   *GetRequestID() {return reqID;}

//-----------------------------------------------------------------------------
//! Asynchronously obtain response data. This is a helper method that allows a
//! client to deal with a passive stream response. This method also handles
//! data response, albeit inefficiently by copying the data response. However,
//! this allows for uniform response processing regardless of response type.
//!
//! @param  buff  pointer to the buffer to receive the data. The buffer must
//!               remain valid until ProcessResponseData() is called.
//! @param  blen  the length of the buffer (i.e. maximum that can be returned).
//-----------------------------------------------------------------------------

        void    GetResponseData(char *buff, int  blen);

//-----------------------------------------------------------------------------
//! Get timeout for initiating the request.
//!
//! @return The timeout value.
//-----------------------------------------------------------------------------

        uint16_t GetTimeOut() {return tOut;}

//-----------------------------------------------------------------------------
//! Notify request that a response is ready to be processed. This method must
//! be supplied by the request object's implementation.
//!
//! @param  eInfo Error information. You can check if an error occurred using
//!               eInfo.hasError() or eInfo.isOK().
//! @param  rInfo Raw response information.
//!
//! @return true  Response processed.
//! @return false Response could not be processed, the request is not active.
//-----------------------------------------------------------------------------

virtual bool    ProcessResponse(const XrdSsiErrInfo  &eInfo,
                                const XrdSsiRespInfo &rInfo)=0;

//-----------------------------------------------------------------------------
//! Handle incoming async stream data or error. This method is called by a
//! stream object after a successful GetResponseData() or an asynchronous
//! stream SetBuff() call.
//!
//! @param  eInfo Error information. You can check if an error occurred using
//!               eInfo.hasError() or eInfo.isOK().
//! @param  buff  Pointer to the buffer given to XrdSsiStream::SetBuff().
//! @param  blen  The number of bytes in buff or an error indication if blen < 0.
//! @param  last  true  This is the last stream segment, no more data remains.
//! @param        false More data may remain in the stream.
//-----------------------------------------------------------------------------

virtual void ProcessResponseData(const XrdSsiErrInfo  &eInfo, char *buff,
                                 int blen, bool last) {}

//-----------------------------------------------------------------------------
//! Release the request buffer of the request bound to this object. This method
//! duplicates the protected method RelRequestBuffer() and exists here for
//! calling safety and consistency relative to the responder.
//-----------------------------------------------------------------------------

        void   ReleaseRequestBuffer();

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param reqid   Pointer to a request ID that can be used to group requests.
//!                See ProcessResponseData() and RestartDataReponse(). If reqid
//!                is nil then held responses are placed in the global queue.
//!                The pointer must be valid for the life of this object.
//!
//! @param tmo     The request initiation timeout value 0 equals default).
//-----------------------------------------------------------------------------

                XrdSsiRequest(const char *reqid=0, uint16_t tmo=0);

protected:

//-----------------------------------------------------------------------------
//! @brief Send or receive a server generated alert.
//!
//! The Alert() method is used server-side to send one or more alerts before a
//! response is posted (alerts afterwards are ignored). To avoid race conditions,
//! server-side alerts should be sent via the Responder's Alert() method.
//! Clients must implement this method in order to receive alerts.
//!
//! @param  aMsg   Reference to the message object containing the alert message.
//!                Non-positive alert lengths cause the alert call to be
//!                ignored. You should call the message RecycleMsg() method
//!                once you have consumed the message to release its resources.
//-----------------------------------------------------------------------------

virtual void    Alert(XrdSsiRespInfoMsg &aMsg) {aMsg.RecycleMsg(false);}

//-----------------------------------------------------------------------------
//! Release the request buffer. Use this method to optimize storage use; this
//! is especially relevant for long-running requests. If the request buffer
//! has been consumed and is no longer needed, early return of the buffer will
//! minimize memory usage. This method is also invoked via XrdSsiResponder.
//!
//!
//! Note: This method is called with the object's recursive mutex locked when
//!       it is invoked via XrdSsiResponder's ReleaseRequestBuffer().
//-----------------------------------------------------------------------------

virtual void    RelRequestBuffer() {}

//-----------------------------------------------------------------------------
//! @brief Set the detached request time to live value.
//!
//! By default, requests are executed in the foreground (i.e. during its
//! execution, if the TCP connection drops, the request is automatically
//! cancelled. When a non-zero time to live is set, the request is executed in
//! the background (i.e. detached) and no persistent TCP connection is required.
//! You must use the XrdSsiService::Attach() method to foreground such a
//! request within the number of seconds specified for dttl or the request is
//! automatically cancelled. The value must be set before passing the request
//! to XrdSsiService::ProcessRequest(). Once the request is started, a request
//! handle is returned which can be passed to XrdSsiService::Attach().
//!
//! @param  detttl The detach time to live value.
//-----------------------------------------------------------------------------

inline void     SetDetachTTL(uint32_t dttl) {detTTL = dttl;}

//-----------------------------------------------------------------------------
//! Set request retry notification. If a non-default value is desired, it must
//! be set prior to calling XrdSsiService::ProcessRequest(). This is a one-time
//! request and retry mode is turned off in the request object afterwards.
//!
//! @param onoff   True to turn retry on and false to turn it off.
//-----------------------------------------------------------------------------

       void     SetRetry(bool onoff);

//-----------------------------------------------------------------------------
//! Set timeout for initiating the request. If a non-default value is desired,
//! it must be set prior to calling XrdSsiService::ProcessRequest().
//!
//! @param tmo     The timeout value.
//-----------------------------------------------------------------------------

       void     SetTimeOut(uint16_t tmo) {tOut = tmo;}

//-----------------------------------------------------------------------------
//! Destructor. This object can only be deleted by the object creator. Once the
//! object is passed to XrdSsiService::ProcessRequest() it may only be deleted
//! after Finished() is called to allow the service to reclaim any resources
//! allocated for the request object.
//-----------------------------------------------------------------------------

virtual        ~XrdSsiRequest() {}

private:
virtual void     BindDone() {}
        void     CleanUp();
        bool     CopyData(char *buff, int blen);
virtual void     Dispose() {}

const char      *reqID;
XrdSsiMutex     *rrMutex;
XrdSsiResponder *theRespond; // Set via XrdSsiResponder::BindRequest()
XrdSsiRespInfo   Resp;       // Set via XrdSsiResponder::SetResponse()
XrdSsiErrInfo    errInfo;
long long        rsvd1;
const char      *epNode;
uint32_t         detTTL;
uint16_t         tOut;
bool             onClient;
char             flags;
static const int isaRetry = 1;
};
#endif
