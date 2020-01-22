
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdVersion.hh"

#include <curl/curl.h>

#include <dlfcn.h>
#include <fcntl.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "XrdTpcState.hh"
#include "XrdTpcStream.hh"
#include "XrdTpcTPC.hh"
#include "XrdTpcCurlMulti.hh"

using namespace TPC;

uint64_t TPCHandler::m_monid{0};
int TPCHandler::m_marker_period = 5;
size_t TPCHandler::m_block_size = 16*1024*1024;
XrdSysMutex TPCHandler::m_monid_mutex;

XrdVERSIONINFO(XrdHttpGetExtHandler, HttpTPC);


// We need to utilize the full URL (including the query string), not just the
// resource name.  The query portion is hidden in the `xrd-http-query` header;
// we take this out and combine it with the resource name.
//
// One special key is `authz`; this is always stripped out and copied to the Authorization
// header (which will later be used for XrdSecEntity).  The latter copy is only done if
// the Authorization header is not already present.
static std::string prepareURL(XrdHttpExtReq &req) {
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

  return req.resource + result.str().c_str();
}


bool TPCHandler::MatchesPath(const char *verb, const char *path) {
    return !strcmp(verb, "COPY") || !strcmp(verb, "OPTIONS");
}

static std::string PrepareURL(const std::string &input) {
    if (!strncmp(input.c_str(), "davs://", 7)) {
        return "https://" + input.substr(7);
    }
    return input;
}

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
        m_log.Emsg("ProcessReq", "Pull request from", src.c_str());
        return ProcessPullReq(src, req);
    }
    header = req.headers.find("Destination");
    if (header != req.headers.end()) {
        return ProcessPushReq(header->second, req);
    }
    m_log.Emsg("ProcessReq", "COPY verb requested but no source or destination specified.");
    return req.SendSimpleResp(400, NULL, NULL, "No Source or Destination specified", 0);
}

TPCHandler::~TPCHandler() {
    m_sfs = NULL;  // NOTE: must delete the SFS here as we may unload the destructor from memory below!
    if (m_handle_base) {
        dlclose(m_handle_base);
        m_handle_base = NULL;
    }
    if (m_handle_chained) {
        dlclose(m_handle_chained);
        m_handle_chained = NULL;
    }
}

TPCHandler::TPCHandler(XrdSysError *log, const char *config, XrdOucEnv *myEnv) :
        m_desthttps(false),
        m_log(*log),
        m_handle_base(NULL),
        m_handle_chained(NULL)
{
    if (!Configure(config, myEnv)) {
        throw std::runtime_error("Failed to configure the HTTP third-party-copy handler.");
    }
}

/**
 * Handle the OPTIONS verb as we have added a new one...
 */
int TPCHandler::ProcessOptionsReq(XrdHttpExtReq &req) {
    return req.SendSimpleResp(200, NULL, (char *) "DAV: 1\r\nDAV: <http://apache.org/dav/propset/fs/1>\r\nAllow: HEAD,GET,PUT,PROPFIND,DELETE,OPTIONS,COPY", NULL, 0);
}

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

int TPCHandler::RedirectTransfer(const std::string &redirect_resource, XrdHttpExtReq &req, XrdOucErrInfo &error) {
    int port;
    const char *ptr = error.getErrText(port);
    if ((ptr == NULL) || (*ptr == '\0') || (port == 0)) {
        char msg[] = "Internal error: redirect without hostname";
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
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
      ss << "?" << opaque;
    }

    return req.SendSimpleResp(307, NULL, const_cast<char *>(ss.str().c_str()), NULL, 0);
}

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

        // Append the authz information
        opaque += (opaque.empty() ? "" : "&");
        opaque += authz;
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

#ifdef XRD_CHUNK_RESP
/**
 * Determine size at remote end.
 */
int TPCHandler::DetermineXferSize(CURL *curl, XrdHttpExtReq &req, State &state,
                                  bool &success) {
    success = false;
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    CURLcode res;
    res = curl_easy_perform(curl);
    if (res == CURLE_HTTP_RETURNED_ERROR) {
        m_log.Emsg("DetermineXferSize", "Remote server failed request", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return req.SendSimpleResp(500, NULL, NULL, const_cast<char *>(curl_easy_strerror(res)), 0);
    } else if (state.GetStatusCode() >= 400) {
        std::stringstream ss;
        ss << "Remote side failed with status code " << state.GetStatusCode();
        m_log.Emsg("DetermineXferSize", "Remote server failed request", ss.str().c_str());
        curl_easy_cleanup(curl);
        return req.SendSimpleResp(500, NULL, NULL, const_cast<char *>(ss.str().c_str()), 0);
    } else if (res) {
        m_log.Emsg("DetermineXferSize", "Curl failed", curl_easy_strerror(res));
        char msg[] = "Unknown internal transfer failure";
        curl_easy_cleanup(curl);
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
    success = true;
    return 0;
}

int TPCHandler::SendPerfMarker(XrdHttpExtReq &req, off_t bytes_transferred) {
    std::stringstream ss;
    const std::string crlf = "\n";
    ss << "Perf Marker" << crlf;
    ss << "Timestamp: " << time(NULL) << crlf;
    ss << "Stripe Index: 0" << crlf;
    ss << "Stripe Bytes Transferred: " << bytes_transferred << crlf;
    ss << "Total Stripe Count: 1" << crlf;
    ss << "End" << crlf;

    return req.ChunkResp(ss.str().c_str(), 0);
}

int TPCHandler::RunCurlWithUpdates(CURL *curl, XrdHttpExtReq &req, State &state,
                                   const char *log_prefix)
{
    // Create the multi-handle and add in the current transfer to it.
    CURLM *multi_handle = curl_multi_init();
    if (!multi_handle) {
        m_log.Emsg(log_prefix, "Failed to initialize a libcurl multi-handle");
        char msg[] = "Failed to initialize internal server memory";
        curl_easy_cleanup(curl);
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }

    CURLMcode mres;
    mres = curl_multi_add_handle(multi_handle, curl);
    if (mres) {
        m_log.Emsg(log_prefix, "Failed to add transfer to libcurl multi-handle",
                   curl_multi_strerror(mres));
        char msg[] = "Failed to initialize internal server handle";
        curl_easy_cleanup(curl);
        curl_multi_cleanup(multi_handle);
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }

    // Start response to client prior to the first call to curl_multi_perform
    int retval = req.StartChunkedResp(201, "Created", "Content-Type: text/plain");
    if (retval) {
        curl_easy_cleanup(curl);
        curl_multi_cleanup(multi_handle);
        return retval;
    }

    // Transfer loop: use curl to actually run the transfer, but periodically
    // interrupt things to send back performance updates to the client.
    int running_handles = 1;
    time_t last_marker = 0;
    CURLcode res = static_cast<CURLcode>(-1);
    do {
        time_t now = time(NULL);
        time_t next_marker = last_marker + m_marker_period;
        if (now >= next_marker) {
            if (SendPerfMarker(req, state.BytesTransferred())) {
                curl_multi_remove_handle(multi_handle, curl);
                curl_easy_cleanup(curl);
                curl_multi_cleanup(multi_handle);
                return -1;
            }
            last_marker = now;
        }
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
                curl_easy_cleanup(easy_handle);
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
        m_log.Emsg(log_prefix, "Internal libcurl multi-handle error",
                   curl_multi_strerror(mres));
        char msg[] = "Internal server error due to libcurl";
        curl_multi_remove_handle(multi_handle, curl);
        curl_easy_cleanup(curl);
        curl_multi_cleanup(multi_handle);

        if ((retval = req.ChunkResp(msg, 0))) {
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
            curl_easy_cleanup(easy_handle);
        }
    } while (msg);

    if (res == -1) { // No transfers returned?!?
        curl_multi_remove_handle(multi_handle, curl);
        curl_easy_cleanup(curl);
        curl_multi_cleanup(multi_handle);
        char msg[] = "Internal state error in libcurl";
        m_log.Emsg(log_prefix, msg);

        if ((retval = req.ChunkResp(msg, 0))) {
            return retval;
        }
        return req.ChunkResp(NULL, 0);
    }
    curl_multi_cleanup(multi_handle);

    // Generate the final response back to the client.
    std::stringstream ss;
    if (res != CURLE_OK) {
        m_log.Emsg(log_prefix, "Remote server failed request", curl_easy_strerror(res));
        ss << "failure: " << curl_easy_strerror(res);
    } else if (state.GetStatusCode() >= 400) {
        ss << "failure: Remote side failed with status code " << state.GetStatusCode();
        m_log.Emsg(log_prefix, "Remote server failed request", ss.str().c_str());
    } else {
        ss << "success: Created";
    }

    if ((retval = req.ChunkResp(ss.str().c_str(), 0))) {
        return retval;
    }
    return req.ChunkResp(NULL, 0);
}
#else
int TPCHandler::RunCurlBasic(CURL *curl, XrdHttpExtReq &req, State &state,
                             const char *log_prefix) {
    CURLcode res;
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res == CURLE_HTTP_RETURNED_ERROR) {
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
        return req.SendSimpleResp(201, NULL, NULL, msg, 0);
    }
}
#endif

int TPCHandler::ProcessPushReq(const std::string & resource, XrdHttpExtReq &req) {
    m_log.Emsg("ProcessPushReq", "Starting a push request for resource", resource.c_str());
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    if (!curl) {
        char msg[] = "Failed to initialize internal transfer resources";
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }
    auto query_header = req.headers.find("xrd-http-fullresource");
    std::string redirect_resource = req.resource;
    if (query_header != req.headers.end()) {
        redirect_resource = query_header->second;
    }

    char *name = req.GetSecEntity().name;
    AtomicBeg(m_monid_mutex);
    uint64_t file_monid = AtomicInc(m_monid);
    AtomicEnd(m_monid_mutex);
    std::unique_ptr<XrdSfsFile> fh(m_sfs->newFile(name, file_monid));
    if (!fh.get()) {
        curl_easy_cleanup(curl);
        char msg[] = "Failed to initialize internal transfer file handle";
        return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }
    std::string full_url = prepareURL(req);

    std::string authz = GetAuthz(req);

    int open_results = OpenWaitStall(*fh, full_url, SFS_O_RDONLY, 0644,
                                     req.GetSecEntity(), authz);
    if (SFS_REDIRECT == open_results) {
        curl_easy_cleanup(curl);
        return RedirectTransfer(redirect_resource, req, fh->error);
    } else if (SFS_OK != open_results) {
        curl_easy_cleanup(curl);
        int code;
        char msg_generic[] = "Failed to open local resource";
        const char *msg = fh->error.getErrText(code);
        if (msg == NULL) msg = msg_generic;
        int status_code = 400;
        if (code == EACCES) status_code = 401;
        int resp_result = req.SendSimpleResp(status_code, NULL, NULL,
                                             const_cast<char *>(msg), 0);
        fh->close();
        return resp_result;
    }
    if (!m_cadir.empty()) {
            curl_easy_setopt(curl, CURLOPT_CAPATH, m_cadir.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_URL, resource.c_str());

    Stream stream(std::move(fh), 0, 0, m_log);
    State state(0, stream, curl, true);
    state.CopyHeaders(req);

#ifdef XRD_CHUNK_RESP
    return RunCurlWithUpdates(curl, req, state, "ProcessPushReq");
#else
    return RunCurlBasic(curl, req, state, "ProcessPushReq");
#endif
}

int TPCHandler::ProcessPullReq(const std::string &resource, XrdHttpExtReq &req) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    if (!curl) {
            char msg[] = "Failed to initialize internal transfer resources";
            return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }
    char *name = req.GetSecEntity().name;
    std::unique_ptr<XrdSfsFile> fh(m_sfs->newFile(name, m_monid++));
    if (!fh.get()) {
            curl_easy_cleanup(curl);
            char msg[] = "Failed to initialize internal transfer file handle";
            return req.SendSimpleResp(500, NULL, NULL, msg, 0);
    }
    auto query_header = req.headers.find("xrd-http-fullresource");
    std::string redirect_resource = req.resource;
    if (query_header != req.headers.end()) {
        redirect_resource = query_header->second;
    }
    XrdSfsFileOpenMode mode = SFS_O_CREAT;
    auto overwrite_header = req.headers.find("Overwrite");
    if ((overwrite_header == req.headers.end()) || (overwrite_header->second == "T")) {
        mode = SFS_O_TRUNC;
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
                curl_easy_cleanup(curl);
                char msg[] = "Invalid request for number of streams";
                m_log.Emsg("ProcessPullReq", msg);
                return req.SendSimpleResp(500, NULL, NULL, msg, 0);
            }
            streams = streams == 0 ? 1 : stream_req;
        }
    }
    std::string full_url = prepareURL(req);
    std::string authz = GetAuthz(req);

    int open_result = OpenWaitStall(*fh, full_url, mode|SFS_O_WRONLY, 0644,
                                    req.GetSecEntity(), authz);
    if (SFS_REDIRECT == open_result) {
        curl_easy_cleanup(curl);
        return RedirectTransfer(redirect_resource, req, fh->error);
    } else if (SFS_OK != open_result) {
        curl_easy_cleanup(curl);
        int code;
        char msg_generic[] = "Failed to open local resource";
        const char *msg = fh->error.getErrText(code);
        if ((msg == NULL) || (*msg == '\0')) msg = msg_generic;
        int status_code = 400;
        if (code == EACCES) status_code = 401;
        if (code == EEXIST) status_code = 412;
        int resp_result = req.SendSimpleResp(status_code, NULL, NULL,
                                             const_cast<char *>(msg), 0);
        fh->close();
        return resp_result;
    }
    if (!m_cadir.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAPATH, m_cadir.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_URL, resource.c_str());
    Stream stream(std::move(fh), streams * m_pipelining_multiplier, m_block_size, m_log);
    State state(0, stream, curl, false);
    state.CopyHeaders(req);

#ifdef XRD_CHUNK_RESP
    if (streams > 1) {
        return RunCurlWithStreams(req, state, "ProcessPullReq", streams);
    } else {
        return RunCurlWithUpdates(curl, req, state, "ProcessPullReq");
    }
#else
    return RunCurlBasic(curl, req, state, "ProcessPullReq");
#endif
}


extern "C" {

XrdHttpExtHandler *XrdHttpGetExtHandler(XrdSysError *log, const char * config, const char * /*parms*/, XrdOucEnv *myEnv) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT)) {
        log->Emsg("Initialize", "libcurl failed to initialize");
        return NULL;
    }

    TPCHandler *retval{NULL};
    if (!config) {
        log->Emsg("Initialize", "TPC handler requires a config filename in order to load");
        return NULL;
    }
    try {
        log->Emsg("Initialize", "Will load configuration for the TPC handler from", config);
        retval = new TPCHandler(log, config, myEnv);
    } catch (std::runtime_error &re) {
        log->Emsg("Initialize", "Encountered a runtime failure when loading ", re.what());
        //printf("Provided env vars: %p, XrdInet*: %p\n", myEnv, myEnv->GetPtr("XrdInet*"));
    }
    return retval;
}

}

