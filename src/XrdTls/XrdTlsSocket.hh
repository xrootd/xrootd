#ifndef __XRD_TLS_SOCKET_HH__
#define __XRD_TLS_SOCKET_HH__
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

#include "XrdTls/XrdTls.hh"

//----------------------------------------------------------------------------
// Forward declarations
//----------------------------------------------------------------------------

class  XrdNetAddrInfo;
class  XrdSysError;
class  XrdTlsContext;
class  XrdTlsPeerCerts;
struct XrdTlsSocketImpl;

//----------------------------------------------------------------------------
//! Socket wrapper for TLS I/O
//----------------------------------------------------------------------------

class XrdTlsSocket
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
  TLS_HS_BLOCK = true,  //!< Always block during handshake
  TLS_HS_NOBLK = false, //!< Do not block during handshake
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
//! @param  serial   - When true, only allows one thread to use the socket
//!                    at a time to prevent SSL errors (default). When false
//!                    does not add this protection, assuming caller does so.
//------------------------------------------------------------------------

  XrdTlsSocket( XrdTlsContext &ctx, int sfd, RW_Mode rwm, HS_Mode hsm,
                bool isClient, bool serial=true );

//------------------------------------------------------------------------
//! Constructor - reserves space for a TLS I/O wrapper. Use the Init()
//! method to fully initialize this object.
//------------------------------------------------------------------------

  XrdTlsSocket();

//------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------

  ~XrdTlsSocket();

//------------------------------------------------------------------------
//! Accept an incoming TLS connection
//!
//! @param  eWhy     - If not nil, receives the associated error message.
//!
//! @return The appropriate TLS return code.
//------------------------------------------------------------------------

  XrdTls::RC Accept(std::string *eMsg=0);

//------------------------------------------------------------------------
//! Establish a TLS connection
//!
//! @param  thehost  - The expected hostname. If nil the peername is not
//!                    verified.
//! @param  eWhy     - If not nil, receives the associated error message.
//!
//! @return TLS_AOK if the operation was successful; otherwise the appropraite
//!                 return code indicating the problem with eWhy, not nil,
//!                 containing a description of the error.
//------------------------------------------------------------------------

  XrdTls::RC Connect(const char *thehost=0, std::string *eWhy=0);

//------------------------------------------------------------------------
//! Obtain context associated with this connection.
//!
//! @return : Tls connection object
//------------------------------------------------------------------------

  XrdTlsContext *Context();

//------------------------------------------------------------------------
//! Get peer certificates associated with the socket.
//!
//! @param  ver      - When true, only return verified certificates.
//!
//! @return A pointer to the object holding the peer certificate and the
//!         associated chain. Nill is returned if there are no certificates
//!         of if verification did not occur but ver was true. The caller
//!         is responsible for deleting the returned object.
//------------------------------------------------------------------------

XrdTlsPeerCerts *getCerts(bool ver=true);

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
//! @param  serial   - When true, only allows one thread to use the socket
//!                    at a time to prevent SSL errors (default). When false
//!                    does not add this protection, assuming caller does so.
//! @param  tid      - Trace identifier to appear in messages. The value must
//!                    have the same lifetime as this object.
//!
//! @return =0       - object has been initialized.
//! @return !0       - an error occurred, the return value is a pointer to a
//!                    message summarizing the error. This message is the same
//!                    as would be thrown by the parameterized constructor.
//------------------------------------------------------------------------

  const char *Init( XrdTlsContext &ctx, int sfd, RW_Mode rwm, HS_Mode hsm,
                    bool isClient, bool serial=true, const char *tid="" );

//------------------------------------------------------------------------
//! Peek at the TLS connection data. If necessary, a handshake will be done.
//!
//! @param  buffer     - Pointer to buffer to hold the data.
//! @param  size       - The size of the buffer in bytes.
//! @param  bytesPeek  - Number of bytes placed in the buffer, if successful.
//!
//! @return TLS_AOK if the operation was successful; otherwise the appropraite
//!                 return code indicating the problem.
//------------------------------------------------------------------------

  XrdTls::RC Peek( char *buffer, size_t size, int &bytesPeek );

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
//! @return TLS_AOK if the operation was successful; otherwise the appropraite
//!                 return code indicating the problem.
//------------------------------------------------------------------------

  XrdTls::RC Read( char *buffer, size_t size, int &bytesRead );

//------------------------------------------------------------------------
//! Set the trace identifier (used when it's updated).
//!
//! @param  tid        - Pointer to trace identifier.
//------------------------------------------------------------------------

  void       SetTraceID(const char *tid);

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
//! @return TLS_AOK if the operation was successful; otherwise the appropraite
//!                 return code indicating the problem.
//------------------------------------------------------------------------

  XrdTls::RC Write( const char *buffer, size_t size, int &bytesOut );

//------------------------------------------------------------------------
//! @return  :  true if the TLS/SSL session is not established yet,
//!             false otherwise
//------------------------------------------------------------------------

  bool NeedHandShake();

//------------------------------------------------------------------------
//! @return The TLS version number being used.
//------------------------------------------------------------------------

  const char *Version();

private:

void AcceptEMsg(std::string *eWhy, const char *reason);
int  Diagnose(const char *what, int sslrc, int tcode);
std::string Err2Text(int sslerr);
bool NeedHS();
bool Wait4OK(bool wantRead);

XrdTlsSocketImpl *pImpl;
};
#endif // __XRD_TLS_IO_HH__
