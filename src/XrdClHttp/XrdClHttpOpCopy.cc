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

#include <string>

using namespace XrdClHttp;
using namespace std::string_literals;

CurlCopyOp::CurlCopyOp(XrdCl::ResponseHandler *handler, const std::string &source_url, const Headers &source_hdrs,
    const std::string &dest_url, const Headers &dest_hdrs, const Headers &connection_hdrs, TpcMode mode, struct timespec timeout, XrdCl::Log *logger,
    CreateConnCalloutType callout) :
        CurlOperation(handler, mode == TpcMode::Pull ? dest_url : source_url, timeout, logger, callout, nullptr)
    {
        m_minimum_rate = 1;

        // The headers of the endpoint the client contacts go on the request
        // itself. The headers of the remote endpoint are forwarded by that
        // endpoint, thus they need the 'TransferHeader' prefix.
        const Headers &regular_hdrs  = mode == TpcMode::Pull ? dest_hdrs : source_hdrs;
        const Headers &transfer_hdrs = mode == TpcMode::Pull ? source_hdrs : dest_hdrs;

        if (mode == TpcMode::Pull)
            m_headers_list.emplace_back("Source"s, source_url);
        else
            m_headers_list.emplace_back("Destination"s, dest_url);

        std::copy(connection_hdrs.begin(), connection_hdrs.end(), std::back_inserter(m_headers_list));
        std::copy(regular_hdrs.begin(),    regular_hdrs.end(),    std::back_inserter(m_headers_list));

        for (const auto &info : transfer_hdrs) {
            m_headers_list.emplace_back("TransferHeader"s + info.first, info.second);
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

    void
    CurlCopyOp::SetProgressHandler(XrdCl::ProgressHandler *handler) noexcept
    {
        m_progress_handler = handler;
    }

    size_t
    CurlCopyOp::WriteCallback(char *buffer, size_t size, size_t nitems, void *this_ptr)
    {
        auto me = reinterpret_cast<CurlCopyOp*>(this_ptr);
        me->UpdateBytes(size * nitems);
        std::string_view str_data(buffer, size * nitems);
        size_t end_line;
        while ((end_line = std::min(str_data.size(), str_data.find('\n'))) > 0) {

            auto cur_line = str_data.substr(0, end_line);

            if (me->m_line_buffer.empty()) {
                me->HandleLine(cur_line);
            } else {
                me->m_line_buffer += cur_line;
                me->HandleLine(me->m_line_buffer);
                me->m_line_buffer.clear();
            }

            if (end_line == str_data.size())
                break;

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
            if (m_bytemark > -1 && m_progress_handler) {
                m_progress_handler->HandleProgress(static_cast<std::size_t>(m_bytemark));
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
    