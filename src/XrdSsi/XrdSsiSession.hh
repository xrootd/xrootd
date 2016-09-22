#ifndef __XRDSSISESSION_HH__
#define __XRDSSISESSION_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i S e s s i o n . h h                       */
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

#include <errno.h>

#include "XrdSsi/XrdSsiErrInfo.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiSession object is used by the Scalable Service Interface to
//! process client requests. There is one session object per logical connection.
//! The XrdSsiServer::Provision() method supplies the object. You must supply a
//! server-side implementation based on the abstract class below. One is
//! already provided for client-side use.
//-----------------------------------------------------------------------------

class XrdSsiRequest;
struct XrdSsiRespInfo;
class XrdSsiTask;

class XrdSsiSession
{
public:
friend class XrdSsiRequest;

//-----------------------------------------------------------------------------
//! Get the session's location (i.e. endpoint hostname and port)
//!
//! @return A pointer to the session's location. It remains valid until the
//!         session is unprovisioned. A null means the session is not bound
//!         to any endpoint.
//-----------------------------------------------------------------------------
inline
const char    *Location() {return sessNode;}

//-----------------------------------------------------------------------------
//! Get the session's name.
//!
//! @return A pointer to the session's name. It remains valid until the
//!         session is unprovisioned.
//-----------------------------------------------------------------------------
inline
const char    *Name() {return sessName;}

//-----------------------------------------------------------------------------
//! Process a new request
//!
//! Client-side: ProcessRequest() should be called with an XrdSsiRequest object
//!              to process a new request by a session. The session object
//!              assumes ownership of the request object until the request is
//!              completed or canceled.
//!
//! Server-side: ProcessRequest() is called when a new request is received.
//!              It is always called using a new thread.
//!
//! @param  reqP   pointer to the object describing the request. This object
//!                is also used to effect a response. Ownwership of the
//!                request object is transfered to the session object. It must
//!                remain valid until after XrdSsiRequest::Finished() is called.
//!
//! @param  tOut   the maximum time the request should take. A value of zero
//!                uses the default value.
//!
//! @return All results are returned via the request's ProcessResponse callback.
//-----------------------------------------------------------------------------

virtual void   ProcessRequest(XrdSsiRequest *reqP, unsigned short tOut=0) = 0;

//-----------------------------------------------------------------------------
//! Unprovision a session. 
//!
//! Client-side: All outstanding requests must be finished by calling
//!              XrdSsiRequest::Finished() prior to calling Unprovision().
//!
//! Server-side: Unprovision() is called only after XrdSsiRequest::Finished()
//!              is called on all outstanding requests indicating cancellation.
//!
//! The session object should release all if its resources and recycle or
//! delete itself.
//!
//! @param  forced when true, the connection was lost.
//!
//! @return true   Session unprovisioned, the object is no longer valid.
//!
//! @return false  Session could not be unprovisioned because there are still
//!                unfinished requests.
//-----------------------------------------------------------------------------

virtual bool   Unprovision(bool forced=false) = 0;

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  sname  pointer to session's name.
//! @param  sloc   pointer to session's location as "hostname:port" (optional).
//-----------------------------------------------------------------------------

               XrdSsiSession(char *sname, char *sloc=0)
                            : sessName(sname), sessNode(sloc) {}

protected:

//-----------------------------------------------------------------------------
//! Notify a session that a request either completed or was canceled. This
//! allows the session to release any resources given to the request object
//! (e.g. data response buffer or a stream). Upon return the object is owned by
//! the object's creator who is responsible for releaasing or recyling the
//! object. This method is automatically invoked by XrdSsiRequest::Finish().
//!
//! @param  rqstP  pointer   to the object describing the request.
//! @param  rInfo  reference to the object describing the response.
//! @param  cancel False -> the request/response interaction completed.
//!                True  -> the request/response interaction aborted because
//!                         of an error or the client requested that the
//!                         request be canceled.
//-----------------------------------------------------------------------------

virtual void   RequestFinished(      XrdSsiRequest  *rqstP,
                               const XrdSsiRespInfo &rInfo,
                                     bool            cancel=false) = 0;

//-----------------------------------------------------------------------------
//! Destructor is protected. You cannot use delete on a session, use 
//! Unprovision() to effectively delete the session object.
//-----------------------------------------------------------------------------

virtual       ~XrdSsiSession() {}

char          *sessName;
char          *sessNode;
};
#endif
