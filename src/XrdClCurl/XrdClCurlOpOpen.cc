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

#include "XrdClCurlFile.hh"
#include "XrdClCurlOps.hh"
#include "XrdClCurlResponseInfo.hh"
#include "XrdClCurlResponses.hh"

#include <XrdCl/XrdClLog.hh>

using namespace XrdClCurl;

CurlOpenOp::CurlOpenOp(XrdCl::ResponseHandler *handler, const std::string &url, struct timespec timeout,
    XrdCl::Log *logger, XrdClCurl::File *file, bool response_info, CreateConnCalloutType callout,
    HeaderCallout *header_callout)
:
    CurlStatOp(handler, url, timeout, logger, response_info, callout, header_callout),
    m_file(file)
{}

void
CurlOpenOp::ReleaseHandle()
{
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_OPENSOCKETDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_SOCKOPTDATA, nullptr);
    CurlStatOp::ReleaseHandle();
}

void
CurlOpenOp::SetOpenProperties(bool setSize)
{
    char *url = nullptr;
    curl_easy_getinfo(m_curl.get(), CURLINFO_EFFECTIVE_URL, &url);
    if (url && m_file) {
        m_file->SetProperty("LastURL", url);
    }

    if (setSize) {
        auto [size, isdir] = GetStatInfo();
        if (!isdir && size >= 0) {
            m_file->SetProperty("XrdClCurlPrefetchSize", std::to_string(size));
        }
    }

    if (!m_headers.GetETag().empty())
    {
        std::string etag = m_headers.GetETag();
        m_file->SetProperty("ETag", etag);
    }
    m_file->SetProperty("Cache-Control", m_headers.GetCacheControl());
}

void
CurlOpenOp::Success()
{
    SetDone(false);
    SetOpenProperties(true);
    auto [size, isdir] = GetStatInfo();
    if (isdir) {
        m_logger->Error(kLogXrdClCurl, "Cannot open a directory");
        Fail(XrdCl::errErrorResponse, kXR_isDirectory, "Cannot open a directory");
        return;
    }
    if (size >= 0) {
        m_file->SetProperty("ContentLength", std::to_string(size));
    }
    SuccessImpl(false);
}

void
CurlOpenOp::Fail(uint16_t errCode, uint32_t errNum, const std::string &msg)
{
    // Note: OpenFlags::New is equivalent to O_CREAT | O_EXCL; OpenFlags::Write is equivalent to O_WRONLY | O_CREAT;
    // OpenFlags::Delete is equivalent to O_CREAT | O_TRUNC;
    if (errCode == XrdCl::errErrorResponse &&  errNum == kXR_NotFound && (m_file->Flags() & (XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write | XrdCl::OpenFlags::Delete))) {
        m_logger->Debug(kLogXrdClCurl, "CurlOpenOp succeeds as 404 was expected");
        SetOpenProperties(false);
        m_file->SetProperty("ContentLength", "0");
        SuccessImpl(false);
        return;
    }
    CurlOperation::Fail(errCode, errNum, msg);
}

void CurlPrefetchOpenOp::Pause()
{
    if (m_first_pause) {
        m_first_pause = false;
    } else {
        CurlReadOp::Pause();
        return;
    }

    // Set the various file-open properties.  Note that we only invoke Pause() if the status code
    // of the response is 200.
    char *url = nullptr;
    curl_easy_getinfo(m_curl.get(), CURLINFO_EFFECTIVE_URL, &url);
    if (url) {
        m_file.SetProperty("LastURL", url);
    }

    auto length = m_headers.GetContentLength();
    m_file.SetProperty("XrdClCurlPrefetchSize", std::to_string(length));

    if (!m_headers.GetETag().empty())
    {
        std::string etag = m_headers.GetETag();
        m_file.SetProperty("ETag", etag);
    }
    m_file.SetProperty("Cache-Control", m_headers.GetCacheControl());

    CurlReadOp::Pause();
}
