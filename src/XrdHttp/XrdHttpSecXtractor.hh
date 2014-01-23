//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: Nov 2012
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











#ifndef __XRDHTTPSECXTRACTOR_H__
#define __XRDHTTPSECXTRACTOR_H__

class XrdLink;
class XrdSecEntity;

class XrdHttpSecXtractor
{
public:

    // Extract security info from the link instaaance, and use it to populate
    // the given XrdSec instance
    virtual int GetSecData(XrdLink *, XrdSecEntity &, SSL *) = 0;


    // Initializes an ssl ctx
    virtual int Init(SSL_CTX *, int) = 0;
//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

             XrdHttpSecXtractor() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdHttpSecXtractor() {}
};

/******************************************************************************/
/*                    X r d H t t p G e t S e c X t r a c t o r               */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the XrdHttpSecXtractor object.
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
//!
//! @return Success: A pointer to an instance of the XrdHttpSecXtractor object.
//!         Failure: A null pointer which causes initialization to fail.
//!

//------------------------------------------------------------------------------

class XrdSysError;

#define XrdHttpSecXtractorArgs XrdSysError       *eDest, \
                               const char        *confg, \
                               const char        *parms

extern "C" XrdHttpSecXtractor *XrdHttpGetSecXtractor(XrdHttpSecXtractorArgs);

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdHttpGetSecXtractor,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
