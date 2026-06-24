/******************************************************************************/
/*                                                                            */
/*                 X r d M o n O p e n S e a r c h . c c                      */
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
/******************************************************************************/

#include <ctime>
#include <unistd.h>

#include <curl/curl.h>

#include "XrdApps/XrdMonCollect/XrdMonOpenSearch.hh"

namespace
{
// Collect the HTTP response body so per-item bulk errors can be detected.
size_t writeCB(char* ptr, size_t sz, size_t nm, void* userp)
{
   ((std::string*)userp)->append(ptr, sz * nm);
   return sz * nm;
}
}

XrdMonOpenSearch::XrdMonOpenSearch(const std::string& url,
                                   const std::string& index,
                                   const std::string& user,
                                   const std::string& pass, bool insec)
                : curl(nullptr), idx(index), insecure(insec), maxRetry(4)
{
   bulkURL = url;
   if (!bulkURL.empty() && bulkURL.back() == '/') bulkURL.pop_back();
   bulkURL += "/_bulk";
   if (!user.empty()) {userpwd = user; userpwd += ':'; userpwd += pass;}
}

XrdMonOpenSearch::~XrdMonOpenSearch()
{
   if (curl) curl_easy_cleanup((CURL*)curl);
}

bool XrdMonOpenSearch::Init(std::string& err)
{
   curl_global_init(CURL_GLOBAL_DEFAULT);
   curl = curl_easy_init();
   if (!curl) {err = "failed to create curl handle"; return false;}
   return true;
}

void XrdMonOpenSearch::Add(std::string& batch, const std::string& jsonDoc) const
{
   batch += "{\"index\":{\"_index\":\"";
   batch += idx;
   batch += "\"}}\n";
   batch += jsonDoc;
   batch += '\n';
}

bool XrdMonOpenSearch::Bulk(const std::string& body, std::string& err)
{
   CURL* c = (CURL*)curl;

   struct curl_slist* hdrs = nullptr;
   hdrs = curl_slist_append(hdrs, "Content-Type: application/x-ndjson");

   int backoff = 1;
   bool ok = false;
   for (int attempt = 0; attempt <= maxRetry; attempt++)
       {std::string resp;
        curl_easy_reset(c);
        curl_easy_setopt(c, CURLOPT_URL, bulkURL.c_str());
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCB);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
        if (!userpwd.empty()) curl_easy_setopt(c, CURLOPT_USERPWD, userpwd.c_str());
        if (insecure)
           {curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
           }

        CURLcode rc = curl_easy_perform(c);
        long code = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

        if (rc == CURLE_OK && code >= 200 && code < 300)
           {// A 2xx still reports per-item failures via "errors":true.
            if (resp.find("\"errors\":true") != std::string::npos)
               err = "OpenSearch reported per-item errors in the bulk response";
               else err.clear();
            ok = true;
            break;
           }

        // Retry transient conditions (network error, 429, 5xx).
        if (rc != CURLE_OK)
           err = curl_easy_strerror(rc);
           else err = "HTTP " + std::to_string(code) + " from OpenSearch";

        bool transient = (rc != CURLE_OK) || code == 429 || (code >= 500);
        if (!transient || attempt == maxRetry) break;
        sleep(backoff);
        if (backoff < 16) backoff *= 2;
       }

   curl_slist_free_all(hdrs);
   return ok;
}
