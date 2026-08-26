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
#include "XrdClHttpWebDav.hh"

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <tinyxml.h>

using namespace XrdClHttp;

CurlListdirOp::CurlListdirOp(XrdCl::ResponseHandler *handler, const std::string &url, const std::string &host_addr,
    bool set_response_info, struct timespec timeout, XrdCl::Log *logger, CreateConnCalloutType callout,
    HeaderCallout *header_callout) :
    CurlOperation(handler, url, timeout, logger, callout, header_callout),
    m_response_info(set_response_info),
    m_host_addr(host_addr)
{
    m_minimum_rate = 1024.0 * 1;
}

bool
CurlListdirOp::Setup(CURL *curl, CurlWorker &worker)
{
    if (!CurlOperation::Setup(curl, worker)) return false;
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, CurlListdirOp::WriteCallback);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "PROPFIND");
    m_headers_list.emplace_back("Depth", "1");

    return true;
}

void
CurlListdirOp::ReleaseHandle()
{
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);
    CurlOperation::ReleaseHandle();
}

size_t
CurlListdirOp::WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
{
    auto me = static_cast<CurlListdirOp*>(this_ptr);
    if (size * nitems + me->m_response.size() > 10'000'000) {
        return me->FailCallback(kXR_ServerError, "Response too large for PROPFIND operation");
    }
    me->UpdateBytes(size * nitems);
    me->m_response.append(buffer, size * nitems);
    return size * nitems;
}

std::pair<CurlListdirOp::DavEntry, bool>
CurlListdirOp::ParseResponse(TiXmlElement *response)
{
    DavEntry entry;
    bool success = false;
    for (auto child = response->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (WebDavElementNameEquals(child, "href")) {
            auto href = child->GetText();
            if (href == nullptr) {
                return {entry, false};
            }
            // NOTE: This is not particularly robust; it assumes that the server is only returning
            // a depth of exactly one.
            std::string_view href_str(href);
            auto first_non_slash = href_str.find_last_not_of('/');
            if (first_non_slash != std::string_view::npos) {
                href_str = href_str.substr(0, first_non_slash + 1);
            }
            auto last_slash = href_str.find_last_of('/');
            if (last_slash != std::string_view::npos) {
                entry.m_name = href_str.substr(last_slash + 1);
            } else {
                entry.m_name = href;
            }
            continue;
        }
    }
    WebDavProperties properties;
    success = ParseWebDavResponseProperties(response, properties);
    if (!success) return {entry, false};
    entry.m_isdir = properties.m_is_dir;
    entry.m_isexec = properties.m_is_executable;
    entry.m_size = properties.m_size;
    entry.m_lastmodified = properties.m_last_modified;
    return {entry, success};
}

void
CurlListdirOp::Success()
{
    SetDone(false);
    m_logger->Debug(kLogXrdClHttp, "CurlListdirOp::Success");

    std::unique_ptr<XrdCl::DirectoryList> dirlist(m_response_info ? new DirectoryListResponse() : new XrdCl::DirectoryList());

    TiXmlDocument doc;
    doc.Parse(m_response.c_str());
    if (doc.Error()) {
        m_logger->Error(kLogXrdClHttp, "Failed to parse XML response: %s", m_response.substr(0, 1024).c_str());
        Fail(XrdCl::errErrorResponse, kXR_FSError, "Server responded to directory listing with invalid XML");
        return;
    }

    auto elem = doc.RootElement();
    if (!WebDavElementNameEquals(elem, "multistatus")) {
        m_logger->Error(kLogXrdClHttp, "Unexpected XML response: %s", m_response.substr(0, 1024).c_str());
        Fail(XrdCl::errErrorResponse, kXR_FSError, "Server responded to directory listing unexpected XML root");
        return;
    }
    bool skip = true;
    for (auto response = elem->FirstChildElement(); response != nullptr; response = response->NextSiblingElement()) {
        if (!WebDavElementNameEquals(response, "response")) {
            continue;
        }

        auto [entry, success] = ParseResponse(response);
        if (!success) {
            m_logger->Error(kLogXrdClHttp, "Failed to parse response element in XML response: %s", m_response.substr(0, 1024).c_str());
            Fail(XrdCl::errErrorResponse, kXR_FSError, "Server responded with invalid directory listing");
            return;
        }
        // Skip the first entry in the response, which is the directory itself
        if (skip) {
            skip = false;
        } else {
            uint32_t flags = XrdCl::StatInfo::Flags::IsReadable;
            if (entry.m_isdir) {
                flags |= XrdCl::StatInfo::Flags::IsDir;
            }
            if (entry.m_isexec) {
                flags |= XrdCl::StatInfo::Flags::XBitSet;
            }
            dirlist->Add(new XrdCl::DirectoryList::ListEntry(m_host_addr, entry.m_name, new XrdCl::StatInfo("nobody", entry.m_size, flags, entry.m_lastmodified)));
        }
    }

    m_logger->Debug(kLogXrdClHttp, "Successful propfind directory listing operation on %s (%u items)", m_url.c_str(), static_cast<unsigned>(dirlist->GetSize()));
    if (m_handler == nullptr) {return;}

    if (m_response_info) {
        static_cast<DirectoryListResponse*>(dirlist.get())->SetResponseInfo(MoveResponseInfo());
    }
    auto obj = new XrdCl::AnyObject();
    obj->Set(dirlist.release());

    auto handle = m_handler;
    m_handler = nullptr;
    handle->HandleResponse(new XrdCl::XRootDStatus(), obj);
}
