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

#include <XrdCl/XrdClLog.hh>

using namespace XrdClHttp;

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
    m_logger->Debug(kLogXrdClHttp, "CurlDeleteOp::Success");
    if (m_handler == nullptr) {return;}

    XrdCl::AnyObject *obj{nullptr};
    if (m_response_info) {
        auto info = new XrdClHttp::DeleteResponseInfo();
        info->SetResponseInfo(MoveResponseInfo());
        obj = new XrdCl::AnyObject();
        obj->Set(info);
    }

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
}
