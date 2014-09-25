#ifndef __SEC_INTERFACE_H__
#define __SEC_INTERFACE_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d S e c I n t e r f a c e . h h                     */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#ifndef WIN32
#include <sys/param.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "XrdSec/XrdSecEntity.hh"

/******************************************************************************/
/*  X r d S e c C r e d e n t i a l s   &   X r d S e c P a r a m e t e r s   */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Generic structure to pass security information back and forth.
//------------------------------------------------------------------------------

struct XrdSecBuffer
{
       int   size;    //!< Size of the buffer or length of data in the buffer
       char *buffer;  //!< Pointer to the buffer

       XrdSecBuffer(char *bp=0, int sz=0) : size(sz), buffer(bp), membuf(bp) {}
      ~XrdSecBuffer() {if (membuf) free(membuf);}

private:
        char *membuf; // Stable copy of the buffer address
};

//------------------------------------------------------------------------------
//! When the buffer is used for credentials, the start of the buffer always
//! holds the credential protocol name (e.g., krb4) as a string. The client
//! will get credentials and the size will be filled out so that the contents
//! of buffer can be easily transmitted to the server.
//------------------------------------------------------------------------------

typedef XrdSecBuffer XrdSecCredentials;

//------------------------------------------------------------------------------
//! When the buffer is used for parameters, the contents must be interpreted
//! in the context that it is used. For instance, the server will send the
//! security configuration parameters on the initial login. The format differs
//! from, say, the x.500 continuation paremeters that would be sent during
//! PKI authentication via an "authmore" return status.
//------------------------------------------------------------------------------

typedef XrdSecBuffer XrdSecParameters;
  
/******************************************************************************/
/*                        X r d S e c P r o t o c o l                         */
/******************************************************************************/
/*!
   The XrdSecProtocol is used to generate authentication credentials and to
   authenticate those credentials. For example, When a server indicates
   that authentication is needed (i.e., it returns security parameters), the 
   client must call XrdSecgetProtocol() to get an appropriate XrdSecProtocol
   (i.e., one specific to the authentication protocol that needs to be used). 
   Then the client can use the first form getCredentials() to generate the 
   appropriate identification information. On subsequent calls in response to
   "authmore" the client must use the second form, providing the additional
   parameters the the server sends. The server uses Authenticate() to verify
   the credentials. When XrdOucErrInfo is null (as it will usually be), error
   messages are routed to standard error. So, for example, a client would

   1) Call XrdSecGetProtocol() to get an appropriate XrdSecProtocol
      (i.e., one specific to the authentication protocol that needs to be used).
      Note that successive calls to XrdSecGetProtocol() using the same
      XrdSecParameters will use the subsequent protocol named in the list of
      protocols that the server returned. Failure is indicated when the list
      is exhausted or none of the protocols apply (which exhausts the list).

   2) Call getCredentials() without supplying any parameters so as to
      generate identification information and send them to the server.

   3) If the server indicates "authmore", call getCredentials() supplying
      the additional parameters sent by the server. The returned credentials
      are then sent to the server using the "authneticate" request code.

   4) Repeat step (3) as often as "authmore" is requested by the server.

   The server uses Authenticate() to verify the credentials and getParms()
   to generate initial parameters to start the authentication process. 

   When XrdOucErrInfo is null (as it will usually be), error messages are
   are routed to standard error.

   Server-side security is handled by the XrdSecService object and, while
   it uses XrdSecProtocol objects to perform authentication, the XrdSecService
   object is used to initialize the security environment and to generate
   the appropriate protocol objects at run-time. For an implementation, see
   XrdSecServer.hh and XrdSecServer.cc.

   MT Requirements: Must be MT_Safe.
*/

class XrdOucErrInfo;

class XrdSecProtocol
{
public:

//------------------------------------------------------------------------------
//! Structure holding the entity's identification. It is filled in by a
//! successful call to Authenticate() (i.e. it returns 0).
//------------------------------------------------------------------------------

XrdSecEntity               Entity;

//------------------------------------------------------------------------------
//! Authenticate a client.
//!
//! @param  cred   Credentials supplied by the client.
//! @param  parms  Place where the address of additional authentication data is
//!                to be placed for another autrhentication handshake.
//! @param  einfo  The error information object where error messages should be
//!                placed. The messages are returned to the client. Should einfo
//!                be null, messages should be written to stderr.
//!
//! @return > 0 -> parms  present (more authentication needed)
//!         = 0 -> Entity present (authentication suceeded)
//!         < 0 -> einfo  present (error has occured)
//------------------------------------------------------------------------------

virtual int                Authenticate  (XrdSecCredentials  *cred,
                                          XrdSecParameters  **parms,
                                          XrdOucErrInfo      *einfo=0)=0;

//------------------------------------------------------------------------------
//! Generate client credentials to be used in the authentication process.
//!
//! @param  parm   Pointer to the information returned by the server either in
//!                the initial login response or the authmore response.
//! @param  einfo  The error information object where error messages should be
//!                placed. The messages are returned to the client. Should einfo
//!                be null, messages should be written to stderr.
//!
//! @return Success: Pointer to credentials to sent to the server. The caller
//!                  is responsible for deleting the object.
//!         Failure: Null pointer with einfo, if supplied, containing the
//!                  reason for the failure.
//------------------------------------------------------------------------------

virtual XrdSecCredentials *getCredentials(XrdSecParameters   *parm=0,
                                          XrdOucErrInfo      *einfo=0)=0;

//------------------------------------------------------------------------------
//! Encrypt data in inbuff using the session key.
//!
//! @param  inbuff   buffer holding data to be encrypted.
//! @param  inlen    length of the data.
//! @param  outbuff  place where a pointer to the encrypted data is placed.
//!
//! @return < 0 Failed, the return value is -errno of the reason. Typically,
//!             -EINVAL    - one or more arguments are invalid.
//!             -NOTSUP    - encryption not supported by the protocol
//!             -ENOENT    - Context not innitialized
//!         = 0 Success, outbuff contains a pointer to the encrypted data.
//!             The caller is responsible for deleting the returned object.
//------------------------------------------------------------------------------

virtual int     Encrypt(const char    * /*inbuff*/,  // Data to be encrypted
                              int       /*inlen*/,   // Length of data in inbuff
                        XrdSecBuffer ** /*outbuff*/  // Returns encrypted data
                             ) {return -ENOTSUP;}

//------------------------------------------------------------------------------
//! Decrypt data in inbuff using the session key.
//!
//! @param  inbuff   buffer holding data to be decrypted.
//! @param  inlen    length of the data.
//! @param  outbuff  place where a pointer to the decrypted data is placed.
//!
//! @return < 0 Failed,the return value is -errno (see Encrypt).
//!         = 0 Success, outbuff contains a pointer to the decrypted data.
//!             The caller is responsible for deleting the returned object.
//------------------------------------------------------------------------------

virtual int     Decrypt(const char  * /*inbuff*/,   // Data to be decrypted
                              int     /*inlen*/,    // Length of data in inbuff
                      XrdSecBuffer ** /*outbuff*/   // Buffer for decrypted data
                              ) {return -ENOTSUP;}

//------------------------------------------------------------------------------
//! Sign data in inbuff using the session key.
//!
//! @param  inbuff   buffer holding data to be signed.
//! @param  inlen    length of the data.
//! @param  outbuff  place where a pointer to the signature is placed.
//!
//! @return < 0 Failed,the return value is -errno (see Encrypt).
//!         = 0 Success, outbuff contains a pointer to the signature.
//!             The caller is responsible for deleting the returned object.
//------------------------------------------------------------------------------

virtual int     Sign(const char  * /*inbuff*/,   // Data to be signed
                           int     /*inlen*/,    // Length of data in inbuff
                   XrdSecBuffer ** /*outbuff*/   // Buffer for the signature
                           ) {return -ENOTSUP;}

//------------------------------------------------------------------------------
//! Verify a signature using the session key.
//!
//! @param  inbuff   buffer holding data to be verified.
//! @param  inlen    length of the data.
//! @param  sigbuff  pointer to the signature data.
//! @param  siglen   length of the signature data.
//!
//! @return < 0 Failed,the return value is -errno (see Encrypt).
//!         = 0 Success, signature is correct.
//!         > 0 Failed to verify, signature does not match inbuff data.
//------------------------------------------------------------------------------

virtual int     Verify(const char  * /*inbuff*/,   // Data to be decrypted
                             int     /*inlen*/,    // Length of data in inbuff
                       const char  * /*sigbuff*/,  // Buffer for signature
                             int     /*siglen*/)   // Length if signature
                      {return -ENOTSUP;}

//------------------------------------------------------------------------------
//! Get the current encryption key (i.e. session key)
//!
//! @param  buff     buffer to hold the key, and may be null.
//! @param  size     size of the buffer.
//!
//! @returns <  0 Failed, returned value if -errno (see Encrypt)
//!          >= 0 The size of the encyption key. The supplied buffer of length
//!               size hold the key. If the buffer address is supplied, the
//!               key is placed in the buffer.
//!
//------------------------------------------------------------------------------

virtual int     getKey(char * /*buff*/=0, int /*size*/=0) {return -ENOTSUP;}

//------------------------------------------------------------------------------
//! Set the current encryption key
//!
//! @param  buff     buffer that holds the key.
//! @param  size     size of the key.
//!
//! @returns: < 0 Failed, returned value if -errno (see Encrypt)
//!           = 0 The new key has been set.
//------------------------------------------------------------------------------

virtual int     setKey(char * /*buff*/, int /*size*/) {return -ENOTSUP;}

//------------------------------------------------------------------------------
//! Delete the protocol object. DO NOT use C++ delete() on this object.
//------------------------------------------------------------------------------

virtual void    Delete()=0; // Normally does "delete this"

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

              XrdSecProtocol(const char *pName) : Entity(pName) {}
protected:

//------------------------------------------------------------------------------
//! Destructor (prevents use of direct delete).
//------------------------------------------------------------------------------

virtual      ~XrdSecProtocol() {}
};
 
/******************************************************************************/
/*           P r o t o c o l   N a m i n g   C o n v e n t i o n s            */
/******************************************************************************/

/*! Each specific protocol resides in a shared library named "libXrdSec<p>.so"
    where <p> is the protocol identifier (e.g., krb5, gsi, etc). The library
    contains a class derived from the XrdSecProtocol object. The library must
    also contain a three extern "C" functions:
    1) XrdSecProtocol<p>Init()
       Called for one-time protocol ininitialization.
    2) XrdSecProtocol<p>Object()
       Called for protocol object instantiation.
    3) XrdSecProtocol<p>Object_
       Inspected for the protocol object xrootd version number used in
       compilation; defined by the XrdVERSIONINFO macro (see later comments).
*/

//------------------------------------------------------------------------------
//! Perform one-time initialization when the shared library containing the
//! the protocol is loaded.
//!
//! @param  who    contains 'c' when called on the client side and 's' when
//!                called on the server side.
//! @param  parms  when who == 'c' (client) the pointer is null.
//!                when who == 's' (server) argument points to any parameters
//!                specified in the config file with the seclib directive. If no
//!                parameters were specified, the pointer may be null.
//! @param  einfo  The error information object where error messages should be
//!                placed. Should einfo be null, messages should be written to
//!                stderr.
//!
//! @return Success: The initial security token to be sent to the client during
//!                  the login phase (i.e. authentication handshake). If no
//!                  token need to be sent, a pointer to null string should be
//!                  returned.
//!         Failure: A null pointer with einfo, if supplied, holding the reason
//!                  for the failure.
//!
//! Notes:   1) Function is called ince in single-thread mode and need not be
//!             thread-safe.
//------------------------------------------------------------------------------

/*! extern "C" char *XrdSecProtocol<p>Init (const char     who,
                                            const char    *parms,
                                            XrdOucErrInfo *einfo) {. . . .}
*/

//------------------------------------------------------------------------------
//! Obtain an instance of a protocol object.
//!
//! @param  who      contains 'c' when called on the client side and 's' when
//!                  called on the server side.
//! @param  host     The client's host name or the IP address as text. An IP
//!                  may be supplied if the host address is not resolvable. Use
//!                  endPoint to get the hostname only if it's actually needed.
//! @param  endPoint the XrdNetAddrInfo object describing the end-point. When
//!                  who == 'c' this is the client, otherwise it is the server.
//! @param  parms    when who == 'c' (client) points to the parms sent by the
//!                  server upon the initial handshake (see Init() above).
//!                  when who == 's' (server) is null.
//! @param  einfo    The error information object where error messages should be
//!                  placed. Should einfo be null, messages should be written to
//!                  stderr.
//!
//! @return Success: A pointer to the protocol object.
//!         Failure: A null pointer with einfo, if supplied, holding the reason
//!                  for the failure.
//!
//! Notes    1) Warning! The protocol *must* allow both 'c' and 's' calls within
//!             the same execution context. This occurs when a server acts like
//!             a client.
//!          2) This function must be thread-safe.
//!          3) Additionally, you *should* declare the xrootd version you used
//!             to compile your plug-in using XrdVERSIONINFO where <name> is
//!             the 1- to 15-character unquoted name of your plugin. This is a
//!             mandatory requirement!
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdSecProtocol<p>Object,<name>);

    extern "C" XrdSecProtocol *XrdSecProtocol<p>Object
                                              (const char              who,
                                               const char             *hostname,
                                                     XrdnetAddrInfo   &endPoint,
                                               const char             *parms,
                                                     XrdOucErrInfo    *einfo
                                              ) {. . .}
*/
  
/******************************************************************************/
/*            P r o t o c o l   O b j e c t   M a n a g e m e n t             */
/******************************************************************************/

//! The following extern "C" functions provide protocol object management. That
//! is, coordinating the use of the right authentication protocol between the
//! client and server. The default implementation may be replaced via a plugin.
  
/******************************************************************************/
/*                     X r d S e c G e t P r o t o c o l                      */
/*                                                                            */
/*                  C l i e n t   S i d e   U S e   O n l y                   */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Create a client security context and get a supported XrdSecProtocol object
//! for one of the protocols suggested by the server and possibly based on the
//! server's hostname or host address, as needed.
//!
//! @param  hostname The client's host name or the IP address as text. An IP
//!                  may be supplied if the host address is not resolvable. Use
//!                  endPoint to get the hostname only if it's actually needed.
//! @param  endPoint the XrdNetAddrInfo object describing the server end-point.
//! @param  sectoken The security token supplied by the server.
//! @param  einfo    The structure to record any error messages. These are
//!                  normally sent to the client. If einfo is a null pointer,
//!                  the messages should be sent to standard error.
//!
//! @return Success: Address of protocol object to be used for authentication.
//!                  If cred was null, a host protocol object should be
//!                  returned if so allowed. The object's delete method should
//!                  be called to release the storage.
//!         Failure: Null, no protocol can be returned. The einfo parameter,
//!                  if supplied, has the reason.
//!
//! Notes:   1) There should be one protocol object per physical TCP/IP
//!             connections.
//!          2) When the connection is closed, the protocol's Delete() method
//!             should be called to properly delete the object.
//!          3) The method and the returned object should be MT-safe.
//!          4) When replacing the default implementation with a plug-in the
//!             extern "C" function below must exist in your shared library.
//!          5) Additionally, you *should* declare the xrootd version you used
//!             to compile your plug-in using XrdVERSIONINFO where <name> is
//!             the 1- to 15-character unquoted name of your plugin. This is a
//!             mandatory requirement!
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Typedef to simplify the encoding of methods returning XrdSecProtocol.
//------------------------------------------------------------------------------

typedef XrdSecProtocol *(*XrdSecGetProt_t)(const char *,
                                           XrdNetAddrInfo &,
                                           XrdSecParameters &,
                                           XrdOucErrInfo *);

/*!
#include "XrdVersion.hh"
XrdVERSIONINFO(XrdSecGetProtocol,<name>);

extern "C" XrdSecProtocol *XrdSecGetProtocol(const char             *hostname,
                                                   XrdNetAddrInfo   &endPoint,
                                                   XrdSecParameters &sectoken,
                                                   XrdOucErrInfo    *einfo=0)
                                            {....}
*/
 
/******************************************************************************/
/*                         X r d S e c S e r v i c e                          */
/*                                                                            */
/*                  S e r v e r   S i d e   U s e   O n l y                   */
/******************************************************************************/
  
/*! The XrdSecService object is the the object that the server uses to obtain
    parameters to be passed to the client on initial contact and to create the
    appropriate protocol on the initial receipt of the client's credentials.
    Server-side processing is a bit more complicated because the set of valid
    protocols needs to be configured and that configuration needs to be supplied
    to the client so that both can agree on a compatible protocol. This object
    is created via a call to XrdSecgetService, defined later on. You may replace
    the default implementation by defining a plugin via the seclib directive.

    Warning: The XrdSecService object as well as any objects returned by it
             should be MT-safe.
*/
  
class XrdSecService
{
public:

//------------------------------------------------------------------------------
//! Obtain security parameters to be sent to the client upon initial contact.
//!
//! @param  size     Where the length of the return parameters are to be placed.
//! @param  endPoint The client's address information. It may also be a null
//!                  pointer if the client's host is immaterial.
//!
//! @return EITHER   The address of the parameter string (which may be
//!                  host-specific if hname was supplied). The length of the
//!                  string must be returned in size parameter.
//!         OR       A null pointer if authentication need not occur for the
//!                  client. The size parameter should be set to zero as well.
//------------------------------------------------------------------------------

virtual const char     *getParms(int &size, XrdNetAddrInfo *endPoint=0) = 0;

//------------------------------------------------------------------------------
//! Obtain a protocol object suitable for authentication based on cred and
//! possibly based on the hostname or host address, as needed.
//!
//! @param  host     The client's host name or the IP address as text. An IP
//!                  may be supplied if the host address is not resolvable or
//!                  resolution has been suppressed (i.e. nodnr). Use endPoint
//!                  to get the hostname if it's actually needed.
//! @param  endPoint the XrdNetAddrInfo object describing the client end-point.
//! @param  cred     The initial credentials supplied by the client, the pointer
//!                  may be null if the client did not supply credentials.
//! @param  einfo    The structure to record any error messages. These are
//!                  normally sent to the client. If einfo is a null pointer,
//!                  the messages should be sent to standard error via an
//!                  XrdSysError object using the supplied XrdSysLogger when the
//!                  the plugin was initialized.
//!
//! @return Success: Address of protocol object to be used for authentication.
//!                  If cred was null, a host protocol object shouldpo be
//!                  returned if so allowed.
//!         Failure: Null, no protocol can be returned. The einfo parameter,
//!                  if supplied, has the reason.
//------------------------------------------------------------------------------

virtual XrdSecProtocol *getProtocol(const char              *host,    // In
                                          XrdNetAddrInfo    &endPoint,// In
                                    const XrdSecCredentials *cred,    // In
                                          XrdOucErrInfo     *einfo)=0;// Out

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

                        XrdSecService() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual                ~XrdSecService() {}
};
  
/******************************************************************************/
/*                      X r d g e t S e c S e r v i c e                       */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Create a server's security context and get an XrdSecService object.
//!
//! @param  lp       The logging object that should be associated with an
//!                  XrdSysError object to route messages.
//! @param  cfn      The configuration file name.
//!
//! @return Success: Pointer to the XrdSecService object. This object is never
//!                  deleted by the server.
//!         Failure: Null pointer which causes initialization to fail.
//!
//! Notes:   1) The XrdSecSgetService function is called once during server
//!             initialization. So, it need not be thread safe.
//!          2) If you choose to replace the default implementation with your
//!             own plugin, the extern "C" function below must be defined in
//!             your plugin shared library.
//!          3) Additionally, you *should* declare the xrootd version you used
//!             to compile your plug-in using XrdVERSIONINFO where <name> is
//!             the 1- to 15-character unquoted name of your plugin. This is a
//!             mandatory requirement!
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//! Typedef to simplify the encoding of methods returning XrdSecService.
//------------------------------------------------------------------------------

class XrdSysLogger;
typedef XrdSecService  *(*XrdSecGetServ_t)(XrdSysLogger *, const char *);

/*!
#include "XrdVersion.hh"
XrdVERSIONINFO(XrdSecgetService,<name>);

extern "C" XrdSecService *XrdSecgetService(XrdSysLogger *lp, const char *cfn)
*/
#endif
