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

CurlDeleteOp::CurlDeleteOp(XrdCl::ResponseHandler *handler, const std::string &url,
        struct timespec timeout, XrdCl::Log *logger,
        bool response_info, CreateConnCalloutType callout,
        HeaderCallout *header_callout)
    : CurlOperation(handler, url, timeout, logger, callout, header_callout)
{}

CurlDeleteOp::~CurlDeleteOp() {}

void
CurlDeleteOp::ReleaseHandle() {
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    CurlOperation::ReleaseHandle();
}

bool
CurlDeleteOp::Setup(CURL *curl, CurlWorker &worker) {
    if (!CurlOperation::Setup(curl, worker)) return false;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");

    return true;
}

void
CurlDeleteOp::Success() {
    SetDone(false);
    m_logger->Debug(kLogXrdClCurl, "CurlDeleteOp::Success");
    if (m_handler == nullptr) {return;}

    XrdCl::AnyObject *obj{nullptr};
    if (m_response_info) {
        auto info = new XrdClCurl::DeleteResponseInfo();
        info->SetResponseInfo(MoveResponseInfo());
        obj = new XrdCl::AnyObject();
        obj->Set(info);
    }

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
}
