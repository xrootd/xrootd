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

#include "XrdClS3DownloadHandler.hh"
#include "XrdClS3Filesystem.hh"

#include <XrdCl/XrdClConstants.hh>
#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>

#include <charconv>

using namespace XrdClS3;

namespace {

class S3DownloadHandler : public XrdCl::ResponseHandler {
public:
    S3DownloadHandler(std::unique_ptr<XrdCl::File> file, XrdCl::ResponseHandler *handler, Filesystem::timeout_t timeout)
        : m_expiry(time(NULL) + timeout), m_file(std::move(file)), m_handler(handler), m_buffer(new XrdCl::Buffer(kReadSize))
    {
        if (timeout == 0) {
            auto val = XrdCl::DefaultRequestTimeout;
            XrdCl::DefaultEnv::GetEnv()->GetInt( "RequestTimeout", val );
            m_expiry += val;
        }
    }

    virtual ~S3DownloadHandler() noexcept = default;

    virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

private:
    time_t m_expiry;                         // Expiration time for the download operation
    std::unique_ptr<XrdCl::File> m_file;     // File we are reading from
    XrdCl::ResponseHandler *m_handler;       // Handler to call with the final result buffer (or failure).
    std::unique_ptr<XrdCl::Buffer> m_buffer; // Buffer to hold the data read from the file
    static constexpr size_t kReadSize = 32 * 1024; // Size of each read operation (32 KB)

    std::pair<Filesystem::timeout_t, bool> GetTimeout() const {
        // Calculate the timeout based on the current time and the expiry time
        time_t now = time(NULL);
        if (now >= m_expiry) {
            return {0, false}; // No time left, return 0 timeout
        }
        return {m_expiry - now, true};
    }

    class ReadHandler : public XrdCl::ResponseHandler {
    public:
        ReadHandler(std::unique_ptr<S3DownloadHandler> parent) : m_parent(std::move(parent)) {}
        virtual ~ReadHandler() noexcept = default;

        virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;
    private:
        std::unique_ptr<S3DownloadHandler> m_parent; // Pointer to the parent handler to access its members
    };

    class CloseHandler : public XrdCl::ResponseHandler {
    public:
        CloseHandler(std::unique_ptr<S3DownloadHandler> parent, std::unique_ptr<XrdCl::XRootDStatus> status) : m_parent(std::move(parent)), m_read_status(std::move(status)) {}
        virtual ~CloseHandler() noexcept = default;

        virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

    private:
        std::unique_ptr<S3DownloadHandler> m_parent; // Pointer to the parent handler to access its members
        std::unique_ptr<XrdCl::XRootDStatus> m_read_status; // Status from the read operation; if nullptr, the read was successful
    };
};

void
S3DownloadHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw)
{
    std::unique_ptr<S3DownloadHandler> self(this);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);

    // If the open failed, we pass the status up the chain.
    if (!status || !status->IsOK()) {
        if (m_handler) m_handler->HandleResponse(status.release(), response.release());
        return;
    }
    auto [timeout, ok] = GetTimeout();
    if (!ok) {
        // If we have no time left, we cannot proceed with the read.
        if (m_handler) {
            m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Download operation timed out"), nullptr);
        }
        return;
    }

    // Open succeeded, so we can now read the file.
    auto st = m_file->Read(0, S3DownloadHandler::kReadSize, m_buffer->GetBufferAtCursor(), new ReadHandler(std::move(self)), timeout);
    if (!st.IsOK()) {
        // If the read request failed, we close the file and return the error.
        std::unique_ptr<CloseHandler> closeHandler(new CloseHandler(std::move(self), std::unique_ptr<XrdCl::XRootDStatus>(new XrdCl::XRootDStatus(st))));
        auto close_st = m_file->Close(closeHandler.get(), timeout);
        if (close_st.IsOK()) {
            closeHandler.release(); // The close handler now owns itself
        } else {
            if (m_handler) {
                m_handler->HandleResponse(new XrdCl::XRootDStatus(close_st), nullptr);
            }
        }
        return;
    }
}

void
S3DownloadHandler::ReadHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw) {
    std::unique_ptr<ReadHandler> self(this);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);

    auto [timeout, ok] = m_parent->GetTimeout();
    if (!ok) {
        // If we have no time left, we cannot proceed with the read.
        if (m_parent->m_handler) {
            m_parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Download operation timed out"), nullptr);
        }
        return;
    }

    if (!status || !status->IsOK()) {
        auto parent = m_parent.get();
        std::unique_ptr<CloseHandler> closeHandler(new CloseHandler(std::move(m_parent), std::move(status)));
        auto st = parent->m_file->Close(closeHandler.get(), timeout);
        if (st.IsOK()) {
            closeHandler.release();
        } else if (parent->m_handler) {
            parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
        }
        return;
    }

    XrdCl::ChunkInfo *chunkInfo = nullptr;
    response->Get(chunkInfo);
    if (!chunkInfo) {
        // If we didn't get a chunk, we can close the file and return.
        auto parent = m_parent.get();
        std::unique_ptr<CloseHandler> closeHandler(new CloseHandler(std::move(m_parent),
            std::unique_ptr<XrdCl::XRootDStatus>(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInternal, 0, "No chunk info received"))));
        auto st = parent->m_file->Close(closeHandler.get(), timeout);
        if (st.IsOK()) {
            closeHandler.release();
        } else if (parent->m_handler) {
            parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
        }
        return;
    }

    // If we got a chunk but the length is zero, that is the end of the file;
    // we can close the file and return.
    if (chunkInfo->GetLength() == 0) {
        m_parent->m_buffer->ReAllocate(m_parent->m_buffer->GetCursor());
        auto parent = m_parent.get();
        std::unique_ptr<CloseHandler> closeHandler(new CloseHandler(std::move(m_parent), nullptr));
        auto st = parent->m_file->Close(closeHandler.get(), timeout);
        if (st.IsOK()) {
            closeHandler.release();
        } else if (parent->m_handler) {
            parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
        }
        return;
    }

    // Read was successful; read additional data if available.
    m_parent->m_buffer->AdvanceCursor(chunkInfo->GetLength());
    m_parent->m_buffer->ReAllocate(m_parent->m_buffer->GetCursor() + S3DownloadHandler::kReadSize);
    auto st = m_parent->m_file->Read(m_parent->m_buffer->GetCursor(), kReadSize, m_parent->m_buffer->GetBufferAtCursor(), self.release(), timeout);
    if (!st.IsOK()) {
        // If the read request failed, close or delete the parent handler.
        auto parent = m_parent.get();
        std::unique_ptr<CloseHandler> closeHandler(new CloseHandler(std::move(m_parent), nullptr));
        auto close_st = parent->m_file->Close(closeHandler.get(), timeout);
        if (close_st.IsOK()) {
            closeHandler.release();
        } else if (parent->m_handler) {
            parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(close_st), nullptr);
        }
    }
}

void
S3DownloadHandler::CloseHandler::HandleResponse(XrdCl::XRootDStatus *status_raw, XrdCl::AnyObject *response_raw) {
    std::unique_ptr<CloseHandler> self(this);
    std::unique_ptr<XrdCl::XRootDStatus> status(status_raw);
    std::unique_ptr<XrdCl::AnyObject> response(response_raw);

    // If there was a read error, then we report that to the handler and ignore the close status.
    if (m_read_status) {
        // If we had a read status, we pass it up the chain.
        if (m_parent->m_handler) {
            m_parent->m_handler->HandleResponse(m_read_status.release(), nullptr);
        }
        return;
    }

    if (!status || !status->IsOK()) {
        if (m_parent->m_handler) {
            m_parent->m_handler->HandleResponse(status.release(), nullptr);
        }
        return;
    }

    // If the close was successful, we can pass the buffer to the handler.
    response.reset(new XrdCl::AnyObject());
    response->Set(m_parent->m_buffer.release(), true); // Take ownership of the buffer
    if (m_parent->m_handler) {
        m_parent->m_handler->HandleResponse(status.release(), response.release());
    }
}

} // namespace

XrdCl::XRootDStatus
XrdClS3::DownloadUrl(const std::string &url, XrdClCurl::HeaderCallout *header_callout, XrdCl::ResponseHandler *handler, Filesystem::timeout_t timeout)
{
    std::unique_ptr<XrdCl::File> http_file(new XrdCl::File());
    // Hack - we need to set a few properties on the file object before the open occurs.
    // However, the "real" (plugin) file object is not created until the open call.
    // This forces the plugin object to be created, so we can set the properties and Open later.
    auto status = http_file->Open(url, XrdCl::OpenFlags::Compress, XrdCl::Access::None, nullptr, Filesystem::timeout_t(0));
    if (!status.IsOK()) {
        return status;
    }


    if (header_callout) {
        auto callout_loc = reinterpret_cast<long long>(header_callout);
        size_t buf_size = 16;
        char callout_buf[buf_size];
        std::to_chars_result result = std::to_chars(callout_buf, callout_buf + buf_size - 1, callout_loc, 16);
        if (result.ec == std::errc{}) {
            std::string callout_str(callout_buf, result.ptr - callout_buf);
            http_file->SetProperty("XrdClCurlHeaderCallout", callout_str);
        }
    }
    http_file->SetProperty("XrdClCurlFullDownload", "true");

    auto http_file_raw = http_file.get();
    S3DownloadHandler *downloadHandler = new S3DownloadHandler(std::move(http_file), handler, timeout);

    return http_file_raw->Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::None, downloadHandler, timeout);
}
