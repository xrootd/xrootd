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







/** @file  XrdHttpReq.hh
 * @brief  Main request/response class, handling the logical status of the communication
 * @author Fabrizio Furano
 * @date   Nov 2012
 * 
 * 
 * 
 */

#ifndef XRDHTTPREQ_HH
#define	XRDHTTPREQ_HH

#include "XProtocol/XProtocol.hh"
#include "XrdHttpChecksumHandler.hh"
#include "XrdHttpReadRangeHandler.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdXrootd/XrdXrootdBridge.hh"

#include <chrono>
#include <map>
#include <string>
#include <vector>

struct DirListInfo {
  std::string path;
  long long size;
  long id;
  long flags;
  long modtime;
};


class XrdHttpProtocol;
class XrdOucEnv;

class XrdHttpReq : public XrdXrootd::Bridge::Result {

public:
  // ----------------
  // Description of the request. The header/body parsing
  // is supposed to populate these fields, for fast access while
  // processing the request

  /// These are the HTTP/DAV requests that we support

  enum ReqType: int {
    rtUnset = -1,
    rtUnknown = 0,
    rtMalformed,
    rtGET,
    rtHEAD,
    rtPUT,
    rtOPTIONS,
    rtPATCH,
    rtDELETE,
    rtPROPFIND,
    rtMKCOL,
    rtMOVE,
    rtPOST,
    rtCount 
  };

private:
  // HTTP response parameters to be sent back to the user
  int httpStatusCode{-1};

  // Stores the first response that was sent as part of the Response Header
  // Used when staus code is updated after for e.g. Chunked Response + X-Transfer-Status request
  int initialStatusCode{-1};

  // HTTP Error code for the response
  // e.g. 8.1, 8.3.1, etc.
  // https://twiki.cern.ch/twiki/bin/view/LCG/WebdavErrorImprovement
  std::string httpErrorCode;
  // HTTP response text with following format:
  // Severity: ErrorCode: free-style text message
  // Severity being OK, WARNING, or ERROR
  // ErrorCode being a decimal numeric plus dot string, i.e. n or n.m or n.m.l,
  // etc. free-style text message any UTF-8 string
  // Optionally, it also contains the trailer headers whereever applicable
  // e.g. X-Transfer-Status: 200: OK
  // or X-Transfer-Status: 500: ERROR: <error message>: <additional text>
  std::string httpErrorBody;


  // The value of the user agent, if specified
  std::string m_user_agent;

  // Whether transfer encoding was requested.
  bool m_transfer_encoding_chunked;
  long long m_current_chunk_offset;
  long long m_current_chunk_size;

  // Whether trailer headers were enabled
  bool m_trailer_headers{false};

  // Whether the client understands our special status trailer.
  // The status trailer allows us to report when an IO error occurred
  // after a response body has started
  bool m_status_trailer{false};

  int parseHost(char *);

  void parseScitag(const std::string & val);

  //xmlDocPtr xmlbody; /* the resulting document tree */
  XrdHttpProtocol *prot;

  void clientMarshallReadAheadList(int nitems);
  void clientUnMarshallReadAheadList(int nitems);


  void getfhandle();

  // Process the checksum response and return a header that should
  // be included in the response.
  int PostProcessChecksum(std::string &digest_header);

  // Process the listing request of a GET request against a directory
  // - final_: True if this is the last entry in the listing.
  int PostProcessListing(bool final_);

  // Send the response for a GET request for a file read (i.e., not a directory)
  // Invoked after the open is successful but before the first read is issued.
  int ReturnGetHeaders();

  /// Cook and send the response after the bridge did something
  /// Return values:
  ///  0->everything OK, additionsl steps may be required
  ///  1->request processed completely
  ///  -1->error
  int PostProcessHTTPReq(bool final = false);

  // Parse a resource string, typically a filename, setting the resource field and the opaque data
  void parseResource(char *url);

  // Set Webdav Error messages
  void generateWebdavErrMsg();

  // Sanitize the resource from http[s]://[host]/ questionable prefix
  void sanitizeResourcePfx();

  // parses the iovN data pointers elements as either a kXR_read or kXR_readv
  // response and fills out a XrdHttpIOList with the corresponding length and
  // buffer pointers. File offsets from kXR_readv responses are not recorded.
  void getReadResponse(XrdHttpIOList &received);

  // notifies the range handler of receipt of bytes and sends the client
  // the data.
  int sendReadResponseSingleRange(const XrdHttpIOList &received);

  // notifies the range handler of receipt of bytes and sends the client
  // the data and necessary headers, assuming multipart/byteranges content type.
  int sendReadResponsesMultiRanges(const XrdHttpIOList &received);

  // If requested by the client, sends any I/O errors that occur during the transfer
  // into a footer.
  int sendFooterError(const std::string &);

  // Set the age header from the file modification time
  void addAgeHeader(std::string & headers);

  // Set the ETag header containing union of stat.st_ino and stat.st_dev
  // See XrdXrootdProtocol::StatGen() for the full definition of etag value.
  void addETagHeader(std::string & headers);

  /**
   * Convenient function to prepare the checksum query to the bridge
   * @param outCksum the checksum that will be requested
   * @param outResourceDigestOpaque the URL that will contain the resource and the digest type to request to the bridge as an opaque
   * @return 0 if successful, -1 if not
   */
  int prepareChecksumQuery(XrdHttpChecksumHandler::XrdHttpChecksumRawPtr & outCksum, XrdOucString & outResourceDigestOpaque);

public:
  XrdHttpReq(XrdHttpProtocol *protinstance, const XrdHttpReadRangeHandler::Configuration &rcfg) :
      readRangeHandler(rcfg), closeAfterError(false), keepalive(true) {

    prot = protinstance;
    length = 0;
    //xmlbody = 0;
    depth = 0;
    opaque = 0;
    writtenbytes = 0;
    fopened = false;
    headerok = false;
    mScitag = -1;
  };

  virtual ~XrdHttpReq();

  virtual void reset();

  int getInitialStatusCode() { return initialStatusCode;}
  int getHttpStatusCode() { return httpStatusCode;}

  void setHttpStatusCode(int code) {
      httpStatusCode = code;
      if (initialStatusCode < 0 && code >= 200 ) { 
        initialStatusCode = code;
      }
  }

  /// Parse the header
  int parseLine(char *line, int len);

  /// Parse the first line of the header
  int parseFirstLine(char *line, int len);

  /// Parse the body of a request, assuming that it's XML and that it's entirely in memory
  int parseBody(char *body, long long len);

  /// Prepare the buffers for sending a readv request
  int ReqReadV(const XrdHttpIOList &cl);
  std::vector<readahead_list> ralist;

  /// Build a partial header for a multipart response
  std::string buildPartialHdr(long long bytestart, long long byteend, long long filesize, char *token);

  /// Build the closing part for a multipart response
  std::string buildPartialHdrEnd(char *token);

  // Appends the opaque info that we have
  // NOTE: this function assumes that the strings are unquoted, and will quote them
  void appendOpaque(XrdOucString &s, XrdSecEntity *secent, char *hash, time_t tnow);

  void addCgi(const std::string & key, const std::string & value);

  // Set the transfer status header, if requested by the client
  void setTransferStatusHeader(std::string &header);

  // Return the current user agent; if none has been specified, returns an empty string
  const std::string &userAgent() const {return m_user_agent;}


  /// The request we got
  ReqType request;
  std::string requestverb;
  
  // We have to keep the headers for possible further processing
  // by external plugins
  std::map<std::string, std::string> allheaders;
  
  /// The resource specified by the request, stripped of opaque data
  XrdOucString resource;
  /// The opaque data, after parsing
  XrdOucEnv *opaque;
  /// The resource specified by the request, including all the opaque data
  XrdOucString resourceplusopaque;
  
  
  /// Tells if we have finished reading the header
  bool headerok;

  /// Tracking the next ranges of data to read during GET
  XrdHttpReadRangeHandler   readRangeHandler;
  bool                      readClosing;

  // Indication that there was a read error and the next
  // request processing state should cleanly close the file.
  bool                      closeAfterError;

  bool keepalive;
  long long length;  // Total size from client for PUT; total length of response TO client for GET.
  int depth;
  bool sendcontinue;

  /// The host field specified in the req
  std::string host;
  /// The destination field specified in the req
  std::string destination;

  /// The requested digest type
  std::string m_want_digest;

  /// The checksum that was ran for this request
  XrdHttpChecksumHandler::XrdHttpChecksumRawPtr m_req_cksum = nullptr;

  /// The checksum algorithm is specified as part of the opaque data in the URL.
  /// Hence, when a digest is generated to satisfy a request, we cache the tweaked
  /// URL in this data member.
  XrdOucString m_resource_with_digest;
  /// The computed digest for the HTTP response header.
  std::string m_digest_header;

  /// Additional opaque info that may come from the hdr2cgi directive
  std::string hdr2cgistr;
  bool m_appended_hdr2cgistr;
  /// Track whether we already appended the oss.asize argument for PUTs.
  bool m_appended_asize{false};

  //
  // Area for coordinating request and responses to/from the bridge
  //


  /// To coordinate multipart responses across multiple calls
  unsigned int rwOpDone, rwOpPartialDone;

  /// The last issued xrd request, often pending
  ClientRequest xrdreq;

  /// The last response data we got
  XResponseType xrdresp;
  XErrorCode xrderrcode;
  std::string etext;
  XrdOucString redirdest;

  /// The latest data chunks got from the xrd layer. These are valid only inside the callbacks!
  const struct iovec *iovP; //!< pointer to data array
  int iovN; //!< array count
  int iovL; //!< byte  count
  bool final; //!< true -> final result

  // The latest stat info got from the xrd layer
  long long etagval;
  long long filesize;
  long fileflags;
  long filemodtime;
  long filectime;
  char fhandle[4];
  bool fopened;

  /// If we want to give a string as a response, we compose it here
  std::string stringresp;

  /// State machine to talk to the bridge
  int reqstate;

  /// In a long write, we track where we have arrived
  long long writtenbytes;

  int mScitag;

  std::string m_origin;

  std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::time_point::min();

  /// Repr-Digest map where the key is the digest name and the value is the base64 encoded digest value
  std::map<std::string,std::string> m_repr_digest;

  /// Want-Repr-Digest map where the key is the digest name and the value is
  /// the preference (between 0 and 9)
  std::map<std::string,uint8_t> m_want_repr_digest;

  enum MonitState {
    NEW,       // Uninitialised state
    ACTIVE,    // First Call to Process Request
    ERR_NET,   // Network Error
    ERR_PROT,  // Filesystem/XRootD error that did not result in a valid HTTP response
               // We see this only during a chunked response
    DONE       // Final state 
  } monState;

  /// Crunch an http request.
  /// Return values:
  ///  0->call Process again
  ///  1->request processed
  ///  -1->error
  int ProcessHTTPReq();


  // ------------
  // Items inherited from the Bridge class
  //

  //-----------------------------------------------------------------------------
  //! Effect a client data response.
  //!
  //! The Data() method is called when Run() resulted in a successful data
  //! response. The method should rewrite the data and send it to the client using
  //! the associated XrdLink object. As an example,
  //! 1) Result::Data(info, iovP, iovN, iovL) is called.
  //! 2) Inspect iovP, rewrite the data.
  //! 3) Send the response: info->linkP->Send(new_iovP, new_iovN, new_iovL);
  //! 4) Handle send errors and cleanup(e.g. deallocate storage).
  //! 5) Return, the exchange is now complete.
  //!
  //! @param  info    the context associated with the result.
  //! @param  iovP    a pointer to the iovec structure containing the xrootd data
  //!                 response about to be sent to the client. The request header
  //!                 is not included in the iovec structure. The elements of this
  //!                 structure must not be modified by the method.
  //! @param  iovN    the number of elements in the iovec structure array.
  //! @param  iovL    total number of data bytes that would be sent to the client.
  //!                 This is simply the sum of all the lengths in the iovec.
  //! @param  final   True is this is the final result. Otherwise, this is a
  //!                 partial result (i.e. kXR_oksofar) and more data will result
  //!                 causing additional callbacks.
  //!
  //! @return true    continue normal processing.
  //!         false   terminate the bridge and close the link.
  //-----------------------------------------------------------------------------

  virtual bool Data(XrdXrootd::Bridge::Context &info, //!< the result context
          const
          struct iovec *iovP, //!< pointer to data array
          int iovN, //!< array count
          int iovL, //!< byte  count
          bool final //!< true -> final result
          );

  //-----------------------------------------------------------------------------
  //! Effect a client acknowledgement.
  //!
  //! The Done() method is called when Run() resulted in success and there is no
  //! associated data for the client (equivalent to a simple kXR_ok response).
  //!
  //! @param  info    the context associated with the result.
  //!
  //! @return true    continue normal processing.
  //!         false   terminate the bridge and close the link.
  //-----------------------------------------------------------------------------

  virtual bool Done(XrdXrootd::Bridge::Context &info); //!< the result context


  //-----------------------------------------------------------------------------
  //! Effect a client error response.
  //!
  //! The Error() method is called when an error was encountered while processing
  //! the Run() request. The error should be reflected to the client.
  //!
  //! @param  info    the context associated with the result.
  //! @param  ecode   the "kXR" error code describing the nature of the error.
  //!                 The code is in host byte format.
  //! @param  etext   a null terminated string describing the error in human terms
  //!
  //! @return true    continue normal processing.
  //!         false   terminate the bridge and close the link.
  //-----------------------------------------------------------------------------

  virtual bool Error(XrdXrootd::Bridge::Context &info, //!< the result context
          int ecode, //!< the "kXR" error code
          const char *etext //!< associated error message
          );

  //-----------------------------------------------------------------------------
  //! Notify callback that a sendfile() request is pending.
  //!
  //! The File() method is called when Run() resulted in a sendfile response (i.e.
  //! sendfile() would have been used to send data to the client). This allows
  //! the callback to reframe the sendfile() data using the Send() method in the
  //! passed context object (see class Context above).
  //!
  //! @param  info    the context associated with the result.
  //! @param  dlen    total number of data bytes that would be sent to the client.
  //!
  //! @return true    continue normal processing.
  //!         false   terminate the bridge and close the link.
  //-----------------------------------------------------------------------------

  virtual int File(XrdXrootd::Bridge::Context &info, //!< the result context
          int dlen //!< byte  count
  );

  //-----------------------------------------------------------------------------
  //! Redirect the client to another host:port.
  //!
  //! The Redir() method is called when the client must be redirected to another
  //! host.
  //!
  //! @param  info    the context associated with the result.
  //! @param  port    the port number in host byte format.
  //! @param  hname   the DNS name of the host or IP address is IPV4 or IPV6
  //!                 format (i.e. "n.n.n.n" or "[ipv6_addr]").
  //!
  //! @return true    continue normal processing.
  //!         false   terminate the bridge and close the link.
  //-----------------------------------------------------------------------------

  virtual bool Redir(XrdXrootd::Bridge::Context &info, //!< the result context
          int port, //!< the port number
          const char *hname //!< the destination host
          );

};



void trim(std::string &str);

#endif	/* XRDHTTPREQ_HH */

