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

using namespace XrdClHttp;

CurlCopyOp::CurlCopyOp(XrdCl::ResponseHandler *handler, const std::string &source_url, const Headers &source_hdrs,
    const std::string &dest_url, const Headers &dest_hdrs, struct timespec timeout, XrdCl::Log *logger,
    CreateConnCalloutType callout) :
        CurlOperation(handler, dest_url, timeout, logger, callout, nullptr),
        m_source_url(source_url)
    {
        m_minimum_rate = 1;
    
        for (const auto &info : source_hdrs) {
            m_headers_list.emplace_back(std::string("TransferHeader") + info.first, info.second);
        }
        for (const auto &info : dest_hdrs) {
            m_headers_list.emplace_back(info.first, info.second);
        }
    }
    
    bool
    CurlCopyOp::Setup(CURL *curl, CurlWorker &worker)
    {
        auto rv = CurlOperation::Setup(curl, worker);
        if (!rv) return false;

        curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, CurlCopyOp::WriteCallback);
        curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, this);
        curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "COPY");
        m_headers_list.emplace_back("Source", m_source_url);

        return true;
    }
    
    void
    CurlCopyOp::Success()
    {
        SetDone(false);
        if (m_handler == nullptr) {return;}
        auto status = new XrdCl::XRootDStatus();
        auto obj = new XrdCl::AnyObject();
        auto handle = m_handler;
        m_handler = nullptr;
        handle->HandleResponse(status, obj);
    }
    
    void
    CurlCopyOp::ReleaseHandle()
    {
        if (m_curl == nullptr) return;
        curl_easy_setopt(m_curl.get(), CURLOPT_WRITEFUNCTION, nullptr);
        curl_easy_setopt(m_curl.get(), CURLOPT_WRITEDATA, nullptr);
        curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
        curl_easy_setopt(m_curl.get(), CURLOPT_HTTPHEADER, nullptr);
        curl_easy_setopt(m_curl.get(), CURLOPT_XFERINFOFUNCTION, nullptr);
        CurlOperation::ReleaseHandle();
    }
    
    size_t
    CurlCopyOp::WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
    {
        auto me = reinterpret_cast<CurlCopyOp*>(this_ptr);
        me->UpdateBytes(size * nitems);
        std::string_view str_data(buffer, size * nitems);
        size_t end_line;
        while ((end_line = str_data.find('\n')) != std::string_view::npos) {
            auto cur_line = str_data.substr(0, end_line);
            if (me->m_line_buffer.empty()) {
                me->HandleLine(cur_line);
            } else {
                me->m_line_buffer += cur_line;
                me->HandleLine(me->m_line_buffer);
                me->m_line_buffer.clear();
            }
            str_data = str_data.substr(end_line + 1);
        }
        me->m_line_buffer = str_data;
    
        return size * nitems;
    }
    
    void
    CurlCopyOp::HandleLine(std::string_view line)
    {
        if (line == "Perf Marker") {
            m_bytemark = -1;
        } else if (line == "End") {
            if (m_bytemark > -1 && m_callback) {
                m_callback->Progress(m_bytemark);
            }
        } else {
            auto key_end_pos = line.find(':');
            if (key_end_pos == line.npos) {
                return; // All the other callback lines should be of key: value format
            }
            auto key = line.substr(0, key_end_pos);
            auto value = ltrim_view(line.substr(key_end_pos + 1));
            if (key == "Stripe Bytes Transferred") {
                try {
                    m_bytemark = std::stoll(std::string(value));
                } catch (...) {
                    // TODO: Log failure
                }
            } else if (key == "success") {
                m_sent_success = true;
            } else if (key == "failure") {
                m_failure = value;
            }
        }
    }
    