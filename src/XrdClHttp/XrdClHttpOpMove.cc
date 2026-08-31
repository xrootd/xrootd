/******************************************************************************/
/* Copyright (C) 2026, XRootD Collaboration                                  */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/******************************************************************************/

#include "XrdClHttpOps.hh"

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClURL.hh>

using namespace XrdClHttp;

CurlMoveOp::CurlMoveOp(XrdCl::ResponseHandler *handler,
    const std::string &source, const std::string &destination,
    struct timespec timeout, XrdCl::Log *logger,
    CreateConnCalloutType callout, HeaderCallout *header_callout) :
    CurlOperation(handler, source, timeout, logger, callout, header_callout)
{
    HttpClientConfig ignored;
    m_destination = ExtractHttpClientConfig(destination, ignored);
    if (m_destination.compare(0, 6, "dav://") == 0)
        m_destination = "http://" + m_destination.substr(6);
    else if (m_destination.compare(0, 7, "davs://") == 0)
        m_destination = "https://" + m_destination.substr(7);
    m_operation_expiry = m_header_expiry;
}

bool CurlMoveOp::Setup(CURL *curl, CurlWorker &worker) {
    if (!CurlOperation::Setup(curl, worker)) return false;
    XrdCl::URL source(m_url);
    XrdCl::URL destination(m_destination);
    if (!source.IsValid() || !destination.IsValid() ||
        source.GetProtocol() != destination.GetProtocol() ||
        source.GetHostName() != destination.GetHostName() ||
        source.GetPort() != destination.GetPort()) {
        m_logger->Error(kLogXrdClHttp,
            "WebDAV MOVE destination must use the source origin");
        return false;
    }
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "MOVE");
    m_headers_list.emplace_back("Destination", m_destination);
    m_headers_list.emplace_back("Overwrite", "F");
    return true;
}

void CurlMoveOp::Fail(uint16_t errCode, uint32_t errNum,
                      const std::string &msg) {
    if (GetStatusCode() == 412) errNum = kXR_ItExists;
    CurlOperation::Fail(errCode, errNum, msg);
}

void CurlMoveOp::Success() {
    if (GetStatusCode() == 207) {
        Fail(XrdCl::errErrorResponse, kXR_ServerError,
            "WebDAV MOVE completed only partially (207 Multi-Status)");
        return;
    }
    SetDone(false);
    if (!m_handler) return;
    auto handler = m_handler;
    m_handler = nullptr;
    handler->HandleResponse(new XrdCl::XRootDStatus(), nullptr);
}

void CurlMoveOp::ReleaseHandle() {
    if (!m_curl) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    CurlOperation::ReleaseHandle();
}
