#ifndef __XRD_TLS_IO_HH__
#define __XRD_TLS_IO_HH__
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

class XrdSysError;
class XrdTlsContext;

//----------------------------------------------------------------------------
//! Socket wrapper for TLS I/O
//----------------------------------------------------------------------------

class XrdTlsConnection
{
public:

enum RW_Mode
{
  TLS_RNB_WNB,   //!< Non-blocking read non-blocking write
  TLS_RNB_WBL,   //!< Non-blocking read     blocking write
  TLS_RBL_WNB,   //!<     blocking read non-blocking write
  TLS_RBL_WBL    //!<     blocking read     blocking write
};

enum HS_Mode
{
  TLS_HS_BLOCK,  //!< Always block during handshake
  TLS_HS_NOBLK,  //!< Do not block during handshake
  TLS_HS_XYBLK   //!< Block during handshake if it conflicts with request
};

//------------------------------------------------------------------------
//! Constructor - creates specified mode TLS I/O wrapper for given socket
//! file descriptor. Note this constructor throws an exception should any
//! error be encountered. Use the parameterless constructor if you wish
//! to avoid handling exceptions. When an exception is thrown, you should
//! print all associated errors by calling GetErrs() or PrintErrs().
//!
//! @param  ctx      - the context for the connection. Be aware that a
//!                    context can be associated wity multiple connections.
//! @param  sfd      - the file descriptor associated with the connection.
//! @param  rwm      - One of the above enums describing how connection I/O
//!                    should be handled.
//! @param  hsm      - One of the above enums describing how handshakes during
//!                    read/write calls should be handled.
//! @param  isClient - When true  initialize for client use.
//!                    Otherwise, initialize for server use.
//------------------------------------------------------------------------

  XrdTlsConnection( XrdTlsContext &ctx, int sfd, RW_Mode rwm,
                                                 HS_Mode hsm, bool isClient );

//------------------------------------------------------------------------
//! Constructor - reserves space for a TLS I/O wrapper. Use the Init()
//! method to fully initialize this object. If an errror occurs, you should
//! print all associated errors by calling ctx.GetErrs() or ctx.PrintErrs().
//------------------------------------------------------------------------

  XrdTlsConnection() : tlsctx(0), ssl(0), sFD(-1), hsDone(false),
                       cAttr(0), hsMode(0) {}

//------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------

  ~XrdTlsConnection() { if (ssl) Shutdown(true); }

//------------------------------------------------------------------------
//! Accept an incoming TLS connection
//------------------------------------------------------------------------

  int Accept();

//------------------------------------------------------------------------
//! Establish a TLS connection
//------------------------------------------------------------------------

  int Connect();

//------------------------------------------------------------------------
//! Obtain context associated with this connection.
//!
//! @return : Tls connection object
//------------------------------------------------------------------------

  XrdTlsContext *Context() {return tlsctx;}

//------------------------------------------------------------------------
//! Retrieve all errors encountered so far.
//!
//! @param  pfx      The message prefix to be used (i.e. pfx: msg).
//!
//! @return A string containing newline separated messages.
//------------------------------------------------------------------------

  std::string GetErrs(const char *pfx=0);

//------------------------------------------------------------------------
//! Initialize this object to handle the specified TLS I/O mode for the
//! given file descriptor. To maintain sanity, an error occurs if the
//! this object is already associated with a connection (use Disconnect()
//! before calling Init() just to keep yourself honest). Should an error
//! occur, you should print all associated errors via ctx.GetErrs() or
//! ctx.PrintErrs(). Errors are thread-specific.
//!
//! @param  ctx      - the context for the connection. Be aware that a
//!                    context can be associated wity multiple connections.
//! @param  sfd      - the file descriptor associated with the connection.
//! @param  rwm      - One of the above enums describing how connection I/O
//!                    should be handled.
//! @param  hsm      - One of the above enums describing how handshakes during
//!                    read/write calls should be handled.
//! @param  isClient - When true  initialize for client use.
//!                    Otherwise, initialize for server use.
//!
//! @return =0       - object has been initialized.
//! @return !0       - an error occurred, the return value is a pointer to a
//!                    message summarizing the error. This message is the same
//!                    as would be thrown by the parameterized constructor.
//------------------------------------------------------------------------

  const char *Init( XrdTlsContext &ctx, int sfd, RW_Mode rwm,
                                                 HS_Mode hsm, bool isClient );
//------------------------------------------------------------------------
//! Print all errors encountered so far.
//!
//! @param  pfx      The message prefix to be used (i.e. pfx: msg).
//! @param  eDest    Message routing object. If nil, messages are routed
//!                  to standard error.
//------------------------------------------------------------------------

  void PrintErrs(const char *pfx="TlsCon", XrdSysError *eDest=0);

//------------------------------------------------------------------------
//! Read from the TLS connection
//! (If necessary, will establish a TLS/SSL session.)
//------------------------------------------------------------------------

  int Read( char *buffer, size_t size, int &bytesRead );

//------------------------------------------------------------------------
//! Tear down a TLS connection
//!
//! @param  force when true, the connection will not wait for a proper
//!               shutdown (i.e. termination handshake).
//------------------------------------------------------------------------

  void Shutdown(bool force=false);

//------------------------------------------------------------------------
//! Write to the TLS connection
//! (If necessary, will establish a TLS/SSL session.)
//------------------------------------------------------------------------

  int Write( char *buffer, size_t size, int &bytesWritten );

//------------------------------------------------------------------------
//! @return  :  true if the TLS/SSL session is not established yet,
//!             false otherwise
//------------------------------------------------------------------------

  bool NeedHandShake()
  {
    return !hsDone;
  }

//------------------------------------------------------------------------
//! Conversion to native OpenSSL connection object
//!
//! @return : SSL connection object
//------------------------------------------------------------------------

  operator SSL*()
  {
    return ssl;
  }

private:

bool Wait4OK(bool wantRead);

//------------------------------------------------------------------------
//! The TLS/SSL object.
//------------------------------------------------------------------------

  XrdTlsContext *tlsctx;
  SSL           *ssl;

//------------------------------------------------------------------------
//! The socket file descriptor
//------------------------------------------------------------------------

  int   sFD;

//------------------------------------------------------------------------
//! True if TSL/SSL handshake has been done, flase otherwise.
//------------------------------------------------------------------------

  bool  hsDone;

// Additional attributes
//
  char cAttr;
  static const int isServer  = 0x01;
  static const int rBlocking = 0x02;
  static const int wBlocking = 0x03;

  char hsMode;
  static const int noBlock = 0;
  static const int rwBlock = 'a';
  static const int xyBlock = 'x';
};

#endif // __XRD_CL_TLS_HH__
