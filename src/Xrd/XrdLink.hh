#ifndef __XRD_LINK_H__
#define __XRD_LINK_H__
/******************************************************************************/
/*                                                                            */
/*                            X r d L i n k . h h                             */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>

#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucSFVec.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "Xrd/XrdJob.hh"
  
/******************************************************************************/
/*                      C l a s s   D e f i n i t i o n                       */
/******************************************************************************/
  
class XrdLinkMatch;
class XrdLinkXeq;
class XrdPollInfo;
class XrdProtocol;
class XrdTlsPeerCerts;
class XrdTlsContext;

class XrdLink : public XrdJob
{
public:

//-----------------------------------------------------------------------------
//! Activate a link by attaching it to a poller object.
//!
//! @return True if activation succeeded and false otherwise.
//-----------------------------------------------------------------------------

bool            Activate();

//-----------------------------------------------------------------------------
//! Obtain the address information for this link.
//!
//! @return Pointer to the XrdAddrInfo object. The pointer is valid while the
//!         end-point is connected.
//-----------------------------------------------------------------------------

XrdNetAddrInfo *AddrInfo();

//-----------------------------------------------------------------------------
//! Obtain the number of queued async requests.
//!
//! @return         The number of async requests queued.
//-----------------------------------------------------------------------------

int             Backlog();

//-----------------------------------------------------------------------------
//! Get a copy of the client's name as known by the link.
//!
//! @param  buff    Pointer to buffer to hold the name.
//! @param  blen    Length of the buffer.
//!
//! @return !0      The length of the name in gthe buffer.
//!         =0      The name could not be returned.
//-----------------------------------------------------------------------------

int             Client(char *buff, int blen);

//-----------------------------------------------------------------------------
//! Close the link.
//!
//! @param  defer   If true, the link is made unaccessible but the link
//!                 object not the file descriptor is released.
//!
//! @return !0      An error occurred, the return value is the errno.
//!         =0      Action successfully completed.
//-----------------------------------------------------------------------------

int             Close(bool defer=false);

//-----------------------------------------------------------------------------
//! Enable the link to field interrupts.
//-----------------------------------------------------------------------------

void            Enable();

//-----------------------------------------------------------------------------
//! Get the associated file descriptor.
//!
//! @return         The file descriptor number.
//-----------------------------------------------------------------------------

int             FDnum();

//-----------------------------------------------------------------------------
//! Find the next link matching certain attributes.
//!
//! @param  curr    Is an internal tracking value that allows repeated calls.
//!                 It must be set to a value of 0 or less on the initial call
//!                 and not touched therafter unless a null pointer is returned.
//! @param  who     If the object use to check if the link matches the wanted
//!                 criterea (typically, client name and host name). If the
//!                 pointer is nil, the next link is always returned.
//!
//! @return !0      Pointer to the link object that matches the criterea. The
//!                 link's reference counter is increased to prevent it from
//!                 being reused. A subsequent call will reduce the number.
//!         =0      No more links exist with the specified criterea.
//-----------------------------------------------------------------------------

static XrdLink *Find(int &curr, XrdLinkMatch *who=0);

//-----------------------------------------------------------------------------
//! Get I/O statistics.
//!
//! @param  inbytes  The number of bytes read.
//! @param  outbytes The number of bytes written.
//! @param  numstall The number of times the link was rescheduled due to
//!                  unavailability.
//! @param  numtardy The number of times the link was delayed due to
//!                  unavailability.
//!
//! @return          The link's reference count. The parameters will hold the
//!                  indicated statistic.
//-----------------------------------------------------------------------------

       int      getIOStats(long long &inbytes, long long &outbytes,
                                int  &numstall,     int  &numtardy);

//-----------------------------------------------------------------------------
//! Find the next client name matching certain attributes.
//!
//! @param  cur     Is an internal tracking value that allows repeated calls.
//!                 It must be set to a value of 0 or less on the initial call
//!                 and not touched therafter unless zero is returned.
//! @param  bname   Pointer to a buffer where the name is to be returned.
//! @param  blen    The length of the buffer.
//! @param  who     If the object use to check if the link matches the wanted
//!                 criterea (typically, client name and host name). If the
//!                 pointer is nil, a match always occurs.
//!
//! @return !0      The length of teh name placed in the buffer.
//!         =0      No more links exist with the specified criterea.
//-----------------------------------------------------------------------------

static int      getName(int &curr, char *bname, int blen, XrdLinkMatch *who=0);

//-----------------------------------------------------------------------------
//! Get the x509 certificate information for this TLS enabled link.
//!
//! @return A pointer to the XrdTlsCerts object holding verified certificates
//!         if such certificates exist. Otherwise a nil pointer is returned.
//!
//! @note Used by various protocols, so XrdTlsPeerCerts is a private header.
//-----------------------------------------------------------------------------

XrdTlsPeerCerts *getPeerCerts();

//-----------------------------------------------------------------------------
//! Obtain current protocol object pointer.
//-----------------------------------------------------------------------------

XrdProtocol    *getProtocol();

//-----------------------------------------------------------------------------
//! Lock or unlock the mutex used for control operations.
//!
//! @param  lk      When true, a lock is obtained. Otherwise it is released.
//!                 The caller is responsible for consistency.
//-----------------------------------------------------------------------------

void            Hold(bool lk);

//-----------------------------------------------------------------------------
//! Get the fully qualified name of the endpoint.
//!
//! @return Pointer to fully qualified host name. The contents are valid
//!         while the endpoint is connected.
//-----------------------------------------------------------------------------

const char     *Host() const {return (const char *)HostName;}

//-----------------------------------------------------------------------------
//! Pointer to the client's link identity.
//-----------------------------------------------------------------------------

char           *ID;      // This is referenced a lot (should have been const).

//-----------------------------------------------------------------------------
//! Obtain the link's instance number.
//!
//! @return The link's instance number.
//-----------------------------------------------------------------------------

unsigned int    Inst() const {return Instance;}

//-----------------------------------------------------------------------------
//! Indicate whether or not the link has an outstanding error.
//!
//! @return True    the link has an outstanding error.
//!                 the link has no outstanding error.
//-----------------------------------------------------------------------------

bool            isFlawed() const;

//-----------------------------------------------------------------------------
//! Indicate whether or not this link is of a particular instance.
//! only be used for display and not for security purposes.
//!
//! @param  inst    the expected instance number.
//!
//! @return true    the link matches the instance number.
//!         false   the link differs the instance number.
//-----------------------------------------------------------------------------

bool            isInstance(unsigned int inst) const;

//-----------------------------------------------------------------------------
//! Obtain the domain trimmed name of the end-point. The returned value should
//! only be used for display and not for security purposes.
//!
//! @return Pointer to the name that remains valid during the link's lifetime.
//-----------------------------------------------------------------------------

const char     *Name() const;

//-----------------------------------------------------------------------------
//! Obtain the network address object for this link. The returned value is
//! valid as long as the end-point is connected. Otherwise, it may change.
//!
//! @return Pointer to the object and remains valid during the link's lifetime.
//-----------------------------------------------------------------------------
const
XrdNetAddr     *NetAddr() const;

//-----------------------------------------------------------------------------
//! Issue a socket peek() and return result (do not use for TLS connections).
//!
//! @param  buff    pointer to buffer to hold data.
//! @param  blen    length of buffer.
//! @param  timeout milliseconds to wait for data. A negative value waits
//!                 forever.
//!
//! @return >=0     buffer holds data equal to the returned value.
//!         < 0     an error or timeout occurred.
//-----------------------------------------------------------------------------

int             Peek(char *buff, int blen, int timeout=-1);

//-----------------------------------------------------------------------------
//! Read data from a link. Note that this call blocks until some data is
//! available. Use Recv() with a timeout to avoid blocking.
//!
//! @param  buff    pointer to buffer to hold data.
//! @param  blen    length of buffer (implies the maximum bytes wanted).
//!
//! @return >=0     buffer holds data equal to the returned value.
//!         < 0     an error occurred.
//-----------------------------------------------------------------------------

int             Recv(char *buff, int blen);

//-----------------------------------------------------------------------------
//! Read data from a link. Note that this call either reads all the data wanted
//! or no data if the passed timeout occurs before any data is present.
//!
//! @param  buff    pointer to buffer to hold data.
//! @param  blen    length of buffer (implies the actual bytes wanted).
//! @param  timeout milliseconds to wait for data. A negative value waits
//!                 forever.
//!
//! @return >=0     buffer holds data equal to the returned value.
//!         < 0     an error occurred. Note that a special error -ENOMSG
//!                 is returned if poll() indicated data was present but
//!                 no bytes were actually read.
//-----------------------------------------------------------------------------

int             Recv(char *buff, int blen, int timeout);

//-----------------------------------------------------------------------------
//! Read data on a link.  Note that this call either reads all the data wanted
//! or no data if the passed timeout occurs before any data is present.
//!
//! @param  iov     pointer to the message vector.
//! @param  iocnt   number of iov elements in the vector.
//! @param  bytes   the sum of the sizes in the vector.
//!
//! @return >=0     number of bytes read.
//!         < 0     an error occurred or when -ETIMEDOUT is returned, no data
//!                 arrived within the timeout period. -ENOMSG is returned
//!                 when poll indicated data was present but 0 bytes were read.
//-----------------------------------------------------------------------------

int             Recv(const struct iovec *iov, int iocnt, int timeout);

//-----------------------------------------------------------------------------
//! Read data from a link. Note that this call reads as much data as it can
//! or until the passed timeout has occurred.
//!
//! @param  buff    pointer to buffer to hold data.
//! @param  blen    length of buffer (implies the maximum bytes wanted).
//! @param  timeout milliseconds to wait for data. A negative value waits
//!                 forever.
//!
//! @return >=0     buffer holds data equal to the returned value.
//!         < 0     an error occurred or when -ETIMEDOUT is returned, no data
//!                 arrived within the timeout period. -ENOMSG is returned
//!                 when poll indicated data was present but 0 bytes were read.
//-----------------------------------------------------------------------------

int             RecvAll(char *buff, int blen, int timeout=-1);

//------------------------------------------------------------------------------
//! Register a host name with this IP address. This is not MT-safe!
//!
//! @param  hName    -> to a true host name which should be fully qualified.
//!                     One of the IP addresses registered to this name must
//!                     match the IP address associated with this object.
//!
//! @return True:    Specified name is now associated with this link.
//!         False:   Nothing changed, registration could not be verified.
//------------------------------------------------------------------------------

bool        Register(const char *hName);

//-----------------------------------------------------------------------------
//! Send data on a link. This calls may block unless the socket was marked
//! nonblocking. If a block would occur, the data is copied for later sending.
//!
//! @param  buff    pointer to buffer to send.
//! @param  blen    length of buffer.
//!
//! @return >=0     number of bytes sent.
//!         < 0     an error or occurred.
//-----------------------------------------------------------------------------

int             Send(const char *buff, int blen);

//-----------------------------------------------------------------------------
//! Send data on a link. This calls may block unless the socket was marked
//! nonblocking. If a block would occur, the data is copied for later sending.
//!
//! @param  iov     pointer to the message vector.
//! @param  iocnt   number of iov elements in the vector.
//! @param  bytes   the sum of the sizes in the vector.
//!
//! @return >=0     number of bytes sent.
//!         < 0     an error occurred.
//-----------------------------------------------------------------------------

int             Send(const struct iovec *iov, int iocnt, int bytes=0);

//-----------------------------------------------------------------------------
//! Send data on a link using sendfile(). This call always blocks until all
//! data is sent. It should only be called if sfOK is true (see below).
//!
//! @param  sdP     pointer to the sendfile vector.
//! @param  sdn     number of elements in the vector.
//!
//! @return >=0     number of bytes sent.
//!         < 0     an error occurred.
//-----------------------------------------------------------------------------

static bool     sfOK;                   // True if Send(sfVec) enabled

typedef XrdOucSFVec sfVec;

int             Send(const sfVec *sdP, int sdn); // Iff sfOK is true

//-----------------------------------------------------------------------------
//! Wait for all outstanding requests to be completed on the link.
//-----------------------------------------------------------------------------

void            Serialize();

//-----------------------------------------------------------------------------
//! Set an error indication on he link.
//!
//! @param  text    a message describing the error.
//!
//! @return  =0     message set, the link is considered in error.
//!          -1     the message pointer was nil.
//-----------------------------------------------------------------------------

int             setEtext(const char *text);

//-----------------------------------------------------------------------------
//! Set the client's link identity.
//!
//! @param  userid pointer to the client's username.
//! @param  procid the client's process id (i.e. pid).
//-----------------------------------------------------------------------------

void            setID(const char *userid, int procid);

//-----------------------------------------------------------------------------
//! Set the client's location.
//!
//! @param  loc    reference to the location information.
//-----------------------------------------------------------------------------

void            setLocation(XrdNetAddrInfo::LocInfo &loc);

//-----------------------------------------------------------------------------
//! Set the link to be non-blocking.
//!
//! @return true   mode has been set.
//! @return false  mode is not supported for this operating system.
//-----------------------------------------------------------------------------

bool            setNB();

//-----------------------------------------------------------------------------
//! Set the link's protocol.
//!
//! @param  pp     pointer to the protocol object.
//! @param  runit  if true, starts running the protocol.
//! @param  push   if true, pushes current protocol to be the alternate one.
//!
//! @return        pointer to the previous protocol (may be nil).
//-----------------------------------------------------------------------------

XrdProtocol    *setProtocol(XrdProtocol *pp, bool runit=false, bool push=false);

//-----------------------------------------------------------------------------
//! Set the link's protocol name.
//!
//! @param  name   name of he protocol being used. The storage must be
//!                valid for the duration of the program.
//-----------------------------------------------------------------------------

void            setProtName(const char *name);

//-----------------------------------------------------------------------------
//! Set the link's parallel usage count.
//!
//! @param  cnt    should be 1 to increased the count and -1 to decrease it.
//-----------------------------------------------------------------------------

void            setRef(int cnt);

//-----------------------------------------------------------------------------
//! Enable or disable TLS on the link.
//
//! @param  enable  if true, TLS is enabled if not already enabled. Otherwise,
//!                 TLS is disabled and the TLS logical connection torn down.
//!                 statistics may be contradictory as they are collected async.
//! @param  ctx     The context to use when enabling TLS. When nil, the default
//!                 context is used.
//!
//! @return         True if successful, false otherwise.
//-----------------------------------------------------------------------------

bool            setTLS(bool enable, XrdTlsContext *ctx=0);

//-----------------------------------------------------------------------------
//! Shutdown the link but otherwise keep it intact.
//!
//! @param  getlock if true, the operation is performed under a lock.
//-----------------------------------------------------------------------------

void            Shutdown(bool getLock);

//-----------------------------------------------------------------------------
//! Obtain link statistics.
//!
//! @param  buff    pointer to the buffer for the xml statistics.
//! @param  blen    length of the buffer.
//! @param  do_sync if true, the statistics self-consistent. Otherwise, the
//!                 statistics may be contradictory as they are collected async.
//!
//! @return         number of bytes placed in the buffer excluding the null byte.
//-----------------------------------------------------------------------------

static int      Stats(char *buff, int blen, bool do_sync=0);

//-----------------------------------------------------------------------------
//! Add all local statistics to the global counters.
//!
//! @param  ctime   if not nil, return the total connect time in seconds.
//-----------------------------------------------------------------------------

void            syncStats(int *ctime=0);

//-----------------------------------------------------------------------------
//! Terminate a connection.
//!
//! @param  owner   pointer to the link ID representing a client who made
//!                 the connection to be terminated. If nil then this is a
//!                 request for the link to terminate another link, if possible.
//! @param  fdnum   the file descriptor number of the link to be terminated.
//! @param  inst    the link's instance number.
//!
//! @return >0      caller should wait this number of seconds and try again.
//! @return =0      link terminated.
//! @return <0      link could not be terminated:
//!                 -EACCES  the links was not created by the passed owner.
//!                 -EPIPE   link already being terminated.
//!                 -ESRCH   fdnum does not refer to a link.
//!                 -ETIME   unsuccessful, too many tries.
//-----------------------------------------------------------------------------

int             Terminate(const char *owner, int fdnum, unsigned int inst);

//-----------------------------------------------------------------------------
//! Return the time the link was made active (i.e. time of connection).
//-----------------------------------------------------------------------------

time_t          timeCon() const;

//-----------------------------------------------------------------------------
//! Return link's reference count.
//-----------------------------------------------------------------------------

int             UseCnt() const;

//-----------------------------------------------------------------------------
//! Mark this link as an in-memory communications bridge (internal use only).
//-----------------------------------------------------------------------------

void            armBridge();

//-----------------------------------------------------------------------------
//! Determine if this link is a bridge.
//!
//! @return true    this link is a bridge.
//! @return false   this link is a plain old link.
//-----------------------------------------------------------------------------

bool            hasBridge() const {return isBridged;}

//-----------------------------------------------------------------------------
//! Determine if this link is using TLS.
//!
//! @param  vprot   if not nil, the TLS protocol version number if returned.
//!                 If the link is not using TLS the version is a null string.
//!
//! @return true    this link is  using TLS.
//! @return false   this link not using TLS.
//-----------------------------------------------------------------------------

bool            hasTLS() const {return isTLS;}

//-----------------------------------------------------------------------------
//! Return TLS protocol version being used.
//!
//! @return The TLS protocol version number. If the link is not using TLS,
//!         a null string is returned;
//-----------------------------------------------------------------------------

const char     *verTLS();

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  lxq     Reference to the implementation.
//-----------------------------------------------------------------------------

                XrdLink(XrdLinkXeq &lxq);

protected:
               ~XrdLink() {}  // Is never deleted!

void            DoIt();       // This is an override of XrdJob::DoIt.
void            ResetLink();
int             Wait4Data(int timeout);

void           *rsvd1[3];     // Reserved for future use
XrdLinkXeq     &linkXQ;       // The implementation
char           *HostName;     // Pointer to the hostname
unsigned int    Instance;     // Instance number of this object
bool            isBridged;    // If true, this link is an in-memory bridge
bool            isTLS;        // If true, this link uses TLS for all I/O
char            rsvd2[2];
};
#endif
