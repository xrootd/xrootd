/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
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

#include "XrdClHttpOps.hh"
#include "XrdClHttpResponses.hh"
#include "XrdClHttpUtil.hh"
#include "XrdClHttpWorker.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <sys/random.h>
#endif
#include <utility>

using namespace XrdClHttp;

std::chrono::steady_clock::duration CurlOperation::m_stall_interval{CurlOperation::m_default_stall_interval};
int CurlOperation::m_minimum_transfer_rate{CurlOperation::m_default_minimum_rate};

namespace {

// For connection callbacks, we don't want to require a real DNS lookup; instead, we
// will generate a fake address in the 169.254.x.y range and use that for the connection.
// This will be fed to libcurl via the CURLOPT_RESOLVE option, which will bypass DNS lookups.

// A randomized counter for generating fake addresses in the 169.254.x.y range
thread_local int64_t fake_dns_counter = -1;

// Map from hostname:port to fake address (e.g., 169.254.x.y:port) and
// std::string pointer for the fake address.
// We must track the std::string pointer so we can pass it to libcurl
// as the CURLOPT_CLOSESOCKETDATA, which is passed to the close socket callback; the
// lifetime of the pointer must be at least as long as the lifetime of the socket
// (we maintain a reference count manually below).
thread_local std::unordered_map<std::string, std::pair<std::string, std::string*>> fake_dns_map;

// Reverse map from fake address (e.g., 169.254.x.y:port) to hostname:port and reference pointer
thread_local std::unordered_map<std::string, std::pair<std::string, std::string*>> reverse_fake_dns_map;

// References to fake addresses in use.  The value is a reference count of sockets using
// this address; when the count goes to zero, we can remove the entry from the above maps.
// The second member is a unique_ptr to the std::string for the fake address, which will be
// cleaned up when the refcount goes to zero.
struct refcount_entry {
    int count;
    std::unique_ptr<std::string> addr;
    std::chrono::steady_clock::time_point last_used;

    bool IsExpired(std::chrono::steady_clock::time_point now) const {
        return (now - last_used) > std::chrono::minutes(1);
    }
};

thread_local std::unordered_map<std::string *, std::unique_ptr<refcount_entry>> fake_dns_refcount;

std::string GenerateFakeEndpoint() {
    if (fake_dns_counter == -1) {
#ifdef __APPLE__
        fake_dns_counter = arc4random();
#else
        errno = 0;
        while (fake_dns_counter < 0 || errno == EINTR) {
            if (getrandom((void*)&fake_dns_counter, sizeof(fake_dns_counter), 0) == sizeof(fake_dns_counter)) {
                break;
            }
        }
#endif
    }
    uint64_t addr = static_cast<uint64_t>(fake_dns_counter);
    uint32_t class_d = addr & 0xff;
    uint32_t class_c = (addr >> 8) & 0xff;
    uint32_t port = 1024 + ((addr >> 16) % (65535 - 1024));
    fake_dns_counter++;

    return std::string("169.254.") + std::to_string(class_c) + "." + std::to_string(class_d) + ":" + std::to_string(port);
}

std::string *GetFakeEndpointForHost(const std::string &host, int port) {
    std::string key = host + ":" + std::to_string(port);
    auto it = fake_dns_map.find(key);
    if (it != fake_dns_map.end()) {
        return it->second.second;
    }
    auto addr = GenerateFakeEndpoint();
    if (reverse_fake_dns_map.find(addr) != reverse_fake_dns_map.end()) {
        return nullptr; // Collision, out of addresses.
    }
    auto addr_ptr_raw = new std::string(addr);
    std::unique_ptr<std::string> addr_ptr(addr_ptr_raw);
    fake_dns_map[key] = {addr, addr_ptr.get()};
    reverse_fake_dns_map[addr] = {key, addr_ptr.get()};
    std::unique_ptr<refcount_entry> new_entry(new refcount_entry{0, std::move(addr_ptr), std::chrono::steady_clock::now()});
    fake_dns_refcount[addr_ptr_raw] = std::move(new_entry);
    return addr_ptr_raw;
}

std::pair<std::string, int> ParseHostPort(const std::string &location) {
    auto pos = location.find("://");
    std::string authority = (pos == std::string::npos) ? location : location.substr(pos + 3);
    std::string schema = (pos == std::string::npos) ? "" : location.substr(0, pos);
    int std_port = (schema == "https" || schema == "davs") ? 443 : 80;
    auto at_pos = authority.find('@');
    std::string hostport = (at_pos == std::string::npos) ? authority : authority.substr(at_pos + 1);
    pos = hostport.find('/');
    if (pos != std::string::npos) {
        hostport = hostport.substr(0, pos);
    }
    pos = hostport.find(':');
    if (pos == std::string::npos) {
        return {hostport, std_port};
    }
    int port = std_port;
    try {
        port = std::stoi(hostport.substr(pos + 1));
    } catch (...) {
        port = std_port;
    }
    return {hostport.substr(0, pos), port};
}

std::string DavToHttp(const std::string &url) {
    if (url.compare(0, 6, "dav://") == 0) {
        return "http://" + url.substr(6);
    }
    if (url.compare(0, 7, "davs://") == 0) {
        return "https://" + url.substr(7);
    }
    return url;
}

} // namespace

std::chrono::steady_clock::time_point CalculateExpiry(struct timespec timeout) {
    if (timeout.tv_sec == 0 && timeout.tv_nsec == 0) {
        return std::chrono::steady_clock::now() + std::chrono::seconds(30);
    }
    return std::chrono::steady_clock::now() + std::chrono::seconds(timeout.tv_sec) + std::chrono::nanoseconds(timeout.tv_nsec);
}

CurlOperation::CurlOperation(XrdCl::ResponseHandler *handler, const std::string &url,
    struct timespec timeout, XrdCl::Log *logger, CreateConnCalloutType callout,
    HeaderCallout *header_callout) :
    CurlOperation::CurlOperation(handler, url, CalculateExpiry(timeout), logger, callout, header_callout)
    {}

CurlOperation::CurlOperation(XrdCl::ResponseHandler *handler, const std::string &url,
    std::chrono::steady_clock::time_point expiry, XrdCl::Log *logger,
    CreateConnCalloutType callout, HeaderCallout *header_callout) :
    m_header_expiry(expiry),
    m_header_callout(header_callout),
    m_last_reset(std::chrono::steady_clock::now()),
    m_last_header_reset(m_last_reset),
    m_start_op(m_last_reset),
    m_header_start(m_last_reset),
    m_conn_callout(callout),
    m_url(DavToHttp(url)),
    m_handler(handler),
    m_curl(nullptr, &curl_easy_cleanup),
    m_logger(logger)
    {}

CurlOperation::~CurlOperation() {}

void
CurlOperation::Fail(uint16_t errCode, uint32_t errNum, const std::string &msg)
{
    SetDone(true);
    if (m_handler == nullptr) {return;}
    if (!msg.empty()) {
        m_logger->Debug(kLogXrdClHttp, "curl operation failed with message: %s", msg.c_str());
    } else {
        m_logger->Debug(kLogXrdClHttp, "curl operation failed with status code %d", errNum);
    }
    auto status = new XrdCl::XRootDStatus(XrdCl::stError, errCode, errNum, msg);
    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(status, nullptr);
}

int
CurlOperation::FailCallback(XErrorCode ecode, const std::string &emsg) {
    m_callback_error_code = ecode;
    m_callback_error_str = emsg;
    m_error = OpError::ErrCallback;
    m_logger->Debug(kLogXrdClHttp, "%s", emsg.c_str());
    return 0;
}

bool
CurlOperation::FinishSetup(CURL *curl)
{
    if (!m_header_callout) {
        m_header_slist.reset();
        for (const auto &header : m_headers_list) {
            m_header_slist.reset(curl_slist_append(m_header_slist.release(),
                (header.first + ": " + header.second).c_str()));
        }
        return curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_header_slist.get()) == CURLE_OK;
    }
    const auto &verb = GetVerbString(GetVerb());

    auto extra_headers = m_header_callout->GetHeaders(verb, m_url, m_headers_list);
    if (!extra_headers) {
        m_logger->Error(kLogXrdClHttp, "Failed to get headers from header callout for %s", m_url.c_str());
        return false;
    }
    m_header_slist.reset();
    for (const auto &header : *extra_headers) {
        if (!strcasecmp(header.first.c_str(), "Content-Length")) {
            auto upload_size = std::stoull(header.second);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, upload_size);
            continue;
        }
        m_header_slist.reset(curl_slist_append(m_header_slist.release(),
            (header.first + ": " + header.second).c_str()));
    }
    return curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_header_slist.get()) == CURLE_OK;
}

const std::string
CurlOperation::GetVerbString(CurlOperation::HttpVerb verb)
{
    switch (verb) {
    case HttpVerb::COPY:
        return "COPY";
    case HttpVerb::DELETE:
        return "DELETE";
    case HttpVerb::GET:
        return "GET";
    case HttpVerb::HEAD:
        return "HEAD";
    case HttpVerb::MKCOL:
        return "MKCOL";
    case HttpVerb::OPTIONS:
        return "OPTIONS";
    case HttpVerb::PROPFIND:
        return "PROPFIND";
    case HttpVerb::PUT:
        return "PUT";
    case HttpVerb::Count:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

size_t
CurlOperation::HeaderCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
{
    std::string header(buffer, size * nitems);
    auto me = static_cast<CurlOperation*>(this_ptr);
    auto now = std::chrono::steady_clock::now();
    if (!me->m_received_header) {
        me->m_received_header = true;
        me->m_header_start = now;
    }
    me->m_header_lastop = now;
    auto rv = me->Header(header);
    return rv ? (size * nitems) : 0;
}

bool
CurlOperation::Header(const std::string &header)
{
    auto result = m_headers.Parse(header);
    // m_logger->Debug(kLogXrdClHttp, "Got header: %s", header.c_str());
    if (!result) {
        m_logger->Debug(kLogXrdClHttp, "Failed to parse response header: %s", header.c_str());
    }
    if (m_headers.HeadersDone()) {
        if (!m_response_info) {
            m_response_info.reset(new ResponseInfo());
        }
        m_response_info->AddResponse(m_headers.MoveHeaders());
    }
    return result;
}

CurlOperation::RedirectAction
CurlOperation::Redirect(std::string &target)
{
    m_callout.reset();
    m_conn_callout_result = -1;
    m_conn_callout_listener = -1;
    m_tried_broker = false;

    auto location = m_headers.GetLocation();
    if (location.empty()) {
        m_logger->Warning(kLogXrdClHttp, "After request to %s, server returned a redirect with no new location", m_url.c_str());
        Fail(XrdCl::errErrorResponse, kXR_ServerError, "Server returned redirect without updated location");
        return RedirectAction::Fail;
    }
    if (location.size() && location[0] == '/') { // hostname not included in the location - redirect to self.
        std::string_view orig_url(m_url);
        auto scheme_loc = orig_url.find("://");
        if (scheme_loc == std::string_view::npos) {
            Fail(XrdCl::errErrorResponse, kXR_ServerError, "Server returned a location with unknown hostname");
            return RedirectAction::Fail;
        }
        auto path_loc = orig_url.find('/', scheme_loc + 3);
        if (path_loc == std::string_view::npos) {
            location = m_url + location;
        } else {
            location = std::string(orig_url.substr(0, path_loc)) + location;
        }
    }
    m_logger->Debug(kLogXrdClHttp, "Request for %s redirected to %s", m_url.c_str(), location.c_str());
    target = location;
    curl_easy_setopt(m_curl.get(), CURLOPT_URL, location.c_str());
    int disable_x509;
    auto env = XrdCl::DefaultEnv::GetEnv();
    if (env->GetInt("HttpDisableX509", disable_x509) && !disable_x509) {
        std::string cert, key;
        env->GetString("HttpClientCertFile", cert);
        env->GetString("HttpClientKeyFile", key);
        if (!cert.empty())
            curl_easy_setopt(m_curl.get(), CURLOPT_SSLCERT, cert.c_str());
        if (!key.empty())
            curl_easy_setopt(m_curl.get(), CURLOPT_SSLKEY, key.c_str());
    }
    m_headers = HeaderParser();

    if (m_conn_callout) {
        auto conn_callout = m_conn_callout(location, *m_response_info);
        if (conn_callout != nullptr) {

            auto [host, port] = ParseHostPort(location);
            if (host.empty() || port == -1) {
                Fail(XrdCl::errInternal, 0, "Failed to parse host and port from URL " + location);
                return RedirectAction::Fail;
            }
            auto fake_addr = GetFakeEndpointForHost(host, port);
            if (!fake_addr || fake_addr->empty()) {
                Fail(XrdCl::errInternal, 0, "Failed to generate a fake address for host " + host);
                return RedirectAction::Fail;
            }
            m_resolve_slist.reset(curl_slist_append(m_resolve_slist.release(),
                (host + ":" + std::to_string(port) + ":" + *fake_addr).c_str()));
            m_logger->Debug(kLogXrdClHttp, "For connection callout in redirect, mapping %s:%d -> %s", host.c_str(), port, fake_addr->c_str());

            m_callout.reset(conn_callout);
            std::string err;
            SetTriedBoker();
            if ((m_conn_callout_listener = m_callout->BeginCallout(err, m_header_expiry)) == -1) {
                auto errMsg = "Failed to start a connection callout request: " + err;
                Fail(XrdCl::errInternal, 0, errMsg.c_str());
                return RedirectAction::Fail;
            }
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, CurlOperation::OpenSocketCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_CLOSESOCKETFUNCTION, CurlOperation::CloseSocketCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, this);
            curl_easy_setopt(m_curl.get(), CURLOPT_CLOSESOCKETDATA, fake_addr);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, CurlOperation::SockOptCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, this);
            curl_easy_setopt(m_curl.get(), CURLOPT_CONNECT_TO, m_resolve_slist.get());
        }
    }
    m_received_header = false;

    m_last_header_reset = m_last_reset = m_header_start = m_start_op = m_header_lastop = std::chrono::steady_clock::now();
    return RedirectAction::Reinvoke;
}

namespace {

size_t
NullCallback(char * /*buffer*/, size_t size, size_t nitems, void * /*this_ptr*/)
{
    return size * nitems;
}

}

void
CurlOperation::SetPaused(bool paused) {
    m_is_paused = paused;
    if (m_is_paused) {
        m_pause_start = std::chrono::steady_clock::now();
    } else if (m_pause_start != std::chrono::steady_clock::time_point{}) {
        m_pause_duration += std::chrono::steady_clock::now() - m_pause_start;
        m_pause_start = std::chrono::steady_clock::time_point{};
    }
}

bool
CurlOperation::StartConnectionCallout(std::string &err)
{
    if ((m_conn_callout_listener = m_callout->BeginCallout(err, m_header_expiry)) == -1) {
        err = "Failed to start a callout for a socket connection: " + err;
        Fail(XrdCl::errInternal, 1, err.c_str());
        return false;
    }
    return true;
}

std::tuple<uint64_t, std::chrono::steady_clock::duration, std::chrono::steady_clock::duration, std::chrono::steady_clock::duration>
CurlOperation::StatisticsReset() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration pre_header{}, post_header{}, pause_duration{};
    if (m_received_header) {
        if (m_last_header_reset < m_header_start) {
            pre_header = m_header_start - m_last_header_reset;
            m_last_header_reset = m_header_start;
        }
        post_header = now - ((m_last_reset < m_header_start) ? m_header_start : m_last_reset);
        m_last_reset = now;
    } else {
        pre_header = now - m_last_header_reset;
        m_last_header_reset = now;
    }
    if (IsPaused()) {
        m_pause_duration += now - m_pause_start;
        m_pause_start = now;
    }
    if (m_pause_duration != std::chrono::steady_clock::duration::zero()) {
        pause_duration = m_pause_duration;
        m_pause_duration = std::chrono::steady_clock::duration::zero();
    }
    auto bytes = m_bytes;
    m_bytes = 0;
    return {bytes, pre_header, post_header, pause_duration};
}

bool
CurlOperation::HeaderTimeoutExpired(const std::chrono::steady_clock::time_point &now) {
    if (m_received_header) return false;

    if (now > m_header_expiry) {
        if (m_error == OpError::ErrNone) m_error = OpError::ErrHeaderTimeout;
        return true;
    }
    return false;
}

bool
CurlOperation::OperationTimeoutExpired(const std::chrono::steady_clock::time_point &now) {
    if (m_operation_expiry == std::chrono::steady_clock::time_point{} ||
        !m_received_header) {
        return false;
    }

    if (now > m_operation_expiry) {
        if (m_error == OpError::ErrNone) m_error = OpError::ErrOperationTimeout;
        return true;
    }
    return false;
}

bool
CurlOperation::TransferStalled(uint64_t xfer, const std::chrono::steady_clock::time_point &now)
{
    // First, check to see how long it's been since any data was sent.
    if (m_last_xfer == std::chrono::steady_clock::time_point()) {
        m_last_xfer = m_header_lastop;
    }
    auto elapsed = now - m_last_xfer;
    uint64_t xfer_diff = 0;
    if (xfer > m_last_xfer_count) {
        xfer_diff = xfer - m_last_xfer_count;
        m_last_xfer_count = xfer;
        m_last_xfer = now;
    }
    if (elapsed > m_stall_interval) {
        if (m_error == OpError::ErrNone) m_error = IsPaused() ? OpError::ErrTransferClientStall : OpError::ErrTransferStall;
        return true;
    }
    if (xfer_diff == 0) {
        // Curl updated us with new timing but the byte count hasn't changed; no need to update the EMA.
        return false;
    }

    // If the transfer is not stalled, then we check to see if the exponentially-weighted
    // moving average of the transfer rate is below the minimum.

    // If the stall interval since the last header hasn't passed, then we don't check for slow transfers.
    auto elapsed_since_last_headerop = now - m_header_lastop;
    if (elapsed_since_last_headerop < m_stall_interval) {
        return false;
    } else if (m_ema_rate < 0) {
        m_ema_rate = xfer / std::chrono::duration<double>(elapsed_since_last_headerop).count();
    }
    // Calculate the exponential moving average of the transfer rate.
    double elapsed_seconds = std::chrono::duration<double>(elapsed).count();
    auto recent_rate = static_cast<double>(xfer_diff) / elapsed_seconds;
    auto alpha = 1.0 - exp(-elapsed_seconds / std::chrono::duration<double>(m_stall_interval).count());
    m_ema_rate = (1.0 - alpha) * m_ema_rate + alpha * recent_rate;
    if (recent_rate < static_cast<double>(m_minimum_rate)) {
        if (m_error == OpError::ErrNone) m_error = OpError::ErrTransferSlow;
        return true;
    }
    return false;
}

bool
CurlOperation::Setup(CURL *curl, CurlWorker &worker)
{
    if (curl == nullptr) {
        throw std::runtime_error("Unable to setup curl operation with no handle");
    }
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        throw std::runtime_error("Unable to get current time");
    }

    m_pause_start = {};
    m_last_header_reset = m_last_reset = m_start_op = m_header_start = m_header_lastop = std::chrono::steady_clock::now();

    m_curl.reset(curl);
    curl_easy_setopt(m_curl.get(), CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(m_curl.get(), CURLOPT_HEADERFUNCTION, CurlStatOp::HeaderCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_HEADERDATA, this);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, NullCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_XFERINFOFUNCTION, CurlOperation::XferInfoCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl.get(), CURLOPT_NOPROGRESS, 0L);
    // Note: libcurl is not threadsafe unless this option is set.
    // Before we set it, we saw deadlocks (and partial deadlocks) in practice.
    curl_easy_setopt(m_curl.get(), CURLOPT_NOSIGNAL, 1L);

    m_parsed_url.reset(new XrdCl::URL(m_url));
    auto env = XrdCl::DefaultEnv::GetEnv();
    int disable_x509;
    if ((env->GetInt("HttpDisableX509", disable_x509) && !disable_x509)) {
        auto [cert, key] = worker.ClientX509CertKeyFile();
        if (!cert.empty()) {
            m_logger->Debug(kLogXrdClHttp, "Using client X.509 credential found at %s", cert.c_str());
            curl_easy_setopt(m_curl.get(), CURLOPT_SSLCERT, cert.c_str());
            if (key.empty()) {
                m_logger->Error(kLogXrdClHttp, "X.509 client credential specified but not the client key");
            } else {
                curl_easy_setopt(m_curl.get(), CURLOPT_SSLKEY, key.c_str());
            }
        }
    }

    if (m_conn_callout) {
        ResponseInfo info;
        auto callout = m_conn_callout(m_url, info);
        if (callout) {
            m_callout.reset(callout);
            m_conn_callout_listener = -1;
            m_conn_callout_result = -1;
            m_tried_broker = false;

            auto [host, port] = ParseHostPort(m_url);
            if (host.empty() || port == -1) {
                throw std::runtime_error ("Failed to parse host and port from URL " + m_url);
            }
            auto fake_addr = GetFakeEndpointForHost(host, port);
            if (!fake_addr || fake_addr->empty()) {
                throw std::runtime_error("Failed to generate a fake address for host " + host);
            }
            m_resolve_slist.reset(curl_slist_append(m_resolve_slist.release(),
                (host + ":" + std::to_string(port) + ":" + *fake_addr).c_str()));
            m_logger->Debug(kLogXrdClHttp, "For connection callout in operation setup, mapping %s:%d -> %s", host.c_str(), port, fake_addr->c_str());

            curl_easy_setopt(m_curl.get(), CURLOPT_CONNECT_TO, m_resolve_slist.get());

            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, CurlOperation::OpenSocketCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_CLOSESOCKETFUNCTION, CurlOperation::CloseSocketCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, this);
            curl_easy_setopt(m_curl.get(), CURLOPT_CLOSESOCKETDATA, fake_addr);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, CurlOperation::SockOptCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, this);
        }
    }

    return true;
}

void
CurlOperation::ReleaseHandle()
{
    m_conn_callout_listener = -1;
    m_conn_callout_result = -1;
    m_tried_broker = false;
    m_callout.reset();

    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_CLOSESOCKETFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_CLOSESOCKETDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SSLCERT, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SSLKEY, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_CONNECT_TO, nullptr);
    m_header_slist.reset();
    m_curl.release();
}

curl_socket_t
CurlOperation::OpenSocketCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address)
{
    auto me = reinterpret_cast<CurlOperation*>(clientp);
    auto fd = me->m_conn_callout_result;
    me->m_conn_callout_result = -1;
    if (fd == -1) {
        std::string err;
        if ((me->m_conn_callout_listener = me->m_callout->BeginCallout(err, me->m_header_expiry)) == -1) {
            me->m_logger->Debug(kLogXrdClHttp, "Failed to start a connection callout request: %s", err.c_str());
        }
        return CURL_SOCKET_BAD;
    } else {
        sockaddr_in *inaddr = reinterpret_cast<sockaddr_in*>(&address->addr);
        char ip_str[INET_ADDRSTRLEN];
        char full_address_str[INET_ADDRSTRLEN + 6];
        inet_ntop(AF_INET, &(inaddr->sin_addr), ip_str, INET_ADDRSTRLEN);
        int port = ntohs(inaddr->sin_port);
        snprintf(full_address_str, sizeof(full_address_str), "%s:%d", ip_str, port);
        me->m_logger->Debug(kLogXrdClHttp, "Recording socket %d for %s", fd, full_address_str);
        auto reverse_iter = reverse_fake_dns_map.find(full_address_str);
        if (reverse_iter == reverse_fake_dns_map.end()) {
            me->m_logger->Error(kLogXrdClHttp, "Failed to find fake DNS reverse entry for %s", full_address_str);
            close(fd);
            return CURL_SOCKET_BAD;
        } else {
            auto iter = fake_dns_refcount.find(reverse_iter->second.second);
            if (iter == fake_dns_refcount.end()) {
                me->m_logger->Error(kLogXrdClHttp, "Failed to find fake DNS refcount entry for %s", full_address_str);
                close(fd);
                return CURL_SOCKET_BAD;
            }
            iter->second->count++;
            iter->second->last_used = std::chrono::steady_clock::now();
        }

        return fd;
    }
}

int
CurlOperation::SockOptCallback(void *clientp, curl_socket_t curlfd, curlsocktype purpose)
{
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}

curl_socket_t
CurlOperation::CloseSocketCallback(void *clientp, curl_socket_t fd)
{
    close(fd);
    auto me = reinterpret_cast<std::string*>(clientp);
    if (me == nullptr) {return 0;}
    auto iter = fake_dns_refcount.find(me);
    if (iter != fake_dns_refcount.end()) {
        iter->second->count--;
        if (iter->second->count <= 0 && iter->second->IsExpired(std::chrono::steady_clock::now())) {
            auto rev_iter = reverse_fake_dns_map.find(*me);
            if (rev_iter != reverse_fake_dns_map.end()) {
                fake_dns_map.erase(rev_iter->second.first);
                reverse_fake_dns_map.erase(rev_iter);
            }
            fake_dns_refcount.erase(iter);
        }
    }

    return 0;
}

void
CurlOperation::CleanupDnsCache()
{
    auto now = std::chrono::steady_clock::now();
    for (auto it = fake_dns_refcount.begin(); it != fake_dns_refcount.end(); ) {
        if (it->second->count <= 0 && it->second->IsExpired(now)) {
            auto rev_iter = reverse_fake_dns_map.find(*it->first);
            if (rev_iter != reverse_fake_dns_map.end()) {
                fake_dns_map.erase(rev_iter->second.first);
                reverse_fake_dns_map.erase(rev_iter);
            }
            it = fake_dns_refcount.erase(it);
        } else {
            ++it;
        }
    }
}

int
CurlOperation::XferInfoCallback(void *clientp, curl_off_t /*dltotal*/, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t ulnow)
{
    auto me = reinterpret_cast<CurlOperation*>(clientp);
    auto now = std::chrono::steady_clock::now();
    if (me->HeaderTimeoutExpired(now) || me->OperationTimeoutExpired(now)) {
        return 1; // return value triggers CURLE_ABORTED_BY_CALLBACK
    }
    uint64_t xfer_bytes = dlnow > ulnow ? dlnow : ulnow;
    if (me->TransferStalled(xfer_bytes, now)) {
        return 1;
    }
    return 0;
}

int
CurlOperation::WaitSocketCallback(std::string &err)
{
    m_conn_callout_result = m_callout ? m_callout->FinishCallout(err) : -1;
    if (m_callout && m_conn_callout_result == -1) {
        m_logger->Error(kLogXrdClHttp, "Error when getting socket callout: %s", err.c_str());
    } else if (m_callout) {
        m_logger->Debug(kLogXrdClHttp, "Got callback socket %d", m_conn_callout_result);
    }
    return m_conn_callout_result;
}
