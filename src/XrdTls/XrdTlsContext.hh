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

#include <string>
  
//----------------------------------------------------------------------------
// Forward declarations
//----------------------------------------------------------------------------

struct XrdTlsContextImpl;
struct XrdTlsSocket;

/******************************************************************************/
/*                         X r d T l s C o n t e x t                          */
/******************************************************************************/

class XrdTlsContext
{
public:

//------------------------------------------------------------------------
//! Obtain SSL context attached to this object
//!
//! @return Pointer to the SSL context. Nil indicates failure.
//------------------------------------------------------------------------

void    *Context();

//------------------------------------------------------------------------
//! Get parameters used to create the context.
//!
//! @return Pointer to a structure contaning initialization parameters.
//------------------------------------------------------------------------

struct CTX_Params
      {std::string cert;   //!< -> certificate path.
       std::string pkey;   //!> -> private key path.
       std::string cadir;  //!> -> ca cert directory.
       std::string cafile; //!> -> ca cert file.
       int         opts;   //!> Options as passed to the constructor.
       int         rsvd;

       CTX_Params() : opts(0), rsvd(0) {}
      ~CTX_Params() {}
      };

const
CTX_Params     *GetParams();

//------------------------------------------------------------------------
//! Simply initialize the TLS library.
//!
//! @return =0       Library initialized.
//!         !0       Library not initialized, return string indicates why.
//!
//! @note Init() is implicitly called by the contructor. Use this method
//!              to use the TLS libraries without instantiating a context.
//------------------------------------------------------------------------
static
const char     *Init();

//------------------------------------------------------------------------
//! Check if certificates are being verified.
//!
//! @return True if certificates are being verified, false otherwise.
//------------------------------------------------------------------------

       bool     x509Verify();

//------------------------------------------------------------------------
//! Constructor. Note that you should use Context() to determine if
//!              construction was successful. A nil return indicates failure.
//!
//! @param  cert     Pointer to the certificate file to be used. If nil,
//!                  a generic context is created for client use.
//! @param  key      Pointer to the private key flle to be used. It must
//!                  correspond to the certificate file. If nil, it is
//!                  assumed that the key is contained in the cert file.
//! @param  cadir    path to the directory containing the CA certificates.
//! @param  cafile   path to the file containing the CA certificates.
//! @param  vdepth   The maximum depth of the certificate chain that must
//!                  validated.
//! @param  opts     Processing options (or'd bitwise):
//!                  debug   - produce the maximum amount of messages.
//!                  dnsok   - trust DNS when verifying hostname.
//!                  hsto    - the handshake timeout value in seconds.
//!                  logVF   - Turn on verification failure logging.
//!                  servr   - This is a server-side context and x509 peer
//!                            certificate validation may be turned off.
//!                  vdept   - The maximum depth of the certificate chain that
//!                            must be validated.
//!
//! @note   a) If neither cadir nor cafile is specified, certificate validation
//!            is *not* performed if and only if the servr option is specified.
//!            Otherwise, the cadir value is obtained from the X509_CERT_DIR
//!            envar and the cafile value is obtained from the X509_CERT_File
//!            envar. If both are nil, context creation fails.
//!         b) You should immediately call Context() after instantiating this
//!            object. A return value of zero means that construction failed.
//!         c) Failure messages are routed to the message callback function
//!            during construction.
//------------------------------------------------------------------------

static const int hsto  = 0x000000ff; //!< Mask to isolate the hsto value
static const int vdept = 0x0000ff00; //!< Mask to isolate the actual value
static const int vdepS = 8;          //!< Bits to shift vdept value
static const int logVF = 0x40000000; //!< Enable verification failure logging
static const int debug = 0x20000000; //!< Output full ssl messages for debuging
static const int servr = 0x10000000; //!< Phis is a server-side context
static const int dnsok = 0x08000000; //!< Trust DNS for host verification

       XrdTlsContext(const char *cert=0,  const char *key=0,
                     const char *cadir=0, const char *cafile=0,
                     int opts=0);

//------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------

      ~XrdTlsContext();

//------------------------------------------------------------------------
//! Disallow any copies of this object
//------------------------------------------------------------------------

      XrdTlsContext( const XrdTlsContext  &ctx ) = delete;
      XrdTlsContext(       XrdTlsContext &&ctx ) = delete;

      XrdTlsContext& operator=( const XrdTlsContext  &ctx ) = delete;
      XrdTlsContext& operator=(       XrdTlsContext &&ctx ) = delete;

private:
   XrdTlsContextImpl *pImpl;
};
#endif // __XRD_TLSCONTEXT_HH__
