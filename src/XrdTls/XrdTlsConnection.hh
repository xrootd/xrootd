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
//! method to fully initialize this object.
//------------------------------------------------------------------------

  XrdTlsConnection() : tlsctx(0), ssl(0), traceID(0), sFD(-1),
                       hsWait(15), hsDone(false), fatal(false),
                       cOpts(0), cAttr(0), hsMode(0) {}

//------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------

  ~XrdTlsConnection() { if (ssl) Shutdown(sdForce); }

//------------------------------------------------------------------------
//! Accept an incoming TLS connection
//------------------------------------------------------------------------

  int Accept();

//------------------------------------------------------------------------
//! Establish a TLS connection
//!
//! @param  thehost  - The expected hostname. If nil the peername is not
//!                    verified.
//! @return 0 upon success or an SSL error code upon failure.
//------------------------------------------------------------------------

  int Connect(const char *thehost=0);

//------------------------------------------------------------------------
//! Obtain context associated with this connection.
//!
//! @return : Tls connection object
//------------------------------------------------------------------------

  XrdTlsContext *Context() {return tlsctx;}

//------------------------------------------------------------------------
//! Convert SSL error code to a reason string.
//!
//! @param  sslerr - The error number (i.e. output of SSL_get_error()).
//!
//! @return A string describing the error.
//------------------------------------------------------------------------

  std::string Err2Text(int sslerr);


//------------------------------------------------------------------------
//! Initialize this object to handle the specified TLS I/O mode for the
//! given file descriptor. Should an error occur, messages are automatically
//! routed to the context message callback before returning.
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
//! @param  tid      - Trace identifier to appear in messages. The value must
//!                    have the same lifetime as this object.
//!
//! @return =0       - object has been initialized.
//! @return !0       - an error occurred, the return value is a pointer to a
//!                    message summarizing the error. This message is the same
//!                    as would be thrown by the parameterized constructor.
//------------------------------------------------------------------------

  const char *Init( XrdTlsContext &ctx, int sfd, RW_Mode rwm, HS_Mode hsm,
                    bool isClient, const char *tid=0 );

//------------------------------------------------------------------------
//! Peek at the TLS connection data. If necessary, a handshake will be done.
//!
//! @param  buffer     - Pointer to buffer to hold the data.
//! @param  size       - The size of the buffer in bytes.
//! @param  bytesPeek  - Number of bytes placed in the buffer, if successful.
//!
//! @return SSL_ERROR_NONE    Operation was successful.
//!         SSL_WANT_READ     Non blocking socket would read  block.
//!         SSL_WANT_WRITE    Non blocking socket would write block.
//!         SSL_ERROR_SYSCALL A syscall error occurred, errno has code.
//!         SSL_ERROR_xxx     Other SSL related error occurred.
//------------------------------------------------------------------------

  int Peek( char *buffer, size_t size, int &bytesPeek );

//------------------------------------------------------------------------
//! Check if data is pending or readable.
//!
//! @param  any      True to return in any data is in the queue. False to
//!                  return the number of processed bytes.
//! @return          any = true:  1 is returned if there is data in the queue
//!                               (processed or not). 0 is returned o/w.
//!                  any = false: the number of processed bytes that are
//!                               available. These are not necesarily data
//!                               bytes. A subsequent read may still return 0.
//------------------------------------------------------------------------

  int Pending(bool any=true);

//------------------------------------------------------------------------
//! Read from the TLS connection. If necessary, a handshake will be done.
//
//! @param  buffer     - Pointer to buffer to hold the data.
//! @param  size       - The size of the buffer in bytes.
//! @param  bytesRead  - Number of bytes placed in the buffer, if successful.
//!
//! @return SSL_ERROR_NONE    Operation was successful.
//!         SSL_WANT_READ     Non blocking socket would read  block.
//!         SSL_WANT_WRITE    Non blocking socket would write block.
//!         SSL_ERROR_SYSCALL A syscall error occurred, errno has code.
//!         SSL_ERROR_xxx     Other SSL related error occurred.
//------------------------------------------------------------------------

  int Read( char *buffer, size_t size, int &bytesRead );

//------------------------------------------------------------------------
//! Tear down a TLS connection
//!
//! @param  One of the following enums:
//!         sdForce - Forced shutdown (violates TLS standard).
//!         sdImmed - Immediate shutdown (don't wait for ack); the default.
//!         sdWait  - Wait for peer acknowledgement (may be slow).
//------------------------------------------------------------------------

  enum SDType {sdForce = 1, sdImmed = 2, sdWait = 3};

  void Shutdown(SDType=sdImmed);

//------------------------------------------------------------------------
//! Write to the TLS connection. If necessary, a handshake will be done.
//!
//! @param  buffer     - Pointer to buffer holding the data.
//! @param  size       - The size of the data to write.
//! @param  bytesOut   - Number of bytes actually written, if successful.
//!
//! @return SSL_ERROR_NONE    Operation was successful.
//!         SSL_WANT_READ     Non blocking socket would read  block.
//!         SSL_WANT_WRITE    Non blocking socket would write block.
//!         SSL_ERROR_SYSCALL A syscall error occurred, errno has code.
//!         SSL_ERROR_xxx     Other SSL related error occurred.
//------------------------------------------------------------------------

  int Write( const char *buffer, size_t size, int &bytesOut );

//------------------------------------------------------------------------
//! @return  :  true if the TLS/SSL session is not established yet,
//!             false otherwise
//------------------------------------------------------------------------

  bool NeedHandShake()
  {
    return !hsDone;
  }

//------------------------------------------------------------------------
//! @return The TLS version number being used.
//------------------------------------------------------------------------

  const char *Version();

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

int  Diagnose(int sslrc);
void FlushErrors(const char *what=0, int sslerr=0);
bool Wait4OK(bool wantRead);

XrdTlsContext   *tlsctx;    //!< Associated context object
SSL             *ssl;       //!< Associated SSL     object
const char      *traceID;   //!< Trace identifier
int              sFD;       //!< Associated file descriptor (never closed)
int              hsWait;    //!< Maximum amount of time to wait for handshake
bool             hsDone;    //!< True if the handshake has completed
bool             fatal;     //!< True if fatal error prevents shutdown call

char             cOpts;     //!< Connection options
static const int xVerify = 0x01;   //!< Peer cetrificate is to be verified
static const int Debug   = 0x02;   //!< Debugging is on

char             cAttr;     //!< Connection attributes
static const int isServer  = 0x01;
static const int rBlocking = 0x02;
static const int wBlocking = 0x04;

char             hsMode;    //!< Handshake handling
static const int noBlock = 0;
static const int rwBlock = 'a';
static const int xyBlock = 'x';
};

#endif // __XRD_TLS_IO_HH__
