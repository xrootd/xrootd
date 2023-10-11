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


#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "Xrd/XrdObject.hh"
#include "XrdXrootd/XrdXrootdBridge.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "Xrd/XrdProtocol.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdHttpChecksumHandler.hh"
#include "XrdHttpReadRangeHandler.hh"
#include "XrdNet/XrdNetPMark.hh"

#include <openssl/ssl.h>

#include <vector>

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
class XrdHttpExtHandler;
struct XrdVersionInfo;
class XrdOucGMap;
class XrdCryptoFactory;

class XrdHttpProtocol : public XrdProtocol {
  
  friend class XrdHttpReq;
  friend class XrdHttpExtReq;
  
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

  /// Perform a checksum request
  int doChksum(const XrdOucString &fname);

  /// Ctor, dtors and copy ctor
  XrdHttpProtocol(const XrdHttpProtocol&) = default;
  XrdHttpProtocol operator =(const XrdHttpProtocol &rhs);
  XrdHttpProtocol(bool imhttps);
  ~XrdHttpProtocol() {
    Cleanup();
  }

  static XrdObjectQ<XrdHttpProtocol> ProtStack;
  XrdObject<XrdHttpProtocol> ProtLink;


  /// Authentication area
  XrdSecEntity SecEntity;

  // XrdHttp checksum handling class
  static XrdHttpChecksumHandler cksumHandler;

  /// configuration for the read range handler
  static XrdHttpReadRangeHandler::Configuration ReadRangeConfig;

  /// called via https
  bool isHTTPS() { return ishttps; }

private:


  /// The resume function
  int (XrdHttpProtocol::*Resume)();

  /// Initialization of the ssl security things
  static bool InitTLS();

  /// Initialization fo security addon
  static bool InitSecurity();

  /// Start a response back to the client
  int StartSimpleResp(int code, const char *desc, const char *header_to_add, long long bodylen, bool keepalive);

  /// Send some generic data to the client
  int SendData(const char *body, int bodylen);

  /// Deallocate resources, in order to reutilize an object of this class
  void Cleanup();

  /// Reset values, counters, in order to reutilize an object of this class
  void Reset();

  /// Handle authentication of the client
  /// @return 0 if successful, otherwise error
  int HandleAuthentication(XrdLink* lp);

  /// After the SSL handshake, retrieve the VOMS info and the various stuff
  /// that is needed for autorization
  int GetVOMSData(XrdLink *lp);

  // Handle gridmap file mapping if present
  // Second argument is the OpenSSL hash of the EEC, if present; this allows
  // a consistent fallback if the user is not in the mapfile.
  //
  // @return 0 if successful, otherwise !0
  int HandleGridMap(XrdLink* lp, const char * eechash);

  /// Get up to blen bytes from the connection. Put them into mybuff.
  /// This primitive, for the way it is used, is not supposed to block
  int getDataOneShot(int blen, bool wait=false);

  /// Create a new BIO object from an XrdLink.  Returns NULL on failure.
  static BIO *CreateBIO(XrdLink *lp);
  
  /// The following records the external handlers that need to be loaded. We
  /// must defer loading these handlers as we need to pass some information
  /// to the handler and that is only known after we process our config file.
  struct extHInfo
        {XrdOucString extHName;  // The instance name (1 to 16 characters)
         XrdOucString extHPath;  // The shared library path
         XrdOucString extHParm;  // The parameter (sort of)

         extHInfo(const char *hName, const char *hPath, const char *hParm)
                 : extHName(hName), extHPath(hPath), extHParm(hParm) {}
        ~extHInfo() {}
  };
  /// Functions related to the configuration
  static int Config(const char *fn, XrdOucEnv *myEnv);
  static const char *Configed();
  static int xtrace(XrdOucStream &Config);
  static int xsslcert(XrdOucStream &Config);
  static int xsslkey(XrdOucStream &Config);
  static int xsecxtractor(XrdOucStream &Config);
  static int xexthandler(XrdOucStream & Config, std::vector<extHInfo> &hiVec);
  static int xsslcadir(XrdOucStream &Config);
  static int xsslcipherfilter(XrdOucStream &Config);
  static int xdesthttps(XrdOucStream &Config);
  static int xlistdeny(XrdOucStream &Config);
  static int xlistredir(XrdOucStream &Config);
  static int xselfhttps2http(XrdOucStream &Config);
  static int xembeddedstatic(XrdOucStream &Config);
  static int xstaticredir(XrdOucStream &Config);
  static int xstaticpreload(XrdOucStream &Config);
  static int xgmap(XrdOucStream &Config);
  static int xsslcafile(XrdOucStream &Config);
  static int xsslverifydepth(XrdOucStream &Config);
  static int xsecretkey(XrdOucStream &Config);
  static int xheader2cgi(XrdOucStream &Config);
  static int xhttpsmode(XrdOucStream &Config);
  static int xtlsreuse(XrdOucStream &Config);
  
  static bool isRequiredXtractor; // If true treat secxtractor errors as fatal
  static XrdHttpSecXtractor *secxtractor;
  
  static bool usingEC;   // using XrdEC
  // Loads the SecXtractor plugin, if available
  static int LoadSecXtractor(XrdSysError *eDest, const char *libName,
                      const char *libParms);
  
  // An oldstyle struct array to hold exthandlers
  #define MAX_XRDHTTPEXTHANDLERS 4
  static struct XrdHttpExtHandlerInfo {
    char name[16];
    XrdHttpExtHandler *ptr;
  } exthandler[MAX_XRDHTTPEXTHANDLERS];
  static int exthandlercnt;
  
  // Loads the ExtHandler plugin, if available
  static int LoadExtHandler(std::vector<extHInfo> &hiVec,
                            const char *cFN, XrdOucEnv &myEnv);

  static int LoadExtHandler(XrdSysError *eDest, const char *libName,
                            const char *configFN, const char *libParms,
                            XrdOucEnv *myEnv, const char *instName);

  // Determines whether one of the loaded ExtHandlers are interested in
  // handling a given request.
  //
  // Returns NULL if there is no matching handler.
  static XrdHttpExtHandler *FindMatchingExtHandler(const XrdHttpReq &);

  // Tells if an ext handler with the given name has already been loaded
  static bool ExtHandlerLoaded(const char *handlername);
  
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

  /// Sends a basic response. If the length is < 0 then it is calculated internally
  int SendSimpleResp(int code, const char *desc, const char *header_to_add, const char *body, long long bodylen, bool keepalive);

  /// Starts a chunked response; body of request is sent over multiple parts using the SendChunkResp
  //  API.
  int StartChunkedResp(int code, const char *desc, const char *header_to_add, long long bodylen, bool keepalive);

  /// Send a (potentially partial) body in a chunked response; invoking with NULL body
  //  indicates that this is the last chunk in the response.
  int ChunkResp(const char *body, long long bodylen);

  /// Send the beginning of a chunked response but not the body; useful when the size
  //  of the chunk is known but the body is not immediately available.
  int ChunkRespHeader(long long bodylen);

  /// Send the footer of the chunk response
  int ChunkRespFooter();

  /// Gets a string that represents the IP address of the client. Must be freed
  char *GetClientIPStr();

  /// Tells that we are just logging in
  bool DoingLogin;
  
  /// Tells that we are just waiting to have N bytes in the buffer
  long ResumeBytes;
  
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
  
  /// Our IP address, as a string. Please remember that this may not be unique for
  /// a given machine, hence we need to keep it here and recompute ad every new connection.
  char *Addr_str;
  
  /// The instance of the DN mapper. Created only when a valid path is given
  static XrdOucGMap      *servGMap;  // Grid mapping service
   
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

  /// OpenSSL stuff
  static char *sslcert, *sslkey, *sslcadir, *sslcafile, *sslcipherfilter;

  /// CRL thread refresh interval
  static int crlRefIntervalSec;

  /// Gridmap file location. The same used by XrdSecGsi
  static char *gridmap;// [s] gridmap file [/etc/grid-security/gridmap]
  static bool isRequiredGridmap; // If true treat gridmap errors as fatal
  static bool compatNameGeneration; // If true, utilize the old algorithm for username generation for unknown users.

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
  
  /// Rules that turn HTTP headers to cgi tokens in the URL, for internal comsumption
  static std::map< std::string, std::string > hdr2cgimap;

  /// Type identifier for our custom BIO objects.
  static int m_bio_type;

  /// C-style vptr table for our custom BIO objects.
  static BIO_METHOD *m_bio_method;

  /// The list of checksums that were configured via the xrd.cksum parameter on the server config file
  static char * xrd_cslist;

  /// Packet marking handler pointer (assigned from the environment during the Config() call)
  static XrdNetPMark * pmarkHandle;
};
#endif
