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

#include "XrdClS3DownloadHandler.hh"
#include "XrdClS3Filesystem.hh"

#include <XrdCl/XrdClFile.hh>

#include <charconv>

using namespace XrdClS3;

namespace {

class S3DownloadHandler : public XrdCl::ResponseHandler {
public:
    S3DownloadHandler(std::unique_ptr<XrdCl::File> file, XrdCl::ResponseHandler *handler, Filesystem::timeout_t timeout)
        : m_expiry(time(NULL) + (timeout ? timeout : 30)), m_file(std::move(file)), m_handler(handler), m_buffer(new XrdCl::Buffer(kReadSize)) {}

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
        ReadHandler(S3DownloadHandler *parent) : m_parent(parent) {}
        virtual ~ReadHandler() noexcept = default;

        virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;
    private:
        S3DownloadHandler *m_parent; // Pointer to the parent handler to access its members
    };

    class CloseHandler : public XrdCl::ResponseHandler {
    public:
        CloseHandler(S3DownloadHandler *parent, XrdCl::XRootDStatus *status) : m_parent(parent), m_read_status(status) {}
        virtual ~CloseHandler() noexcept = default;

        virtual void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override;

    private:
        S3DownloadHandler *m_parent; // Pointer to the parent handler to access its members
        XrdCl::XRootDStatus *m_read_status; // Status from the read operation; if nullptr, the read was successful
    };
};

void
S3DownloadHandler::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response)
{
    std::unique_ptr<S3DownloadHandler> self(this);

    // If the open failed, we pass the status up the chain.
    if (!status || !status->IsOK()) {
        if (m_handler) m_handler->HandleResponse(status, response);
        else {delete response; delete status; }
        return;
    }
    auto [timeout, ok] = GetTimeout();
    if (!ok) {
        // If we have no time left, we cannot proceed with the read.
        if (m_handler) {
            m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Download operation timed out"), nullptr);
        }
        delete response;
        delete status;
        return;
    }

    // Open succeeded, so we can now read the file.
    auto st = m_file->Read(0, S3DownloadHandler::kReadSize, m_buffer->GetBufferAtCursor(), new ReadHandler(this), timeout);
    if (!st.IsOK()) {
        // If the read request failed, we close the file and return the error.
        CloseHandler *closeHandler = new CloseHandler(self.release(), new XrdCl::XRootDStatus(st));
        auto close_st = m_file->Close(closeHandler, timeout);
        if (!close_st.IsOK()) {
            if (m_handler) {
                m_handler->HandleResponse(new XrdCl::XRootDStatus(close_st), nullptr);
            }
            delete this; // Clean up the handler
        }
        return;
    }
    self.release();
    delete response;
    delete status;
}

void
S3DownloadHandler::ReadHandler::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
    std::unique_ptr<ReadHandler> self(this);

    auto [timeout, ok] = m_parent->GetTimeout();
    if (!ok) {
        // If we have no time left, we cannot proceed with the read.
        if (m_parent->m_handler) {
            m_parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errOperationExpired, 0, "Download operation timed out"), nullptr);
        }
        delete response;
        delete status;
        return;
    }

    if (!status || !status->IsOK()) {
        CloseHandler *closeHandler = new CloseHandler(m_parent, status);
        auto st = m_parent->m_file->Close(closeHandler, timeout);
        if (!st.IsOK()) {
            if (m_parent->m_handler) {
                m_parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
            }
            delete m_parent;
        }
        return;
    }
    delete status;

    XrdCl::ChunkInfo *chunkInfo = nullptr;
    response->Get(chunkInfo);
    if (!chunkInfo) {
        delete response;
        // If we didn't get a chunk, we can close the file and return.
        CloseHandler *closeHandler = new CloseHandler(m_parent, new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errInternal, 0, "No chunk info received"));
        auto st = m_parent->m_file->Close(closeHandler, timeout);
        if (!st.IsOK()) {
            if (m_parent->m_handler) {
                m_parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
            }
            delete m_parent;
        }
        return;
    }

    // If we got a chunk but the length is zero, that is the end of the file;
    // we can close the file and return.
    if (chunkInfo->GetLength() == 0) {
        m_parent->m_buffer->ReAllocate(m_parent->m_buffer->GetCursor());
        CloseHandler *closeHandler = new CloseHandler(m_parent, nullptr);
        auto st = m_parent->m_file->Close(closeHandler, timeout);
        if (!st.IsOK()) {
            if (m_parent->m_handler) {
                m_parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(st), nullptr);
            }
            delete m_parent;
        }
        delete response;
        return;
    }

    // Read was successful; read additional data if available.
    m_parent->m_buffer->AdvanceCursor(chunkInfo->GetLength());
    m_parent->m_buffer->ReAllocate(m_parent->m_buffer->GetCursor() + S3DownloadHandler::kReadSize);
    auto st = m_parent->m_file->Read(m_parent->m_buffer->GetCursor(), kReadSize, m_parent->m_buffer->GetBufferAtCursor(), self.release(), timeout);
    delete response;
    if (!st.IsOK()) {
        // If the read request failed, close or delete the parent handler.
        CloseHandler *closeHandler = new CloseHandler(m_parent, nullptr);
        auto close_st = m_parent->m_file->Close(closeHandler, timeout);
        if (!close_st.IsOK()) {
            if (m_parent->m_handler) {
                m_parent->m_handler->HandleResponse(new XrdCl::XRootDStatus(close_st), nullptr);
            }
            delete m_parent;
        }
    }
}

void
S3DownloadHandler::CloseHandler::HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
    std::unique_ptr<CloseHandler> self(this);
    std::unique_ptr<S3DownloadHandler> parent(m_parent);

    // If there was a read error, then we report that to the handler and ignore the close status.
    if (m_read_status) {
        delete response;
        delete status;
        // If we had a read status, we pass it up the chain.
        if (m_parent->m_handler) {
            m_parent->m_handler->HandleResponse(m_read_status, nullptr);
        } else {
            delete m_read_status;
        }
        return;
    }

    if (!status || !status->IsOK()) {
        delete response;
        if (m_parent->m_handler) {
            m_parent->m_handler->HandleResponse(status, nullptr);
        } else {
            delete status;
        }
        return;
    }

    // If the close was successful, we can pass the buffer to the handler.
    delete response;
    response = new XrdCl::AnyObject();
    response->Set(m_parent->m_buffer.release(), true); // Take ownership of the buffer
    if (m_parent->m_handler) {
        m_parent->m_handler->HandleResponse(status, response);
    } else {
        delete response;
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
