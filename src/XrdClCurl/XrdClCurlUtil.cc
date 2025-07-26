/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClCurl client plugin for XRootD.               */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "XrdClCurlFile.hh"
#include "XrdClCurlOps.hh"
#include "XrdClCurlOptionsCache.hh"
#include "XrdClCurlUtil.hh"
#include "XrdClCurlWorker.hh"

#include <XProtocol/XProtocol.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClURL.hh>
#include <XrdCl/XrdClXRootDResponses.hh>
#include <XrdOuc/XrdOucCRC.hh>
#include <XrdSys/XrdSysPageSize.hh>
#include <XrdVersion.hh>

#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include <fcntl.h>
#include <fstream>
#ifdef __APPLE__
#include <pthread.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#endif
#include <unistd.h>

#include <charconv>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace XrdClCurl;

thread_local std::vector<CURL*> HandlerQueue::m_handles;
std::atomic<unsigned> CurlWorker::m_maintenance_period = 5;
std::vector<CurlWorker*> CurlWorker::m_workers;
std::mutex CurlWorker::m_workers_mutex;

// Performance statistics for the worker
std::atomic<uint64_t> CurlWorker::m_conncall_errors = 0;
std::atomic<uint64_t> CurlWorker::m_conncall_req = 0;
std::atomic<uint64_t> CurlWorker::m_conncall_success = 0;
std::atomic<uint64_t> CurlWorker::m_conncall_timeout = 0;
decltype(CurlWorker::m_ops) CurlWorker::m_ops = {};
std::vector<std::atomic<std::chrono::system_clock::rep>*> CurlWorker::m_workers_last_completed_cycle;
std::vector<std::atomic<std::chrono::system_clock::rep>*> CurlWorker::m_workers_oldest_op;
std::mutex CurlWorker::m_worker_stats_mutex;

// Performance statistics for the queue
std::atomic<uint64_t> HandlerQueue::m_ops_consumed = 0; // Count of operations consumed from the queue.
std::atomic<uint64_t> HandlerQueue::m_ops_produced = 0; // Count of operations added to the queue.
std::atomic<uint64_t> HandlerQueue::m_ops_rejected = 0; // Count of operations rejected by the queue.

struct WaitingForBroker {
    CURL *curl{nullptr};
    time_t expiry{0};
};

namespace {

pid_t getthreadid() {
#ifdef __APPLE__
    uint64_t pth_threadid;
    pthread_threadid_np(pthread_self(), &pth_threadid);
    return pth_threadid;
#else
    // NOTE: glibc 2.30 finally provides a gettid() wrapper; however,
    // we currently support RHEL 8, which is based on glibc 2.28.  Until
    // we drop that platform, it's easier to do the syscall directly on Linux
    // instead of additional ifdef calls.
    return syscall(SYS_gettid);
#endif
}

}

bool XrdClCurl::HTTPStatusIsError(unsigned status) {
     return (status < 100) || (status >= 400);
}

std::pair<uint16_t, uint32_t> XrdClCurl::HTTPStatusConvert(unsigned status) {
    //std::cout << "HTTPStatusConvert: " << status << "\n";
    switch (status) {
        case 400: // Bad Request
            return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);
        case 401: // Unauthorized (needs authentication)
            return std::make_pair(XrdCl::errErrorResponse, kXR_NotAuthorized);
        case 402: // Payment Required
        case 403: // Forbidden (failed authorization)
            return std::make_pair(XrdCl::errErrorResponse, kXR_NotAuthorized);
        case 404:
            return std::make_pair(XrdCl::errErrorResponse, kXR_NotFound);
        case 405: // Method not allowed
        case 406: // Not acceptable
            return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);
        case 407: // Proxy Authentication Required
            return std::make_pair(XrdCl::errErrorResponse, kXR_NotAuthorized);
        case 408: // Request timeout
            return std::make_pair(XrdCl::errErrorResponse, kXR_ReqTimedOut);
        case 409: // Conflict
            return std::make_pair(XrdCl::errErrorResponse, kXR_Conflict);
        case 410: // Gone
            return std::make_pair(XrdCl::errErrorResponse, kXR_NotFound);
        case 411: // Length required
        case 412: // Precondition failed
        case 413: // Payload too large
        case 414: // URI too long
        case 415: // Unsupported Media Type
        case 416: // Range Not Satisfiable
        case 417: // Expectation Failed
        case 418: // I'm a teapot
	        return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);
        case 421: // Misdirected Request
        case 422: // Unprocessable Content
            return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);
        case 423: // Locked
            return std::make_pair(XrdCl::errErrorResponse, kXR_FileLocked);
        case 424: // Failed Dependency
        case 425: // Too Early
        case 426: // Upgrade Required
        case 428: // Precondition Required
            return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);
        case 429: // Too Many Requests
            return std::make_pair(XrdCl::errErrorResponse, kXR_Overloaded);
        case 431: // Request Header Fields Too Large
            return std::make_pair(XrdCl::errErrorResponse, kXR_InvalidRequest);
        case 451: // Unavailable For Legal Reasons
            return std::make_pair(XrdCl::errErrorResponse, kXR_Impossible);
        case 500: // Internal Server Error
        case 501: // Not Implemented
        case 502: // Bad Gateway
        case 503: // Service Unavailable
            return std::make_pair(XrdCl::errErrorResponse, kXR_ServerError);
        case 504: // Gateway Timeout
            return std::make_pair(XrdCl::errErrorResponse, kXR_ReqTimedOut);
        case 507: // Insufficient Storage
            return std::make_pair(XrdCl::errErrorResponse, kXR_overQuota);
        case 508: // Loop Detected
        case 510: // Not Extended
        case 511: // Network Authentication Required
            return std::make_pair(XrdCl::errErrorResponse, kXR_ServerError);
    }
    return std::make_pair(XrdCl::errUnknown, status);
}

std::pair<uint16_t, uint32_t> CurlCodeConvert(CURLcode res) {
    switch (res) {
        case CURLE_OK:
            return std::make_pair(XrdCl::errNone, 0);
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
            return std::make_pair(XrdCl::errInvalidAddr, 0);
        case CURLE_LOGIN_DENIED:
        // Commented-out cases are for platforms (RHEL7) where the error
        // codes are undefined.
        //case CURLE_AUTH_ERROR:
        //case CURLE_SSL_CLIENTCERT:
        case CURLE_REMOTE_ACCESS_DENIED:
            return std::make_pair(XrdCl::errLoginFailed, EACCES);
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_SSL_ENGINE_NOTFOUND:
        case CURLE_SSL_ENGINE_SETFAILED:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case 51: // In old curl versions, this is CURLE_PEER_FAILED_VERIFICATION; that constant was changed to be 60 / CURLE_SSL_CACERT
        case CURLE_SSL_SHUTDOWN_FAILED:
        case CURLE_SSL_CRL_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
        case CURLE_SSL_CACERT: // value is 60; merged with CURLE_PEER_FAILED_VERIFICATION
        //case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
        //case CURLE_SSL_INVALIDCERTSTATUS:
            return std::make_pair(XrdCl::errTlsError, 0);
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
            return std::make_pair(XrdCl::errSocketError, EIO);
        case CURLE_COULDNT_CONNECT:
        case CURLE_GOT_NOTHING:
            return std::make_pair(XrdCl::errConnectionError, ECONNREFUSED);
        case CURLE_OPERATION_TIMEDOUT:
#ifdef HAVE_XPROTOCOL_TIMEREXPIRED
            return std::make_pair(XrdCl::errErrorResponse, XErrorCode::kXR_TimerExpired);
#else
            return std::make_pair(XrdCl::errOperationExpired, ESTALE);
#endif
        case CURLE_UNSUPPORTED_PROTOCOL:
        case CURLE_NOT_BUILT_IN:
            return std::make_pair(XrdCl::errNotSupported, ENOSYS);
        case CURLE_FAILED_INIT:
            return std::make_pair(XrdCl::errInternal, 0);
        case CURLE_URL_MALFORMAT:
            return std::make_pair(XrdCl::errInvalidArgs, res);
        //case CURLE_WEIRD_SERVER_REPLY:
        //case CURLE_HTTP2:
        //case CURLE_HTTP2_STREAM:
            return std::make_pair(XrdCl::errCorruptedHeader, res);
        case CURLE_PARTIAL_FILE:
            return std::make_pair(XrdCl::errDataError, res);
        // These two errors indicate a failure in the callback.  That
        // should generate their own failures, meaning this should never
        // get use.
        case CURLE_READ_ERROR:
        case CURLE_WRITE_ERROR:
            return std::make_pair(XrdCl::errInternal, res);
        case CURLE_RANGE_ERROR:
        case CURLE_BAD_CONTENT_ENCODING:
            return std::make_pair(XrdCl::errNotSupported, res);
        case CURLE_TOO_MANY_REDIRECTS:
            return std::make_pair(XrdCl::errRedirectLimit, res);
        default:
            return std::make_pair(XrdCl::errUnknown, res);
    }
}

bool HeaderParser::Base64Decode(std::string_view input, std::array<unsigned char, 32> &output) {
    if (input.size() > 44 || input.size() % 4 != 0) return false;
    if (input.size() == 0) return true;

    std::unique_ptr<BIO, decltype(&BIO_free_all)> b64(BIO_new(BIO_f_base64()), &BIO_free_all);
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    std::unique_ptr<BIO, decltype(&BIO_free_all)> bmem(
        BIO_new_mem_buf(const_cast<char *>(input.data()), input.size()), &BIO_free_all);
    bmem.reset(BIO_push(b64.release(), bmem.release()));

    // Compute expected length of output; used to verify BIO_read consumes all input
    size_t expectedLen = static_cast<size_t>(input.size() * 0.75);
    if (input[input.size() - 1] == '=') {
        expectedLen -= 1;
        if (input[input.size() - 2] == '=') {
            expectedLen -= 1;
        }
    }

    auto len = BIO_read(bmem.get(), &output[0], output.size());

    if (len == -1 || static_cast<size_t>(len) != expectedLen) return false;

    return true;
}

// Parse a single header line.
//
// Curl promises for its callbacks "The header callback is
// called once for each header and only complete header lines
// are passed on to the callback".
bool HeaderParser::Parse(const std::string &header_line)
{
    if (m_recv_all_headers) {
        m_recv_all_headers = false;
        m_recv_status_line = false;
    }

    if (!m_recv_status_line) {
        m_recv_status_line = true;

        std::stringstream ss(header_line);
        std::string item;
        if (!std::getline(ss, item, ' ')) return false;
        m_resp_protocol = item;
        if (!std::getline(ss, item, ' ')) return false;
        try {
            m_status_code = std::stol(item);
        } catch (...) {
            return false;
        }
        if (m_status_code < 100 || m_status_code >= 600) {
            return false;
        }
        if (!std::getline(ss, item, '\n')) return false;
        auto cr_loc = item.find('\r');
        if (cr_loc != std::string::npos) {
            m_resp_message = item.substr(0, cr_loc);
        } else {
            m_resp_message = item;
        }
        return true;
    }

    if (header_line.empty() || header_line == "\n" || header_line == "\r\n") {
        m_recv_all_headers = true;
        return true;
    }

    auto found = header_line.find(":");
    if (found == std::string::npos) {
        return false;
    }

    std::string header_name = header_line.substr(0, found);
    if (!Canonicalize(header_name)) {
        return false;
    }

    found += 1;
    while (found < header_line.size()) {
        if (header_line[found] != ' ') {break;}
        found += 1;
    }
    std::string header_value = header_line.substr(found);
    // Note: ignoring the fact headers are only supposed to contain ASCII.
    // We should trim out UTF-8.
    header_value.erase(header_value.find_last_not_of(" \r\n\t") + 1);

    // Record the line in our header structure.  Will be returned as part
    // of the response info object.
    auto iter = m_headers.find(header_name);
    if (iter == m_headers.end()) {
        m_headers.insert(iter, {header_name, {header_value}});
    } else {
        iter->second.push_back(header_value);
    }

    if (header_name == "Allow") {
        std::string_view val(header_value);
        while (!val.empty()) {
            auto found = val.find(',');
            auto method = val.substr(0, found);
            if (method == "PROPFIND") {
                auto new_verbs = static_cast<unsigned>(m_allow_verbs) | static_cast<unsigned>(VerbsCache::HttpVerb::kPROPFIND);
                m_allow_verbs = static_cast<VerbsCache::HttpVerb>(new_verbs);
            }
            if (found == std::string_view::npos) break;
            val = val.substr(found + 1);
        }
        if (static_cast<unsigned>(m_allow_verbs) & ~static_cast<unsigned>(VerbsCache::HttpVerb::kUnknown)) {
            m_allow_verbs = static_cast<VerbsCache::HttpVerb>(static_cast<unsigned>(m_allow_verbs) & ~static_cast<unsigned>(VerbsCache::HttpVerb::kUnknown));
        }
    } else if (header_name == "Content-Length") {
         try {
             m_content_length = std::stoll(header_value);
         } catch (...) {
             return false;
         }
    }
    else if (header_name == "Content-Type") {
        std::string_view val(header_value);
        auto found = val.find(";");
        auto first_type = val.substr(0, found);
        m_multipart_byteranges = first_type == "multipart/byteranges";
        if (m_multipart_byteranges) {
            auto remainder = val.substr(found + 1);
            found = remainder.find("boundary=");
            if (found != std::string_view::npos) {
                SetMultipartSeparator(remainder.substr(found + 9));
            }
        }
    }
    else if (header_name == "Content-Range") {
        auto found = header_value.find(" ");
        if (found == std::string::npos) {
            return false;
        }
        std::string range_unit = header_value.substr(0, found);
        if (range_unit != "bytes") {
            return false;
        }
        auto range_resp = header_value.substr(found + 1);
        found = range_resp.find("/");
        if (found == std::string::npos) {
            return false;
        }
        auto incl_range = range_resp.substr(0, found);
        found = incl_range.find("-");
        if (found == std::string::npos) {
            return false;
        }
        auto first_pos = incl_range.substr(0, found);
        try {
            m_response_offset = std::stoll(first_pos);
        } catch (...) {
           return false;
        }
        auto last_pos = incl_range.substr(found + 1);
        size_t last_byte;
        try {
           last_byte = std::stoll(last_pos);
        } catch (...) {
           return false;
        }
        m_content_length = last_byte - m_response_offset + 1;
    }
    else if (header_name == "Location") {
        m_location = header_value;
    } else if (header_name == "Digest") {
        ParseDigest(header_value, m_checksums);
    }
    else if (header_name == "Etag")
    {
        // Note, the original hader name is ETag, renamed to Etag in parsing
        // remove additional quotes
        m_etag = header_value;
        m_etag.erase(remove(m_etag.begin(), m_etag.end(), '\"'), m_etag.end());
    }
    else if (header_name == "Cache-Control")
    {
        m_cache_control = header_value;
    }

    return true;
}

// Parse a RFC 3230 header into the checksum info structure
//
// If the parsing fails, the second element of the tuple will be false.
void HeaderParser::ParseDigest(const std::string &digest, XrdClCurl::ChecksumInfo &info) {
    std::string_view view(digest);
    std::array<unsigned char, 32> checksum_value;
    std::string digest_lower;
    while (!view.empty()) {
        auto nextsep = view.find(',');
        auto entry = view.substr(0, nextsep);
        if (nextsep == std::string_view::npos) {
            view = "";
        } else {
            view = view.substr(nextsep + 1);
        }
        nextsep = entry.find('=');
        auto name = entry.substr(0, nextsep);
        auto value = entry.substr(nextsep + 1);
        digest_lower.clear();
        digest_lower.resize(name.size());
        std::transform(name.begin(), name.end(), digest_lower.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        if (digest_lower == "md5") {
            if (value.size() != 24) {
                continue;
            }
            if (Base64Decode(value, checksum_value)) {
                info.Set(XrdClCurl::ChecksumType::kMD5, checksum_value);
            }
        } else if (digest_lower == "crc32c") {
            // XRootD currently incorrectly base64-encodes crc32c checksums; see
            // https://github.com/xrootd/xrootd/issues/2456
            // For backward comaptibility, if this looks like base64 encoded (8
            // bytes long and last two bytes are padding), then we base64 decode.
            if (value.size() == 8 && value[6] == '=' && value[7] == '=') {
                if (Base64Decode(value, checksum_value)) {
                    info.Set(XrdClCurl::ChecksumType::kCRC32C, checksum_value);
                }
                continue;
            }
            std::size_t pos{0};
            unsigned long val;
            try {
                val = std::stoul(value.data(), &pos, 16);
            } catch (...) {
                continue;
            }
            if (pos == value.size()) {
                checksum_value[0] = (val >> 24) & 0xFF;
                checksum_value[1] = (val >> 16) & 0xFF;
                checksum_value[2] = (val >> 8) & 0xFF;
                checksum_value[3] = val & 0xFF;
                info.Set(XrdClCurl::ChecksumType::kCRC32C, checksum_value);
            }
        }
    }
}

// Convert the checksum type to a RFC 3230 digest name as recorded by IANA here:
// https://www.iana.org/assignments/http-dig-alg/http-dig-alg.xhtml
std::string HeaderParser::ChecksumTypeToDigestName(XrdClCurl::ChecksumType type) {
    switch (type) {
        case XrdClCurl::ChecksumType::kMD5:
            return "MD5";
        case XrdClCurl::ChecksumType::kCRC32C:
            return "CRC32c";
        case XrdClCurl::ChecksumType::kSHA1:
            return "SHA";
        case XrdClCurl::ChecksumType::kSHA256:
            return "SHA-256";
        default:
            return "";
    }
}

// This clever approach was inspired by golang's net/textproto
bool HeaderParser::validHeaderByte(unsigned char c)
{
    const static uint64_t mask_lower = 0 |
        uint64_t((1<<10)-1) << '0' |
        uint64_t(1) << '!' |
        uint64_t(1) << '#' |
        uint64_t(1) << '$' |
        uint64_t(1) << '%' |
        uint64_t(1) << '&' |
        uint64_t(1) << '\'' |
        uint64_t(1) << '*' |
        uint64_t(1) << '+' |
        uint64_t(1) << '-' |
        uint64_t(1) << '.';

    const static uint64_t mask_upper = 0 |
        uint64_t((1<<26)-1) << ('a'-64) |
        uint64_t((1<<26)-1) << ('A'-64) |
        uint64_t(1) << ('^'-64) |
        uint64_t(1) << ('_'-64) |
        uint64_t(1) << ('`'-64) |
        uint64_t(1) << ('|'-64) |
        uint64_t(1) << ('~'-64);

    if (c >= 128) return false;
    if (c >= 64) return (uint64_t(1)<<(c-64)) & mask_upper;
    return (uint64_t(1) << c) & mask_lower;
}

bool HeaderParser::Canonicalize(std::string &headerName)
{
    auto upper = true;
    const static int toLower = 'a' - 'A';
    for (size_t idx=0; idx<headerName.size(); idx++) {
        char c = headerName[idx];
        if (!validHeaderByte(c)) {
            return false;
        }
        if (upper && 'a' <= c && c <= 'z') {
            c -= toLower;
        } else if (!upper && 'A' <= c && c <= 'Z') {
            c += toLower;
        }
        headerName[idx] = c;
        upper = c == '-';
    }
    return true;
}

HandlerQueue::HandlerQueue(unsigned max_pending_ops) :
    m_max_pending_ops(max_pending_ops)
{
    int filedes[2];
    auto result = pipe(filedes);
    if (result == -1) {
        throw std::runtime_error(strerror(errno));
    }
    if (fcntl(filedes[0], F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1 || fcntl(filedes[1], F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1) {
        close(filedes[0]);
        close(filedes[1]);
        throw std::runtime_error(strerror(errno));
    }
    m_read_fd = filedes[0];
    m_write_fd = filedes[1];
};

namespace {

// Simple debug function for getting information from libcurl; to enable, you need to
// recompile with GetHandle(true);
int DumpHeader(CURL *handle, curl_infotype type, char *data, size_t size, void *clientp) {
    (void)handle;
    (void)clientp;

    switch (type) {
    case CURLINFO_HEADER_OUT:
        printf("Header > %s\n", std::string(data, size).c_str());
        break;
    default:
        printf("Info: %s", std::string(data, size).c_str());
        break;
    }
    return 0;
}

}

// Trim left and right side of a string_view for space characters
std::string_view XrdClCurl::trim_view(const std::string_view &input_view) {
    auto view = XrdClCurl::ltrim_view(input_view);
    for (size_t idx = 0; idx < input_view.size(); idx++) {
        if (!isspace(view[view.size() - 1 - idx])) {
            return view.substr(0, view.size() - idx);
        }
    }
    return "";
}

// Trim the left side of a string_view for space
std::string_view XrdClCurl::ltrim_view(const std::string_view &input_view) {
    for (size_t idx = 0; idx < input_view.size(); idx++) {
        if (!isspace(input_view[idx])) {
            return input_view.substr(idx);
        }
    }
    return "";
}

CURL *
XrdClCurl::GetHandle(bool verbose) {
    auto result = curl_easy_init();
    if (result == nullptr) {
        return result;
    }

    curl_easy_setopt(result, CURLOPT_USERAGENT, "xrdcl-curl/" XrdVERSION);
    curl_easy_setopt(result, CURLOPT_DEBUGFUNCTION, DumpHeader);
    if (verbose)
        curl_easy_setopt(result, CURLOPT_VERBOSE, 1L);

    auto env = XrdCl::DefaultEnv::GetEnv();
    std::string ca_file;
    if (!env->GetString("CurlCertFile", ca_file) || ca_file.empty()) {
        char *x509_ca_file = getenv("X509_CERT_FILE");
        if (x509_ca_file) {
            ca_file = std::string(x509_ca_file);
        }
    }
    if (!ca_file.empty()) {
        curl_easy_setopt(result, CURLOPT_CAINFO, ca_file.c_str());
    }
    std::string ca_dir;
    if (!env->GetString("CurlCertDir", ca_dir) || ca_dir.empty()) {
        char *x509_ca_dir = getenv("X509_CERT_DIR");
        if (x509_ca_dir) {
            ca_dir = std::string(x509_ca_dir);
        }
    }
    if (!ca_dir.empty()) {
        curl_easy_setopt(result, CURLOPT_CAPATH, ca_dir.c_str());
    }

    curl_easy_setopt(result, CURLOPT_BUFFERSIZE, 32*1024);

    return result;
}

CURL *
HandlerQueue::GetHandle() {
    if (m_handles.size()) {
        auto result = m_handles.back();
        m_handles.pop_back();
        return result;
    }

    return ::GetHandle(false);
}

void
HandlerQueue::RecycleHandle(CURL *curl) {
    m_handles.push_back(curl);
}

void
HandlerQueue::Expire()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    auto now = std::chrono::steady_clock::now();

    // Iterate through the paused transfers, checking if they are done.
    for (auto &op : m_ops) {
        if (!op->IsPaused()) continue;

        if (op->TransferStalled(0, now)) {
            op->ContinueHandle();
        }
    }

    std::vector<decltype(m_ops)::value_type> expired_ops;
    unsigned expired_count = 0;
    auto it = std::remove_if(m_ops.begin(), m_ops.end(),
        [&](const std::shared_ptr<CurlOperation> &handler) {
            auto expired = handler->GetOperationExpiry() < now;
            if (expired) {
                expired_ops.push_back(handler);
                expired_count++;
            }
            return expired;
        });
    m_ops.erase(it, m_ops.end());

    // The contents of our pipe and the in-memory queue are now off by expired_count.
    // Read exactly that many bytes from the pipe and throw them away.
    char throwaway[64];
    unsigned bytes_to_read = expired_count;
    while (bytes_to_read > 0) {
        size_t chunk = std::min<size_t>(sizeof(throwaway), bytes_to_read);
        ssize_t n = read(m_read_fd, throwaway, chunk);
        if (n > 0) {
            bytes_to_read -= n;
        } else if (n == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                // EWOULDBLOCK is a possibility if there's a synchronization error;
                // for now, just continue on as if we were successful in reading out
                // the missing bytes
                break;
            }
        } else {
            break;
        }
    }

    // Note: the failure handler may trigger new operations submitted to the queue
    // (which requires the lock to be held) such as a prefetch operation that gets split
    // into multiple sub-operations.
    //
    // Thus, we must unlock the mutex protecting the queue and avoid touching the shared state of
    // m_ops.
    lk.unlock();
    for (auto &handler : expired_ops) {
        if (handler) handler->Fail(XrdCl::errOperationExpired, 0, "Operation expired while in queue");
    }
}

void
HandlerQueue::Produce(std::shared_ptr<CurlOperation> handler)
{
    auto handler_expiry = handler->GetOperationExpiry();
    std::unique_lock<std::mutex> lk{m_mutex};
    m_producer_cv.wait_until(lk,
        handler_expiry,
        [&]{return m_ops.size() < m_max_pending_ops;}
    );
    if (std::chrono::steady_clock::now() > handler_expiry) {
        lk.unlock();
        handler->Fail(XrdCl::errOperationExpired, 0, "Operation expired while waiting for worker");
        m_ops_rejected.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    m_ops.push_back(handler);
    char ready[] = "1";
    while (true) {
        auto result = write(m_write_fd, ready, 1);
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This should never happen, but if it does, just continue
                // as if we successfully wrote the notification to the pipe.
                break;
            }
            throw std::runtime_error(strerror(errno));
        }
        break;
    }

    lk.unlock();
    m_consumer_cv.notify_one();
    m_ops_produced.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<CurlOperation>
HandlerQueue::Consume(std::chrono::steady_clock::duration dur)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_consumer_cv.wait_for(lk, dur, [&]{return m_ops.size() > 0 || m_shutdown;});
    if (m_shutdown || m_ops.empty()) {
        return {};
    }

    std::shared_ptr<CurlOperation> result = m_ops.front();
    m_ops.pop_front();

    char ready[1];
    while (true) {
        auto result = read(m_read_fd, ready, 1);
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This should never happen, but if it does, just continue
                // as if we successfully read the byte.
                break;
            }
            throw std::runtime_error(strerror(errno));
        }
        break;
    }

    lk.unlock();
    m_producer_cv.notify_one();
    m_ops_consumed.fetch_add(1, std::memory_order_relaxed);

    return result;
}

std::string
HandlerQueue::GetMonitoringJson()
{
    auto consumed = m_ops_consumed.load(std::memory_order_relaxed);
    auto produced = m_ops_produced.load(std::memory_order_relaxed);
    return "{"
            "\"produced\":" + std::to_string(produced) + ","
            "\"consumed\":" + std::to_string(consumed) + ","
            "\"pending\":" + std::to_string(produced - consumed) + ","
            "\"rejected\":" + std::to_string(m_ops_rejected.load(std::memory_order_relaxed)) +
        "}";
}

std::shared_ptr<CurlOperation>
HandlerQueue::TryConsume()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_ops.size() == 0) {
        std::shared_ptr<CurlOperation> result;
        return result;
    }

    std::shared_ptr<CurlOperation> result = m_ops.front();
    m_ops.pop_front();

    char ready[1];
    while (true) {
        auto result = read(m_read_fd, ready, 1); 
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This should never happen, but if it does, just continue
                // as if we successfully read the byte.
                break;
            }
            throw std::runtime_error(strerror(errno));
        }   
        break;
    }

    lk.unlock();
    m_producer_cv.notify_one();
    m_ops_consumed.fetch_add(1, std::memory_order_relaxed);

    return result;
}

void
HandlerQueue::Shutdown()
{
    std::unique_lock lock(m_mutex);
    m_shutdown = true;
    m_consumer_cv.notify_all();
}

void
HandlerQueue::ReleaseHandles()
{
    for (auto handle : m_handles) {
        curl_easy_cleanup(handle);
    }
    m_handles.clear();
}

CurlWorker::CurlWorker(std::shared_ptr<HandlerQueue> queue, VerbsCache &cache, XrdCl::Log* logger) :
    m_cache(cache),
    m_queue(queue),
    m_logger(logger)
{
    {
        std::unique_lock lk(m_worker_stats_mutex);
        m_stats_offset = m_workers_last_completed_cycle.size();
        m_workers_last_completed_cycle.push_back(&m_last_completed_cycle);
        m_workers_oldest_op.push_back(&m_oldest_op);
    }
    int pipeInfo[2];
    if ((pipe(pipeInfo) == -1) || (fcntl(pipeInfo[0], F_SETFD, FD_CLOEXEC)) || (fcntl(pipeInfo[1], F_SETFD, FD_CLOEXEC))) {
        throw std::runtime_error("Failed to create shutdown monitoring pipe for curl worker");
    }
    m_shutdown_pipe_r = pipeInfo[0];
    m_shutdown_pipe_w = pipeInfo[1];

    // Handle setup of the X509 authentication
    auto env = XrdCl::DefaultEnv::GetEnv();
    env->GetString("CurlClientCertFile", m_x509_client_cert_file);
    env->GetString("CurlClientKeyFile", m_x509_client_key_file);
}

std::tuple<std::string, std::string> CurlWorker::ClientX509CertKeyFile() const
{
    return std::make_tuple(m_x509_client_cert_file, m_x509_client_key_file);
}

std::string
CurlWorker::GetMonitoringJson()
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    auto oldest_op = now;
    auto oldest_cycle = now;
    {
        std::unique_lock lk(m_worker_stats_mutex);
        for (const auto &entry : m_workers_last_completed_cycle) {
            if (!entry) {continue;}
            auto cycle = entry->load(std::memory_order_relaxed);
            if (cycle < oldest_cycle) oldest_cycle = cycle;
        }
        for (const auto &entry : m_workers_oldest_op) {
            if (!entry) {continue;}
            auto op = entry->load(std::memory_order_relaxed);
            if (op < oldest_op) oldest_op = op;
        }
    }
    auto oldest_op_dbl = std::chrono::duration<double>(std::chrono::system_clock::time_point(std::chrono::system_clock::duration(oldest_op)).time_since_epoch()).count();
    auto oldest_cycle_dbl = std::chrono::duration<double>(std::chrono::system_clock::time_point(std::chrono::system_clock::duration(oldest_cycle)).time_since_epoch()).count();
    std::string retval = "{"
        "\"oldest_op\":" + std::to_string(oldest_op_dbl) + ","
        "\"oldest_cycle\":" + std::to_string(oldest_cycle_dbl) + ","
    ;

    for (size_t verb_idx = 0; verb_idx < static_cast<int>(XrdClCurl::CurlOperation::HttpVerb::Count); verb_idx++) {
        const auto &verb_str = XrdClCurl::CurlOperation::GetVerbString(static_cast<XrdClCurl::CurlOperation::HttpVerb>(verb_idx));
        for (size_t op_idx = 0; op_idx < 402; op_idx++) {
            if (op_idx == 401) continue;

            auto &op_stats = m_ops[verb_idx][op_idx];
            auto duration = op_stats.m_duration.load(std::memory_order_relaxed);
            if (duration == 0) continue;

            std::string prefix = "http_" + verb_str + "_" + ((op_idx == 402) ? "invalid" : std::to_string(200 + op_idx)) + "_";

            auto duration_dbl = std::chrono::duration<double>(std::chrono::steady_clock::duration(duration)).count();
            retval += "\"" + prefix + "duration\":" + std::to_string(duration_dbl) + ",";

            duration = op_stats.m_pause_duration.load(std::memory_order_relaxed);
            if (duration > 0) {
                duration_dbl = std::chrono::duration<double>(std::chrono::steady_clock::duration(duration)).count();
                retval += "\"" + prefix + "pause_duration\":" + std::to_string(duration_dbl) + ",";
            }

            auto count = op_stats.m_bytes.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "bytes\":" + std::to_string(count) + ",";
            count = op_stats.m_error.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "error\":" + std::to_string(count) + ",";
            count = op_stats.m_finished.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "finished\":" + std::to_string(count) + ",";
            count = op_stats.m_client_timeout.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "client_timeout\":" + std::to_string(count) + ",";
            count = op_stats.m_server_timeout.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "server_timeout\":" + std::to_string(count) + ",";
        }
        {
            auto &op_stats = m_ops[verb_idx][401];
            auto duration = op_stats.m_duration.load(std::memory_order_relaxed);
            if (duration == 0) continue;

            std::string prefix = "http_" + verb_str + "_";

            auto duration_dbl = std::chrono::duration<double>(std::chrono::steady_clock::duration(duration)).count();
            retval += "\"" + prefix + "preheader_duration\":" + std::to_string(duration_dbl) + ",";

            auto count = op_stats.m_started.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "started\":" + std::to_string(count) + ",";
            count = op_stats.m_error.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "preheader_error\":" + std::to_string(count) + ",";
            count = op_stats.m_finished.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "preheader_finished\":" + std::to_string(count) + ",";
            count = op_stats.m_server_timeout.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "preheader_timeout\":" + std::to_string(count) + ",";
            count = op_stats.m_conncall_timeout.load(std::memory_order_relaxed);
            if (count) retval += "\"" + prefix + "conncall_timeout\":" + std::to_string(count) + ",";
        }
    }

    retval +=
        "\"conncall_error\":" + std::to_string(m_conncall_errors.load(std::memory_order_relaxed)) + ","
        "\"conncall_started\":" + std::to_string(m_conncall_req.load(std::memory_order_relaxed)) + ","
        "\"conncall_success\":" + std::to_string(m_conncall_success.load(std::memory_order_relaxed)) + ","
        "\"conncall_timeout\":" + std::to_string(m_conncall_timeout.load(std::memory_order_relaxed)) +
        "}";

    return retval;
}

void
CurlWorker::OpRecord(XrdClCurl::CurlOperation &op, OpKind kind)
{
    int sc = op.GetStatusCode();
    // - We encode everything pre-header as integer "401".  We include a 100-continue request as "pre-header".
    // - Status codes out of the acceptable range are labeled "402"
    // - Otherwise, we store it in the array shifted by 200 (to avoid more sparsity)
    if (sc < 0 || kind == OpKind::Start || sc == 100) {
        sc = 401;
    } else if (sc < 200 || sc >= 600) {
        sc = 402;
    } else {
        sc -= 200;
    }
    auto [bytes, pre_headers, post_headers, pause_duration] = op.StatisticsReset();
    auto &op_stats = m_ops[static_cast<int>(op.GetVerb())][sc];
    op_stats.m_bytes.fetch_add(bytes, std::memory_order_relaxed);
    op_stats.m_duration.fetch_add((sc == 401) ? pre_headers.count() : post_headers.count(), std::memory_order_relaxed);
    op_stats.m_pause_duration.fetch_add(pause_duration.count(), std::memory_order_relaxed);
    if (pre_headers != std::chrono::steady_clock::duration::zero() && sc != 401) {
        auto &old_stats = m_ops[static_cast<int>(op.GetVerb())][401];
        old_stats.m_duration.fetch_add(pre_headers.count(), std::memory_order_relaxed);
    }
    switch (kind) {
    case OpKind::ConncallTimeout:
        op_stats.m_conncall_timeout.fetch_add(1, std::memory_order_relaxed);
    case OpKind::ClientTimeout:
        op_stats.m_client_timeout.fetch_add(1, std::memory_order_relaxed);
        break;
    case OpKind::Error:
        op_stats.m_error.fetch_add(1, std::memory_order_relaxed);
        break;
    case OpKind::Finish:
        op_stats.m_finished.fetch_add(1, std::memory_order_relaxed);
        break;
    case OpKind::Start:
        op_stats.m_started.fetch_add(1, std::memory_order_relaxed);
        break;
    case OpKind::ServerTimeout:
        op_stats.m_server_timeout.fetch_add(1, std::memory_order_relaxed);
        break;
    case OpKind::Update:
        break;
    }
}

void
CurlWorker::RunStatic(CurlWorker *myself)
{
    {
        std::unique_lock lock(m_workers_mutex);
        m_workers.push_back(myself);
        myself->m_shutdown_complete = false;
    }
    try {
        myself->Run();
    } catch (...) {
        myself->m_logger->Warning(kLogXrdClCurl, "Curl worker got an exception");
        {
            std::unique_lock lock(m_workers_mutex);
            auto iter = std::remove_if(m_workers.begin(), m_workers.end(), [&](CurlWorker *worker){return worker == myself;});
            m_workers.erase(iter);
        }
    }
}

void
CurlWorker::Run() {
    // Create a copy of the shared_ptr here.  Otherwise, when the main thread's destructors
    // run, there won't be any other live references to the shared_ptr, triggering cleanup
    // of the condition variable.  Because we purposely don't shutdown the worker threads,
    // those threads may be waiting on the condition variable; destroying a condition variable
    // while a thread is waiting on it is undefined behavior.
    auto queue_ref = m_queue;
    int max_pending = 50;
    XrdCl::DefaultEnv::GetEnv()->GetInt("CurlMaxPendingOps", max_pending);
    m_continue_queue.reset(new HandlerQueue(max_pending));
    auto &queue = *queue_ref.get();
    m_logger->Debug(kLogXrdClCurl, "Started a curl worker");

    CURLM *multi_handle = curl_multi_init();
    if (multi_handle == nullptr) {
        throw std::runtime_error("Failed to create curl multi-handle");
    }

    int running_handles = 0;
    time_t last_maintenance = time(NULL);
    CURLMcode mres = CURLM_OK;

    // Map from a file descriptor that has an outstanding broker request
    // to the corresponding CURL handle.
    std::unordered_map<int, WaitingForBroker> broker_reqs;
    std::vector<struct curl_waitfd> waitfds;

    bool want_shutdown = false;
    while (!want_shutdown) {
        m_last_completed_cycle.store(std::chrono::system_clock::now().time_since_epoch().count());
        auto oldest_op = std::chrono::system_clock::now();
        for (const auto &entry : m_op_map) {
            OpRecord(*entry.second.first, OpKind::Update);
            if (entry.second.second < oldest_op) {
                oldest_op = entry.second.second;
            }
        }
        m_oldest_op.store(oldest_op.time_since_epoch().count());

        // Try continuing any available handles that have more data
        while (true) {
            auto op = m_continue_queue->TryConsume();
            if (!op) {
                break;
            }
            // Avoid race condition where external thread added a continue operation to queue
            // while the curl worker thread failed the transfer.
            if (op->IsDone()) {
                m_logger->Debug(kLogXrdClCurl, "Ignoring continuation of operation that has already completed");
                op->Fail(XrdCl::errInternal, 0, "Operation previously failed; cannot continue");
                continue;
            }
            m_logger->Debug(kLogXrdClCurl, "Continuing the curl handle from op %p on thread %d", op.get(), getthreadid());
            auto curl = op->GetCurlHandle();
            if (!op->ContinueHandle()) {
                op->Fail(XrdCl::errInternal, 0, "Failed to continue the curl handle for the operation");
                OpRecord(*op, OpKind::Error);
                op->ReleaseHandle();
                if (curl) {
                    curl_multi_remove_handle(multi_handle, curl);
                    curl_easy_cleanup(curl);
                    m_op_map.erase(curl);
                }
                running_handles -= 1;
                continue;
            } else {
                auto iter = m_op_map.find(curl);
                if (iter != m_op_map.end()) iter->second.second = std::chrono::system_clock::now();
            }
		}
        // Consume from the shared new operation queue
        while (running_handles < static_cast<int>(m_max_ops)) {
            auto op = running_handles == 0 ? queue.Consume(std::chrono::seconds(1)) : queue.TryConsume();
            if (!op) {
                break;
            }
            auto curl = queue.GetHandle();
            if (curl == nullptr) {
                m_logger->Debug(kLogXrdClCurl, "Unable to allocate a curl handle");
                op->Fail(XrdCl::errInternal, ENOMEM, "Unable to get allocate a curl handle");
                continue;
            }
            try {
                auto rv = op->Setup(curl, *this);
                if (!rv) {
                    m_logger->Debug(kLogXrdClCurl, "Failed to setup the curl handle");
                    op->Fail(XrdCl::errInternal, ENOMEM, "Failed to setup the curl handle for the operation");
                    continue;
                }
                if (!op->FinishSetup(curl)) {
                    m_logger->Debug(kLogXrdClCurl, "Failed to finish setup of the curl handle");
                    op->Fail(XrdCl::errInternal, ENOMEM, "Failed to finish setup of the curl handle for the operation");
                    continue;
                }
            } catch (...) {
                m_logger->Debug(kLogXrdClCurl, "Unable to setup the curl handle");
                op->Fail(XrdCl::errInternal, ENOMEM, "Failed to setup the curl handle for the operation");
                continue;
            }
            op->SetContinueQueue(m_continue_queue);

            if (op->IsDone()) {
                continue;
            }
            m_op_map[curl] = {op, std::chrono::system_clock::now()};

            // If the operation requires the result of the OPTIONS verb to function, then
            // we add that to the multi-handle instead, chaining the two calls together.
            if (op->RequiresOptions()) {
                std::string modified_url;
                std::shared_ptr<CurlOptionsOp> options_op(
                    new CurlOptionsOp(
                        curl, op,
                        std::string(
                            VerbsCache::GetUrlKey(op->GetUrl(), modified_url)
                        ),
                        m_logger, op->GetConnCalloutFunc()
                    )
                );
                // Note this `curl` variable is not local to the conditional; it is the curl handle of the
                // CurlOptionsOp and will be added below to the multi-handle, causing it - not the parent's
                // curl handle - to be executed.
                curl = queue.GetHandle();
                if (curl == nullptr) {
                    m_logger->Debug(kLogXrdClCurl, "Unable to allocate a curl handle");
                    op->Fail(XrdCl::errInternal, ENOMEM, "Unable to get allocate a curl handle");
                    OpRecord(*op, OpKind::Error);
                    continue;
                }
                auto rv = options_op->Setup(curl, *this);
                if (!rv) {
                    m_logger->Debug(kLogXrdClCurl, "Failed to allocate a curl handle for OPTIONS");
                    continue;
                }
                m_op_map[curl] = {options_op, std::chrono::system_clock::now()};
                OpRecord(*options_op, OpKind::Start);
                running_handles += 1;
            } else {
                OpRecord(*op, OpKind::Start);
            }

            auto mres = curl_multi_add_handle(multi_handle, curl);
            if (mres != CURLM_OK) {
                m_logger->Debug(kLogXrdClCurl, "Unable to add operation to the curl multi-handle");
                op->Fail(XrdCl::errInternal, mres, "Unable to add operation to the curl multi-handle");
                OpRecord(*op, OpKind::Error);
                continue;
            }
            m_logger->Debug(kLogXrdClCurl, "Added request for URL %s to worker thread for processing", op->GetUrl().c_str());
            running_handles += 1;
        }

        // Maintain the periodic reporting of thread activity and fail any operations
        // that have expired / timed out.
        time_t now = time(NULL);
        time_t next_maintenance = last_maintenance + m_maintenance_period.load(std::memory_order_relaxed);
        if (now >= next_maintenance) {
            m_queue->Expire();
            m_continue_queue->Expire();
            m_logger->Debug(kLogXrdClCurl, "Curl worker thread %d is running %d operations",
                getthreadid(), running_handles);
            last_maintenance = now;

            // Timeout all the pending broker requests.
            std::vector<std::pair<int, CURL *>> expired_ops;
            for (const auto &entry : broker_reqs) {
                if (entry.second.expiry < now) {
                    expired_ops.emplace_back(entry.first, entry.second.curl);
                }
            }
            for (const auto &entry : expired_ops) {
                auto iter = m_op_map.find(entry.second);
                if (iter == m_op_map.end()) {
                    m_logger->Warning(kLogXrdClCurl, "Found an expired curl handle with no corresponding operation!");
                } else {

                    CurlOptionsOp *options_op = nullptr;
                    if ((options_op = dynamic_cast<CurlOptionsOp*>(iter->second.first.get())) != nullptr) {
                        auto parent_op = options_op->GetOperation();
                        bool parent_op_failed = false;
                        if (parent_op->IsRedirect()) {
                            std::string target;
                            if (parent_op->Redirect(target) == CurlOperation::RedirectAction::Fail) {
                                auto iter = m_op_map.find(options_op->GetParentCurlHandle());
                                if (iter != m_op_map.end()) {
                                    OpRecord(*iter->second.first, OpKind::Error);
                                    iter->second.first->Fail(XrdCl::errErrorResponse, 0, "Failed to send OPTIONS to redirect target");
                                    m_op_map.erase(iter);
                                    running_handles -= 1;
                                }
                                parent_op_failed = true;
                            } else {
                                OpRecord(*parent_op, OpKind::Start);
                            }
                        } else {
                            OpRecord(*parent_op, OpKind::Start);
                        }
                        if (!parent_op_failed){
                            curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                        }
                    }

                    iter->second.first->Fail(XrdCl::errConnectionError, 1, "Timeout: connection never provided for request");
                    iter->second.first->ReleaseHandle();
                    OpRecord(*(iter->second.first), OpKind::ConncallTimeout);
                    m_op_map.erase(entry.second);
                    curl_easy_cleanup(entry.second);
                    running_handles -= 1;
                }
                broker_reqs.erase(entry.first);
                m_conncall_timeout.fetch_add(1, std::memory_order_relaxed);
            }

            // Cleanup the fake connection cache entries.
            XrdClCurl::CurlOperation::CleanupDnsCache();
        }

        waitfds.clear();
        waitfds.resize(3 + broker_reqs.size());

        waitfds[0].fd = queue.PollFD();
        waitfds[0].events = CURL_WAIT_POLLIN;
        waitfds[0].revents = 0;
        waitfds[1].fd = m_continue_queue->PollFD();
        waitfds[1].events = CURL_WAIT_POLLIN;
        waitfds[1].revents = 0;
        waitfds[2].fd = m_shutdown_pipe_r;
        waitfds[2].revents = 0;
        waitfds[2].events = CURL_WAIT_POLLIN | CURL_WAIT_POLLPRI;

        int idx = 3;
        for (const auto &entry : broker_reqs) {
            waitfds[idx].fd = entry.first;
            waitfds[idx].events = CURL_WAIT_POLLIN|CURL_WAIT_POLLPRI;
            waitfds[idx].revents = 0;
            idx += 1;
        }

        long timeo;
        curl_multi_timeout(multi_handle, &timeo);
        // These commented-out lines are purposely left; will need to revisit after the 0.9.1 release;
        // for now, they are too verbose on RHEL7.
        //m_logger->Debug(kLogXrdClCurl, "Curl advises a timeout of %ld ms", timeo);
        if (running_handles && timeo == -1) {
            // Bug workaround: we've seen RHEL7 libcurl have a race condition where it'll not
            // set a timeout while doing the DNS lookup; assume that if there are running handles
            // but no timeout, we've hit this bug.
            //m_logger->Debug(kLogXrdClCurl, "Will sleep for up to 50ms");
            mres = curl_multi_wait(multi_handle, &waitfds[0], waitfds.size(), 50, nullptr);
        } else {
            //m_logger->Debug(kLogXrdClCurl, "Will sleep for up to %d seconds", max_sleep_time);
            //mres = curl_multi_wait(multi_handle, &waitfds[0], waitfds.size(), max_sleep_time*1000, nullptr);
            // Temporary test: we've been seeing DNS lookups timeout on additional platforms.  Switch to always
            // poll as curl_multi_wait doesn't seem to get notified when DNS lookups are done.
            mres = curl_multi_wait(multi_handle, &waitfds[0], waitfds.size(), 50, nullptr);
        }
        if (mres != CURLM_OK) {
            m_logger->Warning(kLogXrdClCurl, "Failed to wait on multi-handle: %d", mres);
        }

        // Iterate through the waiting broker callbacks.
        for (const auto &entry : waitfds) {
            // Ignore the queue's poll fd.
            if (waitfds[0].fd == entry.fd || waitfds[1].fd == entry.fd) {
                continue;
            }
            // Handle shutdown requests
            if ((waitfds[2].fd == entry.fd) && entry.revents) {
                want_shutdown = true;
                break;
            }
            if ((entry.revents & CURL_WAIT_POLLIN) != CURL_WAIT_POLLIN) {
                continue;
            }
            auto handle = broker_reqs[entry.fd].curl;
            auto iter = m_op_map.find(handle);
            if (iter == m_op_map.end()) {
                m_logger->Warning(kLogXrdClCurl, "Internal error: broker responded on FD %d but no corresponding curl operation", entry.fd);
                broker_reqs.erase(entry.fd);
                m_conncall_errors.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            std::string err;
            auto result = iter->second.first->WaitSocketCallback(err);
            if (result == -1) {
                m_logger->Warning(kLogXrdClCurl, "Error when invoking the broker callback: %s", err.c_str());

                CurlOptionsOp *options_op = nullptr;
                if ((options_op = dynamic_cast<CurlOptionsOp*>(iter->second.first.get())) != nullptr) {
                    auto parent_op = options_op->GetOperation();
                    bool parent_op_failed = false;
                    if (parent_op->IsRedirect()) {
                        std::string target;
                        if (parent_op->Redirect(target) == CurlOperation::RedirectAction::Fail) {
                            auto iter = m_op_map.find(options_op->GetParentCurlHandle());
                            if (iter != m_op_map.end()) {
                                OpRecord(*iter->second.first, OpKind::Error);
                                iter->second.first->Fail(XrdCl::errErrorResponse, 0, "Failed to send OPTIONS to redirect target");
                                m_op_map.erase(iter);
                                running_handles -= 1;
                            }
                            parent_op_failed = true;
                        } else {
                            OpRecord(*parent_op, OpKind::Start);
                        }
                    } else {
                        OpRecord(*parent_op, OpKind::Start);
                    }
                    if (!parent_op_failed){
                        curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                    }
                }

                iter->second.first->Fail(XrdCl::errErrorResponse, 1, err);
                OpRecord(*iter->second.first, OpKind::Error);
                m_op_map.erase(handle);
                broker_reqs.erase(entry.fd);
                m_conncall_errors.fetch_add(1, std::memory_order_relaxed);
                running_handles -= 1;
            } else {
                broker_reqs.erase(entry.fd);
                curl_multi_add_handle(multi_handle, handle);
                m_conncall_success.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Do maintenance on the multi-handle
        int still_running;
        auto mres = curl_multi_perform(multi_handle, &still_running);
        if (mres == CURLM_CALL_MULTI_PERFORM) {
            continue;
        } else if (mres != CURLM_OK) {
            m_logger->Warning(kLogXrdClCurl, "Failed to perform multi-handle operation: %d", mres);
            break;
        }

        CURLMsg *msg;
        do {
            int msgq = 0;
            msg = curl_multi_info_read(multi_handle, &msgq);
            if (msg && (msg->msg == CURLMSG_DONE)) {
                if (!msg->easy_handle) {
                    m_logger->Warning(kLogXrdClCurl, "Logic error: got a callback for a null handle");
                    mres = CURLM_BAD_EASY_HANDLE;
                    break;
                }
                auto iter = m_op_map.find(msg->easy_handle);
                if (iter == m_op_map.end()) {
                    m_logger->Error(kLogXrdClCurl, "Logic error: got a callback for an entry that doesn't exist");
                    mres = CURLM_BAD_EASY_HANDLE;
                    break;
                }
                auto op = iter->second.first;
                auto res = msg->data.result;
                bool keep_handle = false;
                bool waiting_on_callout = false;
                if (res == CURLE_OK) {
                    auto sc = op->GetStatusCode();
                    OpRecord(*op, OpKind::Finish);
                    if (HTTPStatusIsError(sc)) {
                        auto httpErr = HTTPStatusConvert(sc);
                        op->Fail(httpErr.first, httpErr.second, op->GetStatusMessage());
                        op->ReleaseHandle();
                        // If this was a failed CurlOptionsOp, then we re-activate the parent handle.
                        // If the parent handle was stopped at a redirect that now returns failure, then
                        // we'll clean it up.
                        CurlOptionsOp *options_op = nullptr;
                        if ((options_op = dynamic_cast<CurlOptionsOp*>(op.get())) != nullptr) {
                            auto parent_op = options_op->GetOperation();
                            bool parent_op_failed = false;
                            if (parent_op->IsRedirect()) {
                                std::string target;
                                if (parent_op->Redirect(target) == CurlOperation::RedirectAction::Fail) {
                                    OpRecord(*parent_op, OpKind::Error);
                                    m_op_map.erase(options_op->GetParentCurlHandle());
                                    running_handles -= 1;
                                    parent_op_failed = true;
                                } else {
                                    OpRecord(*parent_op, OpKind::Start);
                                }
                            } else {
                                OpRecord(*parent_op, OpKind::Start);
                            }
                            // Have curl execute the parent operation
                            if (!parent_op_failed) {
                                curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                            }
                        }
                        // The curl operation was successful, it's just the HTTP request failed; recycle the handle.
                        queue.RecycleHandle(iter->first);
                    } else {
                        CurlOptionsOp *options_op = nullptr;
                        // If this was a successful OPTIONS op, invoke the parent operation.
                        if ((options_op = dynamic_cast<CurlOptionsOp*>(op.get()))) {
                            options_op->Success();
                            options_op->ReleaseHandle();
                            // Note: op is scoped external to the conditional block
                            op = options_op->GetOperation();
                            op->OptionsDone();
                            OpRecord(*op, OpKind::Start);
                            curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                            curl_multi_remove_handle(multi_handle, iter->first);
                            queue.RecycleHandle(iter->first);
                        }
                        // Check to see if the operation ended in a redirect (note: this might)
                        // be invoked a second time if this was the parent operation of an OPTIONS
                        // op.
                        if (op->IsRedirect()) {
                            std::string target;
                            switch (op->Redirect(target)) {
                                case CurlOperation::RedirectAction::Fail:
                                    if (options_op) {
                                        // In this case, we failed immediately after an OPTIONS finished.
                                        // Since there's a Start recorded after the OPTIONS processing, we
                                        // must record an error.
                                        // In the non-OPTIONS case, we never recorded a second start and
                                        // don't need a matching failure.
                                        OpRecord(*op, OpKind::Error);
                                    }
                                    keep_handle = false;
                                    break;
                                case CurlOperation::RedirectAction::Reinvoke:
                                    if (!options_op) {
                                        // In this case, the redirect occurred without any prior
                                        // OPTIONS call.  This implies that `op` is the original call
                                        // and we need to restart it later and record another op start.
                                        keep_handle = true;
                                        OpRecord(*op, OpKind::Start);
                                    }
                                    break;
                                case CurlOperation::RedirectAction::ReinvokeAfterAllow:
                                {
                                    // The redirect resulted in a new endpoint where the cache lookup failed;
                                    // we need to know what HTTP verbs are in the server's Allow list before this
                                    // operation can continue.  Inject a new CurlOptionsOp and chain it to the one
                                    // being processed.  Once the OPTIONS request is done, then we'll restart this
                                    // operation.
                                    std::string modified_url;
                                    target = VerbsCache::GetUrlKey(target, modified_url);
                                    options_op = new CurlOptionsOp(iter->first, op, target, m_logger, op->GetConnCalloutFunc());
                                    std::shared_ptr<CurlOperation> new_op(options_op);
                                    auto curl = queue.GetHandle();
                                    if (curl == nullptr) {
                                        m_logger->Debug(kLogXrdClCurl, "Unable to allocate a curl handle");
                                        op->Fail(XrdCl::errInternal, ENOMEM, "Unable to get allocate a curl handle");
                                        keep_handle = false;
                                        options_op = nullptr;
                                        break;
                                    }
                                    OpRecord(*new_op, OpKind::Start);
                                    try {
                                        auto rv = new_op->Setup(curl, *this);
                                        if (!rv) {
                                            m_logger->Debug(kLogXrdClCurl,  "Unable to configure a curl handle for OPTIONS");
                                            keep_handle = false;
                                            options_op = nullptr;
                                            break;
                                        }
                                    } catch (...) {
                                        m_logger->Debug(kLogXrdClCurl, "Unable to setup the curl handle for the OPTIONS operation");
                                        new_op->Fail(XrdCl::errInternal, ENOMEM, "Failed to setup the curl handle for the OPTIONS operation");
                                        OpRecord(*new_op, OpKind::Error);
                                        keep_handle = false;
                                        break;
                                    }
                                    new_op->SetContinueQueue(m_continue_queue);
                                    m_op_map[curl] = {new_op, std::chrono::system_clock::now()};
                                    auto mres = curl_multi_add_handle(multi_handle, curl);
                                    if (mres != CURLM_OK) {
                                        m_logger->Debug(kLogXrdClCurl, "Unable to add OPTIONS operation to the curl multi-handle: %s", curl_multi_strerror(mres));
                                        op->Fail(XrdCl::errInternal, mres, "Unable to add OPTIONS operation to the curl multi-handle");
                                        OpRecord(*new_op, OpKind::Error);
                                        break;
                                    }
                                    running_handles += 1;
                                    m_logger->Debug(kLogXrdClCurl, "Invoking the OPTIONS operation before redirect to %s", target.c_str());
                                    // The original curl operation needs to be kept around.  Note that because options_op
                                    // is non-nil, we won't re-add the handle to the multi-handle.
                                    keep_handle = true;
                                }
                            }
                            int callout_socket = op->WaitSocket();
                            if ((waiting_on_callout = callout_socket >= 0)) {
                                auto expiry = time(nullptr) + 20;
                                m_logger->Debug(kLogXrdClCurl, "Creating a callout wait request on socket %d", callout_socket);
                                broker_reqs[callout_socket] = {iter->first, expiry};
                                m_conncall_req.fetch_add(1, std::memory_order_relaxed);
                            }
                        } else if (options_op) {
                            // In this case, the OPTIONS call happened before the parent operation was started.
                            curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                        }
                        if (keep_handle) {
                            curl_multi_remove_handle(multi_handle, iter->first);
                            if (!waiting_on_callout && !options_op) {
                                curl_multi_add_handle(multi_handle, iter->first);
                            }
                        } else if (!options_op) {
                            op->Success();
                            op->ReleaseHandle();
                            // If the handle was successful, then we can recycle it.
                            queue.RecycleHandle(iter->first);
                        }
                    }
                } else if (res == CURLE_COULDNT_CONNECT && op->UseConnectionCallout() && !op->GetTriedBoker()) {
                    // In this case, we need to use the broker and the curl handle couldn't reuse
                    // an existing socket.
                    keep_handle = true;
                    op->SetTriedBoker(); // Flag to ensure we try a connection only once per operation.
                    std::string err;
                    int wait_socket = -1;
                    if (!op->StartConnectionCallout(err) || (wait_socket=op->WaitSocket()) == -1) {
                        m_logger->Error(kLogXrdClCurl, "Failed to start broker-based connection: %s", err.c_str());
                        op->ReleaseHandle();
                        keep_handle = false;
                    } else {
                        curl_multi_remove_handle(multi_handle, iter->first);
                        auto expiry = time(nullptr) + 20;
                        m_logger->Debug(kLogXrdClCurl, "Curl operation requires a new TCP socket; waiting for callout to respond on socket %d", wait_socket);
                        broker_reqs[wait_socket] = {iter->first, expiry};
                        m_conncall_req.fetch_add(1, std::memory_order_relaxed);
                    }
                } else {
                    if (res == CURLE_ABORTED_BY_CALLBACK || res == CURLE_WRITE_ERROR) {
                        // We cannot invoke the failure from within a callback as the curl thread and
                        // original thread of execution may fight over the ownership of the handle memory.
                        switch (op->GetError()) {
                        case CurlOperation::OpError::ErrHeaderTimeout:
#ifdef HAVE_XPROTOCOL_TIMEREXPIRED
                            op->Fail(XrdCl::errOperationExpired, 0, "Origin did not respond with headers within timeout");
#else
                            op->Fail(XrdCl::errOperationExpired, 0, "Origin did not respond within timeout");
#endif
                            OpRecord(*op, OpKind::Error);
                            break;
                        case CurlOperation::OpError::ErrCallback: {
                            auto [ecode, emsg] = op->GetCallbackError();
                            op->Fail(XrdCl::errErrorResponse, ecode, emsg);
                            OpRecord(*op, OpKind::Error);
                            break;
                        }
                        case CurlOperation::OpError::ErrOperationTimeout:
                            op->Fail(XrdCl::errOperationExpired, 0, "Operation timed out");
                            OpRecord(*op, op->IsPaused() ? OpKind::ClientTimeout : OpKind::ServerTimeout);
                            break;
                        case CurlOperation::OpError::ErrTransferSlow:
                            op->Fail(XrdCl::errOperationExpired, 0, "Transfer speed below minimum threshold");
                            OpRecord(*op, OpKind::ServerTimeout);
                            break;
                        case CurlOperation::OpError::ErrTransferClientStall:
                            op->Fail(XrdCl::errOperationExpired, 0, "Transfer stalled for too long");
                            OpRecord(*op, OpKind::ClientTimeout);
                            break;
                        case CurlOperation::OpError::ErrTransferStall:
                            op->Fail(XrdCl::errOperationExpired, 0, "Transfer stalled for too long");
                            OpRecord(*op, OpKind::ServerTimeout);
                            break;
                        case CurlOperation::OpError::ErrNone:
                            op->Fail(XrdCl::errInternal, 0, "Operation was aborted without recording an abort reason");
                            OpRecord(*op, OpKind::Error);
                            break;
                        };
                        CurlOptionsOp *options_op = nullptr;
                        if ((options_op = dynamic_cast<CurlOptionsOp*>(op.get())) != nullptr) {
                            auto parent_op = options_op->GetOperation();
                            bool parent_op_failed = false;
                            if (parent_op->IsRedirect()) {
                                std::string target;
                                if (parent_op->Redirect(target) == CurlOperation::RedirectAction::Fail) {
                                    auto iter = m_op_map.find(options_op->GetParentCurlHandle());
                                    if (iter != m_op_map.end()) {
                                        OpRecord(*iter->second.first, OpKind::Error);
                                        iter->second.first->Fail(XrdCl::errErrorResponse, 0, "Failed to send OPTIONS to redirect target");
                                        m_op_map.erase(iter);
                                        running_handles -= 1;
                                    }
                                    parent_op_failed = true;
                                } else {
                                    OpRecord(*parent_op, OpKind::Start);
                                }
                            } else {
                                OpRecord(*parent_op, OpKind::Start);
                            }
                            if (!parent_op_failed){
                                curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                            }
                        }
                    } else {
                        auto xrdCode = CurlCodeConvert(res);
                        m_logger->Debug(kLogXrdClCurl, "Curl generated an error: %s (%d)", curl_easy_strerror(res), res);
                        op->Fail(xrdCode.first, xrdCode.second, curl_easy_strerror(res));
                        OpRecord(*op, OpKind::Error);
                        CurlOptionsOp *options_op = nullptr;
                        if ((options_op = dynamic_cast<CurlOptionsOp*>(op.get())) != nullptr) {
                            auto parent_op = options_op->GetOperation();
                            bool parent_op_failed = false;
                            if (parent_op->IsRedirect()) {
                                std::string target;
                                if (parent_op->Redirect(target) == CurlOperation::RedirectAction::Fail) {
                                    auto iter = m_op_map.find(options_op->GetParentCurlHandle());
                                    if (iter != m_op_map.end()) {
                                        OpRecord(*iter->second.first, OpKind::Error);
                                        iter->second.first->Fail(XrdCl::errErrorResponse, 0, "Failed to send OPTIONS to redirect target");
                                        m_op_map.erase(iter);
                                        running_handles -= 1;
                                    }
                                    parent_op_failed = true;
                                }
                            }
                            if (!parent_op_failed){
                                curl_multi_add_handle(multi_handle, options_op->GetParentCurlHandle());
                            }
                        }
                    }
                    op->ReleaseHandle();
                }
                if (!keep_handle) {
                    curl_multi_remove_handle(multi_handle, iter->first);
                    if (res != CURLE_OK) {
                        curl_easy_cleanup(iter->first);
                    }
                    for (auto &req : broker_reqs) {
                        if (req.second.curl == iter->first) {
                            m_logger->Warning(kLogXrdClCurl, "Curl handle finished while a broker operation was outstanding");
                            m_conncall_errors.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    m_op_map.erase(iter);
                    running_handles -= 1;
                }
            }
        } while (msg);
    }

    for (auto map_entry : m_op_map) {
        if (mres) {
            map_entry.second.first->Fail(XrdCl::errInternal, mres, curl_multi_strerror(mres));
            OpRecord(*map_entry.second.first, OpKind::Error);
        }
        if (multi_handle && map_entry.first) curl_multi_remove_handle(multi_handle, map_entry.first);
    }

    m_queue->ReleaseHandles();
    curl_multi_cleanup(multi_handle);
    {
        std::unique_lock lock(m_shutdown_lock);
        m_shutdown_complete = true;
        m_shutdown_complete_cv.notify_all();
    }
}

void
CurlWorker::Shutdown()
{
    m_queue->Shutdown();
    if (m_shutdown_pipe_w == -1) {
        m_logger->Debug(kLogXrdClCurl, "Curl worker shutdown prior to launch of thread");
        return;
    }
    close(m_shutdown_pipe_w);
    m_shutdown_pipe_w = -1;

    std::unique_lock lock(m_shutdown_lock);
    m_shutdown_complete_cv.wait(lock, [&]{return m_shutdown_complete;});

    {
        std::unique_lock lk(m_worker_stats_mutex);
        m_workers_last_completed_cycle[m_stats_offset] = nullptr;
        m_workers_oldest_op[m_stats_offset] = nullptr;
    }
    m_logger->Debug(kLogXrdClCurl, "Curl worker thread shutdown has completed.");
}

void
CurlWorker::ShutdownAll()
{
    std::unique_lock lock(m_workers_mutex);
    for (auto worker : m_workers) {
        worker->Shutdown();
    }
}
