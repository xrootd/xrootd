#ifndef __XRD_TLSCONTEXT_HH__
#define __XRD_TLSCONTEXT_HH__
//------------------------------------------------------------------------------
// Copyright (c) 2011-2018 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <simonm@cern.ch>
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

#include <openssl/ssl.h>
#include <string>
  
/******************************************************************************/
/*                         X r d T l s C o n t e x t                          */
/******************************************************************************/
  
class XrdSysError;

class XrdTlsContext
{
public:

//------------------------------------------------------------------------
//! Obtain SSL context attached to this object
//!
//! @return Pointer to the SSL context to be used by a server.
//------------------------------------------------------------------------

       SSL_CTX *Context() {return ctx;}

//------------------------------------------------------------------------
//! Retrieve all errors encountered so far.
//!
//! @param  pfx      The message prefix to be used (i.e. pfx: msg).
//!
//! @return A string containing newline separated messages.
//------------------------------------------------------------------------

std::string     GetErrs(const char *pfx=0);

//------------------------------------------------------------------------
//! Print all errors encountered so far.
//!
//! @param  pfx      The message prefix to be used (i.e. pfx: msg).
//! @param  eDest    Message routing object. If nil, messages are routed
//!                  to standard error.
//------------------------------------------------------------------------

       void     PrintErrs(const char *pfx="Tls", XrdSysError *eDest=0);

//------------------------------------------------------------------------
//! Constructor. Note that you should use Context() to determine if
//!              construction was successful. A nil return indicates failure.
//!
//! @param  cert     Pointer to the certificate file to be used. If nil,
//!                  a generic client oriented context is created.
//! @param  key      Pointer to the private key flle to be used. It must
//!                  correspond to the certificate file. If nil, it is
//!                  assumed that the key is contained in the cert file.
//! @param  prot     The protocols that the context should support. Choose
//!                  one of the enums defined below. Note that doSSL includes
//!                  TLS but deprecates SSL protocols mainly for https support.
//------------------------------------------------------------------------

       enum Protocol {doSSL = 0, doTLS};

       XrdTlsContext(const char *cert=0, const char *key=0, Protocol prot=doTLS);

//------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------
      ~XrdTlsContext();

//------------------------------------------------------------------------
//! Conversion to SSL_CTX
//------------------------------------------------------------------------

operator SSL_CTX*() {return ctx;}

//------------------------------------------------------------------------
//! Disallow any copies of this object
//------------------------------------------------------------------------

      XrdTlsContext( const XrdTlsContext  &ctx ) = delete;
      XrdTlsContext(       XrdTlsContext &&ctx ) = delete;

      XrdTlsContext& operator=( const XrdTlsContext  &ctx ) = delete;
      XrdTlsContext& operator=(       XrdTlsContext &&ctx ) = delete;

private:

   SSL_CTX    *ctx;
   const char *eText;
};
#endif // __XRD_TLSCONTEXT_HH__
