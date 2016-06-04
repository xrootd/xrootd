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

#include <stdlib.h>
#include <string.h>

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiRespInfo.hh"
#include "XrdSsi/XrdSsiSession.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiRequest class describes a client request and is used to effect a
//! response to the request via a companion object described by XrdSsiResponder.
//! Client-Side: Use this object to encapsulate your request and hand it off
//!              to XrdSsiSession::ProcessRequest() either use GetResponseData()
//!              or the actual response tructure to get the response data.
//!
//! Server-side: XrdSsiSession::ProcessRequest() is called with this object.
//!              Use the XrdSsiResponder object to post a response.
//!
//! In either case, once the the XrdSsiRequest::Finished() must be invoked
//! after the client-server exchange is complete in order to revert ownership
//! of this object to the object's creator. After which, the object me be
//! deleted or reused.
//!
//! This is an abstract class and several methods need to be implemented:
//!
//! GetRequest()          Manndatory to supply the buffer holding the request
//!                       along with its length.
//! RelRequestBuffer()    Optional, allows memory optimization.
//! ProcessResponse()     Initial response: Mandatary
//! ProcessResponseData() Data    response: Mandatory ony if response data is
//!                                         asynchronously received.
//-----------------------------------------------------------------------------

class XrdSsiPacer;
class XrdSsiResource;
class XrdSsiResponder;
class XrdSsiService;
class XrdSsiStream;

class XrdSsiRequest
{
public:
friend class XrdSsiResponder;
friend class XrdSsiSSRun;
friend class XrdSsiTaskReal;

//-----------------------------------------------------------------------------
//! The following object is used to relay error information from any method
//! dealing with the request object that returns a failure. Also, any error
//! response sent by a server will be recorded in the eInfo object as well.
//-----------------------------------------------------------------------------

XrdSsiErrInfo   eInfo;

//-----------------------------------------------------------------------------
//! Indicate that request processing has been finished. This method calls
//! XrdSsiSession::Complete() on the session object associated with Request().
//!
//! Note: This method locks the object's recursive mutex.
//!
//! @param  cancel False -> the request/response sequence completed normally.
//!                True  -> the request/response sequence aborted because of an
//!                         error or the client cancelled the request.
//!
//! @return true   Complete accepted. Request object may be reclaimed.
//! @return false  Complete cannot be accepted because this request object is
//!                not bound to a session. This indicates a logic error.
//-----------------------------------------------------------------------------

        bool    Finished(bool cancel=false);

//-----------------------------------------------------------------------------
//! Obtain the metadata associated with a response.
//!
//! Note: This method locks the object's recursive mutex.
//!
//! @param  dlen  holds the length of the metadata after the call.
//!
//! @return =0    No metadata available, dlen has been set to zero.
//! @return !0    Pointer to the buffer holding the metadata, dlen has the length
//-----------------------------------------------------------------------------

inline
const char     *GetMetadata(int &dlen)
                           {XrdSsiMutexMon(reqMutex);
                            if ((dlen = Resp.mdlen)) return Resp.mdata;
                            return 0;
                           }

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

virtual char   *GetRequest(int &dlen) = 0;

//-----------------------------------------------------------------------------
//! Obtain the responder associated with this request. This member is set by the
//! responder and needs serialization.
//!
//! Note: This method locks the object's recursive mutex.
//!
//! @return !0    - pointer to the responder object.
//! @retuen =0    - no alternate responder associated with this request.
//-----------------------------------------------------------------------------
inline
XrdSsiResponder*GetResponder() {XrdSsiMutexMon(reqMutex); return theRespond;}

//-----------------------------------------------------------------------------
//! Asynchronously obtain response data. This is a helper method that allows a
//! client to deal with a passive stream response. This method also handles
//! data response, albeit ineffeciently by copying the data response. However,
//! this allows for uniform response processing regardless of response type.
//! See the other from of GetResponseData() for a possible better approach.
//!
//! @param  buff  pointer to the buffer to receive the data. The buffer must
//!               remain valid until the ProcessResponse() is called.
//! @param  blen  the length of the buffer (i.e. maximum that can be returned).
//!
//! @return true  A data return has been successfully scheduled.
//! @return false The stream could not be scheduled; eInfo holds the reason.
//-----------------------------------------------------------------------------

        bool    GetResponseData(char *buff, int  blen);

//-----------------------------------------------------------------------------
//! Obtain the session associated with this request. This member is set by the
//! responder and needs seririalization.
//!
//! Note: This method locks the object's recursive mutex.
//!
//! @return !0    - pointer to the session object.
//! @retuen =0    - no session associated with this request.
//-----------------------------------------------------------------------------
inline
XrdSsiSession  *GetSession() {XrdSsiMutexMon(reqMutex); return theSession;}

//-----------------------------------------------------------------------------
//! Notify request that a response is ready to be processed. This method must
//! be supplied by the request object's implementation.
//!
//! Note: This method is called with the object's recursive mutex locked.
//!
//! @param  rInfo Raw response information.
//! @param  isOK  True:  Normal response.
//!               False: Error  response, the eInfo object holds information.
//!
//! @return true  Response processed.
//! @return false Response could not be processed, the request is not active.
//-----------------------------------------------------------------------------

virtual bool    ProcessResponse(const XrdSsiRespInfo &rInfo, bool isOK=true)=0;

//-----------------------------------------------------------------------------
//! Handle incomming async stream data or error. This method is called by a
//! stream object after a successful GetResponseData() or an asynchronous
//! stream SetBuff() call.
//!
//! Note: This method is called with the object's recursive mutex locked.
//!
//! @param buff  Pointer to the buffer given to XrdSsiStream::SetBuff().
//! @param blen  The number of bytes in buff or an error indication if blen < 0.
//! @param last  true  This is the last stream segment, no more data remains.
//! @param       false More data may remain in the stream.
//! @return      One of the enum PRD_Xeq:
//!              PRD_Normal  - Processing completeted normally, continue.
//!              PRD_Hold    - Processing could not be done now, place request
//!                            in the global FIFO hold queue and resume when
//!                            RestartDataResponse() is called.
//!              PRD_HoldLcl - Processing could not be done now, place request
//!                            in the request ID FIFO local queue and resume when
//!                            RestartDataResponse() is called with the ID that
//!                            passed to the this request object constructor.
//-----------------------------------------------------------------------------

enum PRD_Xeq {PRD_Normal = 0, PRD_Hold = 1, PRD_HoldLcl = 2};

virtual PRD_Xeq ProcessResponseData(char *buff, int blen, bool last)
                {return PRD_Normal;}

//-----------------------------------------------------------------------------
//! Restart a ProcessResponseData() call for a request that was previosly held
//! (see return enums on ProcessResponseData method). This is a client-side
//! only call and is ignored server-side. When a data response is restarted,
//! ProcessResponseData() is called again when the same parameters as existed
//! when the call resulted in a hold action.
//!
//! @param rnum  The number of data responses to restart, as follows:
//!              rnum > 0 Restart up to the specified number.
//!              rnum = 0 Only return the number in the queue, don't restart any.
//!              rnum < 0 Restart all held responses.
//! @param reqid Points to the requestID associated with a hold queue. When not
//!              specified, then the global queue is used to restart responses.
//!
//! @return      The number of responses scheduled for restarting.
//-----------------------------------------------------------------------------

static int      RestartDataResponse(int rnum, const char *reqid=0);

//-----------------------------------------------------------------------------
//! Run this request using a single session (variant 1).
//!
//! @param srvc  Reference to the service object to be used.
//! @param rsrc  Reference to the resource description for the request.
//!              Members in this object are copied so the resource object may
//!              be deleted upon return.
//! @param tmo   the maximum number seconds the operation may last before
//!              it is considered to fail. A zero value uses the default.
//!
//! @return      The results of this call are reflected to the request via
//!              it's ProcessResponse() callback method.
//-----------------------------------------------------------------------------

virtual void    SSRun(XrdSsiService  &srvc,
                      XrdSsiResource &rsrc,
                      unsigned short  tmo=0);

//-----------------------------------------------------------------------------
//! Run this request using a single session (variant 2). Use the resource name
//! and optional resource user with all other resource values defaulted.
//!
//! @param srvc  Reference to the service object to be used.
//! @param rname Pointer to the resource name. It is copied.
//! @param ruser Pointer to the resource user. It is copied.
//! @param tmo   the maximum number seconds the operation may last before
//!              it is considered to fail. A zero value uses the default.
//!
//! @return      The results of this call are reflected to the request via
//!              it's ProcessResponse() callback method.
//-----------------------------------------------------------------------------

virtual void    SSRun(XrdSsiService  &srvc,
                      const char     *rname,
                      const char     *ruser=0,
                      unsigned short  tmo=0);

//-----------------------------------------------------------------------------
//! A handy pointer to allow for chaining these objects. It is initialized to 0.
//! It should only be touched by any object that is the current owner of this
//! object (e.g. the XrdSsiSession object after its ProcessRequest() is called).
//-----------------------------------------------------------------------------

XrdSsiRequest  *nextRequest;

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param reqid   Pointer to a request ID that can be used to group requests.
//!                See ProcessResponseData() and RestartDataReponse(). If reqid
//!                is nil then held responses are placed in the global queue.
//!                The pointer must be valid for the life of this object.
//-----------------------------------------------------------------------------

                XrdSsiRequest(const char *reqid=0)
                             : nextRequest(0),
                               reqMutex(XrdSsiMutex::Recursive), reqID(reqid),
                               theSession(0), theRespond(0), thePacer(0) {}

protected:
//-----------------------------------------------------------------------------
//! Notify the underlying object that the request was bound to a session. This
//! method is meant for server-side internal use only.
//-----------------------------------------------------------------------------

virtual void    BindDone(XrdSsiSession *sessP) {}

//-----------------------------------------------------------------------------
//! Release the request buffer. Use this method to optimize storage use; this
//! is especially relevant for long-running requests. If the request buffer
//! has been consumed and is no longer needed, early return of the buffer will
//! minimize memory usage. This method is only invoked via XrdSsiResponder.
//!
//!
//! Note: This method is called with the object's recursive mutex locked when
//!       it is invoked via XrdSsiResponder's ReleaseRequestBuffer() which is
//!       the only proper way of invoking this method.
//-----------------------------------------------------------------------------

virtual void    RelRequestBuffer() {}

//-----------------------------------------------------------------------------
//! Destructor. This object can only be deleted by the object creator. When the
//! object is passed and accepted by XrdSsiSession::ProcessRequest() it may
//! only be deleted after Finished() is called to allow the session object to
//! reclaim any resources granted to the request object.
//-----------------------------------------------------------------------------

virtual        ~XrdSsiRequest() {}

//-----------------------------------------------------------------------------
//! Get a pointer to the RespInfo structure. This is meant to be used by
//! classes that inherit this class to simplify response handling.
//!
//! @return Pointer to the RespInfo structure.
//-----------------------------------------------------------------------------
inline
const XrdSsiRespInfo *RespP() {return &Resp;}

//-----------------------------------------------------------------------------
//! The following mutex is used to serialize acccess to the request object.
//! It can also be used to serialize access to the underlying object.
//-----------------------------------------------------------------------------

XrdSsiMutex      reqMutex;
const char      *reqID;

private:
        bool     CopyData(char *buff, int blen);

XrdSsiSession   *theSession; // Set via XrdSsiResponder::BindRequest()
XrdSsiResponder *theRespond; // Set via XrdSsiResponder::BindRequest()
XrdSsiRespInfo   Resp;       // Set via XrdSsiResponder::SetResponse()
XrdSsiPacer     *thePacer;
};

//-----------------------------------------------------------------------------
//! We define the GetResponseData() helper method here as we need it to be
//! available in all compilation units and it depends on XrdSsiStream.
//-----------------------------------------------------------------------------

#include "XrdSsi/XrdSsiStream.hh"

inline  bool    XrdSsiRequest::GetResponseData(char *buff, int  blen)
                      {XrdSsiMutexMon(reqMutex);
                       if (Resp.rType == XrdSsiRespInfo::isStream)
                          return Resp.strmP->SetBuff(this, buff, blen);
                       if (Resp.rType == XrdSsiRespInfo::isData)
                          return CopyData(buff, blen);
                       eInfo.Set(0, ENODATA); return false;
                      }
#endif
