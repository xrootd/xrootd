/***************************************************************
 *
 * Copyright (C) 2025, Pelican Project, Morgridge Institute for Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "XrdClCurlOps.hh"
#include "XrdClCurlResponses.hh"
#include "XrdClCurlUtil.hh"
#include "XrdClCurlWorker.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <unistd.h>
#include <cmath>
#include <utility>

using namespace XrdClCurl;

std::chrono::steady_clock::duration CurlOperation::m_stall_interval{CurlOperation::m_default_stall_interval};
int CurlOperation::m_minimum_transfer_rate{CurlOperation::m_default_minimum_rate};

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
    m_conn_callout(callout),
    m_url(url),
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
        m_logger->Debug(kLogXrdClCurl, "curl operation failed with message: %s", msg.c_str());
    } else {
        m_logger->Debug(kLogXrdClCurl, "curl operation failed with status code %d", errNum);
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
    m_logger->Debug(kLogXrdClCurl, "%s", emsg.c_str());
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
    const auto &verb = GetVerb();

    auto extra_headers = m_header_callout->GetHeaders(verb, m_url, m_headers_list);
    if (!extra_headers) {
        m_logger->Error(kLogXrdClCurl, "Failed to get headers from header callout for %s", m_url.c_str());
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

size_t
CurlOperation::HeaderCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
{
    std::string header(buffer, size * nitems);
    auto me = static_cast<CurlOperation*>(this_ptr);
    me->m_received_header = true;
    me->m_header_lastop = std::chrono::steady_clock::now();
    auto rv = me->Header(header);
    return rv ? (size * nitems) : 0;
}

bool
CurlOperation::Header(const std::string &header)
{
    auto result = m_headers.Parse(header);
    // m_logger->Debug(kLogXrdClCurl, "Got header: %s", header.c_str());
    if (!result) {
        m_logger->Debug(kLogXrdClCurl, "Failed to parse response header: %s", header.c_str());
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
        m_logger->Warning(kLogXrdClCurl, "After request to %s, server returned a redirect with no new location", m_url.c_str());
        Fail(XrdCl::errErrorResponse, kXR_ServerError, "Server returned redirect without updated location");
        return RedirectAction::Fail;
    }
    m_logger->Debug(kLogXrdClCurl, "Request for %s redirected to %s", m_url.c_str(), location.c_str());
    target = location;
    curl_easy_setopt(m_curl.get(), CURLOPT_URL, location.c_str());
    int disable_x509;
    auto env = XrdCl::DefaultEnv::GetEnv();
    if (env->GetInt("CurlDisableX509", disable_x509) && !disable_x509) {
        std::string cert, key;
        env->GetString("CurlClientCertFile", cert);
        env->GetString("CurlClientKeyFile", key);
        if (!cert.empty())
            curl_easy_setopt(m_curl.get(), CURLOPT_SSLCERT, cert.c_str());
        if (!key.empty())
            curl_easy_setopt(m_curl.get(), CURLOPT_SSLKEY, key.c_str());
    }
    m_headers = HeaderParser();

    if (m_conn_callout) {
        auto conn_callout = m_conn_callout(location, *m_response_info);
        if (conn_callout != nullptr) {
            m_callout.reset(conn_callout);
            std::string err;
            SetTriedBoker();
            if ((m_conn_callout_listener = m_callout->BeginCallout(err, m_header_expiry)) == -1) {
                auto errMsg = "Failed to start a connection callout request: " + err;
                Fail(XrdCl::errInternal, 0, errMsg.c_str());
                return RedirectAction::Fail;
            }
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, CurlReadOp::OpenSocketCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, this);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, CurlReadOp::SockOptCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, this);
        }
    }
    return RedirectAction::Reinvoke;
}

namespace {

size_t
NullCallback(char * /*buffer*/, size_t size, size_t nitems, void * /*this_ptr*/)
{
    return size * nitems;
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
        if (m_error == OpError::ErrNone) m_error = OpError::ErrTransferStall;
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

    m_header_lastop = std::chrono::steady_clock::now();

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
    if ((env->GetInt("CurlDisableX509", disable_x509) && !disable_x509)) {
        auto [cert, key] = worker.ClientX509CertKeyFile();
        if (!cert.empty()) {
            m_logger->Debug(kLogXrdClCurl, "Using client X.509 credential found at %s", cert.c_str());
            curl_easy_setopt(m_curl.get(), CURLOPT_SSLCERT, cert.c_str());
            if (key.empty()) {
                m_logger->Error(kLogXrdClCurl, "X.509 client credential specified but not the client key");
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
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, CurlReadOp::OpenSocketCallback);
            curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, this);
            curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, CurlReadOp::SockOptCallback);
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
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SSLCERT, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SSLKEY, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);
    m_header_slist.reset();
    m_curl.release();
}

curl_socket_t
CurlOperation::OpenSocketCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address)
{
    auto me = reinterpret_cast<CurlReadOp*>(clientp);
    auto fd = me->m_conn_callout_result;
    me->m_conn_callout_result = -1;
    if (fd == -1) {
        std::string err;
        if ((me->m_conn_callout_listener = me->m_callout->BeginCallout(err, me->m_header_expiry)) == -1) {
            me->m_logger->Debug(kLogXrdClCurl, "Failed to start a connection callout request: %s", err.c_str());
        }
        return CURL_SOCKET_BAD;
    } else {
        return fd;
    }
}

int
CurlOperation::SockOptCallback(void *clientp, curl_socket_t curlfd, curlsocktype purpose)
{
    return CURL_SOCKOPT_ALREADY_CONNECTED;
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
        m_logger->Error(kLogXrdClCurl, "Error when getting socket callout: %s", err.c_str());
    } else if (m_callout) {
        m_logger->Debug(kLogXrdClCurl, "Got connection on socket %d", m_conn_callout_result);
    }
    return m_conn_callout_result;
}
