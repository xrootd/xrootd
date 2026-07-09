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

#include "XrdCl/XrdClAnyObject.hh"

#include <cerrno>
#include <exception>
#include <new>

using namespace XrdClHttp;

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
    catch(const std::bad_alloc &ex)
    {
        m_logger->Warning(kLogXrdClHttp,
            "Failed to add %s to queue: %s", description, ex.what());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError,
            ENOMEM, "out of memory while queueing HTTP operation");
    }
    catch(const std::exception &ex)
    {
        m_logger->Warning(kLogXrdClHttp,
            "Failed to add %s to queue: %s", description, ex.what());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError,
            EIO, ex.what());
    }
    catch(...)
    {
        m_logger->Warning(kLogXrdClHttp,
            "Failed to add %s to queue: unknown exception", description);
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError,
            EIO, "unknown exception while queueing HTTP operation");
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
    std::unique_ptr<XrdClHttp::CurlListdirOp> listdirOp(
        new XrdClHttp::CurlListdirOp(
            handler, full_url,
            m_url.GetHostName() + ":" + std::to_string(m_url.GetPort()),
            SendResponseInfo(), ts, m_logger,
            GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
        )
    );

    try {
        m_queue->Produce(std::move(listdirOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClHttp, "Failed to add dirlist op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
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

    std::unique_ptr<CurlMkcolOp> mkdirOp(
        new CurlMkcolOp(
            handler, full_url, ts, m_logger, SendResponseInfo(), GetConnCallout(),
            m_header_callout.load(std::memory_order_acquire)
        )
    );
    try {
        m_queue->Produce(std::move(mkdirOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClHttp, "Failed to add filesystem mkdir op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
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
                XrdClHttp::ChecksumType::kCRC32C;
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
                    preferred = XrdClHttp::ChecksumType::kCRC32C;
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

    std::unique_ptr<CurlDeleteOp> deleteOp(
        new CurlDeleteOp(
            handler, full_url, ts, m_logger, SendResponseInfo(),
            GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
        )
    );
    try {
        m_queue->Produce(std::move(deleteOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClHttp, "Failed to add filesystem delete op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
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

    std::unique_ptr<CurlStatOp> statOp(
        new CurlStatOp(
            handler, full_url, ts, m_logger, SendResponseInfo(),
            GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
        )
    );
    try {
        m_queue->Produce(std::move(statOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClHttp, "Failed to add filesystem stat op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

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
