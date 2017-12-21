//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2017 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: May 2017
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------











#ifndef __XRDHTTPEXTHANDLER_H__
#define __XRDHTTPEXTHANDLER_H__

#include <map>
#include <string>

class XrdLink;
class XrdSecEntity;
class XrdHttpReq;
class XrdHttpProtocol;

// This class summarizes the content of a request, for consumption by an external plugin
class XrdHttpExtReq {
private:
  XrdHttpProtocol *prot;
  
public:
  XrdHttpExtReq(XrdHttpReq *req, XrdHttpProtocol *pr);
  
  std::string verb, resource;
  std::map<std::string, std::string> &headers;
  
  std::string clientdn, clienthost, clientgroups;
  long long length;
  
  // A view of the XrdSecEntity associated with the request.
  const XrdSecEntity &GetSecEntity() const;

  /// Get a pointer to data read from the client, valid for up to blen bytes from the buffer. Returns the validity
  int BuffgetData(int blen, char **data, bool wait);

  /// Sends a basic response. If the length is < 0 then it is calculated internally
  int SendSimpleResp(int code, const char *desc, const char *header_to_add, const char *body, long long bodylen);

  /// Starts a chunked response; body of request is sent over multiple parts using the SendChunkResp
  //  API.
  int StartChunkedResp(int code, const char *desc, const char *header_to_add);

  /// Send a (potentially partial) body in a chunked response; invoking with NULL body
  //  indicates that this is the last chunk in the response.
  int ChunkResp(const char *body, long long bodylen);
};


/// Base class for a plugin that can handle requests for urls that match
/// a certain set of prefixes
class XrdHttpExtHandler {
  
public:
  
  /// Tells if the incoming path is recognized as one of the paths that have to be processed
  // e.g. applying a prefix matching scheme or whatever
  virtual bool MatchesPath(const char *verb, const char *path) = 0;
  
  /// Process an HTTP request and send the response using the calling
  ///  XrdHttpProtocol instance directly
  /// Returns 0 if ok, non0 if errors
  virtual int ProcessReq(XrdHttpExtReq &) = 0;
  
  /// Initializes the external request handler
  virtual int Init(const char *cfgfile) = 0;
  
  //------------------------------------------------------------------------------
  //! Constructor
  //------------------------------------------------------------------------------
  
  XrdHttpExtHandler() {}
  
  //------------------------------------------------------------------------------
  //! Destructor
  //------------------------------------------------------------------------------
  
  virtual     ~XrdHttpExtHandler() {}
};

/******************************************************************************/
/*                    X r d H t t p G e t E x t H a n d l e   r               */
/******************************************************************************/

//------------------------------------------------------------------------------
//! Obtain an instance of the XrdHttpExtHandler object.
//!
//! This extern "C" function is called when a shared library plug-in containing
//! implementation of this class is loaded. It must exist in the shared library
//! and must be thread-safe.
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  confg -> Name of the configuration file that was used. This pointer
//!                  may be null though that would be impossible.
//! @param  parms -> Argument string specified on the namelib directive. It may
//!                  be null or point to a null string if no parms exist.
//! @param  myEnv -> Environment variables for configuring the external handler;
//!                  it my be null.
//!
//! @return Success: A pointer to an instance of the XrdHttpSecXtractor object.
//!         Failure: A null pointer which causes initialization to fail.
//!

//------------------------------------------------------------------------------

class XrdSysError;
class XrdOucEnv;

#define XrdHttpExtHandlerArgs XrdSysError       *eDest, \
                              const char        *confg, \
                              const char        *parms, \
                              XrdOucEnv         *myEnv

extern "C" XrdHttpExtHandler *XrdHttpGetExtHandler(XrdHttpExtHandlerArgs);

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
 *    XrdVERSIONINFO(XrdHttpGetExtHandler,<name>);
 * 
 *    where <name> is a 1- to 15-character unquoted name identifying your plugin.
 */
#endif
