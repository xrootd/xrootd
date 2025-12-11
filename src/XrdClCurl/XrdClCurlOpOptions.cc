/******************************************************************************/
/* Copyright (C) 2025, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClCurl client plugin for XRootD.               */
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