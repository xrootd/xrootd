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

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>
#include <XrdOuc/XrdOucCRC.hh>
#include <XrdSys/XrdSysPageSize.hh>

using namespace XrdClHttp;

CurlReadOp::CurlReadOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
    const std::string &url, struct timespec timeout, const std::pair<uint64_t, uint64_t> &op,
    char *buffer, size_t sz, XrdCl::Log *logger, CreateConnCalloutType callout,
    HeaderCallout *header_callout) :
        CurlOperation(handler, url, timeout, logger, callout, header_callout),
        m_default_handler(default_handler),
        m_op(op),
        m_buffer(buffer),
        m_buffer_size(sz)
    {}

bool
CurlReadOp::Continue(std::shared_ptr<CurlOperation> op, XrdCl::ResponseHandler *handler, char *buffer, size_t buffer_size)
{
    if (op.get() != this) {
        m_logger->Debug(kLogXrdClHttp, "Interface error: must provide shared pointer to self");
        Fail(XrdCl::errInternal, 0, "Interface error: must provide shared pointer to self");
        return false;
    }
    m_handler = handler;
    m_buffer = buffer;
    m_buffer_size = buffer_size;
    m_written = 0;

    if (!m_prefetch_buffer.empty()) {
        auto prefetch_remaining = m_prefetch_buffer.size() - m_prefetch_buffer_offset;
        auto to_copy = prefetch_remaining > buffer_size ? buffer_size : prefetch_remaining;
        m_written += to_copy;
        memcpy(buffer, m_prefetch_buffer.data() + m_prefetch_buffer_offset, to_copy);
        m_prefetch_buffer_offset += to_copy;
        if (m_prefetch_buffer_offset == m_prefetch_buffer.size()) {
            m_prefetch_buffer.clear();
            m_prefetch_buffer_offset = 0;
        }
    }

    // This handles the case where the transfer finished but its last WriteCallback
    // produced more data than the client buffer could hold, the excess being stored
    // in the prefetch buffer
    // we just need to deliver the response without re-queuing to the continue queue
    if (op->IsDone()) {
        DeliverResponse();
    } else {
        try {
            m_continue_queue->Produce(op);
        } catch (...) {
            Fail(XrdCl::errInternal, ENOMEM, "Failed to continue the curl operation");
            return false;
        }
    }

    return true;
}

bool
CurlReadOp::ContinueHandle()
{
    if (IsDone()) {
        return false;
    }
    if (!m_curl) {
        return false;
    }

    CURLcode rc;
    if ((rc = curl_easy_pause(m_curl.get(), CURLPAUSE_CONT)) != CURLE_OK) {
        m_logger->Error(kLogXrdClHttp, "Failed to continue a paused handle: %s", curl_easy_strerror(rc));
        return false;
    }
    SetPaused(false);
    return m_curl.get();
    }

bool
CurlReadOp::Setup(CURL *curl, CurlWorker &worker)
{
    if (!CurlOperation::Setup(curl, worker)) {return false;}

    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, CurlReadOp::WriteCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this);

    // Note: range requests are inclusive of the end byte, meaning "bytes=0-1023" is a 1024-byte request.
    // This is why we subtract '1' off the end.
    if (m_op.second == 0) {
        Success();
        return true;
    }
    if (m_op.second >= 1024*1024) {
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128*1024);
    }
    else if (m_op.second >= 256*1024) {
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 64*1024);
    }
    else if (m_op.second >= 128*1024) {
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 32*1024);
    }
    // If the requested read size is UINT64_MAX, it means read the entire object;
    // in this case, we do not set the Range header.
    if (m_op.second != UINT64_MAX) {
        auto range_req = "bytes=" + std::to_string(m_op.first) + "-" + std::to_string(m_op.first + m_op.second - 1);
        m_headers_list.emplace_back("Range", range_req);
    }

    return true;
}

void
CurlReadOp::Fail(uint16_t errCode, uint32_t errNum, const std::string &msg)
{
    std::string custom_msg = msg;
    SetDone(true);
    if (m_handler == nullptr && m_default_handler == nullptr) {return;}
    if (!custom_msg.empty()) {
        m_logger->Debug(kLogXrdClHttp, "curl operation at offset %llu failed with message: %s%s", static_cast<long long unsigned>(m_op.first), msg.c_str(), m_err_msg.empty() ? "" : (", server message: " + m_err_msg).c_str());
        custom_msg += " (read operation at offset " + std::to_string(static_cast<long long unsigned>(m_op.first)) + ")";
    } else {
        m_logger->Debug(kLogXrdClHttp, "curl operation at offset %llu failed with status code %d%s", static_cast<long long unsigned>(m_op.first), errNum, m_err_msg.empty() ? "" : (", server message: " + m_err_msg).c_str());
    }
    auto status = new XrdCl::XRootDStatus(XrdCl::stError, errCode, errNum, custom_msg);
    auto handle = m_handler;
    m_handler = nullptr;
    if (handle) handle->HandleResponse(status, nullptr);
    else m_default_handler->HandleResponse(status, nullptr);
}

void
CurlReadOp::DeliverResponse()
{
    if (m_handler == nullptr) {return;}
    auto handle = m_handler;
    auto status = new XrdCl::XRootDStatus();

    auto chunk_info = new XrdCl::ChunkInfo(m_op.first + m_prefetch_object_offset, m_written, m_buffer);
    m_prefetch_object_offset += m_written;
    auto obj = new XrdCl::AnyObject();
    obj->Set(chunk_info);

    // Reset the internal buffers to avoid writes to locations we do not own
    m_buffer = nullptr;
    m_buffer_size = 0;

    m_handler = nullptr;
    // Note: As soon as this is invoked, another thread may continue and start to manipulate
    // the CurlReadOp object.  To avoid race conditions, all reads/writes to member data must
    // be done *before* the callback is invoked.
    handle->HandleResponse(status, obj);
}

void
CurlReadOp::Pause()
{
    SetPaused(true);
    if (m_handler == nullptr) {
        m_logger->Warning(kLogXrdClHttp, "Get operation paused with no callback handler");
        return;
    }
    DeliverResponse();
}

void
CurlReadOp::Success()
{
    SetDone(false);
    if (m_handler == nullptr) {return;}
    auto status = new XrdCl::XRootDStatus();
    auto chunk_info = new XrdCl::ChunkInfo(m_op.first + m_prefetch_object_offset, m_written, m_buffer);
    m_prefetch_object_offset += m_written;
    auto obj = new XrdCl::AnyObject();
    obj->Set(chunk_info);
    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(status, obj);
}

void
CurlReadOp::ReleaseHandle()
{
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, nullptr);
    CurlOperation::ReleaseHandle();
}

size_t
CurlReadOp::WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
{
    return static_cast<CurlReadOp*>(this_ptr)->Write(buffer, size * nitems);
}

size_t
CurlReadOp::Write(char *buffer, size_t length)
{
    //m_logger->Debug(kLogXrdClHttp, "Received a write of size %ld with offset %lld; total received is %ld; remaining is %ld", static_cast<long>(length), static_cast<long long>(m_op.first), static_cast<long>(length + m_written), static_cast<long>(m_op.second - length - m_written));
    if (m_headers.IsMultipartByterange()) {
        return FailCallback(kXR_ServerError, "Server responded with a multipart byterange which is not supported");
    }
    if (m_written == 0 && (m_headers.GetOffset() != m_op.first)) {
        return FailCallback(kXR_ServerError, "Server did not return content with correct offset");
    }
    // If the operation failed, do not copy the body of the response into the buffer; it is likely
    // an error message and not what we want to provide to the consumer buffer.
    if (m_headers.GetStatusCode() > 299) {
        // Record error message; prevent the server from spamming overly-long responses as we
        // buffer them in memory.
        if (m_err_msg.size() < 4*1024) {
            m_err_msg.append(buffer, length);
        }
        UpdateBytes(length);
        return length;
    }
    // The write callback is "all or nothing".  Either you accept the whole thing (buffering
    // in m_prefetch_buffer any data that the client-provided buffer is too small to accept)
    // or you return CURL_WRITEFUNC_PAUSE and the delivery will be retried the next time the
    // handle is unpaused.
    //
    // If `m_buffer` is nullptr, then it indicates we are unpaused while there is no ongoing
    // File::Read operation; this typically happens when the transfer timeout occurs.  Simply
    // re-pause the transfer to go through the libcurl state machine and trigger the failure.
    if (!m_buffer || (m_buffer_size == m_written)) {
        Pause();
        return CURL_WRITEFUNC_PAUSE;
    }
    UpdateBytes(length);
    auto output_remaining = m_buffer_size - m_written;
    auto larger_than_result_buffer = length > output_remaining;
    auto to_copy = larger_than_result_buffer ? output_remaining : length;
    memcpy(m_buffer + m_written, buffer, to_copy);
    m_written += to_copy;
    // We don't have enough space in the buffer to write the response and this is a single-shot
    // read request
    if ((m_op.second <= m_buffer_size) && larger_than_result_buffer) {
        return FailCallback(kXR_ServerError, "Server sent back more data than requested");
    } else if (larger_than_result_buffer) {
        auto input_remaining = length - output_remaining;
        m_prefetch_buffer.append(buffer + to_copy, input_remaining);
        m_prefetch_buffer_offset = 0;
    }
    return length;
}

void                
CurlPgReadOp::Success()
{               
    SetDone(false);
    if (m_handler == nullptr) {return;}
    auto status = new XrdCl::XRootDStatus();

    std::vector<uint32_t> cksums;
    size_t nbpages = m_written / XrdSys::PageSize;
    if (m_written % XrdSys::PageSize) ++nbpages;
    cksums.reserve(nbpages);

    auto buffer = m_buffer;
    size_t size = m_written;
    for (size_t pg=0; pg<nbpages; ++pg)
    {
        auto pgsize = static_cast<size_t>(XrdSys::PageSize);
        if (pgsize > size) pgsize = size;
        cksums.push_back(XrdOucCRC::Calc32C(buffer, pgsize));
        buffer += pgsize;
        size -= pgsize;
    }

    auto page_info = new XrdCl::PageInfo(m_op.first, m_written, m_buffer, std::move(cksums));
    auto obj = new XrdCl::AnyObject();
    obj->Set(page_info);
    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(status, obj);
}
