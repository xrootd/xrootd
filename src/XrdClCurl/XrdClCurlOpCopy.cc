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

using namespace XrdClCurl;

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
    