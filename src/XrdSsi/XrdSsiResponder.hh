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
//! The XrdSsiResponder.hh class provides the session object and its agents a way to
//! respond to a request. It is a companion (friend) class of XrdSsiRequest.
//! This class is only practically meaningful server-side.
//!
//! Any class that needs to post a response, release a request buffer or post
//! stream data to the request object should inherit this class and use its
//! methods to get access to the request object. Typically, the XrdSsiSession
//! implementation class and its agent classes (i.e. classes that do work on
//! its behalf) inherit this class.
//!
//! When the XrdSsiResponder::SetResponse() method is called to post a response
//! the request object's ProcessResponse() method is called. Ownership of the
//! request object does not revert back to the object's creator until the
//! XrdSsiRequest::Finished() method returns. This allows the session to
//! reclaim any response data buffer or stream resource before it gives up
//! control of the request object.
//!
//! This is a real class that interposes itself between the abstract request
//! object and the real (i.e. derived class) session object or its agent.
//-----------------------------------------------------------------------------

class XrdSsiSession;

class XrdSsiResponder
{
public:

//-----------------------------------------------------------------------------
//! Obtain the object that inherited this responder (Version 1). Use this
//! version if the object is identified by an int type value.
//!
//! @param  oType Place to put object's type established at construction time.
//! @param  oInfo Place to put Object's info established at construction time.
//!
//! @return The pointer to the object expressed as a void pointer along with
//!         oType set to the object's type and oInfo set to any information.
//-----------------------------------------------------------------------------

inline void   *GetObject(int &oType, int &oInfo)
                        {oType=objIdent[0]; oInfo=objIdent[1]; return objVal;}

//-----------------------------------------------------------------------------
//! Obtain the object that inherited this responder (Version 2). Use this
//! version if the object is identified by void * handle (e.g. constructor).
//!
//! @param  oHndl Place to put Object's handle established at construction time.
//!
//! @return The pointer to object expressed as a void pointer along with
//!         oHndl set the the object's handle.
//-----------------------------------------------------------------------------

inline void   *GetObject(void *&oHndl) {oHndl = objHandle; return objVal;}

protected:

//-----------------------------------------------------------------------------
//! Take ownership of a request object by binding a request object, to a session
//! object and a responder object if it differs from the session object.
//! This method should only be used by the session object or its agent.
//!
//! @param  rqstP the pointer to the request   object.
//! @param  sessP the pointer to the session   object.
//! @param  respP the pointer to the responder object (optional).
//-----------------------------------------------------------------------------

inline  void    BindRequest(XrdSsiRequest   *rqstP,
                            XrdSsiSession   *sessP,
                            XrdSsiResponder *respP=0)
                            {rqstP->theSession = sessP;
                             rqstP->theRespond = respP;
                             reqP = rqstP; rqstP->Resp.Init();
                            }

//-----------------------------------------------------------------------------
//! Release the request buffer of the request bound to this object.
//-----------------------------------------------------------------------------

inline  void   ReleaseRequestBuffer() {if (reqP) reqP->RelRequestBuffer();}

//-----------------------------------------------------------------------------
//! The following enums are returned by SetResponse() to indicate ending status.
//-----------------------------------------------------------------------------

enum Status {wasPosted=0, //!< Success: The response was successfully posted
             notPosted,   //!< Failure: A request was not bound to this object
                          //!<          or a response has already been posted
             notActive    //!< Failure: Request is no longer active
            };

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
                          {XrdSsiRequest *rP = reqP;
                           if (!reqP) return notPosted;
                           reqP->eInfo.Set(eMsg, eNum);
                           reqP->Resp.eMsg  = reqP->eInfo.Get(reqP->Resp.eNum);
                           reqP->Resp.rType = XrdSsiRespInfo::isError;
                           reqP = 0;
                           return (rP->ProcessResponse(rP->Resp, false)
                                   ? wasPosted : notActive);
                          }

//-----------------------------------------------------------------------------
//! Set a memory buffer containing data as the request response.
//!
//! @param  buff  pointer to a buffer holding the response. The buffer must
//!               remain valid until XrdSsiSession::Complete() is called.
//! @param  blen  the length of the response in buff that is to be sent.
//!
//! @return       See Status enum for possible values.
//-----------------------------------------------------------------------------

inline Status  SetResponse(const char *buff, int blen)
                          {XrdSsiRequest *rP = reqP;
                           if (!reqP) return notPosted;
                           reqP->Resp.buff  = buff; reqP->Resp.blen  = blen;
                           reqP->Resp.rType = XrdSsiRespInfo::isData;
                           reqP = 0;
                           return (rP->ProcessResponse(rP->Resp)
                                   ? wasPosted : notActive);
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
                          {XrdSsiRequest *rP = reqP;
                           if (!reqP) return notPosted;
                           reqP->Resp.fdnum = fdnum;
                           reqP->Resp.fsize = fsize;
                           reqP->Resp.rType = XrdSsiRespInfo::isFile;
                           reqP = 0;
                           return (rP->ProcessResponse(rP->Resp)
                                   ? wasPosted : notActive);
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
                          {XrdSsiRequest *rP = reqP;
                           if (!reqP) return notPosted;
                           reqP->Resp.eNum  = 0;
                           reqP->Resp.strmP = strmP;
                           reqP->Resp.rType = XrdSsiRespInfo::isStream;
                           reqP = 0;
                           return (rP->ProcessResponse(rP->Resp)
                                   ? wasPosted : notActive);
                          }

//-----------------------------------------------------------------------------
//! Get the request bound to this object.
//!
//! @return A pointer to the request object. If it is nil then no request
//!         is currently bound to this object.
//-----------------------------------------------------------------------------
inline
XrdSsiRequest *TheRequest() {return reqP;}

//-----------------------------------------------------------------------------
//! The constructor is protected. This class is meant to be inherited by an
//! object (e.g. XrdSsiSession) that will actually post responses. This version
//! should be used if the object that inherited the responder is identified by
//! an integer type.
//!
//! @param  objP  Pointer to the underlying object (i.e. this pointer).
//! @param  oType The value that identifies the object's type.
//! @param  oInfo Optional additional information associated with the object.
//-----------------------------------------------------------------------------

               XrdSsiResponder(void *objP, int oType, int oInfo=0)
                              : reqP(0), objVal(objP)
                              {objIdent[0] = oType; objIdent[1] = oInfo;}

//-----------------------------------------------------------------------------
//! The constructor is protected. This class is meant to be inherited by an
//! object (e.g. XrdSsiSession) that will actually post responses. This version
//! should be used if the object that inherited the responder is identified by
//! a void * handle (e.g. the underlying constructor).
//!
//! @param  objP  Pointer to the underlying object (i.e. this pointer).
//! @param  objH  The handle value that identifies the object's type.
//-----------------------------------------------------------------------------

               XrdSsiResponder(void *objP, void *objH)
                              : reqP(0), objVal(objP), objHandle(objH) {}

//-----------------------------------------------------------------------------
//! Destructor is protected. You cannot use delete on a respond object, as it
//! is meant to be inherited by a class and not separately instantiated.
//-----------------------------------------------------------------------------

virtual       ~XrdSsiResponder() {}

private:
XrdSsiRequest *reqP;
void          *objVal;
union {int     objIdent[2];
       void   *objHandle;
      };
};
#endif
