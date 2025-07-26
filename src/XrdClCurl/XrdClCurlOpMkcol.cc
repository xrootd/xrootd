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

#include <XrdCl/XrdClLog.hh>

using namespace XrdClCurl;

CurlMkcolOp::CurlMkcolOp(XrdCl::ResponseHandler *handler, const std::string &url,
        struct timespec timeout, XrdCl::Log *logger,
        bool response_info, CreateConnCalloutType callout,
        HeaderCallout *header_callout)
    : CurlOperation(handler, url, timeout, logger, callout, header_callout)
{}

CurlMkcolOp::~CurlMkcolOp() {}

void
CurlMkcolOp::Fail(uint16_t errCode, uint32_t errNum, const std::string &msg)
{
    // Note: the generic status code handler maps HTTP status "405 Method Not Allowed"
    // to kXR_InvalidRequest.
    //
    // However, for the MKCOL operation, 405 maps better to kXR_ItExists
    if (errCode == XrdCl::errErrorResponse &&  errNum == kXR_InvalidRequest && GetStatusCode() == 405) {
        m_logger->Debug(kLogXrdClCurl, "MKCOL was performed on a directory that exists");
        errNum = kXR_ItExists;
    }
    CurlOperation::Fail(errCode, errNum, msg);
}

void
CurlMkcolOp::ReleaseHandle() {
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    CurlOperation::ReleaseHandle();
}

bool
CurlMkcolOp::Setup(CURL *curl, CurlWorker &worker) {
    if (!CurlOperation::Setup(curl, worker)) return false;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "MKCOL");

    return true;
}

void
CurlMkcolOp::Success() {
    SetDone(false);
    m_logger->Debug(kLogXrdClCurl, "CurlMkcolOp::Success");
    if (m_handler == nullptr) {return;}

    XrdCl::AnyObject *obj{nullptr};
    if (m_response_info) {
        auto info = new XrdClCurl::MkdirResponseInfo();
        info->SetResponseInfo(MoveResponseInfo());
        obj = new XrdCl::AnyObject();
        obj->Set(info);
    }

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
}
