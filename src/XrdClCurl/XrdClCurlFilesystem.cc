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

#include "XrdClCurlFactory.hh"
#include "XrdClCurlFilesystem.hh"
#include "XrdClCurlOps.hh"
#include "XrdClCurlResponses.hh"

using namespace XrdClCurl;

Filesystem::Filesystem(const std::string &url, std::shared_ptr<HandlerQueue> queue, XrdCl::Log *log)
    : m_queue(queue),
      m_logger(log),
      m_url(url)
{
    m_logger->Debug(kLogXrdClCurl, "Constructing filesystem object with base URL %s", url.c_str());
    // When constructed from the root protocol handler, we've observed it include the
    // path here (the code paths appear to be slightly different from http://).  Strip
    // it out so it's not included twice later.
    m_url.SetPath("/");
    XrdCl::URL::ParamsMap map;
    m_url.SetParams(map);
}

Filesystem::~Filesystem() noexcept {}

XrdCl::XRootDStatus
Filesystem::DirList(const std::string          &path,
                    XrdCl::DirListFlags::Flags  flags,
                    XrdCl::ResponseHandler     *handler,
                    timeout_t                   timeout )
{
    auto ts = XrdClCurl::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);

    m_logger->Debug(kLogXrdClCurl, "Filesystem::DirList path %s", path.c_str());
    std::unique_ptr<XrdClCurl::CurlListdirOp> listdirOp(
        new XrdClCurl::CurlListdirOp(
            handler, full_url,
            m_url.GetHostName() + ":" + std::to_string(m_url.GetPort()),
            SendResponseInfo(), ts, m_logger,
            GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
        )
    );

    try {
        m_queue->Produce(std::move(listdirOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add dirlist op to queue");
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
                    timeout_t                 timeout )
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
                                      timeout_t                 timeout)
{
    auto ts = XrdClCurl::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);
    m_logger->Debug(kLogXrdClCurl, "Filesystem::MkDir path %s", full_url.c_str());

    std::unique_ptr<CurlMkcolOp> mkdirOp(
        new CurlMkcolOp(
            handler, full_url, ts, m_logger, SendResponseInfo(), GetConnCallout(),
            m_header_callout.load(std::memory_order_acquire)
        )
    );
    try {
        m_queue->Produce(std::move(mkdirOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add filesystem mkdir op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus Filesystem::Query(XrdCl::QueryCode::Code  queryCode,
    const XrdCl::Buffer     &arg,
    XrdCl::ResponseHandler  *handler,
    timeout_t                timeout)
{
    auto ts = XrdClCurl::Factory::GetHeaderTimeoutWithDefault(timeout);

    if (queryCode == XrdCl::QueryCode::Checksum)
    {
        auto url = GetCurrentURL(arg.ToString());
        m_logger->Debug(kLogXrdClCurl, "XrdClCurl::Filesystem::Query checksum path %s", url.c_str());

        XrdClCurl::ChecksumType preferred = XrdClCurl::ChecksumType::kCRC32C;
        XrdCl::URL url_obj;
        url_obj.FromString(url);
        auto iter = url_obj.GetParams().find("cks.type");
        if (iter != url_obj.GetParams().end())
        {
            preferred = XrdClCurl::GetTypeFromString(iter->second);
            if (preferred == XrdClCurl::ChecksumType::kUnknown)
            {
                m_logger->Error(kLogXrdClCurl, "Unknown checksum type %s", iter->second.c_str());
                preferred = XrdClCurl::ChecksumType::kCRC32C;
            }
        }
        // On miss, queue a checksum operation
        std::unique_ptr<CurlChecksumOp> cksumOp(
            new CurlChecksumOp(
                handler, url, preferred, ts, m_logger, SendResponseInfo(),
                GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
            )
        );
        try
        {
            m_queue->Produce(std::move(cksumOp));
        }
        catch (...)
        {
            m_logger->Warning(kLogXrdClCurl, "Failed to add checksum operation to queue");
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
        }
    }
    else if (queryCode == XrdCl::QueryCode::XAttr)
    {
        std::string path = arg.ToString();
        std::string full_url = m_url.GetURL();
        m_logger->Debug(kLogXrdClCurl, "XrdClCurl::Filesystem::Query xattr full_url %s, path %s", full_url.c_str(), path.c_str());
        full_url = m_url.GetURL();
        std::unique_ptr<CurlQueryOp> queryOp(
            new CurlQueryOp(
                handler, path, ts, m_logger,SendResponseInfo(),
                GetConnCallout(), queryCode, m_header_callout.load(std::memory_order_acquire)
            )
        );
        try
        {
            m_queue->Produce(std::move(queryOp));
        }
        catch (...)
        {
            m_logger->Warning(kLogXrdClCurl, "Failed to add xattr query operation to queue");
            return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
        }
    }
    else
    {
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errNotImplemented);
    }
    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
Filesystem::Rm(const std::string      &path,
               XrdCl::ResponseHandler *handler,
               timeout_t               timeout)
{
    auto ts = XrdClCurl::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);
    m_logger->Debug(kLogXrdClCurl, "Filesystem::Rm path %s", full_url.c_str());

    std::unique_ptr<CurlDeleteOp> deleteOp(
        new CurlDeleteOp(
            handler, full_url, ts, m_logger, SendResponseInfo(),
            GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
        )
    );
    try {
        m_queue->Produce(std::move(deleteOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add filesystem delete op to queue");
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOSError);
    }

    return XrdCl::XRootDStatus();
}

XrdCl::XRootDStatus
Filesystem::RmDir(const std::string      &path,
                  XrdCl::ResponseHandler *handler,
                  timeout_t               timeout)
{
    return Rm(path, handler, timeout);
}

bool
Filesystem::SetProperty(const std::string &name,
                        const std::string &value)
{
    if (name == "XrdClCurlHeaderCallout") {
        long long pointer;
        try {
            pointer = std::stoll(value, nullptr, 16);
        } catch (...) {
            pointer = 0;
        }
        if (!pointer) {
            pointer = 0;
        }
        m_header_callout.store(reinterpret_cast<XrdClCurl::HeaderCallout*>(pointer), std::memory_order_release);
    }

    std::unique_lock lock(m_properties_mutex);
    m_properties[name] = value;
    return true;
}

XrdCl::XRootDStatus
Filesystem::Stat(const std::string      &path,
                 XrdCl::ResponseHandler *handler,
                 timeout_t               timeout)
{
    auto ts = XrdClCurl::Factory::GetHeaderTimeoutWithDefault(timeout);

    auto full_url = GetCurrentURL(path);
    m_logger->Debug(kLogXrdClCurl, "Filesystem::Stat path %s", full_url.c_str());

    std::unique_ptr<CurlStatOp> statOp(
        new CurlStatOp(
            handler, full_url, ts, m_logger, SendResponseInfo(),
            GetConnCallout(), m_header_callout.load(std::memory_order_acquire)
        )
    );
    try {
        m_queue->Produce(std::move(statOp));
    } catch (...) {
        m_logger->Warning(kLogXrdClCurl, "Failed to add filesystem stat op to queue");
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
        auto iter = m_properties.find("XrdClCurlQueryParam");
        if (iter != m_properties.end() && !iter->second.empty()) {
            retval += ((retval.find('?') == std::string::npos) ? '?' : ':') + iter->second;
        }
    }
    return retval;
}
