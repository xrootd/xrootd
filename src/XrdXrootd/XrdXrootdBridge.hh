#ifndef __XRDXROOTDBRIDGE_HH_
#define __XRDXROOTDBRIDGE_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d B r i d g e . h h                     */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <string.h>

#include "XProtocol/XPtypes.hh"

//-----------------------------------------------------------------------------
//! Bridge
//!
//! The Bridge object allows other protocols to gain access to the xrootd
//! protocol stack. Almost any kind of request/response protocol can use this
//! class to convert its request to an xrootd protocol request and rewrite the
//! xrootd protocol response to adhere to its protocol specification. Callers
//! of these methods must be thread-safe and must not rely on thread-local
//! storage as bridge requests and responses may or may not be executed using
//! the initiating thread. Also, see the Result object class below.
//-----------------------------------------------------------------------------

struct iovec;
class  XrdLink;
class  XrdSecEntity;
class  XrdXrootdProtocol;

namespace XrdXrootd
{

/******************************************************************************/
/*                     X r d X r o o t d : : B r i d g e                      */
/******************************************************************************/
  
class Bridge
{
public:
class Result;

//-----------------------------------------------------------------------------
//! Create a Bridge object.
//!
//! The first step is to create a Bridge object via a Login() call. The object
//! should correspond to a session (i.e. tied to a particular client) real or
//! not. The returned object must be used to inject xrootd requests into the
//! protocol stack. Response rewrites are handled by the Result object passed as
//! an argument. A successful Login() takes control of the connection. You can
//! still write using the Link object but reads may only occur when your
//! protocol's Process() method is called. Use Disc() to disband the bridge and
//! free its storage. The bridge is automatically disbanded when your protocol's
//! Recycle() method is called. This happens when you explicitly close the link
//! or implicitly when the Process() method returns a negative error code or
//! a callback method returns false.
//!
//! @param  rsltP   a pointer to the result object. This object is used to
//!                 to rewrite xrootd responses to protocol specific responses.
//!                 It must be allocated by the caller. One such object can be
//!                 created for each session or, if the protocol allows, be
//!                 shared by all sessions. It cannot be deleted until all
//!                 references to the object disappear (see the Result class).
//!         linkP   a pointer to the link object that the protocol driver
//!                 created to the client connection.
//!         secP    a pointer to the XrdSecEntity object that describes the
//!                 client's identity.
//!         nameP   An arbitrary 1-to-8 character client name. The Bridge will
//!                 uniquefy this name so that log file messages will track the
//!                 the associated client. The link's identity is set to
//!                 correspond to this name with additional information.
//!         protP   a 1-to-7 character name of the protocol using this bridge
//!                 (e.g. "http").
//!
//! @return bridgeP a pointer to a new instance of this class if a bridge
//!                 could be created, null otherwise. If null is returned, the
//!                 retc variable holds the errno indicating why it failed.
//-----------------------------------------------------------------------------

static
Bridge       *Login(Result       *rsltP, //!< The result callback object
                    XrdLink      *linkP, //!< Client's network connection
                    XrdSecEntity *seceP, //!< Client's identity
                    const char   *nameP, //!< Client's name for tracking
                    const char   *protP  //!< Protocol name for tracking
                   );

//-----------------------------------------------------------------------------
//! Inject an xrootd request into the protocol stack.
//!
//! The Run() method allows you to inject an xrootd-style request into the
//! stack. It must use the same format as a real xrootd client would use across
//! the network. The xrootd protocol reference describes these requests. The
//! Run() method handles the request as if it came through the network with
//! some notable exceptions (see the xdataP and xdataL arguments).
//!
//! @param  xreqP   pointer to the xrootd request. This is the standard 24-byte
//!                 request header common to all xrootd requests in network
//!                 format. The contents of the buffer may be modified by the
//!                 this method. The buffer must not be modified by the caller
//!                 before a response is solicited via the Result object.
//!
//! @param  xdataP  the associated data for this request. Full or partial data
//!                 may be supplied as indicated by the xdataL argument. See
//!                 explanation of xdataL. For write requests, this buffer may
//!                 not be altered or deleted until the Result Free() callback
//!                 is invoked. For other requests, it doesn't matter.
//!
//!                 If the pointer is zero but the "dlen" field is not zero,
//!                 dlen's worth of data is read from the network using the
//!                 associated XdLink object to complete the request.
//!
//! @param  xdataL  specifies the length of data in the buffer pointed to by
//!                 xdataP. Depending on the value and the value in the "dlen"
//!                 field, additional data may be read from the network.
//!
//!                 xdataL  < "dlen": dlen-xdataL additional bytes will be read
//!                                   from the network to complete the request.
//!                 xdataL >= "dlen": no additional bytes will be read from the
//!                                   network. The request data is complete.
//!
//! @return true    the request has been accepted. Processing will start when
//!                 the caller returns from the Process() method.
//!                 A response will come via a Result object callback.
//!         false   the request has been rejected because the bridge is still
//!                 processing a previous request.
//-----------------------------------------------------------------------------

virtual bool  Run(const char *xreqP,       //!< xrootd request header
                        char *xdataP=0,    //!< xrootd request data (optional)
                        int   xdataL=0     //!< xrootd request data length
                 ) = 0;

//-----------------------------------------------------------------------------
//! Disconnect the session from the bridge.
//!
//! The Disc() method allows you to disconnect the session from the bridge and
//! free the storage associated with this object. It may be called when you want
//! to regain control of the connection and delete the Bridge object (note that
//! you cannot use delete on Bridge). The Disc() method must not be called in
//! your protocol Recycle() method as protocol object recycling already implies
//! a Disc() call (i.e. the connection is disbanding the associated protocol).
//!
//! @return true    the bridge has been  dismantled.
//!         false   the bridge cannot be dismantled because it is still
//!                 processing a previous request.
//-----------------------------------------------------------------------------

virtual bool  Disc() = 0;

//-----------------------------------------------------------------------------
//! Set file's sendfile capability.
//!
//! The setSF() method allows you to turn on or off the ability of an open
//! file to be used with the sendfile() system call. This is useful when you
//! must see the data prior to sending to the client (e.g. for encryption).
//!
//! @param  fhandle the filehandle as returned by kXR_open.
//! @param  mode    When true, enables sendfile() otherwise it is disabled.
//!
//! @return =0      Sucessful.
//! @return <0      Call failed. The return code is -errno and usually will
//!                 indicate that the filehandle is not valid.
//-----------------------------------------------------------------------------

virtual int   setSF(kXR_char *fhandle, bool seton=false) = 0;

//-----------------------------------------------------------------------------
//! Set the maximum delay.
//!
//! The setWait() method allows you to specify the maximum amount of time a
//! request may be delayed (i.e. via kXR_wait result) before it generates a
//! kXR_Cancelled error with the associated Error() result callback. The default
//! maximum time is 1 hour. If you specify a time less than or equal to zero,
//! wait requests are reflected back via the Wait() result callback method and
//! you are responsible for handling them. You can request wait notification
//! while still having the wait internally handled using the second parameter.
//! Maximum delays are bridge specific. There is no global value. If you desire
//! something other than the default you must call SetWait for each Login().
//!
//! @param  wtime   the maximum wait time in seconds.
//! @param  notify  When true, issues a Wait callback whenever a wait occurs.
//!                 This is the default when wtime is <= 0.
//!
//-----------------------------------------------------------------------------

virtual void  SetWait(int wime, bool notify=false) = 0;

/******************************************************************************/
/*            X r d X r o o t d : : B r i d g e : : C o n t e x t             */
/******************************************************************************/
  
//-----------------------------------------------------------------------------
//! Provide callback context.
//!
//! The Context object is passed in all Result object callbacks and contains
//! information describing the result context. No public members should be
//! changed by any result callback method. The context object also includes a
//! method that must be used to complete a pending sendfile() result.
//-----------------------------------------------------------------------------
  
class Context
{
public:

      XrdLink   *linkP; //!< -> associated session link object (i.e. connection)
      kXR_unt16  rCode; //!< associated "kXR" request code in host byte order
union{kXR_unt16  num;   //!< associated stream ID as a short
      kXR_char   chr[2];//!< associated stream ID as the original char[2]
     }           sID;   //!< associated request stream ID

//-----------------------------------------------------------------------------
//! Complete a File() callback.
//!
//! The Send() method must be called after the File() callback is invoked to
//! complete data transmission using sendfile(). If Send() is not called the
//! pending sendfile() call is not made and no data is sent to the client.
//!
//! @param  headP   a pointer to the iovec structure containing the data that
//!                 must be sent before the sendfile() data. If there is none,
//!                 the pointer can be null.
//! @param  headN   the number of elements in the headP iovec structure array.
//! @param  tailP   a pointer to the iovec structure containing the data that
//!                 must be sent after the sendfile() data. If there is none,
//!                 the pointer can be null.
//! @param  tailN   the number of elements in the tailP iovec structure array.
//!
//! @return < 0     transmission error has occurred. This can be due to either
//!                 connection failure or data source error (i.e. I/O error).
//!         = 0     data has been successfully sent.
//!         > 0     the supplied context was not generated by a valid File()
//!                 callback. No data has been sent.
//-----------------------------------------------------------------------------

virtual int   Send(const
                   struct iovec *headP, //!< pointer to leading  data array
                   int           headN, //!< array count
                   const
                   struct iovec *tailP, //!< pointer to trailing data array
                   int           tailN  //!< array count
                  )
{
  (void)headP; (void)headN; (void)tailP; (void)tailN;
  return 1;
}

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//-----------------------------------------------------------------------------

              Context(XrdLink *lP, kXR_char *sid, kXR_unt16 req)
                     : linkP(lP), rCode(req)
                       {memcpy(sID.chr, sid, sizeof(sID.chr));}
virtual      ~Context() {}
};

/******************************************************************************/
/*             X r d X r o o t d : : B r i d g e : : R e s u l t              */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! Handle xrootd protocol execution results.
//!
//! The Result object is an abstract class that defines the interface used
//! by the xrootd protocol stack to effect a client response using whatever
//! alternate protocol is needed. You must define an implementation and pass it
//! as an argument to the Login() Bridge method.
//-----------------------------------------------------------------------------

class Result
{
public:

//-----------------------------------------------------------------------------
//! Effect a client data response.
//!
//! The Data() method is called when Run() resulted in a successful data
//! response. The method should rewrite the data and send it to the client using
//! the associated XrdLink object. As an example,
//! 1) Result::Data(info, iovP, iovN, iovL) is called.
//! 2) Inspect iovP, rewrite the data.
//! 3) Send the response: info->linkP->Send(new_iovP, new_iovN, new_iovL);
//! 4) Handle send errors and cleanup(e.g. deallocate storage).
//! 5) Return, the exchange is now complete.
//!
//! @param  info    the context associated with the result.
//! @param  iovP    a pointer to the iovec structure containing the xrootd data
//!                 response about to be sent to the client. The request header
//!                 is not included in the iovec structure. The elements of this
//!                 structure must not be modified by the method.
//! @param  iovN    the number of elements in the iovec structure array.
//! @param  iovL    total number of data bytes that would be sent to the client.
//!                 This is simply the sum of all the lengths in the iovec.
//! @param  final   True is this is the final result. Otherwise, this is a
//!                 partial result (i.e. kXR_oksofar) and more data will result
//!                 causing additional callbacks. For write requests, any
//!                 supplied data buffer may now be reused or freed.
//!
//! @return true    continue normal processing.
//!         false   terminate the bridge and close the link.
//-----------------------------------------------------------------------------

virtual bool  Data(Bridge::Context &info,   //!< the result context
                   const
                   struct iovec    *iovP,   //!< pointer to data array
                   int              iovN,   //!< array count
                   int              iovL,   //!< byte  count
                   bool             final   //!< true -> final result
                  ) = 0;

//-----------------------------------------------------------------------------
//! Effect a client acknowledgement.
//!
//! The Done() method is called when Run() resulted in success and there is no
//! associated data for the client (equivalent to a simple kXR_ok response).
//!
//! @param  info    the context associated with the result.
//!
//! @return true    continue normal processing.
//!         false   terminate the bridge and close the link.
//-----------------------------------------------------------------------------

virtual bool  Done(Bridge::Context &info)=0;//!< the result context

//-----------------------------------------------------------------------------
//! Effect a client error response.
//!
//! The Error() method is called when an error was encountered while processing
//! the Run() request. The error should be reflected to the client.
//!
//! @param  info    the context associated with the result.
//! @param  ecode   the "kXR" error code describing the nature of the error.
//!                 The code is in host byte format.
//! @param  etext   a null terminated string describing the error in human terms
//!
//! @return true    continue normal processing.
//!         false   terminate the bridge and close the link.
//-----------------------------------------------------------------------------

virtual bool  Error(Bridge::Context &info,   //!< the result context
                    int              ecode,  //!< the "kXR" error code
                    const char      *etext   //!< associated error message
                   ) = 0;

//-----------------------------------------------------------------------------
//! Notify callback that a sendfile() request is pending.
//!
//! The File() method is called when Run() resulted in a sendfile response (i.e.
//! sendfile() would have been used to send data to the client). This allows
//! the callback to reframe the sendfile() data using the Send() method in the
//! passed context object (see class Context above).
//!
//! @param  info    the context associated with the result.
//! @param  dlen    total number of data bytes that would be sent to the client.
//!
//! @return true    continue normal processing.
//!         false   terminate the bridge and close the link.
//-----------------------------------------------------------------------------

virtual int   File(Bridge::Context &info,  //!< the result context
                   int              dlen   //!< byte  count
                  ) = 0;

//-----------------------------------------------------------------------------
//! Notify callback that a write buffer is now available for reuse.
//!
//! The Free() method is called when Run() was called to write data and a buffer
//! was supplied. Normally, he buffer is pinned and cannot be reused until the
//! write completes. This callback provides the notification that the buffer is
//! no longer in use. The callback is invoked prior to any other callbacks and
//! is only invoked if a buffer was supplied.
//!
//! @param  info    the context associated with this call.
//! @param  buffP   pointer to the buffer.
//! @param  buffL   the length originally supplied in the Run() call.
//-----------------------------------------------------------------------------

virtual void  Free(Bridge::Context &info,  //!< the result context
                   char            *buffP, //!< pointer to the buffer
                   int              buffL  //!< original length to Run()
                  )
{
  (void)info; (void)buffP; (void)buffL;
}

//-----------------------------------------------------------------------------
//! Redirect the client to another host:port.
//!
//! The Redir() method is called when the client must be redirected to another
//! host.
//!
//! @param  info    the context associated with the result.
//! @param  port    the port number in host byte format.
//! @param  hname   the DNS name of the host or IP address is IPV4 or IPV6
//!                 format (i.e. "n.n.n.n" or "[ipv6_addr]").
//!
//! @return true    continue normal processing.
//!         false   terminate the bridge and close the link.
//-----------------------------------------------------------------------------

virtual bool  Redir(Bridge::Context &info,   //!< the result context
                    int              port,   //!< the port number
                    const char      *hname   //!< the destination host
                   ) = 0;

//-----------------------------------------------------------------------------
//! Effect a client wait.
//!
//! The Wait() method is called when Run() needs to delay a request. Normally,
//! delays are internally handled. However, you can request that delays be
//! reflected via a callback using the Bridge SetWait() method.
//!
//! @param  info    the context associated with the result.
//! @param  wtime   the number of seconds to delay the request.
//! @param  wtext   a null terminated string describing the wait in human terms
//!
//! @return true    continue normal processing.
//!         false   terminate the bridge and close the link.
//-----------------------------------------------------------------------------

virtual bool  Wait(Bridge::Context &info,   //!< the result context
                   int              wtime,  //!< the wait time
                   const char      *wtext   //!< associated message
                   )
{
  (void)info; (void)wtime; (void)wtext;
  return false;
}

//-----------------------------------------------------------------------------
//! Effect a client wait response (waitresp) NOT CURRENTLY IMPLEMENTED!
//!
//! The WaitResp() method is called when an operation ended with a wait for
//! response (waitresp) condition. The wait for response condition indicates
//! that the actual response will be delivered at a later time. You can use
//! context object to determine the operation being delayed. This callback
//! provides you the opportunity to say how the waitresp is to be handled.
//!
//! @param  info    the context associated with the result.
//! @param  wtime   the number of seconds in which a response is expected.
//! @param  wtext   a null terminated string describing the delay in human terms
//!
//! @return !0      pointer to the callback object whose appropriate method
//!                 should be called when the actual response is generated.
//! @return 0       the waitresp will be handled by the bridge application. The
//!                 application is responsible for re-issuing the request when
//!                 the final response is a wait.
//-----------------------------------------------------------------------------
virtual
Bridge::Result *WaitResp(Bridge::Context &info,   //!< the result context
                         int              wtime,  //!< the wait time
                         const char      *wtext   //!< associated message
                        )
{
  (void)info; (void)wtime; (void)wtext;
  return 0;
}

//-----------------------------------------------------------------------------
//! Constructor & Destructor
//-----------------------------------------------------------------------------

              Result() {}
virtual      ~Result() {}
};

//-----------------------------------------------------------------------------
//! Constructor & Destructor
//-----------------------------------------------------------------------------

              Bridge() {}
protected:
virtual      ~Bridge() {}
};
}
#endif
