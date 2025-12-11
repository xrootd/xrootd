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

#include "XrdClCurlOps.hh"

#include <XrdCl/XrdClLog.hh>

using namespace XrdClCurl;

CurlPutOp::CurlPutOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
    const std::string &url, const char *buffer, size_t buffer_size, struct timespec timeout,
    XrdCl::Log *logger, CreateConnCalloutType callout, HeaderCallout *header_callout)
    : CurlOperation(handler, url, timeout, logger, callout, header_callout),
    m_data(buffer, buffer_size),
    m_default_handler(default_handler)
{
}

CurlPutOp::CurlPutOp(XrdCl::ResponseHandler *handler, std::shared_ptr<XrdCl::ResponseHandler> default_handler,
    const std::string &url, XrdCl::Buffer &&buffer, struct timespec timeout,
    XrdCl::Log *logger, CreateConnCalloutType callout, HeaderCallout *header_callout)
    : CurlOperation(handler, url, timeout, logger, callout, header_callout),
    m_owned_buffer(std::move(buffer)),
    m_data(buffer.GetBuffer(), buffer.GetSize()),
    m_default_handler(default_handler)
{

}

void
CurlPutOp::Fail(uint16_t errCode, uint32_t errNum, const std::string &msg)
{
    std::string custom_msg = msg;
    SetDone(true);
    if (m_handler == nullptr && m_default_handler == nullptr) {return;}
    if (!custom_msg.empty()) {
        m_logger->Debug(kLogXrdClCurl, "PUT operation at offset %llu failed with message: %s", static_cast<long long unsigned>(m_offset), msg.c_str());
        custom_msg += " (write operation at offset " + std::to_string(static_cast<long long unsigned>(m_offset)) + ")";
    } else {
        m_logger->Debug(kLogXrdClCurl, "PUT operation at offset %llu failed with status code %d", static_cast<long long unsigned>(m_offset), errNum);
    }
    auto status = new XrdCl::XRootDStatus(XrdCl::stError, errCode, errNum, custom_msg);
    auto handle = m_handler;
    m_handler = nullptr;
    if (handle) handle->HandleResponse(status, nullptr);
    else m_default_handler->HandleResponse(status, nullptr);
}

bool
CurlPutOp::Setup(CURL *curl, CurlWorker &worker)
{
    m_curl_handle = curl;
    if (!CurlOperation::Setup(curl, worker)) return false;

    curl_easy_setopt(m_curl.get(), CURLOPT_UPLOAD, 1);
    curl_easy_setopt(m_curl.get(), CURLOPT_READDATA, this);
    curl_easy_setopt(m_curl.get(), CURLOPT_READFUNCTION, CurlPutOp::ReadCallback);
    if (m_object_size >= 0) {
        curl_easy_setopt(m_curl.get(), CURLOPT_INFILESIZE_LARGE, m_object_size);
    }
    return true;
}

void
CurlPutOp::ReleaseHandle()
{
    curl_easy_setopt(m_curl.get(), CURLOPT_READFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_READDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_UPLOAD, 0);
    // If one uses just `-1` here -- instead of casting it to `curl_off_t`, then on Linux
    // we have observed compilers casting the `-1` to an unsigned, resulting in the file
    // size being set to 4294967295 instead of "unknown".  This causes the second use of the
    // handle to claim to upload a large file, resulting in the client hanging while waiting
    // for more input data (which will never come).
    curl_easy_setopt(m_curl.get(), CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(-1));
    CurlOperation::ReleaseHandle();
}

void
CurlPutOp::Pause()
{
    SetPaused(true);
    if (m_handler == nullptr && m_default_handler == nullptr) {
        m_logger->Warning(kLogXrdClCurl, "Put operation paused with no callback handler");
        return;
    }
    auto handle = m_handler;
    auto status = new XrdCl::XRootDStatus();
    m_handler = nullptr;
    m_owned_buffer.Free();
    // Note: As soon as this is invoked, another thread may continue and start to manipulate
    // the CurlPutOp object.  To avoid race conditions, all reads/writes to member data must
    // be done *before* the callback is invoked.
    if (handle) handle->HandleResponse(status, nullptr);
    else m_default_handler->HandleResponse(status, nullptr);
}

void
CurlPutOp::Success()
{
    SetDone(false);
    if (m_handler == nullptr) {
        m_logger->Warning(kLogXrdClCurl, "Put operation succeeded with no callback handler");
        return;
    }
    auto status = new XrdCl::XRootDStatus();
    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(status, nullptr);
}

bool
CurlPutOp::ContinueHandle()
{
    if (!m_curl_handle) {
		return false;
	}

	CURLcode rc;
	if ((rc = curl_easy_pause(m_curl_handle, CURLPAUSE_CONT)) != CURLE_OK) {
		m_logger->Error(kLogXrdClCurl, "Failed to continue a paused handle: %s", curl_easy_strerror(rc));
		return false;
	}
	SetPaused(false);
	return m_curl_handle;
}

bool
CurlPutOp::Continue(std::shared_ptr<CurlOperation> op, XrdCl::ResponseHandler *handler, const char *buffer, size_t buffer_size)
{
    if (op.get() != this) {
        Fail(XrdCl::errInternal, 0, "Interface error: must provide shared pointer to self");
        return false;
    }
    m_handler = handler;
    m_data = std::string_view(buffer, buffer_size);
    if (!buffer_size)
    {
        m_final = true;
    }

    try {
        m_continue_queue->Produce(op);
    } catch (...) {
        Fail(XrdCl::errInternal, ENOMEM, "Failed to continue the curl operation");
        return false;
    }
    return true;
}

bool
CurlPutOp::Continue(std::shared_ptr<CurlOperation> op, XrdCl::ResponseHandler *handler, XrdCl::Buffer &&buffer)
{
    if (op.get() != this) {
        Fail(XrdCl::errInternal, 0, "Interface error: must provide shared pointer to self");
        return false;
    }
    m_handler = handler;
    m_data = std::string_view(buffer.GetBuffer(), buffer.GetSize());
    if (!buffer.GetSize())
    {
        m_final = true;
    }

    try {
        m_continue_queue->Produce(op);
    } catch (...) {
        Fail(XrdCl::errInternal, ENOMEM, "Failed to continue the curl operation");
        return false;
    }
    return true;
}

size_t CurlPutOp::ReadCallback(char *buffer, size_t size, size_t n, void *v) {
	// The callback gets the void pointer that we set with CURLOPT_READDATA. In
	// this case, it's a pointer to an HTTPRequest::Payload struct that contains
	// the data to be sent, along with the offset of the data that has already
	// been sent.
	auto op = static_cast<CurlPutOp*>(v);
    //op->m_logger->Debug(kLogXrdClCurl, "Read callback with buffer %ld and avail data %ld", size*n, op->m_data.size());

    // TODO: Check for timeouts.  If there was one, abort the callback function
    // and cause the curl worker thread to handle it.

	if (op->m_data.empty()) {
		if (op->m_final) {
			return 0;
		} else {
			op->Pause();
			return CURL_READFUNC_PAUSE;
		}
	}

	size_t request = size * n;
	op->UpdateBytes(request);
	if (request > op->m_data.size()) {
		request = op->m_data.size();
	}

	memcpy(buffer, op->m_data.data(), request);
	op->m_data = op->m_data.substr(request);

	return request;
}
