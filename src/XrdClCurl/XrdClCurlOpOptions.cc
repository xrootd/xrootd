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

void
CurlOptionsOp::Fail(uint16_t errCode, uint32_t errNum, const std::string &etext) {
    CurlOperation::Fail(errCode, errNum, etext);
    auto &cache = VerbsCache::Instance();
    cache.Put(m_url, VerbsCache::HttpVerbs(VerbsCache::HttpVerb::kUnknown));
    // 405 Method Not Supported; indicates that OPTIONS is not understood so
    // we assume it's highly unlikely that non-HTTP verbs are supported (like
    // PROPFIND); however, we don't fail the parent operation.
    //
    // Since the OPTIONS command is considered advisory, we ignore failures.
}

bool
CurlOptionsOp::Setup(CURL *curl, CurlWorker &worker) {
    if (!CurlOperation::Setup(curl, worker)) return false;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, "OPTIONS");
    curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 1L);

    return true;
}

void
CurlOptionsOp::Success() {
    auto &cache = VerbsCache::Instance();
    cache.Put(m_url, m_headers.GetAllowedVerbs());
}

void
CurlOptionsOp::ReleaseHandle()
{
    if (m_curl == nullptr) return;
    curl_easy_setopt(m_curl.get(), CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(m_curl.get(), CURLOPT_NOBODY, 0L);
    CurlOperation::ReleaseHandle();
}