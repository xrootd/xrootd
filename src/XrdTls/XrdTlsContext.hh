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

class  XrdSysLogger;
struct XrdTlsContextImpl;
struct XrdTlsSocket;

/******************************************************************************/
/*                         X r d T l s C o n t e x t                          */
/******************************************************************************/

class XrdTlsContext
{
public:

//------------------------------------------------------------------------
//! Clone a new context from this context.
//!
//! @return Upon success, the pointer to a new XrdTlsContext is returned.
//!         Upon failure, a nil pointer is returned.
//!
//! @note The session cache settings become scSrvr with no identifier,
//!       crl refresh is set off, and the default cipher list is used
//!       for the cloned context.
//------------------------------------------------------------------------

XrdTlsContext *Clone();

//------------------------------------------------------------------------
//! Get the underlying context (should not be used).
//!
//! @return Pointer to the underlying context.
//------------------------------------------------------------------------

void          *Context();

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
//! Determine if this object was correctly built.
//!
//! @return True if this object is usuable and false otherwise.
//------------------------------------------------------------------------

bool            isOK();

//------------------------------------------------------------------------
//! Apply this context to obtain a new SSL session.
//!
//! @return A pointer to a new SSL session if successful and nil otherwise.
//------------------------------------------------------------------------

void           *Session();

//------------------------------------------------------------------------
//! Get or set session cache parameters for generated sessions.
//!
//! @param  id       The identifier to be used (may be nil to keep setting).
//! @param  idlen    The length of the identifier (may be zero as above).
//! @param  opts     One or more bit or'd options (see below).
//!
//! @return The cache settings prior to any changes are returned. When setting
//!         the id, the scIdErr may be returned if the name is too long.
//!         If the context has been pprroperly initialized, zero is returned.
//------------------------------------------------------------------------

static const int scNone = 0x00000000; //!< Do not change any option settings
static const int scOff  = 0x00010000; //!< Turn off cache
static const int scSrvr = 0x00020000; //!< Turn on  cache server mode (default)
static const int scClnt = 0x00040000; //!< Turn on  cache client mode
static const int scKeep = 0x40000000; //!< Info: TLS-controlled flush disabled
static const int scIdErr= 0x80000000; //!< Info: Id not set, is too long
static const int scFMax = 0x00007fff; //!< Maximum flush interval in seconds
                                      //!  When 0 keeps the current setting

      int       SessionCache(int opts=scNone, const char *id=0, int idlen=0);

//------------------------------------------------------------------------
//! Set allowed ciphers for this context.
//!
//! @param  ciphers  The colon separated list of allowable ciphers.
//!
//! @return True if at least one cipher can be used; false otherwise. When
//!         false is reurned, this context is no longer usable.
//------------------------------------------------------------------------

bool            SetContextCiphers(const char *ciphers);

//------------------------------------------------------------------------
//! Set allowed default ciphers.
//!
//! @param  ciphers  The colon separated list of allowable ciphers.
//------------------------------------------------------------------------
static
void            SetDefaultCiphers(const char *ciphers);

//------------------------------------------------------------------------
//! Set CRL refresh time. By default, CRL's are not refreshed.
//!
//! @param  refsec   The number of seconds between refreshes. Minumum is 30.
//!                  However, if the value is <0, refreshing is stopped.
//!
//! @return True if the CRL refresh thread was started; false otherwise.
//------------------------------------------------------------------------

      bool      SetCrlRefresh(int refsec);

//------------------------------------------------------------------------
//! Check if certificates are being verified.
//!
//! @return True if certificates are being verified, false otherwise.
//------------------------------------------------------------------------

       bool     x509Verify();

//------------------------------------------------------------------------
//! Constructor. Note that you should use isOK() to determine if construction
//!              was successful. A false return indicates failure.
//!
//! @param  cert     Pointer to the certificate file to be used. If nil,
//!                  a generic context is created for client use.
//! @param  key      Pointer to the private key flle to be used. It must
//!                  correspond to the certificate file. If nil, it is
//!                  assumed that the key is contained in the cert file.
//! @param  cadir    path to the directory containing the CA certificates.
//! @param  cafile   path to the file containing the CA certificates.
//! @param  opts     Processing options (or'd bitwise):
//!                  dnsok   - trust DNS when verifying hostname.
//!                  hsto    - the handshake timeout value in seconds.
//!                  logVF   - Turn on verification failure logging.
//!                  servr   - This is a server-side context and x509 peer
//!                            certificate validation may be turned off.
//!                  vdept   - The maximum depth of the certificate chain that
//!                            must be validated (max is 255).
//!
//! @note   a) If neither cadir nor cafile is specified, certificate validation
//!            is *not* performed if and only if the servr option is specified.
//!            Otherwise, the cadir value is obtained from the X509_CERT_DIR
//!            envar and the cafile value is obtained from the X509_CERT_File
//!            envar. If both are nil, context creation fails.
//!         b) Additionally for client-side contructions, if cert or key is
//!            not specified their locations come from X509_USER_PROXY and
//!            X509_USER_KEY. These may be nil in which case a generic
//!            context is created with a local key-pair and no certificate.
//!         c) You should immediately call isOK() after instantiating this
//!            object. A return value of false means that construction failed.
//!         d) Failure messages are routed to the message callback function
//!            during construction.
//------------------------------------------------------------------------

static const int hsto  = 0x000000ff; //!< Mask to isolate the hsto value
static const int vdept = 0x0000ff00; //!< Mask to isolate the actual value
static const int vdepS = 8;          //!< Bits to shift vdept value
static const int logVF = 0x40000000; //!< Enable verification failure logging
static const int servr = 0x20000000; //!< This is a server-side context
static const int dnsok = 0x10000000; //!< Trust DNS for host verification

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
