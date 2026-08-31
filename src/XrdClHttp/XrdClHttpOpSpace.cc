/******************************************************************************/
/* Copyright (C) 2026, XRootD Collaboration                                  */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/******************************************************************************/

#include "XrdClHttpOps.hh"
#include "XrdClHttpWebDav.hh"

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <tinyxml.h>

#include <limits>

using namespace XrdClHttp;

CurlSpaceOp::CurlSpaceOp(XrdCl::ResponseHandler *handler,
    const std::string &url, struct timespec timeout, XrdCl::Log *logger,
    CreateConnCalloutType callout, HeaderCallout *header_callout) :
    CurlOperation(handler, url, timeout, logger, callout, header_callout),
    m_request("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
              "<d:propfind xmlns:d=\"DAV:\"><d:prop>"
              "<d:quota-available-bytes/><d:quota-used-bytes/>"
              "</d:prop></d:propfind>")
{
    m_operation_expiry = m_header_expiry;
}

bool CurlSpaceOp::Setup(CURL *curl, CurlWorker &worker) {
    if (!CurlOperation::Setup(curl, worker)) return false;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "PROPFIND");
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS, m_request.c_str());
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE,
        static_cast<long>(m_request.size()));
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, CurlSpaceOp::WriteCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this);
    m_headers_list.emplace_back("Depth", "0");
    m_headers_list.emplace_back("Content-Type", "application/xml; charset=utf-8");
    return true;
}

size_t CurlSpaceOp::WriteCallback(char *buffer, size_t size, size_t nitems,
                                  void *this_ptr) {
    auto operation = static_cast<CurlSpaceOp *>(this_ptr);
    const auto bytes = size * nitems;
    if (bytes + operation->m_response.size() > 1024 * 1024)
        return operation->FailCallback(kXR_ServerError,
            "Response too large for WebDAV quota query");
    operation->UpdateBytes(bytes);
    operation->m_response.append(buffer, bytes);
    return bytes;
}

void CurlSpaceOp::Success() {
    TiXmlDocument document;
    document.Parse(m_response.c_str());
    auto root = document.RootElement();
    if (document.Error() || !WebDavElementNameEquals(root, "multistatus")) {
        Fail(XrdCl::errErrorResponse, kXR_FSError,
            "Server returned invalid XML for WebDAV quota query");
        return;
    }
    WebDavQuota quota;
    bool parsed = false;
    for (auto response = root->FirstChildElement(); response;
         response = response->NextSiblingElement()) {
        if (WebDavElementNameEquals(response, "response") &&
            ParseWebDavResponseQuota(response, quota)) {
            parsed = true;
            break;
        }
    }
    if (!parsed || quota.m_used > std::numeric_limits<uint64_t>::max() - quota.m_available) {
        Fail(XrdCl::errErrorResponse, kXR_Unsupported,
            "Server did not return valid WebDAV quota properties");
        return;
    }
    auto total = quota.m_used + quota.m_available;
    auto value = "oss.space=" + std::to_string(total) +
        "&oss.free=" + std::to_string(quota.m_available) +
        "&oss.used=" + std::to_string(quota.m_used) +
        "&oss.maxf=" + std::to_string(quota.m_available);
    auto buffer = new XrdCl::Buffer();
    buffer->FromString(value);
    auto object = new XrdCl::AnyObject();
    object->Set(buffer);
    SetDone(false);
    auto handler = m_handler;
    m_handler = nullptr;
    handler->HandleResponse(new XrdCl::XRootDStatus(), object);
}

void CurlSpaceOp::ReleaseHandle() {
    if (!m_curl) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE, 0L);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    CurlOperation::ReleaseHandle();
}
