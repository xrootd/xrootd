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

#include "XrdClCurlOps.hh"
#include "XrdClCurlResponses.hh"
#include "XrdClCurlUtil.hh"

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <iomanip>
#include <sstream>

using namespace XrdClCurl;

CurlChecksumOp::CurlChecksumOp(XrdCl::ResponseHandler *handler, const std::string &url, XrdClCurl::ChecksumType preferred,
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

    m_headers_list.emplace_back("Want-Digest", XrdClCurl::HeaderParser::ChecksumTypeToDigestName(m_preferred_cksum));

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

    std::array<unsigned char, XrdClCurl::g_max_checksum_length> value;
    auto type = XrdClCurl::ChecksumType::kUnknown;
    if (checksums.IsSet(m_preferred_cksum)) {
        value = checksums.Get(m_preferred_cksum);
        type = m_preferred_cksum;
    } else {
        bool isset;
        std::tie(type, value, isset) = checksums.GetFirst();
        if (!isset) {
            m_logger->Error(kLogXrdClCurl, "Checksums not found in response for %s", m_url.c_str());
            auto handle = m_handler;
            m_handler = nullptr;
            handle->HandleResponse(new XrdCl::XRootDStatus(XrdCl::stError, XrdCl::errCheckSumError), nullptr);
            return; 
        }
    }
    std::stringstream ss;
    for (size_t idx = 0; idx < XrdClCurl::GetChecksumLength(type); ++idx) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value[idx]);
    }

    std::string response = XrdClCurl::GetTypeString(type) + " " + ss.str();
    auto buf = new XrdClCurl::QueryResponse();
    buf->FromString(response);
    buf->SetResponseInfo(MoveResponseInfo());
    
    auto obj = new XrdCl::AnyObject();
    obj->Set(static_cast<XrdCl::Buffer*>(buf));

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
    // Does not call CurlStatOp::Success() as we don't need to invoke a stat info callback
}
