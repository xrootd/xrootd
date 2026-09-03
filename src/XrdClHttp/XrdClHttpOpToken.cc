/******************************************************************************/
/* Copyright (C) 2026 by European Organization for Nuclear Research (CERN)   */
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
/******************************************************************************/

#include "XrdClHttpOps.hh"
#include "XrdClHttpToken.hh"

#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClLog.hh>

#include <limits>
#include <utility>

using namespace XrdClHttp;

CurlTokenOp::CurlTokenOp(XrdCl::ResponseHandler *handler,
                         std::shared_ptr<XrdCl::ResponseHandler> handler_owner,
                         const std::string &url,
                         HttpVerb verb,
                         HeaderList headers,
                         const std::string &request_body,
                         const std::string &response_key,
                         struct timespec timeout, XrdCl::Log *logger,
                         CreateConnCalloutType callout)
    : CurlOperation(handler, url, timeout, logger, callout, nullptr),
      m_handler_owner(std::move(handler_owner)),
      m_verb(verb),
      m_request_body(request_body),
      m_response_key(response_key)
{
    m_operation_expiry = m_header_expiry;
    m_headers_list = std::move(headers);
}

CurlTokenOp::CurlTokenOp(XrdCl::ResponseHandler *handler,
                         std::shared_ptr<XrdCl::ResponseHandler> handler_owner,
                         const std::string &url,
                         HttpVerb verb,
                         HeaderList headers,
                         const std::string &request_body,
                         const std::string &response_key,
                         std::chrono::steady_clock::time_point expiry,
                         XrdCl::Log *logger,
                         CreateConnCalloutType callout)
    : CurlOperation(handler, url, expiry, logger, callout, nullptr),
      m_handler_owner(std::move(handler_owner)),
      m_verb(verb),
      m_request_body(request_body),
      m_response_key(response_key)
{
    m_operation_expiry = m_header_expiry;
    m_headers_list = std::move(headers);
}

CurlTokenOp::CurlTokenOp(XrdCl::ResponseHandler *handler,
                         const std::string &url,
                         const std::string &request_body,
                         struct timespec timeout, XrdCl::Log *logger,
                         CreateConnCalloutType callout)
    : CurlTokenOp(handler, {}, url, HttpVerb::POST,
          {{"Content-Type", "application/macaroon-request"}}, request_body,
          "macaroon", timeout, logger, callout)
{
}

bool
CurlTokenOp::Setup(CURL *curl, CurlWorker &worker)
{
    if (!CurlOperation::Setup(curl, worker)) return false;

    if (m_verb == HttpVerb::POST) {
        curl_easy_setopt(m_curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS,
                         m_request_body.data());
        curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(m_request_body.size()));
    } else if (m_verb == HttpVerb::GET) {
        // Set this explicitly: handles are pooled and a prior user may have
        // configured a request body.
        curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS, nullptr);
        curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(-1));
        curl_easy_setopt(m_curl.get(), CURLOPT_HTTPGET, 1L);
    } else {
        Fail(XrdCl::errInternal, 0,
             "Unsupported HTTP verb for token request");
        return false;
    }
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION,
                     CurlTokenOp::WriteCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this);
    return true;
}

void
CurlTokenOp::ReleaseHandle()
{
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDS, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                     static_cast<curl_off_t>(-1));
    // CURLOPT_POSTFIELDS selects POST even when its value is null.  Restore a
    // neutral GET state last so a pooled handle cannot turn a later read into
    // an empty POST.
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    CurlOperation::ReleaseHandle();
}

CurlOperation::RedirectAction
CurlTokenOp::Redirect(std::string &target)
{
    auto action = CurlOperation::Redirect(target);
    if (action == RedirectAction::Fail) return action;

    std::string normalized;
    if (!NormalizeTokenUrl(target, normalized)) {
        Fail(XrdCl::errErrorResponse, kXR_ServerError,
             "Refusing to redirect a token request to a non-HTTPS URL");
        return RedirectAction::Fail;
    }

    target = std::move(normalized);
    curl_easy_setopt(m_curl.get(), CURLOPT_URL, target.c_str());
    m_response.clear();
    return action;
}

void
CurlTokenOp::Success()
{
    if (GetStatusCode() != 200) {
        Fail(XrdCl::errErrorResponse, kXR_ServerError,
             "Token endpoint returned an unexpected HTTP status");
        return;
    }

    std::string value;
    std::string error;
    if (!ParseJsonStringResponse(m_response, m_response_key, value, error)) {
        Fail(XrdCl::errErrorResponse, kXR_ServerError, error);
        return;
    }

    SetDone(false);
    if (m_handler == nullptr) return;

    auto response = new XrdCl::Buffer();
    response->FromString(value);
    auto object = new XrdCl::AnyObject();
    object->Set(response);

    auto handler = m_handler;
    m_handler = nullptr;
    handler->HandleResponse(new XrdCl::XRootDStatus(), object);
}

size_t
CurlTokenOp::WriteCallback(char *buffer, size_t size, size_t nitems,
                           void *this_ptr)
{
    auto operation = static_cast<CurlTokenOp *>(this_ptr);
    if (size != 0 && nitems > std::numeric_limits<size_t>::max() / size) {
        return operation->FailCallback(kXR_ServerError,
            "Token response exceeds maximum size");
    }
    return operation->Write(buffer, size * nitems);
}

size_t
CurlTokenOp::Write(const char *buffer, size_t length)
{
    UpdateBytes(length);
    if (length >= kMaxTokenResponseSize ||
        m_response.size() >= kMaxTokenResponseSize - length) {
        return FailCallback(kXR_ServerError,
                            "Token response exceeds maximum size");
    }
    m_response.append(buffer, length);
    return length;
}
