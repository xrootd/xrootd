/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClS3 client plugin for XRootD.                 */
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

#include "XrdClS3Factory.hh"
#include "XrdClS3File.hh"

#include <XrdCl/XrdClLog.hh>

using namespace XrdClS3;

namespace {

class OpenResponseHandler : public XrdCl::ResponseHandler {
public:
    OpenResponseHandler(bool *is_opened, XrdCl::ResponseHandler *handler)
        : m_is_opened(is_opened),
          m_handler(handler)
    {
    }

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
        // Delete the handler; since we're injecting results a File object, no one owns us
        std::unique_ptr<OpenResponseHandler> owner(this);

        if (status && status->IsOK()) {
            if (m_is_opened) *m_is_opened = true;
        }
        if (m_handler) m_handler->HandleResponse(status, response);
        else delete response;
    }

private:
    bool *m_is_opened;

    // A reference to the handler we are wrapping.  Note we don't own the handler
    // so this is not a unique_ptr.
    XrdCl::ResponseHandler *m_handler;
};

class CloseResponseHandler : public XrdCl::ResponseHandler {
public:
    CloseResponseHandler(bool *is_opened, XrdCl::ResponseHandler *handler)
        : m_is_opened(is_opened),
          m_handler(handler)
    {
    }

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
        std::unique_ptr<CloseResponseHandler> owner(this);

        if (status && status->IsOK()) {
            if (m_is_opened) *m_is_opened = false;
        }
        if (m_handler) m_handler->HandleResponse(status, response);
        else delete response;
    }

private:
    bool *m_is_opened;

    // A reference to the handler we are wrapping.  Note we don't own the handler
    // so this is not a unique_ptr.
    XrdCl::ResponseHandler *m_handler;
};

} // namespace

File::File(XrdCl::Log *log) :
        m_logger(log),
        m_wrapped_file()
{
}

File::~File() noexcept {}

XrdCl::XRootDStatus
File::Close(XrdCl::ResponseHandler *handler,
            time_t                  timeout)
{
    return m_wrapped_file->Close(new CloseResponseHandler(&m_is_opened, handler), timeout);
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
    std::unique_lock lock(m_properties_mutex);
    const auto p = m_properties.find(name);
    if (p == std::end(m_properties)) {
        return false;
    }

    value = p->second;
    return true;
}

std::tuple<XrdCl::XRootDStatus, std::string, XrdCl::File*>
File::GetFileHandle(const std::string &s3_url) {
    if (m_wrapped_file) {
        return std::make_tuple(XrdCl::XRootDStatus{}, m_url, m_wrapped_file.get());
    }

    std::string s3_noslash_url;
    auto schema_loc = s3_url.find("://");
    if (schema_loc != std::string::npos) {
        auto path_loc = s3_url.find('/', schema_loc + 3);
        if (path_loc != std::string::npos) {
            if (s3_url.size() >= path_loc && s3_url[path_loc + 1] == '/') {
                s3_noslash_url = s3_url.substr(0, path_loc) + s3_url.substr(path_loc + 1);
            }
        }
    }

    std::string https_url, err_msg;
    if (!Factory::GenerateHttpUrl(s3_noslash_url.empty() ? s3_url : s3_noslash_url, https_url, nullptr, err_msg)) {
        return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, err_msg), "", nullptr);
    }
    auto loc = https_url.find('/', 8); // strlen("https://") -> 8
    if (loc == std::string::npos) {
        return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, "Invalid generated URL"), "", nullptr);
    }

    XrdCl::URL url;
    if (!url.FromString(https_url)) {
        return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, "Invalid generated XrdCl URL"), "", nullptr);
    }
    m_url = https_url;
    std::unique_ptr<XrdCl::File> wrapped_file(new XrdCl::File());
    // Hack - we need to set a few properties on the file object before the open occurs.
    // However, the "real" (plugin) file object is not created until the open call.
    // This forces the plugin object to be created, so we can set the properties and Open later.
    auto status = wrapped_file->Open(url.GetURL(), XrdCl::OpenFlags::Compress, XrdCl::Access::None, nullptr, time_t(0));
    if (!status.IsOK()) {
        return std::make_tuple(status, "", nullptr);
    }

    std::stringstream ss;
    ss << std::hex << reinterpret_cast<long long>(&m_header_callout);
    if (!wrapped_file->SetProperty("XrdClHttpHeaderCallout", ss.str())) {
        return std::make_tuple(XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidAddr, 0, "Failed to setup header callout"), "", nullptr);
    }
    m_wrapped_file.reset(wrapped_file.release());

    return std::make_tuple(XrdCl::XRootDStatus{}, https_url, m_wrapped_file.get());
}

XrdCl::XRootDStatus
File::Open(const std::string      &url,
           XrdCl::OpenFlags::Flags flags,
           XrdCl::Access::Mode     mode,
           XrdCl::ResponseHandler *handler,
           time_t                  timeout)
{
    if (IsOpen()) {
        m_logger->Error(kLogXrdClS3, "URL %s already open", url.c_str());
        return XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInvalidOp);
    }

    auto [st, https_url, fs] = GetFileHandle(url);
    if (!st.IsOK()) {
        return st;
    }

    return fs->Open(https_url, flags, mode, new OpenResponseHandler(&m_is_opened, handler), timeout);
}

XrdCl::XRootDStatus
File::PgRead(uint64_t                offset,
             uint32_t                size,
             void                   *buffer,
             XrdCl::ResponseHandler *handler,
             time_t                  timeout)
{
    return m_wrapped_file->PgRead(offset, size, buffer, handler, timeout);
}

XrdCl::XRootDStatus
File::Read(uint64_t                offset,
           uint32_t                size,
           void                   *buffer,
           XrdCl::ResponseHandler *handler,
           time_t                  timeout)
{

    return m_wrapped_file->Read(offset, size, buffer, handler, timeout);
}

bool
File::SetProperty(const std::string &name,
                  const std::string &value)
{
    std::unique_lock lock(m_properties_mutex);
    m_properties[name] = value;
    return true;
}

XrdCl::XRootDStatus
File::Stat(bool                    force,
           XrdCl::ResponseHandler *handler,
           time_t                  timeout)
{
    return m_wrapped_file->Stat(force, handler, timeout);
}

XrdCl::XRootDStatus
File::VectorRead(const XrdCl::ChunkList &chunks,
                 void                   *buffer,
                 XrdCl::ResponseHandler *handler,
                 time_t                  timeout )
{
    return m_wrapped_file->VectorRead(chunks, buffer, handler, timeout);
}

XrdCl::XRootDStatus
File::Write(uint64_t            offset,
            uint32_t                size,
            const void             *buffer,
            XrdCl::ResponseHandler *handler,
            time_t                  timeout)
{
    return m_wrapped_file->Write(offset, size, buffer, handler, timeout);
}

XrdCl::XRootDStatus
File::Write(uint64_t             offset,
            XrdCl::Buffer          &&buffer,
            XrdCl::ResponseHandler  *handler,
            time_t                   timeout)
{
    return m_wrapped_file->Write(offset, std::move(buffer), handler, timeout);
}

std::shared_ptr<XrdClHttp::HeaderCallout::HeaderList>
File::S3HeaderCallout::GetHeaders(const std::string &verb,
                                  const std::string &url,
                                  const XrdClHttp::HeaderCallout::HeaderList &headers)
{
    std::string auth_token, err_msg;
    std::shared_ptr<HeaderList> header_list(new HeaderList(headers));
    if (Factory::GenerateV4Signature(url, verb, *header_list, auth_token, err_msg)) {
        header_list->emplace_back("Authorization", auth_token);
    } else {
        m_parent.m_logger->Error(kLogXrdClS3, "Failed to generate V4 signature: %s", err_msg.c_str());
        return nullptr;
    }
    return header_list;
}
