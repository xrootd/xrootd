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


#ifndef __XRDHTTP_PROTOCOL_H__
#define __XRDHTTP_PROTOCOL_H__

/** @file  XrdHttpProtocol.hh
 * @brief  A pragmatic implementation of the HTTP/DAV protocol for the Xrd framework
 * @author Fabrizio Furano
 * @date   Nov 2012
 * 
 * 
 * 
 */


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "Xrd/XrdObject.hh"
#include "XrdXrootd/XrdXrootdBridge.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "Xrd/XrdProtocol.hh"
#include "XrdOuc/XrdOucHash.hh"

#include <openssl/ssl.h>

#include "XrdHttpReq.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/


#ifndef __GNUC__
#define __attribute__(x)
#endif

class XrdOucTokenizer;
class XrdOucTrace;
class XrdBuffer;
class XrdLink;
class XrdXrootdProtocol;
class XrdHttpSecXtractor;
struct XrdVersionInfo;


class XrdHttpProtocol : public XrdProtocol {
  
  friend class XrdHttpReq;
  
public:

  /// Read and apply the configuration
  static int Configure(char *parms, XrdProtocol_Config *pi);

  /// Override from the base class
  void DoIt() {
    if (Resume) (*this.*Resume)();
  }

  /// Tells if the oustanding bytes on the socket match this protocol implementation
  XrdProtocol *Match(XrdLink *lp);

  /// Process data incoming from the socket
  int Process(XrdLink *lp); //  Sync: Job->Link.DoIt->Process

  
  /// Recycle this instance
  void Recycle(XrdLink *lp, int consec, const char *reason);

  /// Get activity stats
  int Stats(char *buff, int blen, int do_sync = 0);




  /// Perform a Stat request
  int doStat(char *fname);



  /// Ctor, dtors and copy ctor
  XrdHttpProtocol operator =(const XrdHttpProtocol &rhs);
  XrdHttpProtocol(bool imhttps);
  ~XrdHttpProtocol() {
    Cleanup();
  }

  static XrdObjectQ<XrdHttpProtocol> ProtStack;
  XrdObject<XrdHttpProtocol> ProtLink;



  /// Sends a basic response. If the length is < 0 then it is calculated internally
  int SendSimpleResp(int code, char *desc, char *header_to_add, char *body, int bodylen);


private:


  /// The resume function
  int (XrdHttpProtocol::*Resume)();

  /// Initialization of the ssl security things
  static int InitSecurity();

  /// Send some generic data to the client
  int SendData(char *body, int bodylen);

  /// Deallocate resources, in order to reutilize an object of this class
  void Cleanup();

  /// Reset values, counters, in order to reutilize an object of this class
  void Reset();

  /// After the SSL handshake, retrieve the VOMS info and the various stuff
  /// that is needed for autorization
  int GetVOMSData(XrdLink *lp);

  /// Get up to blen bytes from the connection. Put them into mybuff.
  /// This primitive, for the way it is used, is not supposed to block
  int getDataOneShot(int blen, bool wait=false);

  
  /// Functions related to the configuration
  static int Config(const char *fn);
  static int xsecl(XrdOucStream &Config);
  static int xtrace(XrdOucStream &Config);
  static int xsslcert(XrdOucStream &Config);
  static int xsslkey(XrdOucStream &Config);
  static int xsecxtractor(XrdOucStream &Config);
  static int xsslcadir(XrdOucStream &Config);
  static int xdesthttps(XrdOucStream &Config);
  static int xlistdeny(XrdOucStream &Config);
  static int xlistredir(XrdOucStream &Config);
  static int xselfhttps2http(XrdOucStream &Config);
  static int xembeddedstatic(XrdOucStream &Config);
  static int xstaticredir(XrdOucStream &Config);
  static int xstaticpreload(XrdOucStream &Config);
  static int xsslcafile(XrdOucStream &Config);
  static int xsslverifydepth(XrdOucStream &Config);
  static int xsecretkey(XrdOucStream &Config);

  static XrdHttpSecXtractor *secxtractor;
  // Loads the SecXtractor plugin, if available
  static int LoadSecXtractor(XrdSysError *eDest, const char *libName,
                      const char *libParms);

  /// Circular Buffer used to read the request
  XrdBuffer *myBuff;
  /// The circular pointers
  char *myBuffStart, *myBuffEnd;
  
  /// A nice var to hold the current header line
  XrdOucString tmpline;
  
  /// How many bytes still fit into the buffer in a contiguous way
  int BuffAvailable();
  /// How many bytes in the buffer
  int BuffUsed();
  /// How many bytes free in the buffer
  int BuffFree();
  
  /// Consume some bytes from the buffer
  void BuffConsume(int blen);
  /// Get a pointer, valid for up to blen bytes from the buffer. Returns the validity
  int BuffgetData(int blen, char **data, bool wait);
  /// Copy a full line of text from the buffer into dest. Zero if no line can be found in the buffer
  int BuffgetLine(XrdOucString &dest);

  
  /// Gets a string that represents the IP address of the client. Must be freed
  char *GetClientIPStr();
  
  /// Tells that we are just logging in
  bool DoingLogin;
  
  /// Tells that we are just waiting to have N bytes in the buffer
  long ResumeBytes;
  
  /// Global, static SSL context
  static SSL_CTX *sslctx;

  /// Private SSL context
  SSL *ssl;

  /// Private SSL bio
  BIO *sbio;

  /// bio to print SSL errors
  static BIO *sslbio_err;

  /// Tells if the client is https
  bool ishttps;

  /// Flag to tell if the https handshake has finished, in the case of an https
  /// connection being established
  bool ssldone;

  static XrdCryptoFactory *myCryptoFactory;
protected:



  // Statistical area
  //
//  static XrdXrootdStats *SI;
//  int numReads; // Count for kXR_read
//  int numReadP; // Count for kXR_read pre-preads
//  int numReadV; // Count for kR_readv
//  int numSegsV; // Count for kR_readv segmens
//  int numWrites; // Count
//  int numFiles; // Count
//
//  int cumReads; // Count less numReads
//  int cumReadP; // Count less numReadP
//  int cumReadV; // Count less numReadV
//  int cumSegsV; // Count less numSegsV
//  int cumWrites; // Count less numWrites
//  long long totReadP; // Bytes

  static XrdScheduler *Sched; // System scheduler
  static XrdBuffManager *BPool; // Buffer manager
  static XrdSysError eDest; // Error message handler
  static XrdSecService *CIA; // Authentication Server

  /// The link we are bound to
  XrdLink *Link;

  /// Authentication area
  XrdSecEntity SecEntity;

  /// The Bridge that we use to exercise the xrootd internals
  XrdXrootd::Bridge *Bridge;

  
  /// Area for coordinating request and responses to/from the bridge
  /// This also can process HTTP/DAV stuff
  XrdHttpReq CurrentReq;


  //
  // Processing configuration values
  //

  /// Timeout for reading the handshake
  static int hailWait;

  /// Timeout for reading data
  static int readWait;

  /// Our port
  static int Port;
  
  /// Our port, as a string
  static char * Port_str;
  
  /// Our IP address, as a string
  static char * Addr_str;

  /// Windowsize
  static int Window;
  static char *SecLib;

  /// OpenSSL stuff
  static char *sslcert, *sslkey, *sslcadir, *sslcafile;

  /// The key used to calculate the url hashes
  static char *secretkey;

  /// Depth of verification of a certificate chain
  static int sslverifydepth;

  /// True if the redirections must be towards https targets
  static bool isdesthttps;
  
  /// Url to redirect to in the case a listing is requested
  static char *listredir;
  
  /// If true, any form of listing is denied
  static bool listdeny;
  
  /// If client is HTTPS, self-redirect with HTTP+token
  static bool selfhttps2http;
  
  /// If true, use the embedded css and icons
  static bool embeddedstatic;
  
  // Url to redirect to in the case a /static is requested
  static char *staticredir;

  // Hash that keeps preloaded files
  struct StaticPreloadInfo {
    char *data;
    int len;
  };
  static XrdOucHash<StaticPreloadInfo> *staticpreload;

  /// Our role
  static kXR_int32 myRole;
  
};
#endif
