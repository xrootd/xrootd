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

#ifndef XRDCLCURL_CURLOPS_HH
#define XRDCLCURL_CURLOPS_HH

#include "XrdClCurlConnectionCallout.hh"
#include "XrdClCurlHeaderCallout.hh"
#include "XrdClCurlResponseInfo.hh"
#include "XrdClCurlUtil.hh"

#include <XrdCl/XrdClBuffer.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

namespace XrdCl {

class Log;
class ResponseHandler;
class URL;

}

class TiXmlElement;

namespace XrdClCurl {

class CurlWorker;
class File;
class ResponseInfo;

class CurlOperation {
public:
    using HeaderList = std::vector<std::pair<std::string, std::string>>;

    enum class HttpVerb {
        COPY,
        DELETE,
        HEAD,
        GET,
        MKCOL,
        OPTIONS,
        PROPFIND,
        PUT,
        Count
    };

    // Operation constructor when the timeout is given as an offset from now.
    CurlOperation(XrdCl::ResponseHandler *handler, const std::string &url, struct timespec timeout,
        XrdCl::Log *log, CreateConnCalloutType, HeaderCallout *header_callout);


    // Operation constructor when the timeout is given as an absolute time.
    CurlOperation(XrdCl::ResponseHandler *handler, const std::string &url, std::chrono::steady_clock::time_point expiry,
        XrdCl::Log *log, CreateConnCalloutType, HeaderCallout *header_callout);

    virtual ~CurlOperation();

    CurlOperation(const CurlOperation &) = delete;

    // Finish the setup of the curl handle
    //
    // Used for configuring any extra headers
    bool FinishSetup(CURL *curl);

    virtual bool Setup(CURL *curl, CurlWorker &);

    virtual void Fail(uint16_t errCode, uint32_t errNum, const std::string &);

    virtual void ReleaseHandle();

    virtual void Success() = 0;

    // Returns the connection callout function for this operation
    CreateConnCalloutType GetConnCalloutFunc() const {return m_conn_callout;}

    // Return the HTTP verb to use with this operation.
    virtual HttpVerb GetVerb() const = 0;

    // Return a string version of the HTTP operation
    static const std::string GetVerbString(HttpVerb);

    // Returns when the curl header timeout expires.
    //
    // The first byte of the header must be received before this time.
    std::chrono::steady_clock::time_point GetHeaderExpiry() const {return m_header_expiry;}

    // Returns when the curl operation expires
    std::chrono::steady_clock::time_point GetOperationExpiry() {
        if (m_last_xfer == std::chrono::steady_clock::time_point()) {
            return GetHeaderExpiry();
        }
        return m_last_xfer + m_stall_interval;
    }

    // Clean up the thread-local DNS cache for fake lookups associated with the
    // connection callback cache.
    static void CleanupDnsCache();

    // Invoked when the worker thread is ready to resume a request after a pause.
    //
    // Pauses occur when a PUT request has started but is waiting on more data
    // from the client; when additional data has arrived, the operation will
    // be continued and this function called by the worker thread.
	virtual bool ContinueHandle() {return true;}

    // Set the continue queue to use for when a paused handle is ready to
    // be re-run.
	virtual void SetContinueQueue(std::shared_ptr<XrdClCurl::HandlerQueue> queue) {}

    enum class RedirectAction {
        Fail, // The redirect parsing failed and Fail() was called
        Reinvoke, // Reinvoke the curl handle, following redirect
        ReinvokeAfterAllow, // Reinvoke the Redirect function once the allowed verbs are known.
    };
    // Handle a redirect to a different URL.
    // Returns Reinvoke if the curl handle should be invoked again immediately.
    // Returns ReinvokeAfterAllow if the redirect should be invoked after the allowed verbs are known.
    //    In this case, the operation will set the target to the redirect target.
    // Implementations must call Fail() if the handler should not re-invoke the curl handle.
    virtual RedirectAction Redirect(std::string &target);

    // Indicate whether the result of the operation is a redirect.
    //
    // This relies on the response headers having been parsed and available; anything in
    // the 30X range is considered a redirect.
    bool IsRedirect() const {return m_headers.GetStatusCode() >= 300 && m_headers.GetStatusCode() < 400;}

    // If returns non-negative, the result is a FD that should be waited on after a broker connection request.
    virtual int WaitSocket() {return m_conn_callout_listener;}
    // Callback when the `WaitSocket` is active for read.
    virtual int WaitSocketCallback(std::string &err);

    // Connection broker-related functionality.
    // When the broker URL is set, the operation will use the connection broker to get a TCP socket
    // to the remote server.  Note that we will try the operation initially without in case the curl
    // handle has an existing socket it can reuse.  If reuse fails, then the operation is going to fail
    // with CURLE_COULDNT_CONNECT and we will retry (once) to connect via the broker.  This is all
    // done outside curl's open socket callback to ensure the event loop stays non-blocking.

    bool StartConnectionCallout(std::string &err); // Start the connection callout process for a URL.
    bool UseConnectionCallout() {return m_callout.get();} // Returns true if the callout should be tried.
    bool GetTriedBoker() const {return m_tried_broker;} // Returns true if the connection broker has been tried.
    void SetTriedBoker() {m_tried_broker = true;} // Note that the connection broker has been attempted.

    // Returns whethe the OPTIONS call needs to be made before the operation is started.
    bool virtual RequiresOptions() const {return false;}

    // Invoked after the OPTIONS request is done and results are available
    void virtual OptionsDone() {}

    // Returns the URL that was used for the operation.
    const std::string &GetUrl() const {return m_url;}

    // Returns the response info for the operation
    std::unique_ptr<ResponseInfo> GetResponseInfo();

    // Returns true if the header timeout has expired.
    //
    // The "header timeout" fires if the remote service has not returned any
    // headers or data within the specified time.
    // If the header timeout has expired - and no error has already been set -
    // the m_error will be set
    bool HeaderTimeoutExpired(const std::chrono::steady_clock::time_point &now);

    // Returns true if the operation timeout has expired.
    //
    // Some operations (HEAD, PROPFIND for open) return nearly no data and thus have
    // no need for adaptive timeouts.  Instead, we use a fixed timeout.
    // If the header timeout has expired - and no error has already been set -
    // the m_error will be set
    bool OperationTimeoutExpired(const std::chrono::steady_clock::time_point &now);

    // Returns true if the body timeout has expired.
    //
    // The "body timeout" fires if the remote service has not returned any
    // data within the specified time.
    // If the body timeout has expired - and no error has already been set -
    // the m_error will be set
    bool TransferStalled(uint64_t xfer_bytes, const std::chrono::steady_clock::time_point &now);

    enum OpError {
        ErrNone,                // No error
        ErrHeaderTimeout,       // Header was not sent back in time
        ErrCallback,            // Error in the read/write callback (e.g., response too large for propfind)
        ErrOperationTimeout,    // Entire curl request operation has timed out
        ErrTransferClientStall, // Transfer stalled while client had paused it (no data was available)
        ErrTransferStall,       // Transfer has stalled, not receiving any data within 60 seconds
        ErrTransferSlow,        // Average transfer rate is below the minimum
    };

    // Return the libcurl handle owned by this operation.
    CURL *GetCurlHandle() const {return m_curl.get();}

    // Return the error generated by the operation itself (separate from a curl error)
    OpError GetError() const {return m_error;}

    // Move response info to the caller.
    std::unique_ptr<ResponseInfo> MoveResponseInfo() {return std::move(m_response_info);}

    // Return the error generated by the callback (e.g., server has incorrect multipart framing)
    std::pair<XErrorCode, const std::string &> GetCallbackError() const {return std::make_pair(m_callback_error_code, m_callback_error_str);}

    // Returns the HTTP status code (-1 if the response has not been parsed)
    int GetStatusCode() const {return m_headers.GetStatusCode();}

    // Returns the HTTP status message (empty if the response has not been parsed)
    std::string GetStatusMessage() const {return m_headers.GetStatusMessage();}

    // Return true if the transfer is done
    bool IsDone() const {return m_done;}

    // Return true if the operation is paused in libcurl
    bool IsPaused() const {return m_is_paused;}

    // Returns true if the operation has been marked as failed.
    bool HasFailed() const {return m_has_failed.load(std::memory_order_acquire);}

    // Resets the statistics for the operation and returns a tuple of:
    // - bytes transferred,
    // - duration between operation start and header receipt.
    // - duration between header receipt and now.
    // - duration the operation has spent on pause in libcurl (waiting for client data)
    // These numbers are reset to zero each time the `StatisticsReset` function is called.
    std::tuple<uint64_t, std::chrono::steady_clock::duration, std::chrono::steady_clock::duration, std::chrono::steady_clock::duration> StatisticsReset();

    // Sets the stall timeout for the operation in seconds.
    static void SetStallTimeout(int stall_interval)
    {
        std::chrono::seconds seconds{stall_interval};
        m_stall_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds);
    }

    // Sets the stall timeout for the operation
    static void SetStallTimeout(const std::chrono::steady_clock::duration &stall_interval)
    {
        m_stall_interval = stall_interval;
    }

    // Gets the code's default stall timeout in seconds
    static int GetDefaultStallTimeout()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(m_default_stall_interval).count();
    }

    // Gets the code's default slow transfer rate
    static int GetDefaultSlowRateBytesSec()
    {
        return m_default_minimum_rate;
    }

    // Sets the slow transfer rate for transfer operations.
    static void SetSlowRateBytesSec(int rate)
    {
        m_minimum_transfer_rate = rate;
    }

protected:

    // Update the count of bytes transferred
    void UpdateBytes(uint64_t bytes) {m_bytes += bytes;}

    // Set failure from a callback function.
    // The Fail() function may invoke libcurl functions and hence cannot be invoked from a
    // libcurl callback.  This stores the failure in the object itself and the worker
    // thread will invoke the `Fail()` after libcurl fails the handle.
    int FailCallback(XErrorCode ecode, const std::string &emsg);

    // Set the pause status
    void SetPaused(bool paused);

    // The default minimum transfer rate for the operation, in bytes / sec
    static constexpr int m_default_minimum_rate{1024 * 256}; // 256 KB/sec

    // The current global instance's minimum transfer rate for "transfer type"
    // operations (GET, PUT).  Defaults to the m_default_minimum_rate but can be
    // overridden by configuration.
    static int m_minimum_transfer_rate;

    // The minimum transfer rate for this operation, in bytes / sec
    int m_minimum_rate{m_minimum_transfer_rate};

    // The expiration of the entire operation.
    std::chrono::steady_clock::time_point m_operation_expiry;

    // The expiration time for receiving the first header.
    std::chrono::steady_clock::time_point m_header_expiry;

    // Any additional headers to send with the request.
    HeaderCallout *m_header_callout;

private:
    bool Header(const std::string &header);
    static size_t HeaderCallback(char *buffer, size_t size, size_t nitems, void *data);

    // Information about the responses received for this operation.
    std::unique_ptr<ResponseInfo> m_response_info;

    // The "stall time" for the body transfer.
    // If the body transfer has not been updated in this time, the operation
    // will be marked as expired.
    //
    // This is also used for the calculation of the interval of the EMA rate
    static constexpr std::chrono::steady_clock::duration m_default_stall_interval{std::chrono::seconds(60)};
    static std::chrono::steady_clock::duration m_stall_interval;

    OpError m_error{ErrNone};
    XErrorCode m_callback_error_code{kXR_noErrorYet}; // Stored error that occurred in a callback.
    std::string m_callback_error_str; // Stored error message that occurred in a callback.
    bool m_tried_broker{false};
    bool m_received_header{false};
    bool m_done{false};
    std::atomic<bool> m_has_failed{false};
    bool m_is_paused{false};
    int m_conn_callout_result{-1}; // The result of the connection callout
    int m_conn_callout_listener{-1}; // The listener socket for the connection callout
    uint64_t m_bytes{0}; // Count of bytes transferred by operation since last StatisticsReset()
    std::chrono::steady_clock::time_point m_last_reset{}; // Time of last StatisticsReset()
    std::chrono::steady_clock::time_point m_last_header_reset{}; // Time of last StatisticsReset() for header statistics
    std::chrono::steady_clock::time_point m_start_op{}; // Time when the entire operation was started.
    std::chrono::steady_clock::time_point m_header_start{}; // Time when the first header was received.
    std::chrono::steady_clock::time_point m_pause_start{}; // Time of the last pause start/reset
    std::chrono::steady_clock::duration m_pause_duration{}; // Accumulated pause time since last statistics update.

    // List of custom headers for the operation.
    std::unique_ptr<struct curl_slist, void(*)(struct curl_slist *)> m_header_slist{nullptr, &curl_slist_free_all};

    // The callout class for connection creation.
    CreateConnCalloutType m_conn_callout{nullptr};

    // The last time header data was received.
    std::chrono::steady_clock::time_point m_header_lastop;

    // The last time data was transferred.
    std::chrono::steady_clock::time_point m_last_xfer;

    // The last recorded number of bytes that had been transferred.
    uint64_t m_last_xfer_count{0};

    // The exponential moving average of the transfer rate
    double m_ema_rate{-1.0};

    // Object representing the state of the callout for a connected socket.
    std::unique_ptr<ConnectionCallout> m_callout;
    std::unique_ptr<XrdCl::URL> m_parsed_url{nullptr};

    // A map of endpoints to IP addresses for the CURLOPT_CONNECT_TO option.
    std::unique_ptr<struct curl_slist, void(*)(struct curl_slist *)> m_resolve_slist{nullptr, &curl_slist_free_all};

    static curl_socket_t OpenSocketCallback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address);
    static int SockOptCallback(void *clientp, curl_socket_t curlfd, curlsocktype purpose);
    static curl_socket_t CloseSocketCallback(void *clientp, curl_socket_t item);

    // Periodic transfer info callback function invoked by curl; used for more fine-grained timeouts.
    static int XferInfoCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

protected:
    void SetDone(bool has_failed) {m_done = true; m_has_failed.store(has_failed, std::memory_order_release);}
    const std::string m_url;
    XrdCl::ResponseHandler *m_handler{nullptr};
    std::unique_ptr<CURL, void(*)(CURL *)> m_curl;
    HeaderParser m_headers;
    std::vector<std::pair<std::string, std::string>> m_headers_list;
    XrdCl::Log *m_logger;
};

// Query the remote service using the OPTIONS verb.
//
// This is used to determine the capabilities of the remote service,
// such as whether it supports the PROPFIND verb.
// Note this does not take an XrdCl::ResponseHandler callback but is meant to be
// invoked directly by a libcurl worker which, based on the response, will use
// it to invoke the original operation.
class CurlOptionsOp final : public CurlOperation {
public:
    CurlOptionsOp(CURL *curl, std::shared_ptr<CurlOperation> op, const std::string &url,
        XrdCl::Log *log, CreateConnCalloutType callout) :
        CurlOperation(nullptr, url, op->GetHeaderExpiry(), log, callout, {}),
        m_parent(op),
        m_parent_curl(curl)
    {
        m_operation_expiry = m_header_expiry;
    }

    virtual ~CurlOptionsOp() {}

    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;
    void Fail(uint16_t errCode, uint32_t errNum, const std::string &) override;
    void ReleaseHandle() override;

    // Returns the parent operation that has been paused while waiting for the
    // OPTIONS response.
    std::shared_ptr<CurlOperation> GetOperation() const {return m_parent;}

    // Returns the parent operation's curl handle that has been paused while
    // waiting for the OPTIONS response.
    CURL *GetParentCurlHandle() const {return m_parent_curl;}

    virtual HttpVerb GetVerb() const override {return HttpVerb::OPTIONS;}

private:
    std::shared_ptr<CurlOperation> m_parent;
    CURL *m_parent_curl{nullptr};
};

// An operation representing a `stat` operation.
//
// Queries the remote service and parses out the response to a `stat` buffer.
// Depending on the remote service, this may be a HEAD or PROPFIND request.
class CurlStatOp : public CurlOperation {
public:
    CurlStatOp(XrdCl::ResponseHandler *handler, const std::string &url, struct timespec timeout,
        XrdCl::Log *log, bool response_info, CreateConnCalloutType callout, HeaderCallout *header_callout) :
    CurlOperation(handler, url, timeout, log, callout, header_callout),
    m_response_info(response_info)
    {
        m_operation_expiry = m_header_expiry;
    }

    virtual ~CurlStatOp() {}

    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;
    RedirectAction Redirect(std::string &target) override;
    void ReleaseHandle() override;

    bool virtual RequiresOptions() const override;
    void virtual OptionsDone() override;

    std::pair<int64_t, bool> GetStatInfo();

    virtual HttpVerb GetVerb() const override {return m_is_propfind ? HttpVerb::PROPFIND : HttpVerb::HEAD;}

protected:
    // Mark the operation as a success and, as requested, return the stat info back
    // to the object handler.
    //
    // Returning the info is optional as the CurlOpenOp derives from this clasa and
    // if stat info is returned from an open without being requested then the
    // object is leaked
    void SuccessImpl(bool returnObj);

private:
    // Parse the properties element of a PROPFIND response.
    std::pair<int64_t, bool> ParseProp(TiXmlElement *prop);
    // Callback for writing the response body to the internal buffer.
    static size_t WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr);

    // Whether the response info variant of the info object should be sent
    bool m_response_info{false};
    // Whether the stat request is made using the PROPFIND verb.
    bool m_is_propfind{false};
    // Whether the stat response indicated that the object is a directory.
    bool m_is_dir{false};
    std::string m_response; // Body of the response (if using PROPFIND)
    int64_t m_length{-1}; // Length of the object from the response
};

class CurlOpenOp final : public CurlStatOp {
public:
    CurlOpenOp(XrdCl::ResponseHandler *handler, const std::string &url, struct timespec timeout,
        XrdCl::Log *logger, XrdClCurl::File *file, bool response_info, CreateConnCalloutType callout,
        HeaderCallout *header_callout);

    virtual ~CurlOpenOp() {}

    void ReleaseHandle() override;
    void Success() override;

    // Invoked to handle a failure-to-open (HEAD returns non-200)
    //
    // If the open operation is invoked for a file with the `New` flag set, this
    // may be a success if the remote server returned a 404.
    void Fail(uint16_t errCode, uint32_t errNum, const std::string &) override;

private:
    // Set various common properties after an open has completed.
    //
    // If `setSize` is set, then we'll set the file size as a file property.
    // This is made optional because the open operation may succeed after a 404
    // (if this was invoked by an open with O_CREAT set); in such a case, setting
    // the size is nonsensical because the file doesn't exist.
    void SetOpenProperties(bool setSize);

    XrdClCurl::File *m_file{nullptr};
};

// Query the origin for a checksum via a HEAD request.
//
// Since the open op is a PROPFIND, we need a second operation for checksums.
// We expect the checksum only is done after a successful transfer.
class CurlChecksumOp final : public CurlStatOp {
    public:
        CurlChecksumOp(XrdCl::ResponseHandler *handler, const std::string &url, XrdClCurl::ChecksumType preferred,
            struct timespec timeout, XrdCl::Log *logger,
            bool response_info, CreateConnCalloutType callout, HeaderCallout *header_callout);

        virtual ~CurlChecksumOp() {}

        virtual HttpVerb GetVerb() const override {return HttpVerb::HEAD;}
        virtual void OptionsDone() override;
        bool Setup(CURL *curl, CurlWorker &) override;
        void Success() override;
        RedirectAction Redirect(std::string &target) override;
        void ReleaseHandle() override;

    private:
        XrdClCurl::ChecksumType m_preferred_cksum{XrdClCurl::ChecksumType::kCRC32C};
        XrdClCurl::File *m_file{nullptr};
    };

// Operation issuing a DELETE request to the remote server.
//
class CurlDeleteOp final : public CurlOperation {
public:
    CurlDeleteOp(XrdCl::ResponseHandler *handler, const std::string &url,
        struct timespec timeout, XrdCl::Log *logger,
        bool response_info, CreateConnCalloutType callout,
        HeaderCallout *header_callout);

    virtual ~CurlDeleteOp();

    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;
    void ReleaseHandle() override;

    virtual HttpVerb GetVerb() const override {return HttpVerb::DELETE;}

private:
    bool m_response_info{false}; // Indicate whether to give extended information in the response.
};

// Operation issuing a MKCOL request to the remote server.
//
// Creates a "directory" on the remote side
//
class CurlMkcolOp final : public CurlOperation {
public:
CurlMkcolOp(XrdCl::ResponseHandler *handler, const std::string &url,
        struct timespec timeout, XrdCl::Log *logger,
        bool response_info, CreateConnCalloutType callout,
        HeaderCallout *header_callout);

    virtual ~CurlMkcolOp();

    void Fail(uint16_t errCode, uint32_t errNum, const std::string &msg) override;
    void ReleaseHandle() override;
    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;

    virtual HttpVerb GetVerb() const override {return HttpVerb::MKCOL;}

private:
    bool m_response_info{false}; // Indicate whether to give extended information in the response.
};

//  Cache control query
//
class CurlQueryOp final : public CurlStatOp {
public:
 CurlQueryOp(XrdCl::ResponseHandler *handler, const std::string &url, struct timespec timeout,
        XrdCl::Log *log, bool response_info, CreateConnCalloutType callout, int queryCode, HeaderCallout *header_callout) :
    CurlStatOp(handler, url, timeout, log, response_info, callout, header_callout),
    m_queryCode(queryCode)
    {
    }

    virtual ~CurlQueryOp() {}

    void Success() override;

    int  m_queryCode;
    std::string m_queryVal;
};

class CurlReadOp : public CurlOperation {
public:
    CurlReadOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
        const std::string &url, struct timespec timeout, const std::pair<uint64_t, uint64_t> &op,
        char *buffer, size_t sz, XrdCl::Log *logger, CreateConnCalloutType callout, HeaderCallout *header_callout);

    virtual ~CurlReadOp() {}

    // Start continuation of a previously-started operation with additional data.
    bool Continue(std::shared_ptr<CurlOperation> op, XrdCl::ResponseHandler *handler, char *buffer, size_t buffer_size);

    // Make state changes necessary to the curl handle for it to unpause.
    bool ContinueHandle() override;

    // Pause the GET operation; indicates the current buffer was sent successfully
    // but the operation is not yet complete.  Will invoke the current callback.
    virtual void Pause();

    bool Setup(CURL *curl, CurlWorker &) override;
    void Fail(uint16_t errCode, uint32_t errNum, const std::string &msg) override;
    void Success() override;
    void ReleaseHandle() override;

	virtual void SetContinueQueue(std::shared_ptr<XrdClCurl::HandlerQueue> queue) override {
		m_continue_queue = queue;
	}

    virtual HttpVerb GetVerb() const override {return HttpVerb::GET;}


private:
    static size_t WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr);
    size_t Write(char *buffer, size_t size);

    // Extra response data from curl that overflowed the last buffer
    //
    // libcurl's callback is "all or nothing": you cannot accept part of a buffer
    // then pause the operation until the user provides a new buffer.  Hence, we keep
    // this as the "overflow" buffer; next time Continue() is called, we will process
    // this data first.
    std::string m_prefetch_buffer;

    // Offset into m_prefetch_buffer pointing at the first byte of unconsumed data.
    size_t m_prefetch_buffer_offset{0};

    // Offset into the object, for the current Continue() call, relative to m_op.first
    off_t m_prefetch_object_offset{0};

    // Default callback handler; used when the HTTP operation times out while there
    // is no ongoing CurlFile read operation.
    std::shared_ptr<XrdCl::ResponseHandler> m_default_handler;

protected:
    std::pair<uint64_t, uint64_t> m_op;
    uint64_t m_written{0}; // Bytes written into the current client-provided buffer
    char *m_buffer{nullptr}; // Buffer passed by XrdCl; we do not own it.
    size_t m_buffer_size{0}; // Size of the provided buffer

    // When the read fails, the body of the response will be copied
    // here instead of invoking the callback.
    std::string m_err_msg;

    // Reference to the continue queue to use when the operation should be resumed.
    std::shared_ptr<XrdClCurl::HandlerQueue> m_continue_queue;
};

// Open operation that is actually an entire-object GET
class CurlPrefetchOpenOp : public CurlReadOp {
public:
    CurlPrefetchOpenOp(XrdClCurl::File &file, XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
        const std::string &url, struct timespec timeout, const std::pair<uint64_t, uint64_t> &op,
        char *buffer, size_t sz, XrdCl::Log *logger, CreateConnCalloutType callout, HeaderCallout *header_callout)
    : CurlReadOp(handler, default_handler, url, timeout, op, buffer, sz, logger, callout, header_callout), m_file(file)
    {}

    // Special handling of the first "Pause" operation after the read
    // has started.  Do the correct invocation of success or failure.
    virtual void Pause() override;

private:
    bool m_first_pause{true};
    XrdClCurl::File &m_file;
};

class CurlVectorReadOp : public CurlOperation {
    public:

        CurlVectorReadOp(XrdCl::ResponseHandler *handler, const std::string &url, struct timespec timeout,
            const XrdCl::ChunkList &op_list, XrdCl::Log *logger, CreateConnCalloutType callout, HeaderCallout *header_callout);

        virtual ~CurlVectorReadOp() {}

        bool Setup(CURL *curl, CurlWorker &) override;
        void Fail(uint16_t errCode, uint32_t errNum, const std::string &msg) override;
        void Success() override;
        void ReleaseHandle() override;

        // Set the expected separator between parts of a response;
        // not expected to be used externally except by unit tests.
        void SetSeparator(const std::string &sep) {
            m_headers.SetMultipartSeparator(sep);
        }

        // Set the status code for the operation
        void SetStatusCode(int sc) {m_headers.SetStatusCode(sc);}

        // Invoke the write callback for the vector read.
        //
        // Note: made public to help unit testing of the class; not intended for direct invocation.
        size_t Write(char *buffer, size_t size);

        virtual HttpVerb GetVerb() const override {return HttpVerb::GET;}

    private:
        static size_t WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr);

        // Calculate the next request buffer the current response buffer will service.
        // Sets the m_response_idx and m_skip_bytes
        void CalculateNextBuffer();

    protected:
        size_t m_response_idx{0}; // The offset in the m_chunk_list which the current response chunk will write into.
        off_t m_chunk_buffer_idx{0}; // Current offset in requested chunk where we are writing bytes.
        off_t m_bytes_consumed{0}; // Total number of bytes used for results serving the request.
        uint64_t m_skip_bytes{0}; // Count of bytes to skip in the next response (if response chunk contains unneeded bytes).
        std::string m_response_headers; // Buffer of an incomplete response line from a prior curl write operation.
        std::pair<off_t, off_t> m_current_op{-1, -1}; // The (offset, length) of the current response chunk.
        std::unique_ptr<XrdCl::VectorReadInfo> m_vr; // The response buffers for the client.
        XrdCl::ChunkList m_chunk_list; // The requested chunks from the client.
};

class CurlPgReadOp final : public CurlReadOp {
public:
    CurlPgReadOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
        const std::string &url, struct timespec timeout, const std::pair<uint64_t, uint64_t> &op,
        char *buffer, size_t buffer_size, XrdCl::Log *logger, CreateConnCalloutType callout,
        HeaderCallout *header_callout)
    :
        CurlReadOp(handler, default_handler, url, timeout, op, buffer, buffer_size, logger, callout, header_callout)
    {}

    virtual ~CurlPgReadOp() {}

    void Success() override;

    virtual HttpVerb GetVerb() const override {return HttpVerb::GET;}

};

class CurlListdirOp final : public CurlOperation {
public:
    CurlListdirOp(XrdCl::ResponseHandler *handler, const std::string &url, const std::string &host_addr, bool response_info,
        struct timespec timeout, XrdCl::Log *logger, CreateConnCalloutType callout, HeaderCallout *header_callout);

    virtual ~CurlListdirOp() {}

    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;
    void ReleaseHandle() override;

    virtual HttpVerb GetVerb() const override {return HttpVerb::PROPFIND;}

private:
    struct DavEntry {
        std::string m_name;
        bool m_isdir{false};
        bool m_isexec{false};
        int64_t m_size{-1};
        time_t m_lastmodified{-1};
    };
    // Parses the properties element of a PROPFIND response into a DavEntry object
    //
    // - prop: The properties element to parse
    // - Returns: A pair containing the DavEntry object and a boolean indicating success or not
    bool ParseProp(DavEntry &entry, TiXmlElement *prop);

    // Indicate whether the operation should use the extended "response info" object in response
    const bool m_response_info{false};

    // Parses the response element of a PROPFIND
    std::pair<DavEntry, bool> ParseResponse(TiXmlElement *response);

    // Callback for writing the response body to the internal buffer.
    static size_t WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr);

    // Whether the provided URL is an origin URL (and hence PROPFIND can be done directly).
    bool m_is_origin{false};

    // Response body from the PROPFIND request.
    std::string m_response;

    // Host address (hostname:port) of the data federation
    std::string m_host_addr;
};

// A third-party-copy operation
//
// Invoke the COPY verb to move a file between two HTTP endpoints.
class CurlCopyOp final : public CurlOperation {
public:
    using Headers = std::vector<std::pair<std::string, std::string>>;

    CurlCopyOp(XrdCl::ResponseHandler *handler, const std::string &source_url, const Headers &source_hdrs, const std::string &dest_url, const Headers &dest_hdrs, struct timespec timeout,
        XrdCl::Log *logger, CreateConnCalloutType callout);

    virtual ~CurlCopyOp() {}

    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;
    void ReleaseHandle() override;

    class CurlProgressCallback {
    public:
        virtual ~CurlProgressCallback() {}
        virtual void Progress(off_t bytemark) = 0;
    };

    void SetCallback(std::unique_ptr<CurlProgressCallback> callback);

    virtual HttpVerb GetVerb() const override {return HttpVerb::COPY;}

private:
    // Callback for writing the response body to the internal buffer.
    static size_t WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr);

    // Handle a line of information in the control channel.
    void HandleLine(std::string_view line);

    // Returns true if the control channel has not gotten data recently enough.
    bool ControlChannelTimeoutExpired() const;

    // Source of the TPC transfer
    std::string m_source_url;

    // Buffer of current response line
    std::string m_line_buffer;

    // A callback object for when a performance marker is received
    std::unique_ptr<CurlProgressCallback> m_callback;

    // The performance marker indication of bytes processed.
    off_t m_bytemark{-1};

    // Whether the COPY operation indicated a success status in the control channel:
    bool m_sent_success{false};

    // Failure string sent back in the control channel:
    std::string m_failure;
};

// An upload operation
//
// Invoke a PUT on the remote HTTP server; assumes that Writes are done
// in a single-stream
class CurlPutOp final : public CurlOperation {
public:
    CurlPutOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
        const std::string &url, const char *buffer, size_t buffer_size,
        struct timespec timeout, XrdCl::Log *logger, CreateConnCalloutType callout,
        HeaderCallout *header_callout);
    CurlPutOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
        const std::string &url, XrdCl::Buffer &&buffer,
        struct timespec timeout, XrdCl::Log *logger, CreateConnCalloutType callout,
        HeaderCallout *header_callout);

    virtual ~CurlPutOp() {}

    void Fail(uint16_t errCode, uint32_t errNum, const std::string &msg) override;
    bool Setup(CURL *curl, CurlWorker &) override;
    void Success() override;
    void ReleaseHandle() override;
    bool ContinueHandle() override;

	virtual void SetContinueQueue(std::shared_ptr<XrdClCurl::HandlerQueue> queue) override {
		m_continue_queue = queue;
	}

    // Start continuation of a previously-started operation with additional data.
    //
    // Since the CurlPutOp itself is kept as a reference-counted pointer by the
    // XrdClCurl::File handle, we need to pass a shared pointer to the continue queue.
    // Hence the awkward interface of needing to be provided a shared pointer to oneself.
    bool Continue(std::shared_ptr<CurlOperation> op, XrdCl::ResponseHandler *handler, const char *buffer, size_t buffer_size);
    bool Continue(std::shared_ptr<CurlOperation> op, XrdCl::ResponseHandler *handler, XrdCl::Buffer &&buffer);

    // Pause the put operation; indicates the current buffer was sent successfully
    // but the operation is not yet complete.
    void Pause();

    virtual HttpVerb GetVerb() const override {return HttpVerb::PUT;}

private:

    // Callback function for libcurl when it would like to read data from m_data
    // (and write it to the remote socket).
    static size_t ReadCallback(char *buffer, size_t size, size_t n, void *v);

    // Handle that represents the current operation to libcurl
    CURL *m_curl_handle{nullptr};

    // Reference to the continue queue to use when the operation should be resumed.
    std::shared_ptr<XrdClCurl::HandlerQueue> m_continue_queue;

    // The buffer of data to upload (if the CurlPutOp owns the buffer).
    XrdCl::Buffer m_owned_buffer;

    // The non-owned view of the data to upload.
    // This may reference m_owned_buffer or an externally-owned `const char *`.
    std::string_view m_data;

    // The default handler to invoke if an File::Write operation is not pending.
    // Typically used for timeouts/errors on the PUT operation between client
    // writes.
    std::shared_ptr<XrdCl::ResponseHandler> m_default_handler;

    // File pointer offset
    off_t m_offset{0};

    // The final size of the object to be uploaded; -1 if not known
    off_t m_object_size{-1};

    bool m_final{false};
};

} // namespace XrdClCurl

#endif // XRDCLCURL_CURLOPS_HH
