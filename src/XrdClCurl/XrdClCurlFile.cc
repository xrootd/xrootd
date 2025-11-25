/***************************************************************
 *
 * Copyright (C) 2025, Morgridge Institute for Research
 *
 ***************************************************************/

#include "XrdClCurlFile.hh"
#include "XrdClCurlFilesystem.hh"
#include "XrdClCurlOps.hh"
#include "XrdClCurlParseTimeout.hh"
#include "XrdClCurlResponses.hh"
#include "XrdClCurlUtil.hh"
#include "XrdClCurlWorker.hh"

#include <XrdCl/XrdClConstants.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClStatus.hh>
#include <XrdCl/XrdClURL.hh>
#include <XrdOuc/XrdOucCRC.hh>
#include <XrdSys/XrdSysPageSize.hh>
#include <XrdOuc/XrdOucJson.hh>

#include <charconv>
#include <iostream>

using namespace XrdClCurl;

std::atomic<uint64_t> File::m_prefetch_count = 0;
std::atomic<uint64_t> File::m_prefetch_expired_count = 0;
std::atomic<uint64_t> File::m_prefetch_failed_count = 0;
std::atomic<uint64_t> File::m_prefetch_reads_hit = 0;
std::atomic<uint64_t> File::m_prefetch_reads_miss = 0;
std::atomic<uint64_t> File::m_prefetch_bytes_used = 0;

namespace {

// A response handler for the file open operation when "full download" is requested.
//
// In this case, the open triggers a GET of the entire object with a zero-sized buffer;
// that means the response handler is invoked as soon as the GET response is started.
// Subsequent calls to Read() will return the data from the GET response.
class OpenFullDownloadResponseHandler : public XrdCl::ResponseHandler {
public:
    OpenFullDownloadResponseHandler(bool *is_opened, bool send_response_info, XrdCl::ResponseHandler *handler)
        : m_send_response_info(send_response_info), m_is_opened(is_opened), m_handler(handler)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
        std::unique_ptr<OpenFullDownloadResponseHandler> holder(this);
        std::unique_ptr<XrdCl::AnyObject> response_holder(response);
        std::unique_ptr<XrdCl::XRootDStatus> status_holder(status);

        if (!status || !status->IsOK()) {
            if (m_handler) m_handler->HandleResponse(status_holder.release(), response_holder.release());
            return;
        }
        if (m_is_opened) *m_is_opened = true;
        if (!m_handler) {
            return;
        }
        if (m_send_response_info) {
            XrdCl::ChunkInfo *ci = nullptr;
            response->Get(ci);
            if (!ci) {
                m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInternal, ENOENT, "No ChunkInfo in response"), nullptr);
                return;
            }
            std::unique_ptr<XrdClCurl::ReadResponseInfo> read_response_info(static_cast<XrdClCurl::ReadResponseInfo *>(ci));
            auto info = read_response_info->GetResponseInfo();
            XrdClCurl::OpenResponseInfo *open_info(new XrdClCurl::OpenResponseInfo());
            open_info->SetResponseInfo(std::move(info));
            auto obj = new XrdCl::AnyObject();
            obj->Set(open_info);
            m_handler->HandleResponse(status_holder.release(), obj);
        } else {
            m_handler->HandleResponse(status_holder.release(), nullptr);
        }
    }
private:
    bool m_send_response_info; // If true, the response handler will set the response info object.
    bool *m_is_opened;  // If the file-open is successful, this will be set to true.
    XrdCl::ResponseHandler *m_handler;  // The handler to call with the final result
};

// A response handler for the "normal" open mode (which typically translates
// to a HEAD or PROPFIND).
class OpenResponseHandler : public XrdCl::ResponseHandler {
public:
    OpenResponseHandler(bool *is_opened, XrdCl::ResponseHandler *handler)
        : m_is_opened(is_opened), m_handler(handler)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
        std::unique_ptr<OpenResponseHandler> holder(this);
        std::unique_ptr<XrdCl::AnyObject> response_holder(response);
        std::unique_ptr<XrdCl::XRootDStatus> status_holder(status);

        if (!status || !status->IsOK()) {
            if (m_handler) m_handler->HandleResponse(status_holder.release(), response_holder.release());
            return;
        }
        if (m_is_opened) *m_is_opened = true;
        if (!m_handler) {
            return;
        }
        m_handler->HandleResponse(status_holder.release(), response_holder.release());
    }

private:
    bool *m_is_opened;  // If the file-open is successful, this will be set to true.
    XrdCl::ResponseHandler *m_handler;  // The handler to call with the final result
};

// A response handler that transforms the read result into a PageInfo object.
// This is used for page reads which require a checksum of each page; note
// this is computed client-side whereas for the xroot protocol the checksum is computed server-side.
class PgReadResponseHandler : public XrdCl::ResponseHandler {
public:
    PgReadResponseHandler(XrdCl::ResponseHandler *handler)
        : m_handler(handler)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
        std::unique_ptr<PgReadResponseHandler> holder(this);
        if (!status || !status->IsOK()) {
            if (m_handler) m_handler->HandleResponse(status, response);
            else delete response;
            return;
        }
        if (!m_handler) {
            delete response;
            return;
        }

        // Transform the read result ChunkInfo into a PageInfo.
        XrdCl::ChunkInfo *ci = nullptr;
        response->Get(ci);
        if (!ci) {
            delete response;
            if (m_handler) m_handler->HandleResponse(status, nullptr);
            return;
        }
        std::vector<uint32_t> cksums;
        size_t nbpages = ci->GetLength() / XrdSys::PageSize;
        if (ci->GetLength() % XrdSys::PageSize) ++nbpages;
        cksums.reserve(nbpages);

        auto buffer = static_cast<const char *>(ci->GetBuffer());
        size_t size = ci->GetLength();
        for (size_t pg=0; pg<nbpages; ++pg)
        {
            auto pgsize = static_cast<size_t>(XrdSys::PageSize);
            if (pgsize > size) pgsize = size;
            cksums.push_back(XrdOucCRC::Calc32C(buffer, pgsize));
            buffer += pgsize;
            size -= pgsize;
        }

        auto page_info = new XrdCl::PageInfo(ci->GetOffset(), ci->GetLength(), ci->GetBuffer(), std::move(cksums));
        auto obj = new XrdCl::AnyObject();
        obj->Set(page_info);
        delete response;
        auto handle = m_handler;
        m_handler = nullptr;
        handle->HandleResponse(status, obj);
    }

private:
    XrdCl::ResponseHandler *m_handler;
};

// A response handler for close operations that require creating a zero-length
// object.
class CloseCreateHandler : public XrdCl::ResponseHandler {
public:
    CloseCreateHandler(XrdCl::ResponseHandler *handler)
        : m_handler(handler)
    {}

    virtual void HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw) {
        std::unique_ptr<CloseCreateHandler> self(this);
        std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
        std::unique_ptr<XrdCl::AnyObject> response(response_raw);

        if (m_handler) m_handler->HandleResponse(status.release(), nullptr);
    }

private:
    XrdCl::ResponseHandler *m_handler;
};

} // anonymous namespace

// Note: these values are typically overwritten by `CurlFactory::CurlFactory`;
// they are set here just to avoid uninitialized globals.
struct timespec XrdClCurl::File::m_min_client_timeout = {2, 0};
struct timespec XrdClCurl::File::m_default_header_timeout = {9, 5};
struct timespec XrdClCurl::File::m_fed_timeout = {5, 0};


File::~File() noexcept {
    auto handler = m_put_handler.load(std::memory_order_acquire);
    if (handler) {
        // We must wait for all ongoing writes to complete; the XrdCl::File
        // destructor will trigger a Close() operation when it is called without
        // waiting for the Close to finish, then invoke our destructor.
        // If the Close() is still ongoing, then the handler will receive a
        // callback after its memory is freed.
        handler->WaitForCompletion();
        delete handler;
    }
}

CreateConnCalloutType
File::GetConnCallout() const {
    std::string pointer_str;
    if (!GetProperty("XrdClConnectionCallout", pointer_str) && pointer_str.empty()) {
        return nullptr;
    }
    long long pointer;
    try {
        pointer = std::stoll(pointer_str, nullptr, 16);
    } catch (...) {
        return nullptr;
    }
    if (!pointer) {
        return nullptr;
    }
    return reinterpret_cast<CreateConnCalloutType>(pointer);
}

struct timespec
File::ParseHeaderTimeout(const std::string &timeout_string, XrdCl::Log *logger)
{
    struct timespec ts = File::GetDefaultHeaderTimeout();
    if (!timeout_string.empty()) {
        std::string errmsg;
        // Parse the provided timeout and decrease by a second if we can (if it's below a second, halve it).
        // The thinking is that if the client needs a response in N seconds, then we ought to set the internal
        // timeout to (N-1) seconds to provide enough time for our response to arrive at the client.
        if (!XrdClCurl::ParseTimeout(timeout_string, ts, errmsg)) {
            logger->Error(kLogXrdClCurl, "Failed to parse xrdclcurl.timeout parameter: %s", errmsg.c_str());
        } else if (ts.tv_sec >= 1) {
                ts.tv_sec--;
        } else {
            ts.tv_nsec /= 2;
        }
    }
    const auto mct = File::GetMinimumHeaderTimeout();
    if (ts.tv_sec < mct.tv_sec ||
        (ts.tv_sec == mct.tv_sec && ts.tv_nsec < mct.tv_nsec))
    {
        ts.tv_sec = mct.tv_sec;
        ts.tv_nsec = mct.tv_nsec;
    }

    return ts;
}

struct timespec
File::GetHeaderTimeoutWithDefault(time_t oper_timeout, const struct timespec &header_timeout)
{
    if (oper_timeout == 0) {
        int val = XrdCl::DefaultRequestTimeout;
        XrdCl::DefaultEnv::GetEnv()->GetInt( "RequestTimeout", val );
        oper_timeout = val;
    }
    if (oper_timeout <= 0) {
        return header_timeout;
    }
    if (oper_timeout == header_timeout.tv_sec) {
        return {header_timeout.tv_sec, 0};
    } else if (header_timeout.tv_sec < oper_timeout) {
        return header_timeout;
    } else { // header timeout is larger than the operation timeout
        return {oper_timeout, 0};
    }
}

struct timespec
File::GetHeaderTimeout(time_t oper_timeout) const
{
    return GetHeaderTimeoutWithDefault(oper_timeout, m_header_timeout);
}

std::string
File::GetMonitoringJson()
{
    return "{\"prefetch\": {"
        "\"count\": " + std::to_string(m_prefetch_count) + ","
        "\"expired\": " + std::to_string(m_prefetch_expired_count) + ","
        "\"failed\": " + std::to_string(m_prefetch_failed_count) + ","
        "\"reads_hit\": " + std::to_string(m_prefetch_reads_hit) + ","
        "\"reads_miss\": " + std::to_string(m_prefetch_reads_miss) + ","
        "\"bytes_used\": " + std::to_string(m_prefetch_bytes_used) +
    "}}";
}

XrdCl::XRootDStatus
File::Open(const std::string      &url,
           XrdCl::OpenFlags::Flags flags,
           XrdCl::Access::Mode     mode,
           XrdCl::ResponseHandler *handler,
           timeout_t               timeout)
{
    if (m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "URL %s already open", url.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }

    // Note: workaround for a design flaw of the XrdCl API.
    //
    // Any properties we set on the file *prior* to opening it are sent to the
    // XrdCl base implementation, not the plugin object.  Hence, they are effectively
    // ignored because the later `GetProperty` accesses a different object.  We want
    // the SetProperty calls to take effect because they are needed for successfully
    // `Open`ing the file.  There's no way to "setup the plugin", "set properties", and
    // then "open file" because the first and third operations are part of the same API
    // call.  We thus allow the caller to trigger the plugin loading by doing a special
    // `Open` call (flags set to Compress, access mode None) that is a no-op.
    //
    // Contrast the XrdCl::File plugin loading style with XrdCl::Filesystem; the latter
    // gets a target URL on construction, before any operations are done, allowing
    // the `SetProperty` to work.
    if ((flags == XrdCl::OpenFlags::Compress) && (mode == XrdCl::Access::None) &&
        (handler == nullptr) && (timeout == 0))
    {
        return XrdCl::XRootDStatus();
    }

    m_open_flags = flags;

    m_header_timeout.tv_nsec = m_default_header_timeout.tv_nsec;
    m_header_timeout.tv_sec = m_default_header_timeout.tv_sec;
    auto parsed_url = XrdCl::URL();
    parsed_url.SetPort(0);
    if (!parsed_url.FromString(url)) {
        m_logger->Error(kLogXrdClCurl, "Failed to parse provided URL as a valid URL: %s", url.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs);
    }
    auto pm = parsed_url.GetParams();
    auto iter = pm.find("xrdclcurl.timeout");
    std::string timeout_string = (iter == pm.end()) ? "" : iter->second;
    m_header_timeout = ParseHeaderTimeout(timeout_string, m_logger);
    pm["xrdclcurl.timeout"] = XrdClCurl::MarshalDuration(m_header_timeout);
    parsed_url.SetParams(pm);
    iter = pm.find("oss.asize");
    if (iter != pm.end()) {
        off_t asize;
        auto ec = std::from_chars(iter->second.c_str(), iter->second.c_str() + iter->second.size(), asize);
        if ((ec.ec == std::errc()) && (ec.ptr == iter->second.c_str() + iter->second.size()) && asize >= 0) {
            m_asize = asize;
        } else {
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp, 0, "Unable to parse oss.asize to a valid size");
        }
        pm.erase(iter);
        parsed_url.SetParams(pm);
    }

    m_url = parsed_url.GetURL();
    m_last_url = "";
    m_url_current = "";

    auto ts = GetHeaderTimeout(timeout);

    bool full_download = m_full_download.load(std::memory_order_relaxed);
    m_default_prefetch_handler.reset(new PrefetchDefaultHandler(*this));
    if (full_download) {
        m_default_prefetch_handler->m_prefetch_enabled.store(true, std::memory_order_relaxed);
    }

    if (full_download && !(flags & XrdCl::OpenFlags::Write)) {
        m_logger->Debug(kLogXrdClCurl, "Opening %s in full download mode", m_url.c_str());

        handler = new OpenFullDownloadResponseHandler(&m_is_opened, SendResponseInfo(), handler);
        m_prefetch_size = std::numeric_limits<off_t>::max();
        auto [status, ok] = ReadPrefetch(0, 0, nullptr, handler, timeout, false);
        if (ok) {
            return status;
        } else {
            m_logger->Error(kLogXrdClCurl, "Failed to start prefetch of data at open (URL %s): %s", m_url.c_str(), status.ToString().c_str());
            return status;
        }
    }


    m_logger->Debug(kLogXrdClCurl, "Opening %s (with timeout %lld)", m_url.c_str(), (long long) timeout);

    // This response handler sets the m_is_opened flag to true if the open callback is successfully invoked.
    handler = new OpenResponseHandler(&m_is_opened, handler);

    std::shared_ptr<XrdClCurl::CurlOpenOp> openOp(
        new XrdClCurl::CurlOpenOp(
            handler, GetCurrentURL(), ts, m_logger, this, SendResponseInfo(), GetConnCallout(),
            &m_default_header_callout
        )
    );
    try {
        m_queue->Produce(std::move(openOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add open op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
File::Close(XrdCl::ResponseHandler *handler,
                        timeout_t timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot close.  URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }
    m_is_opened = false;

    std::unique_ptr<XrdCl::XRootDStatus> status(new XrdCl::XRootDStatus{});
    if (m_put_op && !m_put_op->HasFailed()) {
        auto put_size = m_put_offset.load(std::memory_order_relaxed);
        if (m_asize >= 0 && put_size == m_asize) {
            if (put_size == m_asize) {
                m_logger->Debug(kLogXrdClCurl, "Closing a finished file %s", m_url.c_str());
            } else {
                m_logger->Debug(kLogXrdClCurl, "Closing a file %s with partial size (offset %llu, expected %lld)",
                                m_url.c_str(), static_cast<unsigned long long>(put_size), static_cast<long long>(m_asize));
                status.reset(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp,
                    0, "Cannot close file with partial size"));
            }
        } else {
            m_logger->Debug(kLogXrdClCurl, "Flushing final write buffer on close");
            auto put_handler = m_put_handler.load(std::memory_order_acquire);
            if (put_handler) {
                return put_handler->QueueWrite(std::make_pair(nullptr, 0), handler);
            } else {
                m_logger->Error(kLogXrdClCurl, "Internal state error - put operation ongoing without handle");
                return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
            }
        }
    } else if (!m_put_op && m_open_flags & XrdCl::OpenFlags::Write) {
        timespec ts;
        timespec_get(&ts, TIME_UTC);
        ts.tv_sec += timeout;
        m_asize = 0;
        auto handler_wrapper = new PutResponseHandler(new CloseCreateHandler(handler));
        m_put_handler.store(handler_wrapper, std::memory_order_release);
        m_put_op.reset(new XrdClCurl::CurlPutOp(
            handler_wrapper, m_default_put_handler, m_url, nullptr, 0, ts, m_logger,
            GetConnCallout(), &m_default_header_callout
        ));
        handler_wrapper->SetOp(m_put_op);
        m_url_current = "";
        m_last_url = "";
        m_logger->Debug(kLogXrdClCurl, "Creating a zero-sized object at %s for close", m_url.c_str());
        try {
            m_queue->Produce(m_put_op);
        } catch (...) {
            m_put_handler.store(nullptr, std::memory_order_release);
            m_logger->Warning(kLogXrdClCurl, "Failed to add put op to queue");
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
        }
        return {};
    }

    m_logger->Debug(kLogXrdClCurl, "Closed %s", m_url.c_str());
    m_url_current = "";
    m_last_url = "";

    if (handler) {
        handler->HandleResponse(status.release(), nullptr);
    }
    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
File::Stat(bool                    /*force*/,
           XrdCl::ResponseHandler *handler,
           timeout_t               timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot stat.  URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }

    std::string content_length_str;
    int64_t content_length;
    if (!GetProperty("ContentLength", content_length_str)) {
        m_logger->Error(kLogXrdClCurl, "Content length missing for %s", m_url.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }
    try {
        content_length = std::stoll(content_length_str);
    } catch (...) {
        m_logger->Error(kLogXrdClCurl, "Content length not an integer for %s", m_url.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }
    if (content_length < 0) {
        m_logger->Error(kLogXrdClCurl, "Content length negative for %s", m_url.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidResponse);
    }

    m_logger->Debug(kLogXrdClCurl, "Successful stat operation on %s (size %lld)", m_url.c_str(), static_cast<long long>(content_length));
    auto stat_info = new XrdCl::StatInfo("nobody", content_length,
        XrdCl::StatInfo::Flags::IsReadable, time(NULL));
    auto obj = new XrdCl::AnyObject();
    obj->Set(stat_info);

    handler->HandleResponse(new XrdCl::XRootDStatus(), obj);
    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
File::Fcntl(const XrdCl::Buffer &arg, XrdCl::ResponseHandler *handler,
           timeout_t               timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot run fcntl.  URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }

    auto obj = new XrdCl::AnyObject();
    std::string as = arg.ToString();
    try
    {
        XrdCl::QueryCode::Code code = (XrdCl::QueryCode::Code)std::stoi(as);
        if (code == XrdCl::QueryCode::XAttr)
        {
            nlohmann::json xatt;
            std::string etagRes;
            if (GetProperty("ETag", etagRes))
            {
                xatt["ETag"] = etagRes;
            }
            std::string cc;
            if (GetProperty("Cache-Control", cc))
            {
                if (cc.find("must-revalidate") != std::string::npos)
                {
                    xatt["revalidate"] = true;
                }
                size_t fm = cc.find("max-age=");
                if (fm != std::string::npos)
                {
                    fm += 9; // idx of the first character after the make-age= match
                    for (size_t i = fm; i < cc.length(); i++)
                    {
                        if (!std::isdigit(cc[i]))
                        {
                            std::string sa = cc.substr(fm, i);
                            long int a = std::stol(sa);
                            time_t t = time(NULL) + a;
                            xatt["expire"] = t;
                            break;
                        }
                    }
                }
            }
            XrdCl::Buffer *respBuff = new XrdCl::Buffer();
            m_logger->Debug(kLogXrdClCurl, "Fcntl conent %s", xatt.dump().c_str());
            respBuff->FromString(xatt.dump());
            obj->Set(respBuff);
        }
        //
        //   Query codes supported by  XrdCl::File::Fctnl
        //
        else {
            std::string msg;
            XrdCl::XRootDStatus status = XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
            switch (code) {
            case XrdCl::QueryCode::Stats:
                msg = "Server status query not supported.";
                break;
            case XrdCl::QueryCode::Checksum: // fallthrough
            case XrdCl::QueryCode::ChecksumCancel:
                msg = "Checksum query not supported.";
                break;
            case XrdCl::QueryCode::Config:
                msg = "Server configuration query not supported.";
                break;
            case XrdCl::QueryCode::Space:
                msg = "Local space stats query not supported.";
                break;
            case XrdCl::QueryCode::Opaque: // fallthrough
            case XrdCl::QueryCode::OpaqueFile:
                // XrdCl implementation dependent
                msg = "Opaque query not supported.";
                break;
            case XrdCl::QueryCode::Prepare:
                msg = "Prepare status query not supported.";
                break;
            default:
                msg = "Invalid information query type code";
            }
            m_logger->Error(kLogXrdClCurl, "%s", msg.c_str());
            return status;
        }
    }
    catch (const std::exception& e)
    {
        m_logger->Warning(kLogXrdClCurl, "Failed to parse query code %s", e.what());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errDataError);
    }

    handler->HandleResponse(new XrdCl::XRootDStatus(), obj);
    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
File::Read(uint64_t                offset,
           uint32_t                size,
           void                   *buffer,
           XrdCl::ResponseHandler *handler,
           timeout_t               timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot read.  URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }
    auto [status, ok] = ReadPrefetch(offset, size, buffer, handler, timeout, false);
    if (ok) {
        if (status.IsOK()) {
            m_logger->Debug(kLogXrdClCurl, "Read %s (%d bytes at offset %lld) will be served from prefetch handler", m_url.c_str(), size, static_cast<long long>(offset));
        } else {
            m_logger->Warning(kLogXrdClCurl, "Read %s (%d bytes at offset %lld) failed: %s", m_url.c_str(), size, static_cast<long long>(offset), status.GetErrorMessage().c_str());
        }
        return status;
    } else if (m_full_download.load(std::memory_order_relaxed)) {
        std::unique_lock lock(m_default_prefetch_handler->m_prefetch_mutex);
        if (m_prefetch_op && m_prefetch_op->IsDone() && (static_cast<off_t>(offset) == m_prefetch_offset.load(std::memory_order_acquire))) {
            if (handler) {
                auto ci = new XrdCl::ChunkInfo(offset, 0, buffer);
                auto obj = new XrdCl::AnyObject();
                obj->Set(ci);
                handler->HandleResponse(new XrdCl::XRootDStatus{}, obj);
            }
            return XrdCl::XRootDStatus{};
        }
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp, 0, "Non-sequential read detected when in full-download mode");
    }

    auto ts = GetHeaderTimeout(timeout);
    auto url = GetCurrentURL();
    m_logger->Debug(kLogXrdClCurl, "Read %s (%d bytes at offset %lld with timeout %lld)", url.c_str(), size, static_cast<long long>(offset), static_cast<long long>(ts.tv_sec));

    std::shared_ptr<XrdClCurl::CurlReadOp> readOp(
        new XrdClCurl::CurlReadOp(
            handler, m_default_prefetch_handler, url, ts, std::make_pair(offset, size),
            static_cast<char*>(buffer), size, m_logger,
           GetConnCallout(), &m_default_header_callout
        )
    );
    try {
        m_queue->Produce(std::move(readOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add read op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
}

std::tuple<XrdCl::XRootDStatus, bool>
File::ReadPrefetch(uint64_t offset, uint64_t size, void *buffer, XrdCl::ResponseHandler *handler, timeout_t timeout, bool isPgRead)
{
    // Check if prefetching is enabled; if not, return early.
    auto prefetch_enabled = m_default_prefetch_handler->m_prefetch_enabled.load(std::memory_order_relaxed);
    if (!prefetch_enabled) {
        m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
        m_logger->Dump(kLogXrdClCurl, "%sRead prefetch skipping due to prefetching being disabled", isPgRead ? "Pg": "");
        return std::make_tuple(XrdCl::XRootDStatus{}, false);
    }
    std::unique_lock lock(m_default_prefetch_handler->m_prefetch_mutex);
    if (m_prefetch_size == -1) {
        m_logger->Debug(kLogXrdClCurl, "%sRead prefetch skipping due to unknown file size", isPgRead ? "Pg": "");
        m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
        m_default_prefetch_handler->m_prefetch_enabled = false;
    }
    prefetch_enabled = m_default_prefetch_handler->m_prefetch_enabled;
    if (!prefetch_enabled) {
        m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
        return std::make_tuple(XrdCl::XRootDStatus{}, false);
    }

    if (isPgRead) {
        handler = new PgReadResponseHandler(handler);
    }

    auto url = GetCurrentURL();
    if (!m_prefetch_op) {
        auto ts = GetHeaderTimeout(timeout);
        if (m_prefetch_size == INT64_MAX) {
            m_logger->Debug(kLogXrdClCurl, "%sRead %s (%llu bytes at offset %lld with timeout %lld; starting prefetch full object)", isPgRead ? "Pg" : "", url.c_str(), static_cast<unsigned long long>(size), static_cast<long long>(offset), static_cast<long long>(ts.tv_sec));
        } else {
            m_logger->Debug(kLogXrdClCurl, "%sRead %s (%llu bytes at offset %lld with timeout %lld; starting prefetch of size %lld)", isPgRead ? "Pg" : "", url.c_str(), static_cast<unsigned long long>(size), static_cast<long long>(offset), static_cast<long long>(ts.tv_sec), static_cast<long long>(m_prefetch_size));
        }

        try {
            // Note we don't set m_last_prefetch_handler here; the constructor will do this automatically if necessary.
            new PrefetchResponseHandler(*this, offset, size, &m_prefetch_offset, static_cast<char *>(buffer), handler, nullptr, timeout);
        } catch (std::runtime_error &exc) {
            m_logger->Warning(kLogXrdClCurl, "Failed to create prefetch response handler: %s", exc.what());
            m_default_prefetch_handler->m_prefetch_enabled = false;
            m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
            return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError), true);
        }

        // If we are prefetching as part of an open (i.e., a "full download"), there's special handling logic
        // to pass along the response headers as file properties.
        m_prefetch_op.reset(
            m_is_opened ?
            new XrdClCurl::CurlReadOp(
                m_last_prefetch_handler, m_default_prefetch_handler, url, ts, std::make_pair(offset, m_prefetch_size),
                static_cast<char*>(buffer), size, m_logger,
                GetConnCallout(), &m_default_header_callout
            )
            :
            new XrdClCurl::CurlPrefetchOpenOp(
                *this, m_last_prefetch_handler, m_default_prefetch_handler, url, ts,
                std::make_pair(offset, m_prefetch_size), static_cast<char*>(buffer), size, m_logger,
                GetConnCallout(), &m_default_header_callout
            )
        );
        lock.unlock();
        m_prefetch_count.fetch_add(1, std::memory_order_relaxed);
        m_prefetch_reads_hit.fetch_add(1, std::memory_order_relaxed);
        m_prefetch_offset.store(offset + size, std::memory_order_release);
        try {
            m_queue->Produce(m_prefetch_op);
        } catch (...) {
            m_logger->Warning(kLogXrdClCurl, "Failed to add prefetch read op to queue");
            lock.lock();
            m_prefetch_op.reset();
            m_default_prefetch_handler->m_prefetch_enabled = false;
            m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
            return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError), true);
        }
        return std::make_tuple(XrdCl::XRootDStatus{}, true);
    }
    if (m_prefetch_op->IsDone()) {
        // Prefetch operation has completed (maybe failed); cannot re-use it.
        m_default_prefetch_handler->m_prefetch_enabled = false;
        m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
        m_logger->Dump(kLogXrdClCurl, "%sRead prefetch skipping due to prefetching being already complete", isPgRead ? "Pg": "");
        return std::make_tuple(XrdCl::XRootDStatus{}, false);
    }

    auto expected_offset = static_cast<off_t>(offset);
    if (!m_prefetch_offset.compare_exchange_strong(expected_offset, static_cast<off_t>(offset + size), std::memory_order_acq_rel)) {
        // Out-of-order read; can't handle the prefetch.
        m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
        m_logger->Dump(kLogXrdClCurl, "%sRead prefetch skipping due to out-of-order reads (requested %lld; current offset %lld)", isPgRead ? "Pg": "", static_cast<long long>(offset), static_cast<long long>(expected_offset));
        return std::make_tuple(XrdCl::XRootDStatus{}, false);
    }
    if (m_logger->GetLevel() >= XrdCl::Log::LogLevel::DebugMsg) {
        m_logger->Debug(kLogXrdClCurl, "%sRead %s (%llu bytes at offset %lld; using ongoing prefetch)", isPgRead ? "Pg" : "", GetCurrentURL().c_str(), static_cast<unsigned long long>(size), static_cast<long long>(offset));
    }
    try {
        // Notice we don't set m_last_prefetch_handler here; as soon as the constructor is invoked, another thread could have
        // invoked the handler's callback and deleted it.
        new PrefetchResponseHandler(*this, offset, size, &m_prefetch_offset, static_cast<char *>(buffer), handler, &lock, timeout);
    } catch (std::runtime_error &exc) {
        m_logger->Warning(kLogXrdClCurl, "Failed to create prefetch response handler: %s", exc.what());
        m_default_prefetch_handler->m_prefetch_enabled = false;
        m_prefetch_reads_miss.fetch_add(1, std::memory_order_relaxed);
        return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError), true);
    }

    return std::make_tuple(XrdCl::XRootDStatus{}, true);
}

XrdCl::XRootDStatus
File::VectorRead(const XrdCl::ChunkList &chunks,
                 void                   *buffer,
                 XrdCl::ResponseHandler *handler,
                 timeout_t               timeout )
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot do vector read: URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    } else if (m_full_download.load(std::memory_order_relaxed)) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp, 0, "Only sequential reads are supported when in full-download mode");
    }
    if (chunks.empty()) {
        if (handler) {
            auto status = new XrdCl::XRootDStatus();
            auto vr = std::make_unique<XrdCl::VectorReadInfo>();
            vr->SetSize(0);
            auto obj = new XrdCl::AnyObject();
            obj->Set(vr.release());
            handler->HandleResponse(status, obj);
        }
        return XrdCl::XRootDStatus();
    }

    auto ts = GetHeaderTimeout(timeout);
    auto url = GetCurrentURL();
    m_logger->Debug(kLogXrdClCurl, "Read %s (%lld chunks; first chunk is %u bytes at offset %lld with timeout %lld)", url.c_str(), static_cast<long long>(chunks.size()), static_cast<unsigned>(chunks[0].GetLength()), static_cast<long long>(chunks[0].GetOffset()), static_cast<long long>(ts.tv_sec));

    std::shared_ptr<XrdClCurl::CurlVectorReadOp> readOp(
        new XrdClCurl::CurlVectorReadOp(
            handler, url, ts, chunks, m_logger, GetConnCallout(), &m_default_header_callout
        )
    );
    try {
        m_queue->Produce(std::move(readOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add vector read op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
File::Write(uint64_t                offset,
            uint32_t                size,
            const void             *buffer,
            XrdCl::ResponseHandler *handler,
            timeout_t               timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot write: URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    } else if (m_full_download.load(std::memory_order_relaxed)) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp, 0, "Only sequential reads are supported when in full-download mode");
    }
    m_default_prefetch_handler->DisablePrefetch();

    auto ts = GetHeaderTimeout(timeout);
    auto url = GetCurrentURL();
    m_logger->Debug(kLogXrdClCurl, "Write %s (%d bytes at offset %lld with timeout %lld)", url.c_str(), size, static_cast<long long>(offset), static_cast<long long>(ts.tv_sec));

    auto handler_wrapper = m_put_handler.load(std::memory_order_relaxed);
    if (!handler_wrapper) {
        handler_wrapper = new PutResponseHandler(handler);
        PutResponseHandler *expected_value = nullptr;
        if (!m_put_handler.compare_exchange_strong(expected_value, handler_wrapper, std::memory_order_acq_rel)) {
            delete handler_wrapper;
            return expected_value->QueueWrite(std::make_pair(buffer, size), handler);
        }

        if (offset != 0) {
            m_put_handler.store(nullptr, std::memory_order_release);
            delete handler_wrapper;
            m_logger->Warning(kLogXrdClCurl, "Cannot start PUT operation at non-zero offset");
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, 0, "HTTP uploads must start at offset 0");
        }
        m_put_op.reset(new XrdClCurl::CurlPutOp(
            handler_wrapper, m_default_put_handler, url, static_cast<const char*>(buffer), size, ts, m_logger,
            GetConnCallout(), &m_default_header_callout
        ));
        handler_wrapper->SetOp(m_put_op);
        m_put_offset.fetch_add(size, std::memory_order_acq_rel);
        try {
            m_queue->Produce(m_put_op);
        } catch (...) {
            m_put_handler.store(nullptr, std::memory_order_release);
            delete handler_wrapper;
            m_logger->Warning(kLogXrdClCurl, "Failed to add put op to queue");
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
        }
        return XrdCl::XRootDStatus();
    }

    auto old_offset = m_put_offset.fetch_add(size, std::memory_order_acq_rel);
    if (static_cast<off_t>(offset) != old_offset) {
        m_put_offset.fetch_sub(size, std::memory_order_acq_rel);
        m_logger->Warning(kLogXrdClCurl, "Requested write offset at %lld does not match current file descriptor offset at %lld",
            static_cast<long long>(offset), static_cast<long long>(old_offset));
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, 0, "Requested write offset does not match current offset");
    }
    return handler_wrapper->QueueWrite(std::make_pair(buffer, size), handler);
}

XrdCl::XRootDStatus
File::Write(uint64_t                offset,
            XrdCl::Buffer         &&buffer,
            XrdCl::ResponseHandler *handler,
            timeout_t               timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot write: URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }
    m_default_prefetch_handler->DisablePrefetch();

    auto ts = GetHeaderTimeout(timeout);
    auto url = GetCurrentURL();
    m_logger->Debug(kLogXrdClCurl, "Write %s (%d bytes at offset %lld with timeout %lld)", url.c_str(), static_cast<int>(buffer.GetSize()), static_cast<long long>(offset), static_cast<long long>(ts.tv_sec));

    auto handler_wrapper = m_put_handler.load(std::memory_order_relaxed);
    if (!handler_wrapper) {
        handler_wrapper = new PutResponseHandler(handler);
        PutResponseHandler *expected_value = nullptr;
        if (!m_put_handler.compare_exchange_strong(expected_value, handler_wrapper, std::memory_order_acq_rel)) {
            delete handler_wrapper;
            return expected_value->QueueWrite(std::move(buffer), handler);
        }

        if (offset != 0) {
            m_put_handler.store(nullptr, std::memory_order_release);
            delete handler_wrapper;
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, 0, "HTTP uploads must start at offset 0");
        }
        m_put_op.reset(new XrdClCurl::CurlPutOp(
            handler_wrapper, m_default_put_handler, url, std::move(buffer), ts, m_logger,
            GetConnCallout(), &m_default_header_callout
        ));
        handler_wrapper->SetOp(m_put_op);
        m_put_offset.fetch_add(buffer.GetSize(), std::memory_order_acq_rel);
        try {
            m_queue->Produce(m_put_op);
        } catch (...) {
            m_put_handler.store(nullptr, std::memory_order_release);
            delete handler_wrapper;
            m_logger->Warning(kLogXrdClCurl, "Failed to add put op to queue");
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
        }
        return XrdCl::XRootDStatus();
    }

    auto old_offset = m_put_offset.fetch_add(buffer.GetSize(), std::memory_order_acq_rel);
    if (static_cast<off_t>(offset) != old_offset) {
        m_put_offset.fetch_sub(buffer.GetSize(), std::memory_order_acq_rel);
        m_logger->Warning(kLogXrdClCurl, "Requested write offset at %lld does not match current file descriptor offset at %lld",
            static_cast<long long>(offset), static_cast<long long>(old_offset));
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, 0, "Requested write offset does not match current offset");
    }
    return handler_wrapper->QueueWrite(std::move(buffer), handler);
}

XrdCl::XRootDStatus
File::PgRead(uint64_t                offset,
             uint32_t                size,
             void                   *buffer,
             XrdCl::ResponseHandler *handler,
             timeout_t               timeout)
{
    if (!m_is_opened) {
        m_logger->Error(kLogXrdClCurl, "Cannot pgread.  URL isn't open");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }
    auto [status, ok] = ReadPrefetch(offset, size, buffer, handler, timeout, true);
    if (ok) {
        if (status.IsOK()) {
            m_logger->Debug(kLogXrdClCurl, "PgRead %s (%d bytes at offset %lld) will be served from prefetch handler", m_url.c_str(), size, static_cast<long long>(offset));
        } else {
            m_logger->Warning(kLogXrdClCurl, "PgRead %s (%d bytes at offset %lld) failed: %s", m_url.c_str(), size, static_cast<long long>(offset), status.GetErrorMessage().c_str());
        }
        return status;
    } else if (m_full_download.load(std::memory_order_relaxed)) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp, 0, "Non-sequential read detected when in full-download mode");
    }

    auto ts = GetHeaderTimeout(timeout);
    auto url = GetCurrentURL();
    m_logger->Debug(kLogXrdClCurl, "PgRead %s (%d bytes at offset %lld)", url.c_str(), size, static_cast<long long>(offset));

    std::shared_ptr<XrdClCurl::CurlPgReadOp> readOp(
        new XrdClCurl::CurlPgReadOp(
            handler, m_default_prefetch_handler, url, ts, std::make_pair(offset, size),
            static_cast<char*>(buffer), size, m_logger,
            GetConnCallout(), &m_default_header_callout
        )
    );

    try {
        m_queue->Produce(std::move(readOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add read op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
}

bool
File::IsOpen() const
{
    return m_is_opened;
}

bool
File::GetProperty(const std::string &name,
                        std::string &value) const
{
    if (name == "CurrentURL") {
        value = GetCurrentURL();
        return true;
    }

    if (name == "IsPrefetching") {
        value = m_default_prefetch_handler->IsPrefetching() ? "true" : "false";
        return true;
    }

    std::shared_lock lock(m_properties_mutex);
    if (name == "LastURL") {
        value = m_last_url;
        return true;
    }

    const auto p = m_properties.find(name);
    if (p == std::end(m_properties)) {
        return false;
    }

    value = p->second;
    return true;
}

bool File::SendResponseInfo() const {
    std::string val;
    return GetProperty(ResponseInfoProperty, val) && val == "true";
}

bool
File::SetProperty(const std::string &name,
                  const std::string &value)
{
    if (name == "XrdClCurlHeaderCallout") {
        long long pointer;
        try {
            pointer = std::stoll(value, nullptr, 16);
        } catch (...) {
            pointer = 0;
        }
        m_header_callout.store(reinterpret_cast<XrdClCurl::HeaderCallout*>(pointer), std::memory_order_release);
    } else if (name == "XrdClCurlFullDownload") {
        if (value == "true") {
            auto prefetch_handler = m_default_prefetch_handler;
            if (prefetch_handler) {
                std::unique_lock lock(prefetch_handler->m_prefetch_mutex);
                prefetch_handler->m_prefetch_enabled.store(true, std::memory_order_relaxed);
            }
            m_full_download.store(true, std::memory_order_relaxed);
        }
    }

    std::unique_lock lock(m_properties_mutex);

    m_properties[name] = value;
    if (name == "LastURL") {
        m_last_url = value;
        m_url_current = "";
    }
    else if (name == "XrdClCurlQueryParam") {
        CalculateCurrentURL(value);
    }
    else if (name == "XrdClCurlMaintenancePeriod") {
        unsigned period;
        auto ec = std::from_chars(value.c_str(), value.c_str() + value.size(), period);
        if ((ec.ec == std::errc()) && (ec.ptr == value.c_str() + value.size()) && period > 0) {
            m_logger->Debug(kLogXrdClCurl, "Setting maintenance period to %u", period);
            CurlWorker::SetMaintenancePeriod(period);
        }
    }
    else if (name == "XrdClCurlStallTimeout") {
        std::string errmsg;
        timespec ts;
        if (!ParseTimeout(value, ts, errmsg)) {
            m_logger->Debug(kLogXrdClCurl, "Failed to parse timeout value (%s): %s", value.c_str(), errmsg.c_str());
        } else {
            CurlOperation::SetStallTimeout(std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec});
        }
    }
    else if (name == "XrdClCurlPrefetchSize") {
        off_t size;
        auto ec = std::from_chars(value.c_str(), value.c_str() + value.size(), size);
        if ((ec.ec == std::errc()) && (ec.ptr == value.c_str() + value.size())) {
            lock.unlock();
            std::unique_lock lock2(m_default_prefetch_handler->m_prefetch_mutex);
            m_prefetch_size = size;
        } else {
            m_logger->Debug(kLogXrdClCurl, "XrdClCurlPrefetchSize value (%s) was not parseable", value.c_str());
        }
    }
    return true;
}

const std::string
File::GetCurrentURL() const {
    {
        std::shared_lock lock(m_properties_mutex);

        if (!m_url_current.empty()) {
            return m_url_current;
        } else if (m_url.empty() && m_last_url.empty()) {
            return "";
        }
    }
    std::unique_lock lock(m_properties_mutex);

    auto iter = m_properties.find("XrdClCurlQueryParam");
    if (iter == m_properties.end()) {
        return m_last_url.empty() ? m_url : m_last_url;
    }
    CalculateCurrentURL(iter->second);

    return m_url_current;
}

void
File::CalculateCurrentURL(const std::string &value) const {
    const auto &last_url = m_last_url.empty() ? m_url : m_last_url;
    if (value.empty()) {
        m_url_current = last_url;
    } else {
        auto loc = last_url.find('?');
        if (loc == std::string::npos) {
            m_url_current = last_url + '?' + value;
        } else {
            XrdCl::URL url(last_url);
            auto map = url.GetParams(); // Make a copy of the pre-existing parameters
            url.SetParams(value); // Parse the new value
            auto update_map = url.GetParams();
            for (const auto &entry : map) {
                if (update_map.find(entry.first) == update_map.end()) {
                    update_map[entry.first] = entry.second;
                }
            }
            bool first = true;
            std::stringstream ss;
            for (const auto &entry : update_map) {
                ss << (first ? "?" : "&") << entry.first << "=" << entry.second;
                first = false;
            }
            m_url_current = last_url.substr(0, loc) + ss.str();
        }
    }
}

File::PrefetchResponseHandler::PrefetchResponseHandler(
    File &parent, off_t offset, size_t size, std::atomic<off_t> *prefetch_offset, char *buffer,
    XrdCl::ResponseHandler *handler, std::unique_lock<std::mutex> *lock, timeout_t timeout
)
    : m_parent(parent),
    m_handler(handler),
    m_buffer(buffer),
    m_size(size),
    m_offset(offset),
    m_prefetch_offset(prefetch_offset),
    m_timeout(timeout)
{
    if (parent.m_last_prefetch_handler) {
        parent.m_last_prefetch_handler->m_next = this;
        parent.m_last_prefetch_handler = this;
    } else {
        m_parent.m_last_prefetch_handler = this;
        // If lock is nullptr, then we are guaranteed that this is called during the creation
        // of the m_prefetch_op and can skip this check.
        if (lock && m_parent.m_prefetch_op) {
            // If continuing the prefetch operation fails, then the failure callback
            // will be invoked; the callback requires the mutex and hence we need to unlock it
            // here to avoid a deadlock.
            lock->unlock();
            if (!parent.m_prefetch_op->Continue(parent.m_prefetch_op, this, buffer, size)) {
                lock->lock();
                // As soon as we unlock the lock, another thread could have used finished the
                // operation (which deletes the object); we must be careful to not touch the
                // object (reference m_*) in the meantime.
                if (parent.m_last_prefetch_handler == this)
                    parent.m_last_prefetch_handler = nullptr;
                throw std::runtime_error("Failed to continue prefetch operation");
            }
        }
    }
}

void
File::PrefetchResponseHandler::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
    // Ensure that we are deleted once the callback is done.
    std::unique_ptr<PrefetchResponseHandler> owner(this);

    bool mismatched_size = false;
    if (status) {
        if (status->IsOK() && response) {
            XrdCl::ChunkInfo *ci = nullptr;
            response->Get(ci);
            if (ci) {
                auto missing_bytes = m_size - ci->GetLength();
                if (missing_bytes) {
                    mismatched_size = true;
                    m_prefetch_offset->fetch_sub(missing_bytes, std::memory_order_relaxed);
                }
                m_prefetch_bytes_used.fetch_add(ci->GetLength(), std::memory_order_relaxed);
            }
        } else if (!status->IsOK()) {
            m_prefetch_failed_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    PrefetchResponseHandler *next;
    {
        std::unique_lock lock(m_parent.m_default_prefetch_handler->m_prefetch_mutex);
        next = m_next;
    }
    if (next) {
        if (status && status->IsOK() && !mismatched_size) {
            m_parent.m_prefetch_op->Continue(m_parent.m_prefetch_op, next, next->m_buffer, next->m_size);
        } else {
            // On failure resubmit subsequent operations.
            // All the subsequent ops also depend on us having the expected read length (otherwise the
            // file offsets are incorrect).  If there's a mismatched read size (shorter actual bytes available
            // than what is originally requested), then that's another sign of potential issue and we disable
            // the prefetch mechanism.
            m_parent.m_default_prefetch_handler->DisablePrefetch();
            next->ResubmitOperation();
        }
    }

    {
        std::unique_lock lock(m_parent.m_default_prefetch_handler->m_prefetch_mutex);
        if (m_parent.m_last_prefetch_handler == this) {
            m_parent.m_last_prefetch_handler = nullptr;
        }
        if (!status || !status->IsOK()) {
            m_parent.m_prefetch_op.reset();
            m_parent.m_default_prefetch_handler->m_prefetch_enabled = false;
        }
    }

    if (m_handler) m_handler->HandleResponse(status, response);
    else delete response;
}

void
File::PrefetchResponseHandler::ResubmitOperation()
{
    m_parent.m_logger->Debug(kLogXrdClCurl, "Resubmitting waiting prefetch operations as new reads due to prefetch failure");
    PrefetchResponseHandler *next = this;
    while (next) {
        auto cur = next;
        auto st = next->m_parent.Read(next->m_offset, next->m_size, next->m_buffer, next->m_handler, next->m_timeout);
        if (!st.IsOK() && next->m_handler) {
            next->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
        }
        {
            std::unique_lock lock(next->m_parent.m_default_prefetch_handler->m_prefetch_mutex);
            next = next->m_next;
        }
        delete cur;
    }
}

void
File::PrefetchDefaultHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw) {
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    if (status && !status->IsOK()) {
        if ((status->code == XrdCl::errOperationExpired) && (status->GetErrorMessage().find("Transfer stalled for too long") != std::string::npos)) {
            m_prefetch_expired_count.fetch_add(1, std::memory_order_relaxed);
            m_logger->Debug(kLogXrdClCurl, "Prefetch data for %s went unused; disabling.", m_url.c_str());
        } else {
            m_prefetch_failed_count.fetch_add(1, std::memory_order_relaxed);
            m_logger->Warning(kLogXrdClCurl, "Disabling prefetch of %s due to error: %s", m_url.c_str(), status->ToStr().c_str());
        }
    }
    DisablePrefetch();
}

void
File::PutDefaultHandler::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
    delete response;
    if (status) {
        m_logger->Warning(kLogXrdClCurl, "Failing future write calls due to error: %s", status->ToStr().c_str());
        delete status;
    }
}

std::shared_ptr<XrdClCurl::HeaderCallout::HeaderList>
File::HeaderCallout::GetHeaders(const std::string &verb,
                                const std::string &url,
                                const HeaderList &headers)
{
    auto parent_callout = m_parent.m_header_callout.load(std::memory_order_acquire);
    std::shared_ptr<std::vector<std::pair<std::string, std::string>>> result_headers;
    if (parent_callout != nullptr) {
        result_headers = parent_callout->GetHeaders(verb, url, headers);
    } else {
        result_headers.reset(new std::vector<std::pair<std::string, std::string>>{});
        for (const auto & info : headers) {
            result_headers->emplace_back(info.first, info.second);
        }
    }
    if (m_parent.m_asize >= 0 && verb == "PUT") {
        if (!result_headers) {
            result_headers.reset(new std::vector<std::pair<std::string, std::string>>{});
        }
        auto iter = std::find_if(result_headers->begin(), result_headers->end(),
            [](const auto &pair) { return !strcasecmp(pair.first.c_str(), "Content-Length"); });
        if (iter == result_headers->end()) {
            result_headers->emplace_back("Content-Length", std::to_string(m_parent.m_asize));
        }
    } else if (!result_headers) {
        result_headers.reset(new std::vector<std::pair<std::string, std::string>>{});
    }
    return result_headers;
}

File::PutResponseHandler::PutResponseHandler(XrdCl::ResponseHandler *handler)
    : m_active_handler(handler)
{}

void
File::PutResponseHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw)
{
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);

    // Note: if the handler owns the file object (as in the case of Pelican's writeback
    // response handler), then the callback may cause the file to be deleted - and hence
    // this instance of PutResponseHandler to be deleted.  However, if m_active is true,
    // the destructor will wait until it's set to false; that cannot occur until ProcessQueue
    // is invoked.
    //
    // Hence, we must ensure that ProcessQueue is called before the callback handler, which
    // may either set m_active to false or generate work in separate threads, allowing the
    // work to proceed and avoiding a deadlocked thread.
    auto current_handler = m_active_handler;
    if (ProcessQueue() && current_handler) {
        current_handler->HandleResponse(status.release(), response.release());
    }
}

XrdCl::Status
File::PutResponseHandler::QueueWrite(std::variant<std::pair<const void *, size_t>, XrdCl::Buffer> buffer, XrdCl::ResponseHandler *handler)
{
    if (m_op->HasFailed()) {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp, 0, "Cannot continue writing to open file after error");
    }
    std::lock_guard<std::mutex> lg(m_mutex);
    if (!m_active) {
        m_active = true;
        m_active_handler = handler;
        if (std::holds_alternative<XrdCl::Buffer>(buffer)) {
            if (!m_op->Continue(m_op, this, std::move(std::get<XrdCl::Buffer>(buffer)))) {
                m_active = false;
                m_cv.notify_all();
                return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError, ENOSPC, "Cannot continue PUT operation");
            }
        } else {
            auto buffer_info = std::get<std::pair<const void *, size_t>>(buffer);
            if (!m_op->Continue(m_op, this, static_cast<const char *>(buffer_info.first), buffer_info.second)) {
                m_active = false;
                m_cv.notify_all();
                return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError, ENOSPC, "Cannot continue PUT operation");
            }
        }
    } else {
        m_pending_writes.emplace_back(std::move(buffer), handler);
    }
    return XrdCl::Status{};
}

// Start the next pending write operation.
bool
File::PutResponseHandler::ProcessQueue() {
    std::lock_guard<std::mutex> lg(m_mutex);
    if (m_pending_writes.empty()) {
        // No pending writes; mark the operation as inactive.
        m_active = false;
        m_active_handler = nullptr;
        m_cv.notify_all();
        return true;
    }

    // Start the next pending write.
    auto & [buffer, handler] = m_pending_writes.front();
    bool rv;
    m_active_handler = handler;
    if (std::holds_alternative<XrdCl::Buffer>(buffer)) {
        rv = m_op->Continue(m_op, this, std::move(std::get<XrdCl::Buffer>(buffer)));
    } else {
        auto buffer_info = std::get<std::pair<const void *, size_t>>(buffer);
        rv = m_op->Continue(m_op, this, static_cast<const char *>(buffer_info.first), buffer_info.second);
    }
    m_pending_writes.pop_front();
    if (!rv) {
        // The continuation failed; mark the operation as inactive and
        // invoke all pending handlers with the error.
        if (m_active_handler) {
            m_active_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError, ENOSPC, "Cannot continue PUT operation"), nullptr);
        }
        for (auto& [_, h] : m_pending_writes) {
            if (h) {
                h->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError, ENOSPC, "Cannot continue PUT operation"), nullptr);
            }
        }
        m_active = false;
        m_cv.notify_all();
        return false;
    }
    return true;
}

void
File::PutResponseHandler::WaitForCompletion() {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [&]{return !m_active;});
}
