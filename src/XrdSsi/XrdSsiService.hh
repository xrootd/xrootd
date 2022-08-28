#ifndef __XRDSSISERVICE_HH__
#define __XRDSSISERVICE_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i S e r v i c e . h h                       */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <string>
  
//-----------------------------------------------------------------------------
//! The XrdSsiService object is used by the Scalable Service Interface to
//! process client requests.
//!
//! There may be many client-side service objects, as needed. However, only
//! one such object is obtained server-side. The object is used to effect all
//! service requests and handle responses.
//!
//! Client-side: the service object is obtained via the object pointed to by
//!              XrdSsiProviderClient defined in libXrdSsi.so
//!
//! Server-side: the service object is obtained via the object pointed to by
//!              XrdSsiProviderServer defined in the plugin shared library.
//-----------------------------------------------------------------------------

class XrdSsiErrInfo;
class XrdSsiRequest;
class XrdSsiResource;

class XrdSsiService
{
public:

//-----------------------------------------------------------------------------
//! Obtain the version of the abstract class used by underlying implementation.
//! The version returned must match the version compiled in the loading library.
//! If it does not, initialization fails.
//-----------------------------------------------------------------------------

static const int SsiVersion = 0x00020000;

       int     GetVersion() {return SsiVersion;}

//-----------------------------------------------------------------------------
//! @brief Attach to a backgrounded request.
//!
//! When a client calls Attach() the server-side Attach() is invoked to
//! indicate that the backgrounded request is now a foreground request. Many
//! times such notification is not needed so a default nil implementation is
//! provided. Server-side Attach() calls are always passed the original request
//! object reference so that it can pair up the request with the attach.
//!
//! @param  eInfo    Reference to an error info object which will contain the
//!                  error describing why the attach failed (i.e. return false).
//!
//! @param  handle   Reference to the handle provided to the callback method
//!                  XrdSsiRequest::ProcessResponse() via isHandle response type.
//!                  This is always an empty string on server-side calls.
//!
//! @param  reqRef   Reference to the request object that is to attach to the
//!                  backgrounded request. It need not be the original request
//!                  object (client-side) but it always is the original request
//!                  object server-side.
//!
//! @param  resP     A pointer to the resource object describing the request
//!                  resources. This is meaningless for client calls and should
//!                  not be specified. For server-side calls, it can be used to
//!                  reauthorize the request since the client performing the
//!                  attach may be different from the client that actually
//!                  started the request.
//!
//! @return true     Continue normally, no issues detected.
//!         false    An exception occurred, the eInfo object has the reason. For
//!                  server side calls this provides the service the ability to
//!                  reject request reattachment.
//-----------------------------------------------------------------------------

virtual bool   Attach(      XrdSsiErrInfo  &eInfo,
                      const std::string    &handle,
                            XrdSsiRequest  &reqRef,
                            XrdSsiResource *resP=0
                     ) {return true;}

//-----------------------------------------------------------------------------
//! @brief Prepare for processing subsequent resource request.
//!
//! This method is meant to be used server-side to optimize subsequent request
//! processing, perform authorization, and allow a service to stall or redirect
//! requests. It is optional and a default implementation is provided that
//! simply asks the provider if the resource exists on the server. Clients need
//! not call or implement this method.
//!
//! @param  eInfo    The object where error information is to be placed.
//! @param  rDesc    Reference to the resource object that describes the
//!                  resource subsequent requests will use.
//!
//! @return true     Continue normally, no issues detected.
//!         false    An exception occurred, the eInfo object has the reason.
//!
//! Special notes for server-side processing:
//!
//! 1) Two special errors are recognized that allow for a client retry:
//!
//!    resP->eInfo.eNum = EAGAIN (client should retry elsewhere)
//!    resP->eInfo.eMsg = the host name where the client is redirected
//!    resP->eInfo.eArg = the port number to be used by the client
//!
//!    resP->eInfo.eNum = EBUSY  (client should wait and then retry).
//!    resP->eInfo.eMsg = an optional reason for the wait.
//!    resP->eInfo.eArg = the number of seconds the client should wait.
//-----------------------------------------------------------------------------

virtual bool   Prepare(XrdSsiErrInfo &eInfo, const XrdSsiResource &rDesc);

//-----------------------------------------------------------------------------
//! @brief Process a request; client-side or server-side.
//!
//! When a client calls ProcessRequest() the same method is called server-side
//! with the same parameters that the client specified except for timeOut which
//! is always set to zero server-side.
//!
//! @param  reqRef   Reference to the Request  object that describes the
//!                  request.
//!
//! @param  resRef   Reference to the Resource object that describes the
//!                  resource that the request will be using.
//!
//! All results are returned via the request object callback methods.
//! For background queries, the XrdSsiRequest::ProcessResponse() is 
//! called with a response type of isHandle when the request is handed
//! off to the endpoint for execution (see XrdSsiRequest::SetDetachTTL).
//-----------------------------------------------------------------------------

virtual void   ProcessRequest(XrdSsiRequest  &reqRef,
                              XrdSsiResource &resRef
                             ) = 0;

//-----------------------------------------------------------------------------
//! @brief Stop the client-side service. This is never called server-side.
//!
//! @param  immed    When true, the service is only stopped if here are no
//!                  active requests. Otherwise, after all requests have
//!                  finished. the service object is deleted.
//!
//! @return true     Service has been stopped. Once all requests have been
//!                  completed, the service object will be deleted.
//! @return false    Service cannot be stopped because there are still active
//!                  foreground requests and the immed parameter was true.
//-----------------------------------------------------------------------------

virtual bool   Stop(bool immed=false) {return !immed;}

//-----------------------------------------------------------------------------
//! Constructor
//-----------------------------------------------------------------------------

               XrdSsiService() {}
protected:

//-----------------------------------------------------------------------------
//! Destructor. The service object cannot be explicitly deleted. Use Stop().
//-----------------------------------------------------------------------------

virtual       ~XrdSsiService() {}
};
#endif
