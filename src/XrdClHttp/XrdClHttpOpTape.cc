/******************************************************************************/
/*                                                                            */
/*                  X r d C l H t t p O p T a p e . c c                       */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
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
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#include "XrdClHttpOps.hh"
#include "XrdClHttpTape.hh"
#include "XrdClHttpWorker.hh"

#include <XrdCl/XrdClAnyObject.hh>
#include <XrdCl/XrdClDefaultEnv.hh>

#include <cerrno>
#include <cstdlib>
#include <exception>
#include <stdexcept>

using namespace XrdClHttp;

namespace
{
class TapeError : public std::runtime_error
{
public:
    TapeError(uint16_t statusCode, uint32_t code,
              const std::string &message):
        std::runtime_error(message), m_statusCode(statusCode), m_code(code)
    {
    }

    uint16_t statusCode() const { return m_statusCode; }
    uint32_t code() const { return m_code; }

private:
    uint16_t m_statusCode;
    uint32_t m_code;
};

TapeError GetTapeError(const std::exception_ptr &exception,
                       const std::string &context)
{
    try
    {
        if(exception) std::rethrow_exception(exception);
    }
    catch(const TapeError &ex)
    {
        return ex;
    }
    catch(...)
    {
    }

    const auto ex = DescribeException(exception, context);
    return TapeError(XrdCl::errInternal, ex.code, ex.message);
}
}

CurlTapeOp::CurlTapeOp(XrdCl::ResponseHandler *handler,
    const std::string &url, std::unique_ptr<TapeOperation> tape,
    struct timespec timeout, XrdCl::Log *logger,
    CreateConnCalloutType callout, HeaderCallout *header_callout)
:
    CurlOperation(handler, url, timeout, logger, callout, header_callout),
    m_tape(std::move(tape))
{
}

CurlTapePrepareOp::CurlTapePrepareOp(XrdCl::ResponseHandler *handler,
    const std::string &url, const std::vector<std::string> &file_list,
    XrdCl::PrepareFlags::Flags flags, struct timespec timeout,
    XrdCl::Log *logger, CreateConnCalloutType callout,
    HeaderCallout *header_callout)
:
    CurlTapeOp(handler, url,
        std::make_unique<TapeOperation>(url, file_list, flags), timeout,
        logger, callout, header_callout)
{
}

CurlTapeQueryOp::CurlTapeQueryOp(XrdCl::ResponseHandler *handler,
    const std::string &url, XrdCl::QueryCode::Code query_code,
    const XrdCl::Buffer &arg, struct timespec timeout,
    XrdCl::Log *logger, CreateConnCalloutType callout,
    HeaderCallout *header_callout)
:
    CurlTapeOp(handler, url,
        std::make_unique<TapeOperation>(url, query_code, arg), timeout,
        logger, callout, header_callout)
{
}

CurlTapeOp::~CurlTapeOp() = default;

bool
CurlTapeOp::Setup(CURL *curl, CurlWorker &worker)
{
    try
    {
        const auto status = m_tape->Start(m_request);
        if(!status.IsOK())
        {
            throw TapeError(status.code, status.errNo,
                            status.GetErrorMessage());
        }

        m_request_url = m_request.url;
        if(!CurlOperation::Setup(curl, worker))
        {
            throw TapeError(XrdCl::errInternal, EIO,
                "failed to initialize " + RequestDescription());
        }
        m_worker = &worker;
        if(!ConfigureRequest())
        {
            throw TapeError(XrdCl::errInternal, EIO,
                "failed to configure " + RequestDescription());
        }
        return true;
    }
    catch(...)
    {
        if(!m_curl) m_curl.reset(curl);
        const auto ex = GetTapeError(std::current_exception(),
            "while preparing a Tape REST request");
        Fail(ex.statusCode(), ex.code(), ex.what());
    }
    return false;
}

bool
CurlTapeOp::ConfigureRequest()
{
    if(!m_curl) return false;

    m_response.clear();
    m_headers_list.emplace_back("Accept", "application/json");

    bool hasX509Credential = false;
    auto env = XrdCl::DefaultEnv::GetEnv();
    if(m_worker && ClientX509Enabled(env))
    {
        const auto [cert, key] = m_worker->ClientX509CertKeyFile();
        hasX509Credential = !cert.empty() && !key.empty();
    }
    const std::string token = GetBearerToken();
    const char *configuredProtocols = std::getenv("XrdSecPROTOCOL");
    if(ShouldUseBearerToken(
         configuredProtocols ? configuredProtocols : "",
         hasX509Credential, !token.empty()))
    {
        m_headers_list.emplace_back("Authorization", "Bearer " + token);
    }

    if(curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION,
                        CurlTapeOp::WriteCallback) != CURLE_OK
       || curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this) != CURLE_OK)
    {
        return false;
    }
#if CURL_AT_LEAST_VERSION(7, 85, 0)
    if(curl_easy_setopt(m_curl.get(), CURLOPT_PROTOCOLS_STR, "https,http")
         != CURLE_OK
       || curl_easy_setopt(m_curl.get(), CURLOPT_REDIR_PROTOCOLS_STR,
                           "https,http") != CURLE_OK)
    {
        return false;
    }
#else
    const long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS;
    if(curl_easy_setopt(m_curl.get(), CURLOPT_PROTOCOLS, protocols) != CURLE_OK
       || curl_easy_setopt(m_curl.get(), CURLOPT_REDIR_PROTOCOLS, protocols)
            != CURLE_OK)
    {
        return false;
    }
#endif

    switch(m_request.method)
    {
        case TapeHttpMethod::Get:
            if(curl_easy_setopt(m_curl.get(), CURLOPT_HTTPGET, 1L) != CURLE_OK)
              return false;
            break;
        case TapeHttpMethod::Post:
            m_headers_list.emplace_back("Content-Type", "application/json");
            if(curl_easy_setopt(m_curl.get(), CURLOPT_POST, 1L) != CURLE_OK
               || curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS,
                                   m_request.body.c_str()) != CURLE_OK
               || curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                   static_cast<curl_off_t>(m_request.body.size())) != CURLE_OK)
            {
                return false;
            }
            break;
        case TapeHttpMethod::Delete:
            if(curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE")
                 != CURLE_OK)
              return false;
            break;
    }
    return true;
}

void
CurlTapeOp::Success()
{
    try
    {
        TapeHttpRequest nextRequest;
        std::string response;
        bool complete = false;
        const auto status = m_tape->Advance(
            m_curl.get(), GetStatusCode(), m_response,
            nextRequest, response, complete);
        if(!status.IsOK())
        {
            throw TapeError(status.code, status.errNo,
                            status.GetErrorMessage());
        }
        if(complete)
        {
            Complete(response);
            return;
        }

        m_request = std::move(nextRequest);
        if(!m_worker)
        {
            throw TapeError(XrdCl::errInternal, EIO,
                "worker unavailable before starting " + RequestDescription());
        }
        if(!SetupNextRequest(m_request.url, *m_worker))
        {
            throw TapeError(XrdCl::errInternal, EIO,
                "failed to reset the curl handle for " + RequestDescription());
        }
        if(!ConfigureRequest())
        {
            throw TapeError(XrdCl::errInternal, EIO,
                "failed to configure " + RequestDescription());
        }
        if(!FinishSetup(m_curl.get()))
        {
            throw TapeError(XrdCl::errInternal, EIO,
                "failed to configure headers for " + RequestDescription());
        }
    }
    catch(...)
    {
        const auto ex = GetTapeError(std::current_exception(),
            "while processing " + RequestDescription());
        Fail(ex.statusCode(), ex.code(), ex.what());
    }
}

void
CurlTapeOp::Fail(uint16_t errCode, uint32_t errNum,
                 const std::string &message)
{
    std::string detail = message;
    if(GetStatusCode() >= 400)
    {
        detail = RequestDescription() + " failed: "
            + TapeProblemResponse(GetStatusCode(), m_response);
    }
    CurlOperation::Fail(errCode, errNum, detail);
}

CurlOperation::HttpVerb
CurlTapeOp::GetVerb() const
{
    switch(m_request.method)
    {
        case TapeHttpMethod::Get: return HttpVerb::GET;
        case TapeHttpMethod::Post: return HttpVerb::POST;
        case TapeHttpMethod::Delete: return HttpVerb::DELETE;
    }
    return HttpVerb::GET;
}

std::string
CurlTapeOp::RequestDescription() const
{
    return "Tape REST " + GetVerbString(GetVerb()) + " " + m_request.url;
}

void
CurlTapeOp::ReleaseHandle()
{
    if(m_curl == nullptr) return;

    curl_easy_setopt(m_curl.get(), CURLOPT_ERRORBUFFER, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_POST, 0L);
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                     static_cast<curl_off_t>(0));
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    CurlOperation::ReleaseHandle();
}

size_t
CurlTapeOp::WriteCallback(char *buffer, size_t size, size_t nitems, void *data)
{
    return static_cast<CurlTapeOp *>(data)->Write(buffer, size * nitems);
}

size_t
CurlTapeOp::Write(char *buffer, size_t size)
{
    try
    {
        m_response.append(buffer, size);
        UpdateBytes(size);
        return size;
    }
    catch(...)
    {
        const auto ex = DescribeException(std::current_exception(),
            "while buffering a Tape REST response");
        return FailCallback(kXR_ServerError, ex.message);
    }
}

void
CurlTapeOp::Complete(const std::string &response)
{
    if(m_handler == nullptr)
    {
        SetDone(false);
        return;
    }

    auto buffer = std::make_unique<XrdCl::Buffer>();
    buffer->FromString(response);

    auto obj = std::make_unique<XrdCl::AnyObject>();
    auto status = std::make_unique<XrdCl::XRootDStatus>();
    obj->Set(buffer.release());

    SetDone(false);
    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(status.release(), obj.release());
}
