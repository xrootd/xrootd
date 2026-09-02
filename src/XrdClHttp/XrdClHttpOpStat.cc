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

#include <tinyxml.h>

#include <algorithm>
#include <cstring>

using namespace XrdClHttp;

namespace {

bool ElementNameEquals(const TiXmlElement *element, const char *expected)
{
    if (!element || !element->Value()) return false;
    const char *name = element->Value();
    const char *separator = std::strrchr(name, ':');
    return !strcasecmp(separator ? separator + 1 : name, expected);
}

void SetDepthHeader(
    std::vector<std::pair<std::string, std::string>> &headers,
    const std::string &value)
{
    auto depth = std::find_if(headers.begin(), headers.end(),
        [](const auto &header) { return !strcasecmp(header.first.c_str(), "Depth"); });
    if (depth == headers.end()) {
        headers.emplace_back("Depth", value);
    } else {
        depth->second = value;
    }
}

}

// OPTIONS information is available.
//
// Reconfigure curl handle, as necessary, to use PROPFIND
void
CurlStatOp::OptionsDone()
{
    auto &instance = VerbsCache::Instance();
    auto target = m_headers.GetLocation();
    auto verbs = instance.Get(target.empty() ? m_url : target);
    if (verbs.IsSet(VerbsCache::HttpVerb::kPROPFIND)) {
        curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "PROPFIND");
        SetDepthHeader(m_headers_list, "0");
        curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 0L);
        m_is_propfind = true;
        if (!FinishSetup(m_curl.get())) {
            m_logger->Error(kLogXrdClHttp,
                "Failed to apply PROPFIND headers after OPTIONS for %s",
                m_url.c_str());
        }
    } else {
        m_is_propfind = false;
        curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 1L);
    }
}

CurlOperation::RedirectAction
CurlStatOp::Redirect(std::string &target)
{
    auto headers = m_headers;
    auto result = CurlOperation::Redirect(target);
    if (result == CurlOperation::RedirectAction::Fail) {
        return result;
    }
    auto &instance = VerbsCache::Instance();
    auto verbs = instance.Get(target);
    if (verbs.IsSet(VerbsCache::HttpVerb::kUnset)) {
        m_headers = std::move(headers);
        return CurlOperation::RedirectAction::ReinvokeAfterAllow;
    }

    if (verbs.IsSet(VerbsCache::HttpVerb::kPROPFIND)) {
        curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "PROPFIND");
        SetDepthHeader(m_headers_list, "0");
        curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 0L);
        m_is_propfind = true;
        if (!FinishSetup(m_curl.get())) {
            Fail(XrdCl::errInternal, 0,
                 "Failed to apply PROPFIND headers after redirect");
            return CurlOperation::RedirectAction::Fail;
        }
    } else {
        m_is_propfind = false;
        curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 1L);
    }
    return CurlOperation::RedirectAction::Reinvoke;
}

bool
CurlStatOp::Setup(CURL *curl, CurlWorker &worker)
{
    if (!CurlOperation::Setup(curl, worker)) return false;
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, CurlStatOp::WriteCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this);

    auto &instance = VerbsCache::Instance();
    auto verbs = instance.Get(m_url);
    if (verbs.IsSet(VerbsCache::HttpVerb::kPROPFIND)) {
        curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "PROPFIND");
        m_headers_list.emplace_back("Depth", "0");
        curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 0L);
        m_is_propfind = true;
    } else {
        curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 1L);
    }
    return true;
}

void
CurlStatOp::ReleaseHandle()
{
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 0L);
    if (m_is_propfind) {
        curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    }
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    CurlOperation::ReleaseHandle();
}

size_t
CurlStatOp::WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
{
    auto me = static_cast<CurlStatOp*>(this_ptr);
    if (me->m_is_propfind) {
        if (size * nitems + me->m_response.size() > 1'000'000) {
            me->m_logger->Error(kLogXrdClHttp, "Response too large for PROPFIND operation");
            return 0;
        }
        me->UpdateBytes(size * nitems);
        me->m_response.append(buffer, size * nitems);
    }
    return size * nitems;
}

std::pair<int64_t, bool>
CurlStatOp::ParseProp(TiXmlElement *prop) {
    if (prop == nullptr) {
        return {-1, false};
    }
    for (auto child = prop->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (ElementNameEquals(child, "getcontentlength")) {
            auto len = child->GetText();
            if (len) {
                m_length = std::stoll(len);
            }
        } else if (ElementNameEquals(child, "resourcetype")) {
            m_is_dir = false;
            for (auto type = child->FirstChildElement(); type != nullptr;
                 type = type->NextSiblingElement()) {
                if (ElementNameEquals(type, "collection")) {
                    m_is_dir = true;
                    break;
                }
            }
        }
    }
    if (m_length < 0 && m_is_dir) {
        // Don't require length for directories; fake it as zero
        m_length = 0;
    }
    return {m_length, m_is_dir};
}

std::pair<int64_t, bool>
CurlStatOp::GetStatInfo() {
    if (!m_is_propfind) {
        m_length = m_headers.GetContentLength();
        return {m_length, false};
    }
    if (m_length >= 0) {
        return {m_length, m_is_dir};
    }

    TiXmlDocument doc;
    doc.Parse(m_response.c_str());
    if (doc.Error()) {
        m_logger->Error(kLogXrdClHttp, "Failed to parse XML response: %s", m_response.substr(0, 1024).c_str());
        return {-1, false};
    }

    auto elem = doc.RootElement();
    if (!ElementNameEquals(elem, "multistatus")) {
        m_logger->Error(kLogXrdClHttp, "Unexpected XML response: %s", m_response.substr(0, 1024).c_str());
        return {-1, false};
    }
    auto found_response = false;
    for (auto response = elem->FirstChildElement(); response != nullptr; response = response->NextSiblingElement()) {
        if (ElementNameEquals(response, "response")) {
            found_response = true;
            elem = response;
            break;
        }
    }
    if (!found_response) {
        m_logger->Error(kLogXrdClHttp, "Failed to find response element in XML response: %s", m_response.substr(0, 1024).c_str());
        return {-1, false};
    }
    for (auto child = elem->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (!ElementNameEquals(child, "propstat")) {
            continue;
        }
        for (auto prop = child->FirstChildElement(); prop != nullptr; prop = prop->NextSiblingElement()) {
            if (ElementNameEquals(prop, "prop")) {
                return ParseProp(prop);
            }
        }
    }
    m_logger->Error(kLogXrdClHttp, "Failed to find properties in XML response: %s", m_response.substr(0, 1024).c_str());
    return {-1, false};
}

bool CurlStatOp::RequiresOptions() const
{
    auto &instance = VerbsCache::Instance();
    auto verbs = instance.Get(m_url);
    return verbs.IsSet(VerbsCache::HttpVerb::kUnset);
}

void CurlStatOp::Success()
{
    SuccessImpl(true);
}

void
CurlStatOp::SuccessImpl(bool returnObj)
{
    SetDone(false);
    m_logger->Debug(kLogXrdClHttp, "CurlStatOp::Success");
    if (m_handler == nullptr) {return;}
    XrdCl::AnyObject *obj = nullptr;
    if (returnObj) {
        auto [size, isdir] = GetStatInfo();
        if (size < 0) {
            m_logger->Error(kLogXrdClHttp, "Failed to get stat info for %s", m_url.c_str());
            Fail(XrdCl::errErrorResponse, kXR_FSError, "Server responded without object size");
            return;
        }
        if (m_is_propfind) {
            m_logger->Debug(kLogXrdClHttp, "Successful propfind operation on %s (size %lld, isdir %d)", m_url.c_str(), static_cast<long long>(size), isdir);
        } else {
            m_logger->Debug(kLogXrdClHttp, "Successful stat operation on %s (size %lld)", m_url.c_str(), static_cast<long long>(size));
        }

        XrdCl::StatInfo *stat_info;
        if (m_response_info){
            auto info = new XrdClHttp::StatResponse("nobody", size,
            XrdCl::StatInfo::Flags::IsReadable | (isdir ? XrdCl::StatInfo::Flags::IsDir : 0), time(NULL));
            info->SetResponseInfo(MoveResponseInfo());
            stat_info = info;
        } else {
            stat_info = new XrdCl::StatInfo("nobody", size,
            XrdCl::StatInfo::Flags::IsReadable | (isdir ? XrdCl::StatInfo::Flags::IsDir : 0), time(NULL));
        }
        obj = new XrdCl::AnyObject();
        obj->Set(stat_info);
    } else if (m_response_info) {
        auto info = new XrdClHttp::OpenResponseInfo();
        info->SetResponseInfo(MoveResponseInfo());
        obj = new XrdCl::AnyObject();
        obj->Set(info);
    }

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
}
