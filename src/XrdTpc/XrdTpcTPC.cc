#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdVersion.hh"

#include "XrdXrootd/XrdXrootdTpcMon.hh"

#include <curl/curl.h>

#include <dlfcn.h>
#include <fcntl.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <iostream> // Delete later!!!

#include "XrdTpcState.hh"
#include "XrdTpcStream.hh"
#include "XrdTpcTPC.hh"
#include "XrdTpcCurlMulti.hh"
#include <fstream>

using namespace TPC;

XrdXrootdTpcMon* TPCHandler::TPCLogRecord::tpcMonitor = 0;

uint64_t TPCHandler::m_monid{0};
int TPCHandler::m_marker_period = 5;
size_t TPCHandler::m_block_size = 16*1024*1024;
size_t TPCHandler::m_small_block_size = 1*1024*1024;
XrdSysMutex TPCHandler::m_monid_mutex;

XrdVERSIONINFO(XrdHttpGetExtHandler, HttpTPC);

/******************************************************************************/
/*   T P C H a n d l e r : : T P C L o g R e c o r d   D e s t r u c t o r    */
/******************************************************************************/
  
TPCHandler::TPCLogRecord::~TPCLogRecord()
{
// Record monitoring data is enabled
//
   if (tpcMonitor)
      {XrdXrootdTpcMon::TpcInfo monInfo;

       monInfo.clID = clID.c_str();
       monInfo.begT = begT;
       gettimeofday(&monInfo.endT, 0);

       if (log_prefix == "PullRequest")
          {monInfo.dstURL = local.c_str();
           monInfo.srcURL = remote.c_str();
          } else {
           monInfo.dstURL = remote.c_str();
           monInfo.srcURL = local.c_str();
           monInfo.opts |= XrdXrootdTpcMon::TpcInfo::isaPush;
          }

       if (!status) monInfo.endRC = 0;
          else if (tpc_status > 0) monInfo.endRC = tpc_status;
                  else  monInfo.endRC = 1;
       monInfo.strm  = static_cast<unsigned char>(streams);
       monInfo.fSize = (bytes_transferred < 0 ? 0 : bytes_transferred);
       if (!isIPv6) monInfo.opts |= XrdXrootdTpcMon::TpcInfo::isIPv4;

       tpcMonitor->Report(monInfo);
      }
}
  
/******************************************************************************/
/*               C u r l D e l e t e r : : o p e r a t o r ( )                */
/******************************************************************************/
  
void CurlDeleter::operator()(CURL *curl)
{
    if (curl) curl_easy_cleanup(curl);
}

/******************************************************************************/
/*           s o c k o p t _ s e t c l o e x e c _ c a l l b a c k            */
/******************************************************************************/
  
/**
 * The callback that will be called by libcurl when the socket has been created
 * https://curl.se/libcurl/c/CURLOPT_SOCKOPTFUNCTION.html
 *
 * Note: that this callback has been replaced by the opensocket_callback as it
 *       was needed for monitoring to report what IP protocol was being used.
 *       It has been kept in case we will need this callback in the future.
 */
int TPCHandler::sockopt_setcloexec_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose) {
    int oldFlags = fcntl(curlfd,F_GETFD,0);
    if(oldFlags < 0) {
        return CURL_SOCKOPT_ERROR;
    }
    oldFlags |= FD_CLOEXEC;
    if(!fcntl(curlfd,F_SETFD,oldFlags)) {
        return CURL_SOCKOPT_OK;
    }
    return CURL_SOCKOPT_ERROR;
}

/******************************************************************************/
/*                   o p e n s o c k e t _ c a l l b a c k                    */
/******************************************************************************/
  
  
/**
 * The callback that will be called by libcurl when the socket is about to be
 * opened so we can capture the protocol that will be used.
 */
int TPCHandler::opensocket_callback(void *clientp,
                                    curlsocktype purpose,
                                    struct curl_sockaddr *aInfo)
{
  //Return a socket file descriptor (note the clo_exec flag will be set).
  int fd = XrdSysFD_Socket(aInfo->family, aInfo->socktype, aInfo->protocol);
  // See what kind of address will be used to connect
  //
  if(fd < 0) {
    return CURL_SOCKET_BAD;
  }
  TPCLogRecord * rec = (TPCLogRecord *)clientp;
  if (purpose == CURLSOCKTYPE_IPCXN && clientp)
  {XrdNetAddr thePeer(&(aInfo->addr));
    rec->isIPv6 =  (thePeer.isIPType(XrdNetAddrInfo::IPv6)
                    && !thePeer.isMapped());
    // Register the socket to the packet marking manager
    rec->pmarkManager.addFd(fd,&aInfo->addr);
  }

  return fd;
}

int TPCHandler::closesocket_callback(void *clientp, curl_socket_t fd) {
  TPCLogRecord * rec = (TPCLogRecord *)clientp;

  // Destroy the PMark handle associated to the file descriptor before closing it.
  // Otherwise, we would lose the socket usage information if the socket is closed before
  // the PMark handle is closed.
  rec->pmarkManager.endPmark(fd);

  return close(fd);
}

/******************************************************************************/
/*                            p r e p a r e U R L                             */
/******************************************************************************/
  
// We need to utilize the full URL (including the query string), not just the
// resource name.  The query portion is hidden in the `xrd-http-query` header;
// we take this out and combine it with the resource name.
//
// One special key is `authz`; this is always stripped out and copied to the Authorization
// header (which will later be used for XrdSecEntity).  The latter copy is only done if
// the Authorization header is not already present.
//
// hasSetOpaque will be set to true if at least one opaque data has been set in the URL that is returned,
// false otherwise
static std::string prepareURL(XrdHttpExtReq &req, bool & hasSetOpaque) {
    std::map<std::string, std::string>::const_iterator iter = req.headers.find("xrd-http-query");
    if (iter == req.headers.end() || iter->second.empty()) {return req.resource;}

    auto has_authz_header = req.headers.find("Authorization") != req.headers.end();

    std::istringstream requestStream(iter->second);
    std::string token;
    std::stringstream result;
    bool found_first_header = false;
    while (std::getline(requestStream, token, '&')) {
        if (token.empty()) {
            continue;
        } else if (!strncmp(token.c_str(), "authz=", 6)) {
            if (!has_authz_header) {
                req.headers["Authorization"] = token.substr(6);
                has_authz_header = true;
            }
        } else if (!found_first_header) {
            result << "?" << token;
            found_first_header = true;
        } else {
            result << "&" << token;
        }
    }
    hasSetOpaque = found_first_header;
    return req.resource + result.str().c_str();
}

static std::string prepareURL(XrdHttpExtReq &req) {
    bool foundHeader;
    return prepareURL(req,foundHeader);
}

/******************************************************************************/
/*           e n c o d e _ x r o o t d _ o p a q u e _ t o _ u r i            */
/******************************************************************************/
  
// When processing a redirection from the filesystem layer, it is permitted to return
// some xrootd opaque data.  The quoting rules for xrootd opaque data are significantly
// more permissive than a URI (basically, only '&' and '=' are disallowed while some
// URI parsers may dislike characters like '"').  This function takes an opaque string
// (e.g., foo=1&bar=2&baz=") and makes it safe for all URI parsers.
std::string encode_xrootd_opaque_to_uri(CURL *curl, const std::string &opaque)
{
    std::stringstream parser(opaque);
    std::string sequence;
    std::stringstream output;
    bool first = true;
    while (getline(parser, sequence, '&')) {
        if (sequence.empty()) {continue;}
        size_t equal_pos = sequence.find('=');
        char *val = NULL;
        if (equal_pos != std::string::npos)
            val = curl_easy_escape(curl, sequence.c_str() + equal_pos + 1, sequence.size()  - equal_pos - 1);
        // Do not emit parameter if value exists and escaping failed.
        if (!val && equal_pos != std::string::npos) {continue;}

        if (!first) output << "&";
        first = false;
        output << sequence.substr(0, equal_pos);
        if (val) {
            output << "=" << val;
            curl_free(val);
        }
    }
    return output.str();
}

/******************************************************************************/
/*           T P C H a n d l e r : : C o n f i g u r e C u r l C A            */
/******************************************************************************/
  
void
TPCHandler::ConfigureCurlCA(CURL *curl)
{
    auto ca_filename = m_ca_file ? m_ca_file->CAFilename() : "";
    auto crl_filename = m_ca_file ? m_ca_file->CRLFilename() : "";
    if (!ca_filename.empty() && !crl_filename.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_filename.c_str());
        //Check that the CRL file contains at least one entry before setting this option to curl
        //Indeed, an empty CRL file will make curl unhappy and therefore will fail
        //all HTTP TPC transfers (https://github.com/xrootd/xrootd/issues/1543)
        std::ifstream in(crl_filename, std::ifstream::ate | std::ifstream::binary);
        if(in.tellg() > 0 && m_ca_file->atLeastOneValidCRLFound()){
            curl_easy_setopt(curl, CURLOPT_CRLFILE, crl_filename.c_str());
        } else {
            std::ostringstream oss;
            oss << "No valid CRL file has been found in the file " << crl_filename << ". Disabling CRL checking.";
            m_log.Log(Warning,"TpcHandler",oss.str().c_str());
        }
    }
    else if (!m_cadir.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAPATH, m_cadir.c_str());
    }
    if (!m_cafile.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, m_cafile.c_str());
    }
}


bool TPCHandler::MatchesPath(const char *verb, const char *path) {
    return !strcmp(verb, "COPY") || !strcmp(verb, "OPTIONS");
}

/******************************************************************************/
/*                            P r e p a r e U R L                             */
/******************************************************************************/
  
static std::string PrepareURL(const std::string &input) {
    if (!strncmp(input.c_str(), "davs://", 7)) {
        return "https://" + input.substr(7);
    }
    return input;
}

/******************************************************************************/
/*                T P C H a n d l e r : : P r o c e s s R e q                 */
/******************************************************************************/
  
int TPCHandler::ProcessReq(XrdHttpExtReq &req) {
    if (req.verb == "OPTIONS") {
        return ProcessOptionsReq(req);
    }
    auto header = req.headers.find("Credential");
    if (header != req.headers.end()) {
        if (header->second != "none") {
            m_log.Emsg("ProcessReq", "COPY requested an unsupported credential type: ", header->second.c_str());
            return req.SendSimpleResp(400, NULL, NULL, "COPY requestd an unsupported Credential type", 0);
        }
    }
    header = req.headers.find("Source");
    if (header != req.headers.end()) {
        std::string src = PrepareURL(header->second);
        return ProcessPullReq(src, req);
    }
    header = req.headers.find("Destination");
    if (header != req.headers.end()) {
        return ProcessPushReq(header->second, req);
    }
    m_log.Emsg("ProcessReq", "COPY verb requested but no source or destination specified.");
    return req.SendSimpleResp(400, NULL, NULL, "No Source or Destination specified", 0);
}

/******************************************************************************/
/*                 T P C H a n d l e r   D e s t r u c t o r                  */
/******************************************************************************/
  
TPCHandler::~TPCHandler() {
    m_sfs = NULL;
}

/******************************************************************************/
/*                T P C H a n d l e r   C o n s t r u c t o r                 */
/******************************************************************************/
  
TPCHandler::TPCHandler(XrdSysError *log, const char *config, XrdOucEnv *myEnv) :
        m_desthttps(false),
        m_timeout(60),
        m_first_timeout(120),
        m_log(log->logger(), "TPC_"),
        m_sfs(NULL)
{
    if (!Configure(config, myEnv)) {
        throw std::runtime_error("Failed to configure the HTTP third-party-copy handler.");
    }

// Extract out the TPC monitoring object (we share it with xrootd).
//
   XrdXrootdGStream *gs = (XrdXrootdGStream*)myEnv->GetPtr("Tpc.gStream*");
   if (gs)
      TPCLogRecord::tpcMonitor = new XrdXrootdTpcMon("http",log->logger(),*gs);
}

/******************************************************************************/
/*         T P C H a n d l e r : : P r o c e s s O p t i o n s R e q          */
/******************************************************************************/
  
/**
 * Handle the OPTIONS verb as we have added a new one...
 */
int TPCHandler::ProcessOptionsReq(XrdHttpExtReq &req) {
    return req.SendSimpleResp(200, NULL, (char *) "DAV: 1\r\nDAV: <http://apache.org/dav/propset/fs/1>\r\nAllow: HEAD,GET,PUT,PROPFIND,DELETE,OPTIONS,COPY", NULL, 0);
}

/******************************************************************************/
/*                  T P C H a n d l e r : : G e t A u t h z                   */
/******************************************************************************/
  
std::string TPCHandler::GetAuthz(XrdHttpExtReq &req) {
    std::string authz;
    auto authz_header = req.headers.find("Authorization");
    if (authz_header != req.headers.end()) {
        char * quoted_url = quote(authz_header->second.c_str());
        std::stringstream ss;
        ss << "authz=" << quoted_url;
        free(quoted_url);
        authz = ss.str();
    }
    return authz;
}

/******************************************************************************/
/*          T P C H a n d l e r : : R e d i r e c t T r a n s f e r           */
/******************************************************************************/
  
int TPCHandler::RedirectTransfer(CURL *curl, const std::string &redirect_resource,
    XrdHttpExtReq &req, XrdOucErrInfo &error, TPCLogRecord &rec)
{
    int port;
    const char *ptr = error.getErrText(port);
    if ((ptr == NULL) || (*ptr == '\0') || (port == 0)) {
        rec.status = 500;
        char msg[] = "Internal error: redirect without hostname";
        logTransferEvent(LogMask::Error, rec, "REDIRECT_INTERNAL_ERROR", msg);
        return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }

    // Construct redirection URL taking into consideration any opaque info
    std::string rdr_info = ptr;
    std::string host, opaque;
    size_t pos = rdr_info.find('?');
    host = rdr_info.substr(0, pos);

    if (pos != std::string::npos) {
      opaque = rdr_info.substr(pos + 1);
    }

    std::stringstream ss;
    ss << "Location: http" << (m_desthttps ? "s" : "") << "://" << host << ":" << port << "/" << redirect_resource;

    if (!opaque.empty()) {
      ss << "?" << encode_xrootd_opaque_to_uri(curl, opaque);
    }

    rec.status = 307;
    logTransferEvent(LogMask::Info, rec, "REDIRECT", ss.str());
    return req.SendSimpleResp(rec.status, NULL, const_cast<char *>(ss.str().c_str()),
        NULL, 0);
}

/******************************************************************************/
/*             T P C H a n d l e r : : O p e n W a i t S t a l l              */
/******************************************************************************/
  
int TPCHandler::OpenWaitStall(XrdSfsFile &fh, const std::string &resource,
                      int mode, int openMode, const XrdSecEntity &sec,
                      const std::string &authz)
{
    int open_result;
    while (1) {
        int orig_ucap = fh.error.getUCap();
        fh.error.setUCap(orig_ucap | XrdOucEI::uIPv64);
        std::string opaque;
        size_t pos = resource.find('?');
        // Extract the path and opaque info from the resource
        std::string path = resource.substr(0, pos);

        if (pos != std::string::npos) {
          opaque = resource.substr(pos + 1);
        }

        // Append the authz information if there are some
        if(!authz.empty()) {
            opaque += (opaque.empty() ? "" : "&");
            opaque += authz;
        }
        open_result = fh.open(path.c_str(), mode, openMode, &sec, opaque.c_str());

        if ((open_result == SFS_STALL) || (open_result == SFS_STARTED)) {
            int secs_to_stall = fh.error.getErrInfo();
            if (open_result == SFS_STARTED) {secs_to_stall = secs_to_stall/2 + 5;}
            sleep(secs_to_stall);
        }
        break;
    }
    return open_result;
}

/******************************************************************************/
/* XRD_CHUNK_RESP:                                                            */
/*         T P C H a n d l e r : : D e t e r m i n e X f e r S i z e          */
/******************************************************************************/
  
#ifdef XRD_CHUNK_RESP



/**
 * Determine size at remote end.
 */
int TPCHandler::DetermineXferSize(CURL *curl, XrdHttpExtReq &req, State &state,
                                  bool &success, TPCLogRecord &rec, bool shouldReturnErrorToClient) {
    success = false;
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    CURLcode res;
    res = curl_easy_perform(curl);
    //Immediately set the CURLOPT_NOBODY flag to 0 as we anyway
    //don't want the next curl call to do be a HEAD request
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    if (res == CURLE_HTTP_RETURNED_ERROR) {
        std::stringstream ss;
        ss << "Remote server failed request: " << curl_easy_strerror(res);
        rec.status = 500;
        logTransferEvent(LogMask::Error, rec, "SIZE_FAIL", ss.str());
        return shouldReturnErrorToClient ? req.SendSimpleResp(rec.status, NULL, NULL, const_cast<char *>(curl_easy_strerror(res)), 0) : -1;
    } else if (state.GetStatusCode() >= 400) {
        std::stringstream ss;
        ss << "Remote side failed with status code " << state.GetStatusCode();
        rec.status = 500;
        logTransferEvent(LogMask::Error, rec, "SIZE_FAIL", ss.str());
        return shouldReturnErrorToClient ? req.SendSimpleResp(rec.status, NULL, NULL, const_cast<char *>(ss.str().c_str()), 0): -1;
    } else if (res) {
        std::stringstream ss;
        ss << "HTTP library failed: " << curl_easy_strerror(res);
        rec.status = 500;
        logTransferEvent(LogMask::Error, rec, "SIZE_FAIL", ss.str());
        char msg[] = "Unknown internal transfer failure";
        return shouldReturnErrorToClient ? req.SendSimpleResp(rec.status, NULL, NULL, msg, 0) : -1;
    }
    std::stringstream ss;
    ss << "Successfully determined remote size for pull request: "
       << state.GetContentLength();
    logTransferEvent(LogMask::Debug, rec, "SIZE_SUCCESS", ss.str());
    success = true;
    return 0;
}

int TPCHandler::GetContentLengthTPCPull(CURL *curl, XrdHttpExtReq &req, uint64_t &contentLength, bool & success, TPCLogRecord &rec) {
    State state(curl);
    //Don't forget to copy the headers of the client's request before doing the HEAD call. Otherwise, if there is a need for authentication,
    //it will fail
    state.CopyHeaders(req);
    int result;
    //In case we cannot get the content length, we don't return anything to the client
    if ((result = DetermineXferSize(curl, req, state, success, rec, false)) || !success) {
        return result;
    }
    contentLength = state.GetContentLength();
    return result;
}
  
/******************************************************************************/
/* XRD_CHUNK_RESP:                                                            */
/*            T P C H a n d l e r : : S e n d P e r f M a r k e r             */
/******************************************************************************/
  
int TPCHandler::SendPerfMarker(XrdHttpExtReq &req, TPCLogRecord &rec, TPC::State &state) {
    std::stringstream ss;
    const std::string crlf = "\n";
    ss << "Perf Marker" << crlf;
    ss << "Timestamp: " << time(NULL) << crlf;
    ss << "Stripe Index: 0" << crlf;
    ss << "Stripe Bytes Transferred: " << state.BytesTransferred() << crlf;
    ss << "Total Stripe Count: 1" << crlf;
    // Include the TCP connection associated with this transfer; used by
    // the TPC client for monitoring purposes.
    std::string desc = state.GetConnectionDescription();
    if (!desc.empty())
        ss << "RemoteConnections: " << desc << crlf;
    ss << "End" << crlf;
    rec.bytes_transferred = state.BytesTransferred();
    logTransferEvent(LogMask::Debug, rec, "PERF_MARKER");

    return req.ChunkResp(ss.str().c_str(), 0);
}

/******************************************************************************/
/* XRD_CHUNK_RESP:                                                            */
/*            T P C H a n d l e r : : S e n d P e r f M a r k e r             */
/******************************************************************************/
  
int TPCHandler::SendPerfMarker(XrdHttpExtReq &req, TPCLogRecord &rec, std::vector<State*> &state,
    off_t bytes_transferred)
{
    // The 'performance marker' format is largely derived from how GridFTP works
    // (e.g., the concept of `Stripe` is not quite so relevant here).  See:
    //    https://twiki.cern.ch/twiki/bin/view/LCG/HttpTpcTechnical
    // Example marker:
    //    Perf Marker\n
    //    Timestamp: 1537788010\n
    //    Stripe Index: 0\n
    //    Stripe Bytes Transferred: 238745\n
    //    Total Stripe Count: 1\n
    //    RemoteConnections: tcp:129.93.3.4:1234,tcp:[2600:900:6:1301:268a:7ff:fef6:a590]:2345\n
    //    End\n
    //
    std::stringstream ss;
    const std::string crlf = "\n";
    ss << "Perf Marker" << crlf;
    ss << "Timestamp: " << time(NULL) << crlf;
    ss << "Stripe Index: 0" << crlf;
    ss << "Stripe Bytes Transferred: " << bytes_transferred << crlf;
    ss << "Total Stripe Count: 1" << crlf;
    // Build a list of TCP connections associated with this transfer; used by
    // the TPC client for monitoring purposes.
    bool first = true;
    std::stringstream ss2;
    for (std::vector<State*>::const_iterator iter = state.begin();
        iter != state.end(); iter++)
    {
        std::string desc = (*iter)->GetConnectionDescription();
        if (!desc.empty()) {
            ss2 << (first ? "" : ",") << desc;
            first = false;
        }
    }
    if (!first)
        ss << "RemoteConnections: " << ss2.str() << crlf;
    ss << "End" << crlf;
    rec.bytes_transferred = bytes_transferred;
    logTransferEvent(LogMask::Debug, rec, "PERF_MARKER");

    return req.ChunkResp(ss.str().c_str(), 0);
}

/******************************************************************************/
/* XRD_CHUNK_RESP:                                                            */
/*        T P C H a n d l e r : : R u n C u r l W i t h U p d a t e s         */
/******************************************************************************/
  
int TPCHandler::RunCurlWithUpdates(CURL *curl, XrdHttpExtReq &req, State &state,
    TPCLogRecord &rec)
{
    // Create the multi-handle and add in the current transfer to it.
    CURLM *multi_handle = curl_multi_init();
    if (!multi_handle) {
        rec.status = 500;
        logTransferEvent(LogMask::Error, rec, "CURL_INIT_FAIL",
            "Failed to initialize a libcurl multi-handle");
        char msg[] = "Failed to initialize internal server memory";
        return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }

    //curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128*1024);

    CURLMcode mres;
    mres = curl_multi_add_handle(multi_handle, curl);
    if (mres) {
        rec.status = 500;
        std::stringstream ss;
        ss << "Failed to add transfer to libcurl multi-handle: " << curl_multi_strerror(mres);
        logTransferEvent(LogMask::Error, rec, "CURL_INIT_FAIL", ss.str());
        char msg[] = "Failed to initialize internal server handle";
        curl_multi_cleanup(multi_handle);
        return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }

    // Start response to client prior to the first call to curl_multi_perform
    int retval = req.StartChunkedResp(201, "Created", "Content-Type: text/plain");
    if (retval) {
        curl_multi_cleanup(multi_handle);
        logTransferEvent(LogMask::Error, rec, "RESPONSE_FAIL",
            "Failed to send the initial response to the TPC client");
        return retval;
    } else {
        logTransferEvent(LogMask::Debug, rec, "RESPONSE_START",
            "Initial transfer response sent to the TPC client");
    }

    // Transfer loop: use curl to actually run the transfer, but periodically
    // interrupt things to send back performance updates to the client.
    int running_handles = 1;
    time_t last_marker = 0;
    // Track how long it's been since the last time we recorded more bytes being transferred.
    off_t last_advance_bytes = 0;
    time_t last_advance_time = time(NULL);
    time_t transfer_start = last_advance_time;
    CURLcode res = static_cast<CURLcode>(-1);
    do {
        time_t now = time(NULL);
        time_t next_marker = last_marker + m_marker_period;
        if (now >= next_marker) {
            off_t bytes_xfer = state.BytesTransferred();
            if (bytes_xfer > last_advance_bytes) {
                last_advance_bytes = bytes_xfer;
                last_advance_time = now;
            }
            if (SendPerfMarker(req, rec, state)) {
                curl_multi_remove_handle(multi_handle, curl);
                curl_multi_cleanup(multi_handle);
                logTransferEvent(LogMask::Error, rec, "PERFMARKER_FAIL",
                    "Failed to send a perf marker to the TPC client");
                return -1;
            }
            int timeout = (transfer_start == last_advance_time) ? m_first_timeout : m_timeout;
            if (now > last_advance_time + timeout) {
                const char *log_prefix = rec.log_prefix.c_str();
                bool tpc_pull = strncmp("Pull", log_prefix, 4) == 0;

                state.SetErrorCode(10);
                std::stringstream ss;
                ss << "Transfer failed because no bytes have been "
                   << (tpc_pull ? "received from the source (pull mode) in "
                                : "transmitted to the destination (push mode) in ") << timeout << " seconds.";
                state.SetErrorMessage(ss.str());
                curl_multi_remove_handle(multi_handle, curl);
                curl_multi_cleanup(multi_handle);
                break;
            }
            last_marker = now;
        }
        // The transfer will start after this point, notify the packet marking manager
      rec.pmarkManager.startTransfer(&req);
        mres = curl_multi_perform(multi_handle, &running_handles);
        if (mres == CURLM_CALL_MULTI_PERFORM) {
            // curl_multi_perform should be called again immediately.  On newer
            // versions of curl, this is no longer used.
            continue;
        } else if (mres != CURLM_OK) {
            break;
        } else if (running_handles == 0) {
            break;
        }

        rec.pmarkManager.beginPMarks();
        //printf("There are %d running handles\n", running_handles);

        // Harvest any messages, looking for CURLMSG_DONE.
        CURLMsg *msg;
        do {
            int msgq = 0;
            msg = curl_multi_info_read(multi_handle, &msgq);
            if (msg && (msg->msg == CURLMSG_DONE)) {
                CURL *easy_handle = msg->easy_handle;
                res = msg->data.result;
                curl_multi_remove_handle(multi_handle, easy_handle);
            }
        } while (msg);

        int64_t max_sleep_time = next_marker - time(NULL);
        if (max_sleep_time <= 0) {
            continue;
        }
        int fd_count;
#ifdef HAVE_CURL_MULTI_WAIT
        mres = curl_multi_wait(multi_handle, NULL, 0, max_sleep_time*1000, &fd_count);
#else
        mres = curl_multi_wait_impl(multi_handle, max_sleep_time*1000, &fd_count);
#endif
        if (mres != CURLM_OK) {
            break;
        }
    } while (running_handles);

    if (mres != CURLM_OK) {
        std::stringstream ss;
        ss << "Internal libcurl multi-handle error: " << curl_multi_strerror(mres);
        logTransferEvent(LogMask::Error, rec, "TRANSFER_CURL_ERROR", ss.str());

        char msg[] = "Internal server error due to libcurl";
        curl_multi_remove_handle(multi_handle, curl);
        curl_multi_cleanup(multi_handle);

        if ((retval = req.ChunkResp(msg, 0))) {
            logTransferEvent(LogMask::Error, rec, "RESPONSE_FAIL",
                "Failed to send error message to the TPC client");
            return retval;
        }
        return req.ChunkResp(NULL, 0);
    }

    // Harvest any messages, looking for CURLMSG_DONE.
    CURLMsg *msg;
    do {
        int msgq = 0;
        msg = curl_multi_info_read(multi_handle, &msgq);
        if (msg && (msg->msg == CURLMSG_DONE)) {
            CURL *easy_handle = msg->easy_handle;
            res = msg->data.result;
            curl_multi_remove_handle(multi_handle, easy_handle);
        }
    } while (msg);

    if (!state.GetErrorCode() && res == static_cast<CURLcode>(-1)) { // No transfers returned?!?
        curl_multi_remove_handle(multi_handle, curl);
        curl_multi_cleanup(multi_handle);
        char msg[] = "Internal state error in libcurl";
        logTransferEvent(LogMask::Error, rec, "TRANSFER_CURL_ERROR", msg);

        if ((retval = req.ChunkResp(msg, 0))) {
            logTransferEvent(LogMask::Error, rec, "RESPONSE_FAIL",
                "Failed to send error message to the TPC client");
            return retval;
        }
        return req.ChunkResp(NULL, 0);
    }
    curl_multi_cleanup(multi_handle);

    state.Flush();

    rec.bytes_transferred = state.BytesTransferred();
    rec.tpc_status = state.GetStatusCode();

    // Explicitly finalize the stream (which will close the underlying file
    // handle) before the response is sent.  In some cases, subsequent HTTP
    // requests can occur before the filesystem is done closing the handle -
    // and those requests may occur against partial data.
    state.Finalize();

    // Generate the final response back to the client.
    std::stringstream ss;
    bool success = false;
    if (state.GetStatusCode() >= 400) {
        std::string err = state.GetErrorMessage();
        std::stringstream ss2;
        ss2 << "Remote side failed with status code " << state.GetStatusCode();
        if (!err.empty()) {
            std::replace(err.begin(), err.end(), '\n', ' ');
            ss2 << "; error message: \"" << err << "\"";
        }
        logTransferEvent(LogMask::Error, rec, "TRANSFER_FAIL", ss2.str());
        ss << "failure: " << ss2.str();
    } else if (state.GetErrorCode()) {
        std::string err = state.GetErrorMessage();
        if (err.empty()) {err = "(no error message provided)";}
        else {std::replace(err.begin(), err.end(), '\n', ' ');}
        std::stringstream ss2;
        ss2 << "Error when interacting with local filesystem: " << err;
        logTransferEvent(LogMask::Error, rec, "TRANSFER_FAIL", ss2.str());
        ss << "failure: " << ss2.str();
    } else if (res != CURLE_OK) {
        std::stringstream ss2;
        ss2 << "HTTP library failure: " << curl_easy_strerror(res);
        logTransferEvent(LogMask::Error, rec, "TRANSFER_FAIL", ss2.str());
        ss << "failure: " << curl_easy_strerror(res);
    } else {
        ss << "success: Created";
        success = true;
    }

    if ((retval = req.ChunkResp(ss.str().c_str(), 0))) {
        logTransferEvent(LogMask::Error, rec, "TRANSFER_ERROR",
            "Failed to send last update to remote client");
        return retval;
    } else if (success) {
        logTransferEvent(LogMask::Info, rec, "TRANSFER_SUCCESS");
        rec.status = 0;
    }
    return req.ChunkResp(NULL, 0);
}

/******************************************************************************/
/* !XRD_CHUNK_RESP:                                                           */
/*              T P C H a n d l e r : : R u n C u r l B a s i c               */
/******************************************************************************/
  
#else
int TPCHandler::RunCurlBasic(CURL *curl, XrdHttpExtReq &req, State &state,
                             TPCLogRecord &rec) {
    const char *log_prefix = rec.log_prefix.c_str();
    CURLcode res;
    res = curl_easy_perform(curl);
    state.Flush();
    state.Finalize();
    if (state.GetErrorCode()) {
        std::string err = state.GetErrorMessage();
        if (err.empty()) {err = "(no error message provided)";}
        else {std::replace(err.begin(), err.end(), '\n', ' ');}
        std::stringstream ss2;
        ss2 << "Error when interacting with local filesystem: " << err;
        logTransferEvent(LogMask::Error, rec, "TRANSFER_FAIL", ss2.str());
        ss << "failure: " << ss2.str();
    } else if (res == CURLE_HTTP_RETURNED_ERROR) {
        m_log.Emsg(log_prefix, "Remote server failed request", curl_easy_strerror(res));
        return req.SendSimpleResp(500, NULL, NULL,
                                  const_cast<char *>(curl_easy_strerror(res)), 0);
    } else if (state.GetStatusCode() >= 400) {
        std::stringstream ss;
        ss << "Remote side failed with status code " << state.GetStatusCode();
        m_log.Emsg(log_prefix, "Remote server failed request", ss.str().c_str());
        return req.SendSimpleResp(500, NULL, NULL,
                                  const_cast<char *>(ss.str().c_str()), 0);
    } else if (res) {
        m_log.Emsg(log_prefix, "Curl failed", curl_easy_strerror(res));
        char msg[] = "Unknown internal transfer failure";
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    } else {
        char msg[] = "Created";
        rec.status = 0;
        return req.SendSimpleResp(201, NULL, NULL, msg, 0);
    }
}
#endif

/******************************************************************************/
/*            T P C H a n d l e r : : P r o c e s s P u s h R e q             */
/******************************************************************************/
  
int TPCHandler::ProcessPushReq(const std::string & resource, XrdHttpExtReq &req) {
    TPCLogRecord rec(req.pmark);
    rec.log_prefix = "PushRequest";
    rec.local = req.resource;
    rec.remote = resource;
    char *name = req.GetSecEntity().name;
    req.GetClientID(rec.clID);
    if (name) rec.name = name;
    logTransferEvent(LogMask::Info, rec, "PUSH_START", "Starting a push request");

    ManagedCurlHandle curlPtr(curl_easy_init());
    auto curl = curlPtr.get();
    if (!curl) {
        char msg[] = "Failed to initialize internal transfer resources";
        rec.status = 500;
        logTransferEvent(LogMask::Error, rec, "PUSH_FAIL", msg);
        return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
//  curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockopt_setcloexec_callback);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, opensocket_callback);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &rec);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETFUNCTION, closesocket_callback);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETDATA, &rec);
    auto query_header = req.headers.find("xrd-http-fullresource");
    std::string redirect_resource = req.resource;
    if (query_header != req.headers.end()) {
        redirect_resource = query_header->second;
    }

    AtomicBeg(m_monid_mutex);
    uint64_t file_monid = AtomicInc(m_monid);
    AtomicEnd(m_monid_mutex);
    std::unique_ptr<XrdSfsFile> fh(m_sfs->newFile(name, file_monid));
    if (!fh.get()) {
        rec.status = 500;
        logTransferEvent(LogMask::Error, rec, "OPEN_FAIL",
            "Failed to initialize internal transfer file handle");
        char msg[] = "Failed to initialize internal transfer file handle";
        return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }
    std::string full_url = prepareURL(req);

    std::string authz = GetAuthz(req);

    int open_results = OpenWaitStall(*fh, full_url, SFS_O_RDONLY, 0644,
                                     req.GetSecEntity(), authz);
    if (SFS_REDIRECT == open_results) {
        int result = RedirectTransfer(curl, redirect_resource, req, fh->error, rec);
        return result;
    } else if (SFS_OK != open_results) {
        int code;
        char msg_generic[] = "Failed to open local resource";
        const char *msg = fh->error.getErrText(code);
        if (msg == NULL) msg = msg_generic;
        rec.status = 400;
        if (code == EACCES) rec.status = 401;
        else if (code == EEXIST) rec.status = 412;
        logTransferEvent(LogMask::Error, rec, "OPEN_FAIL", msg);
        int resp_result = req.SendSimpleResp(rec.status, NULL, NULL,
                                             const_cast<char *>(msg), 0);
        fh->close();
        return resp_result;
    }
    ConfigureCurlCA(curl);
    curl_easy_setopt(curl, CURLOPT_URL, resource.c_str());

    Stream stream(std::move(fh), 0, 0, m_log);
    State state(0, stream, curl, true);
    state.CopyHeaders(req);

#ifdef XRD_CHUNK_RESP
    return RunCurlWithUpdates(curl, req, state, rec);
#else
    return RunCurlBasic(curl, req, state, rec);
#endif
}

/******************************************************************************/
/*            T P C H a n d l e r : : P r o c e s s P u l l R e q             */
/******************************************************************************/
  
int TPCHandler::ProcessPullReq(const std::string &resource, XrdHttpExtReq &req) {
    TPCLogRecord rec(req.pmark);
    rec.log_prefix = "PullRequest";
    rec.local = req.resource;
    rec.remote = resource;
    char *name = req.GetSecEntity().name;
    req.GetClientID(rec.clID);
    if (name) rec.name = name;
    logTransferEvent(LogMask::Info, rec, "PULL_START", "Starting a pull request");

    ManagedCurlHandle curlPtr(curl_easy_init());
    auto curl = curlPtr.get();
    if (!curl) {
            char msg[] = "Failed to initialize internal transfer resources";
            rec.status = 500;
            logTransferEvent(LogMask::Error, rec, "PULL_FAIL", msg);
            return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
//  curl_easy_setopt(curl,CURLOPT_SOCKOPTFUNCTION,sockopt_setcloexec_callback);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, opensocket_callback);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &rec);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETFUNCTION, closesocket_callback);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETDATA, &rec);
    std::unique_ptr<XrdSfsFile> fh(m_sfs->newFile(name, m_monid++));
    if (!fh.get()) {
            char msg[] = "Failed to initialize internal transfer file handle";
             rec.status = 500;
            logTransferEvent(LogMask::Error, rec, "PULL_FAIL", msg);
            return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
    }
    auto query_header = req.headers.find("xrd-http-fullresource");
    std::string redirect_resource = req.resource;
    if (query_header != req.headers.end()) {
        redirect_resource = query_header->second;
    }
    XrdSfsFileOpenMode mode = SFS_O_CREAT;
    auto overwrite_header = req.headers.find("Overwrite");
    if ((overwrite_header == req.headers.end()) || (overwrite_header->second == "T")) {
        if (! usingEC) mode = SFS_O_TRUNC;
    }
    int streams = 1;
    {
        auto streams_header = req.headers.find("X-Number-Of-Streams");
        if (streams_header != req.headers.end()) {
            int stream_req = -1;
            try {
                stream_req = std::stol(streams_header->second);
            } catch (...) { // Handled below
            }
            if (stream_req < 0 || stream_req > 100) {
                char msg[] = "Invalid request for number of streams";
                rec.status = 500;
                logTransferEvent(LogMask::Info, rec, "INVALID_REQUEST", msg);
                return req.SendSimpleResp(rec.status, NULL, NULL, msg, 0);
            }
            streams = stream_req == 0 ? 1 : stream_req;
        }
    }
    rec.streams = streams;
    bool hasSetOpaque = false;
    std::string full_url = prepareURL(req, hasSetOpaque);
    std::string authz = GetAuthz(req);
    curl_easy_setopt(curl, CURLOPT_URL, resource.c_str());
    ConfigureCurlCA(curl);
#ifdef XRD_CHUNK_RESP
    {
        //Get the content-length of the source file and pass it to the OSS layer
        //during the open
        uint64_t sourceFileContentLength = 0;
        bool success;
        GetContentLengthTPCPull(curl, req, sourceFileContentLength, success, rec);
        if(success) {
            //In the case we cannot get the information from the source server (offline or other error)
            //we just don't add the size information to the opaque of the local file to open
            full_url += hasSetOpaque ? "&" : "?";
            full_url += "oss.asize=" + std::to_string(sourceFileContentLength);
        }
    }
#endif
    int open_result = OpenWaitStall(*fh, full_url, mode|SFS_O_WRONLY, 0644,
                                    req.GetSecEntity(), authz);
    if (SFS_REDIRECT == open_result) {
        int result = RedirectTransfer(curl, redirect_resource, req, fh->error, rec);
        return result;
    } else if (SFS_OK != open_result) {
        int code;
        char msg_generic[] = "Failed to open local resource";
        const char *msg = fh->error.getErrText(code);
        if ((msg == NULL) || (*msg == '\0')) msg = msg_generic;
        rec.status = 400;
        if (code == EACCES) rec.status = 401;
        else if (code == EEXIST) rec.status = 412;
        logTransferEvent(LogMask::Error, rec, "OPEN_FAIL", msg);
        int resp_result = req.SendSimpleResp(rec.status, NULL, NULL,
                                             const_cast<char *>(msg), 0);
        fh->close();
        return resp_result;
    }
    Stream stream(std::move(fh), streams * m_pipelining_multiplier, streams > 1 ? m_block_size : m_small_block_size, m_log);
    State state(0, stream, curl, false);
    state.CopyHeaders(req);

#ifdef XRD_CHUNK_RESP
    if (streams > 1) {
        return RunCurlWithStreams(req, state, streams, rec);
    } else {
        return RunCurlWithUpdates(curl, req, state, rec);
    }
#else
    return RunCurlBasic(curl, req, state, rec);
#endif
}

/******************************************************************************/
/*          T P C H a n d l e r : : l o g T r a n s f e r E v e n t           */
/******************************************************************************/
  
void TPCHandler::logTransferEvent(LogMask mask, const TPCLogRecord &rec,
        const std::string &event, const std::string &message)
{
    if (!(m_log.getMsgMask() & mask)) {return;}

    std::stringstream ss;
    ss << "event=" << event << ", local=" << rec.local << ", remote=" << rec.remote;
    if (rec.name.empty())
       ss << ", user=(anonymous)";
    else
       ss << ", user=" << rec.name;
    if (rec.streams != 1)
       ss << ", streams=" << rec.streams;
    if (rec.bytes_transferred >= 0)
       ss << ", bytes_transferred=" << rec.bytes_transferred;
    if (rec.status >= 0)
       ss << ", status=" << rec.status;
    if (rec.tpc_status >= 0)
       ss << ", tpc_status=" << rec.tpc_status;
    if (!message.empty())
       ss << "; " << message;
    m_log.Log(mask, rec.log_prefix.c_str(), ss.str().c_str());
}

/******************************************************************************/
/*                  X r d H t t p G e t E x t H a n d l e r                   */
/******************************************************************************/
  
extern "C" {

XrdHttpExtHandler *XrdHttpGetExtHandler(XrdSysError *log, const char * config, const char * /*parms*/, XrdOucEnv *myEnv) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT)) {
        log->Emsg("TPCInitialize", "libcurl failed to initialize");
        return NULL;
    }

    TPCHandler *retval{NULL};
    if (!config) {
        log->Emsg("TPCInitialize", "TPC handler requires a config filename in order to load");
        return NULL;
    }
    try {
        log->Emsg("TPCInitialize", "Will load configuration for the TPC handler from", config);
        retval = new TPCHandler(log, config, myEnv);
    } catch (std::runtime_error &re) {
        log->Emsg("TPCInitialize", "Encountered a runtime failure when loading ", re.what());
        //printf("Provided env vars: %p, XrdInet*: %p\n", myEnv, myEnv->GetPtr("XrdInet*"));
    }
    return retval;
}

}
