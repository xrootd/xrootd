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

#include "XrdClHttpFactory.hh"
#include "XrdClHttpFilesystem.hh"
#include "XrdClHttpOps.hh"
#include "XrdClHttpResponses.hh"
#include "XrdClHttpTokenFileParser.hh"

#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPropertyList.hh"
#include "XProtocol/XProtocol.hh"

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

using namespace XrdClHttp;
using namespace std::string_literals;

namespace
{

// Blocks the calling thread until the operation it is given to completes.
class SyncResponseHandler final : public XrdCl::ResponseHandler
{
public:
    // The operation gives the ownership of both objects to this handler. The
    // result of the operation comes from the operation itself, thus this
    // handler reads neither of them and releases them immediately.
    void HandleResponse(XrdCl::XRootDStatus *status,
                        XrdCl::AnyObject    *response) override
    {
        if (status != nullptr)
        {
            const std::unique_ptr<XrdCl::XRootDStatus> owned_status{status};
        }

        if (response != nullptr)
        {
            const std::unique_ptr<XrdCl::AnyObject> owned_response{response};
        }

        {
            const std::lock_guard<std::mutex> lock{mutex};
            is_ready = true;
        }

        is_ready_changed.notify_all();
    }

    // Return false when the wait takes more than the timeout.
    bool wait(std::chrono::seconds timeout = std::chrono::seconds(0))
    {
        std::unique_lock<std::mutex> lock{mutex};

        bool ready = true;

        if (timeout.count() > 0)
            ready = is_ready_changed.wait_for(lock, timeout, [this]{ return is_ready; });
        else
            is_ready_changed.wait(lock, [this]{ return is_ready; });

        is_ready = false;

        return ready;
    }

private:
    std::mutex mutex{};
    std::condition_variable is_ready_changed{};
    bool is_ready{false};
};

// Adds fixed headers, such as the Authorization header of one endpoint, to
// the headers of a request.
class ExtraHeaderCallout final : public XrdClHttp::HeaderCallout
{
public:
    explicit ExtraHeaderCallout(HeaderList extra) : extra(std::move(extra)) {}

    std::shared_ptr<HeaderList> GetHeaders(const std::string &,
                                           const std::string &,
                                           const HeaderList &headers) override
    {
        auto result = std::make_shared<HeaderList>(headers);
        result->insert(result->end(), extra.begin(), extra.end());
        return result;
    }

private:
    const HeaderList extra;
};

}

Filesystem::Filesystem(const std::string &url, std::shared_ptr<HandlerQueue> queue, XrdCl::Log *log)
    : m_queue(queue),
      m_logger(log),
      m_url(url)
{
    m_logger->Debug(kLogXrdClHttp, "Constructing filesystem object with base URL %s", url.c_str());
    // When constructed from the root protocol handler, we've observed it include the
    // path here (the code paths appear to be slightly different from http://).  Strip
    // it out so it's not included twice later.
    m_url.SetPath("/");
    XrdCl::URL::ParamsMap map;
    m_url.SetParams(map);
}

Filesystem::~Filesystem() noexcept {}

XrdCl::XRootDStatus
Filesystem::QueueOperation(std::unique_ptr<CurlOperation> operation,
                           const char *description)
{
    try
    {
        m_queue->Produce(std::move(operation));
    }
    catch(const std::exception &ex)
    {
        m_logger->Warning(kLogXrdClHttp,
            "Failed to add %s to queue: %s", description, ex.what());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError,
                                  EIO, ex.what());
    }
    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
Filesystem::DirList(const std::string          &path,
                    XrdCl::DirListFlags::Flags  flags,
                    XrdCl::ResponseHandler     *handler,
                    time_t                      timeout )
{
    auto ts = XrdClHttp::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);

    m_logger->Debug(kLogXrdClHttp, "Filesystem::DirList path %s", path.c_str());
    auto listdirOp = std::make_unique<XrdClHttp::CurlListdirOp>(
        handler, full_url,
        m_url.GetHostName() + ":" + std::to_string(m_url.GetPort()),
        SendResponseInfo(), ts, m_logger,
        GetConnCallout(), m_header_callout.load(std::memory_order_acquire));
    return QueueOperation(std::move(listdirOp), "directory list operation");
}

CreateConnCalloutType
Filesystem::GetConnCallout() const {
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

bool
Filesystem::GetProperty(const std::string &name,
                        std::string       &value) const
{
    std::shared_lock lock(m_properties_mutex);

    const auto p = m_properties.find(name);
    if (p == std::end(m_properties)) {
        return false;
    }

    value = p->second;
    return true;
}

// Trivial implementation of the "locate" call
//
// On Linux, this is invoked by the XrdCl client prior to directory listings.
// Given there's no concept of multiple locations currently, we just return
// the original host and port as the available "location".
XrdCl::XRootDStatus
Filesystem::Locate( const std::string        &path,
                    XrdCl::OpenFlags::Flags   flags,
                    XrdCl::ResponseHandler   *handler,
                    time_t                    timeout )
{
    if (!handler) return XrdCl::XRootDStatus();

    auto locateInfo = std::make_unique<XrdCl::LocationInfo>();
    locateInfo->Add(XrdCl::LocationInfo::Location(m_url.GetHostName() + ":" + std::to_string(m_url.GetPort()), XrdCl::LocationInfo::ServerOnline, XrdCl::LocationInfo::Read));

    auto obj = std::make_unique<XrdCl::AnyObject>();
    obj->Set(locateInfo.release());
    handler->HandleResponse(new XrdCl::XRootDStatus(), obj.release());

    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus Filesystem::MkDir(const std::string        &path,
                                      XrdCl::MkDirFlags::Flags  flags,
                                      XrdCl::Access::Mode       mode,
                                      XrdCl::ResponseHandler   *handler,
                                      time_t                    timeout)
{
    auto ts = XrdClHttp::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);
    m_logger->Debug(kLogXrdClHttp, "Filesystem::MkDir path %s", full_url.c_str());

    auto mkdirOp = std::make_unique<CurlMkcolOp>(
        handler, full_url, ts, m_logger, SendResponseInfo(), GetConnCallout(),
        m_header_callout.load(std::memory_order_acquire));
    return QueueOperation(std::move(mkdirOp), "filesystem mkdir operation");
}

XrdCl::XRootDStatus Filesystem::Prepare(
    const std::vector<std::string> &fileList,
    XrdCl::PrepareFlags::Flags      flags,
    uint8_t                         priority,
    XrdCl::ResponseHandler         *handler,
    time_t                          timeout)
{
    (void)priority;

    if(fileList.empty())
    {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs,
            EINVAL, "missing prepare file list");
    }

    const auto ts = XrdClHttp::Factory::GetHeaderTimeoutWithDefault(timeout);
    auto tapeOp = std::make_unique<CurlTapePrepareOp>(
        handler, m_url.GetURL(), fileList, flags, ts, m_logger,
        GetConnCallout(),
        m_header_callout.load(std::memory_order_acquire));
    return QueueOperation(std::move(tapeOp), "Tape prepare operation");
}

XrdCl::XRootDStatus Filesystem::Query(XrdCl::QueryCode::Code  queryCode,
    const XrdCl::Buffer     &arg,
    XrdCl::ResponseHandler  *handler,
    time_t                   timeout)
{
    const auto ts = XrdClHttp::Factory::GetHeaderTimeoutWithDefault(timeout);
    std::unique_ptr<CurlOperation> operation;
    const char *description = nullptr;

    switch(queryCode)
    {
        case XrdCl::QueryCode::Prepare:
        case XrdCl::QueryCode::Opaque:
            operation = std::make_unique<CurlTapeQueryOp>(
                handler, m_url.GetURL(), queryCode, arg, ts, m_logger,
                GetConnCallout(),
                m_header_callout.load(std::memory_order_acquire));
            description = "Tape query operation";
            break;
        case XrdCl::QueryCode::Checksum:
        {
            const auto url = GetCurrentURL(arg.ToString());
            m_logger->Debug(kLogXrdClHttp,
                "XrdClHttp::Filesystem::Query checksum path %s", url.c_str());

            XrdClHttp::ChecksumType preferred =
                XrdClHttp::ChecksumType::kAll;
            XrdCl::URL urlObj;
            urlObj.FromString(url);
            const auto iter = urlObj.GetParams().find("cks.type");
            if(iter != urlObj.GetParams().end())
            {
                preferred = XrdClHttp::GetTypeFromString(iter->second);
                if(preferred == XrdClHttp::ChecksumType::kUnknown)
                {
                    m_logger->Error(kLogXrdClHttp,
                        "Unknown checksum type %s", iter->second.c_str());
                    return XrdCl::XRootDStatus(XrdCl::stError,
                        XrdCl::errInvalidArgs, EINVAL,
                        "unknown checksum type '" + iter->second + "'");
                }
            }
            operation = std::make_unique<CurlChecksumOp>(
                handler, url, preferred, ts, m_logger, SendResponseInfo(),
                GetConnCallout(),
                m_header_callout.load(std::memory_order_acquire));
            description = "checksum operation";
            break;
        }
        case XrdCl::QueryCode::XAttr:
        {
            const std::string path = arg.ToString();
            m_logger->Debug(kLogXrdClHttp,
                "XrdClHttp::Filesystem::Query xattr full_url %s, path %s",
                m_url.GetURL().c_str(), path.c_str());
            operation = std::make_unique<CurlQueryOp>(
                handler, path, ts, m_logger, SendResponseInfo(),
                GetConnCallout(), queryCode,
                m_header_callout.load(std::memory_order_acquire));
            description = "xattr query operation";
            break;
        }
        default:
            return XrdCl::XRootDStatus(
                XrdCl::stError, XrdCl::errNotImplemented);
    }

    return QueueOperation(std::move(operation), description);
}

XrdCl::XRootDStatus
Filesystem::Rm(const std::string      &path,
               XrdCl::ResponseHandler *handler,
               time_t                  timeout)
{
    auto ts = XrdClHttp::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);
    m_logger->Debug(kLogXrdClHttp, "Filesystem::Rm path %s", full_url.c_str());

    auto deleteOp = std::make_unique<CurlDeleteOp>(
        handler, full_url, ts, m_logger, SendResponseInfo(),
        GetConnCallout(), m_header_callout.load(std::memory_order_acquire));
    return QueueOperation(std::move(deleteOp), "filesystem delete operation");
}

XrdCl::XRootDStatus
Filesystem::RmDir(const std::string      &path,
                  XrdCl::ResponseHandler *handler,
                  time_t                  timeout)
{
    return Rm(path, handler, timeout);
}

bool
Filesystem::SetProperty(const std::string &name,
                        const std::string &value)
{
    if (name == "XrdClHttpHeaderCallout") {
        long long pointer;
        try {
            pointer = std::stoll(value, nullptr, 16);
        } catch (...) {
            pointer = 0;
        }
        if (!pointer) {
            pointer = 0;
        }
        m_header_callout.store(reinterpret_cast<XrdClHttp::HeaderCallout*>(pointer), std::memory_order_release);
    }

    std::unique_lock lock(m_properties_mutex);
    m_properties[name] = value;
    return true;
}

XrdCl::XRootDStatus
Filesystem::Stat(const std::string      &path,
                 XrdCl::ResponseHandler *handler,
                 time_t                  timeout)
{
    auto ts = XrdClHttp::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);
    m_logger->Debug(kLogXrdClHttp, "Filesystem::Stat path %s", full_url.c_str());

    auto statOp = std::make_unique<CurlStatOp>(
        handler, full_url, ts, m_logger, SendResponseInfo(),
        GetConnCallout(), m_header_callout.load(std::memory_order_acquire));
    return QueueOperation(std::move(statOp), "filesystem stat operation");
}

// Returns true if the URL uses the http or the https protocol. A third party
// copy is possible only between these two protocols.
static bool is_http_url( const std::string &url )
{
    const std::string protocol = XrdCl::URL(url).GetProtocol();
    return protocol == "http" || protocol == "https" || protocol == "dav" || protocol == "davs";
}

// The operations a third party copy runs. The unit tests replace them by
// declaring XrdClHttp::StatOp and XrdClHttp::CopyOp before they include this
// file, because the lookup of the unqualified name stops at namespace
// XrdClHttp.
namespace XrdClHttp
{
    namespace ThirdPartyCopyOpTypes
    {
        using StatOp = CurlStatOp;
        using CopyOp = CurlCopyOp;
    }
}

using namespace XrdClHttp::ThirdPartyCopyOpTypes;

// Inside namespace XrdClHttp for the same name lookup of StatOp as above.
namespace XrdClHttp
{
// Stat the URL with the given extra headers. Returns the size, or nothing when
// the stat fails, does not finish in time, or cannot be queued.
static std::optional<std::size_t> stat_size(HandlerQueue              &queue,
                                            const std::string         &url,
                                            const CurlCopyOp::Headers &hdrs,
                                            time_t                     timeout,
                                            XrdCl::Log                *log,
                                            const char                *what)
{
    try
    {
        const auto callout = std::make_shared<ExtraHeaderCallout>(hdrs);
        const auto rh = std::make_shared<SyncResponseHandler>();
        const auto op = std::make_shared<StatOp>(rh.get(), url, timespec{timeout, 0}, log, true, nullptr, callout.get());

        // The queue may run the operation after this function returns, thus
        // the operation keeps its handler and header callout alive.
        queue.Produce(std::shared_ptr<StatOp>(op.get(), [callout, rh, op](auto){}));

        if (!rh->wait(std::chrono::seconds(timeout)))
            log->Warning(kLogXrdClHttp, "Failed to stat the %s: Operation timed out", what);
        else if (!op->IsDone() || op->HasFailed())
            log->Debug(kLogXrdClHttp, "Failed to stat the %s", what);
        else
            return static_cast<std::size_t>(op->GetStatInfo().first);
    }
    catch (const std::exception &e) {
        log->Warning(kLogXrdClHttp, "Failed to stat the %s - %s", what, e.what());
    }

    return std::nullopt;
}
}

XrdCl::XRootDStatus Filesystem::ThirdPartyCopy( const std::string            &source,
                                                const std::string            &dest,
                                                const XrdCl::PropertyList    *properties,
                                                XrdCl::ProgressHandler       *progress_handler,
                                                time_t                        timeout )
{
    XrdCl::Log *const log = XrdCl::DefaultEnv::GetLog();
    log->Debug(kLogXrdClHttp, "XrdClHttp::ThirdPartyCopy src %s dst %s", source.c_str(), dest.c_str());

    if (!is_http_url(source) || !is_http_url(dest))
    {
        log->Error(kLogXrdClHttp, "Third party copy can only be done between http(s) protocols");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, 0,
                                   "Third party copy can only be done between http(s) protocols");
    }

    TpcMode mode = TpcMode::Pull;
    int number_of_streams = 1;
    time_t init_timeout = 0;
    time_t tpc_timeout = 0;
    std::string token_file;
    bool force_overwrite = false;

    if (properties)
    {
        mode = properties->Get<std::string>("thirdPartyMode") != "push" ? TpcMode::Pull : TpcMode::Push;
        properties->Get("initTimeout", init_timeout);
        properties->Get("tpcTimeout", tpc_timeout);
        token_file = properties->Get<std::string>("thirdPartyTokenFile");
        properties->Get("force", force_overwrite);
    }

    // The tpcTimeout property takes precedence over the timeout argument.
    if (tpc_timeout == 0)
        tpc_timeout = timeout;

    if (auto env = XrdCl::DefaultEnv::GetEnv(); env)
        env->GetInt("SubStreamsPerChannel", number_of_streams);

    CurlCopyOp::Headers headers;
    CurlCopyOp::Headers src_hdrs;
    CurlCopyOp::Headers dst_hdrs;

    if (!token_file.empty() && !ParseTokenFile(token_file, src_hdrs, dst_hdrs))
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errAuthFailed, 0,
                                   "Failed to parse the token file '" + token_file + "'");

    const auto is_unsecure = [] (const CurlCopyOp::Headers &hdrs, const std::string &url)
    {
        return XrdCl::URL(url).GetProtocol() != "https"s &&
               std::any_of(hdrs.begin(), hdrs.end(),
                           [] (const auto &header) { return header.first == "Authorization"s; });
    };

    if (is_unsecure(src_hdrs, source) || is_unsecure(dst_hdrs, dest))
    {
        log->Error(kLogXrdClHttp, "Refusing to send an Authorization header over an unencrypted http URL");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidArgs, 0,
                                   "Refusing to send an Authorization header over an unencrypted http URL");
    }

    // In push mode the source writes the destination with a PUT, which may
    // replace an existing file whatever the Overwrite header says. Thus check
    // the destination first, as the destination server may not honor it.
    if (mode == TpcMode::Push && !force_overwrite &&
        stat_size(*m_queue, dest, dst_hdrs, init_timeout, log, "destination"))
    {
        log->Error(kLogXrdClHttp, "The destination %s exists", dest.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errErrorResponse, kXR_ItExists,
                                   "The destination exists; use --force to replace it");
    }

    headers.emplace_back("X-Number-Of-Streams"s, std::to_string(number_of_streams));

    // The destination headers reach the destination in both modes: directly
    // in pull mode, with the TransferHeader prefix in push mode.
    dst_hdrs.emplace_back("Overwrite"s, force_overwrite ? "T"s : "F"s);

    std::size_t size = 0;

    if (progress_handler)
    {
        size = stat_size(*m_queue, source, src_hdrs, init_timeout, log, "source").value_or(0);
        progress_handler->HandleProgress(0, size);
    }

    const auto rh = std::make_shared<SyncResponseHandler>();
    const auto op = std::make_shared<CopyOp>(rh.get(), source, src_hdrs, dest, dst_hdrs, headers,
                                             mode, timespec{tpc_timeout, 0}, log, nullptr);
    op->SetProgressHandler(progress_handler);

    try
    {
        m_queue->Produce(std::shared_ptr<CopyOp>(op.get(), [rh, op](auto){}));
    }
    catch (const std::exception &e) {
        log->Warning(kLogXrdClHttp, "Failed to add copy op to queue - %s", e.what());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInternal);
    }

    if (!rh->wait(std::chrono::seconds(tpc_timeout)))
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Operation expired: Operation timed out"s);

    if (op->IsDone() && !op->IsSentSuccessfully())
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errPipelineFailed, 0, op->GetSendingFailureMessage());

    if (progress_handler && size > 0)
        progress_handler->HandleProgress(size, size);

    return XrdCl::XRootDStatus();
}

bool Filesystem::SendResponseInfo() const {
    std::string val;
    return GetProperty(ResponseInfoProperty, val) && val == "true";
}

std::string Filesystem::GetCurrentURL(const std::string &path) const {

    // Compute the URL without trailing slash.
    auto prefix = m_url.GetURL();
    std::string_view prefix_view = prefix;
    while (!prefix_view.empty() && prefix_view[prefix_view.size() - 1] == '/')
        prefix_view = prefix_view.substr(0, prefix_view.size() - 1);

    // Compute the target path without the '/' prefix
    std::string_view path_view = path;
    while (!path_view.empty() && path_view[0] == '/')
        path_view = path_view.substr(1);
    auto retval = std::string(prefix_view) + "/" + std::string(path_view);

    // Add in the query parameters, if relevant.
    {
        std::shared_lock lock(m_properties_mutex);
        auto iter = m_properties.find("XrdClHttpQueryParam");
        if (iter != m_properties.end() && !iter->second.empty()) {
            retval += ((retval.find('?') == std::string::npos) ? '?' : ':') + iter->second;
        }
    }
    return retval;
}
