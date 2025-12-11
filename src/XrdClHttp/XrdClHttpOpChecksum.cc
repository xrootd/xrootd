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
#include "XrdClHttpResponses.hh"
#include "XrdClHttpUtil.hh"

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <iomanip>
#include <sstream>

using namespace XrdClHttp;

CurlChecksumOp::CurlChecksumOp(XrdCl::ResponseHandler *handler, const std::string &url, XrdClHttp::ChecksumType preferred,
    struct timespec timeout, XrdCl::Log *logger, bool response_info, CreateConnCalloutType callout,
    HeaderCallout *header_callout)
:
    CurlStatOp(handler, url, timeout, logger, response_info, callout, header_callout),
    m_preferred_cksum(preferred)
{}

// Override to prevent the parent CurlStatOp from switching verb to PROPFIND
void
CurlChecksumOp::OptionsDone()
{}

bool
CurlChecksumOp::Setup(CURL *curl, CurlWorker &worker)
{
    auto rv = CurlStatOp::Setup(curl, worker);
    if (!rv) return false;

    curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 1L);
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);

    m_headers_list.emplace_back("Want-Digest", XrdClHttp::HeaderParser::ChecksumTypeToDigestName(m_preferred_cksum));

    return true;
}

CurlOperation::RedirectAction
CurlChecksumOp::Redirect(std::string &target)
{
    auto result = CurlOperation::Redirect(target);
    curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 1L);
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    return result;
}

void
CurlChecksumOp::ReleaseHandle()
{
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);

    CurlStatOp::ReleaseHandle();
}

void
CurlChecksumOp::Success()
{
    SetDone(false);
    auto checksums = m_headers.GetChecksums();

    std::array<unsigned char, XrdClHttp::g_max_checksum_length> value;
    auto type = XrdClHttp::ChecksumType::kUnknown;
    if (checksums.IsSet(m_preferred_cksum)) {
        value = checksums.Get(m_preferred_cksum);
        type = m_preferred_cksum;
    } else {
        bool isset;
        std::tie(type, value, isset) = checksums.GetFirst();
        if (!isset) {
            m_logger->Error(kLogXrdClHttp, "Checksums not found in response for %s", m_url.c_str());
            auto handle = m_handler;
            m_handler = nullptr;
            handle->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errCheckSumError), nullptr);
            return; 
        }
    }
    std::stringstream ss;
    for (size_t idx = 0; idx < XrdClHttp::GetChecksumLength(type); ++idx) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value[idx]);
    }

    std::string response = XrdClHttp::GetTypeString(type) + " " + ss.str();
    auto buf = new XrdClHttp::QueryResponse();
    buf->FromString(response);
    buf->SetResponseInfo(MoveResponseInfo());
    
    auto obj = new XrdCl::AnyObject();
    obj->Set(static_cast<XrdCl::Buffer*>(buf));

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
    // Does not call CurlStatOp::Success() as we don't need to invoke a stat info callback
}
