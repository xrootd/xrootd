/******************************************************************************/
/*                                                                            */
/*                       X r d M o n O t l p . c c                            */
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

#include "XrdApps/XrdMonCollect/XrdMonOtlp.hh"

using json = nlohmann::json;

/******************************************************************************/
/*                       X r d M o n O t l p B a t c h                        */
/******************************************************************************/

namespace
{
// Encode one JSON scalar (or, as a fallback, a nested value) as an OTLP AnyValue.
// 64-bit integers are strings per proto3 JSON; nested objects/arrays are encoded
// as their JSON text under stringValue (a pragmatic choice that keeps the export
// flat and lossless without an explicit kvlist/array mapping).
//
json toAnyValue(const json& v)
{
   if (v.is_string())           return json{{"stringValue", v.get<std::string>()}};
   if (v.is_boolean())          return json{{"boolValue",   v.get<bool>()}};
   if (v.is_number_unsigned())
      return json{{"intValue", std::to_string(v.get<std::uint64_t>())}};
   if (v.is_number_integer())
      return json{{"intValue", std::to_string(v.get<std::int64_t>())}};
   if (v.is_number_float())     return json{{"doubleValue", v.get<double>()}};
   return json{{"stringValue", v.dump()}};
}

// Re-encode a flat dotted-key object (the collector's resource/attributes) as an
// OTLP KeyValue array: [{"key":"file.path","value":{"stringValue":"..."}}, ...].
//
json toKeyValues(const json& obj)
{
   json arr = json::array();
   if (obj.is_object())
      for (auto it = obj.begin(); it != obj.end(); ++it)
          arr.push_back({{"key", it.key()}, {"value", toAnyValue(it.value())}});
   return arr;
}

// A present string field, else empty.
std::string strField(const json& doc, const char* key)
{
   auto it = doc.find(key);
   return (it != doc.end() && it->is_string()) ? it->get<std::string>()
                                               : std::string();
}
}

void XrdMonOtlpBatch::add(const json& doc)
{
   const bool isSpan = doc.contains("kind");
   auto& groups = isSpan ? spanGroups : logGroups;

   const json  empty = json::object();
   const json& res   = doc.contains("resource") ? doc["resource"] : empty;
   Group& g = groups[res.dump()];
   if (g.records.empty()) g.resource = toKeyValues(res);   // once per resource

   json rec;
   json attrs = doc.contains("attributes") ? toKeyValues(doc["attributes"])
                                           : json::array();
   if (isSpan)
      {// The span document already carries OTLP-compatible name/kind/status and
       // the *UnixNano times; pass them through and attach the id linkage.
       rec["traceId"] = strField(doc, "traceId");
       rec["spanId"]  = strField(doc, "spanId");
       std::string parent = strField(doc, "parentSpanId");
       if (!parent.empty()) rec["parentSpanId"] = parent;
       if (doc.contains("name"))              rec["name"] = doc["name"];
       if (doc.contains("kind"))              rec["kind"] = doc["kind"];
       if (doc.contains("startTimeUnixNano")) rec["startTimeUnixNano"] = doc["startTimeUnixNano"];
       if (doc.contains("endTimeUnixNano"))   rec["endTimeUnixNano"]   = doc["endTimeUnixNano"];
       if (doc.contains("status"))            rec["status"] = doc["status"];
       rec["attributes"] = std::move(attrs);
      }
      else
      {if (doc.contains("timeUnixNano"))         rec["timeUnixNano"] = doc["timeUnixNano"];
       if (doc.contains("observedTimeUnixNano")) rec["observedTimeUnixNano"] = doc["observedTimeUnixNano"];
       if (doc.contains("severityNumber"))       rec["severityNumber"] = doc["severityNumber"];
       if (doc.contains("severityText"))         rec["severityText"] = doc["severityText"];
       std::string tr = strField(doc, "traceId"), sp = strField(doc, "spanId");
       if (!tr.empty()) rec["traceId"] = tr;
       if (!sp.empty()) rec["spanId"]  = sp;
       // A human-readable body from the event name (also kept in attributes).
       auto ait = doc.find("attributes");
       if (ait != doc.end())
          {auto eit = ait->find("event.name");
           if (eit != ait->end() && eit->is_string())
              rec["body"] = json{{"stringValue", eit->get<std::string>()}};
          }
       rec["attributes"] = std::move(attrs);
      }

   g.records.push_back(std::move(rec));
}

std::string XrdMonOtlpBatch::takeBody(std::map<std::string, Group>& groups,
                                      const char* resourceKey,
                                      const char* scopeKey,
                                      const char* recordsKey)
{
   json blocks = json::array();
   for (auto& kv : groups)
       {json scope;
        scope["scope"]["name"] = "xrdmoncollect";
        scope[recordsKey]      = std::move(kv.second.records);

        json block;
        block["resource"]["attributes"] = std::move(kv.second.resource);
        block[scopeKey] = json::array({std::move(scope)});
        blocks.push_back(std::move(block));
       }
   groups.clear();

   json body;
   body[resourceKey] = std::move(blocks);
   return body.dump();
}

std::string XrdMonOtlpBatch::takeLogsBody()
{
   return takeBody(logGroups, "resourceLogs", "scopeLogs", "logRecords");
}

std::string XrdMonOtlpBatch::takeTracesBody()
{
   return takeBody(spanGroups, "resourceSpans", "scopeSpans", "spans");
}

/******************************************************************************/
/*                           X r d M o n O t l p                              */
/******************************************************************************/

#ifdef XRDMON_HAVE_CURL

#include <ctime>
#include <unistd.h>

#include <curl/curl.h>

namespace
{
size_t otlpDiscardCB(char* ptr, size_t sz, size_t nm, void* userp)
{
   ((std::string*)userp)->append(ptr, sz * nm);
   return sz * nm;
}
}

XrdMonOtlp::XrdMonOtlp(const std::string& url, const std::string& token,
                       bool insec)
          : curl(nullptr), insecure(insec), maxRetry(4)
{
   std::string base = url;
   if (!base.empty() && base.back() == '/') base.pop_back();
   logsURL   = base + "/v1/logs";
   tracesURL = base + "/v1/traces";
   if (!token.empty()) authHdr = "Authorization: Bearer " + token;
}

XrdMonOtlp::~XrdMonOtlp()
{
   if (curl) curl_easy_cleanup((CURL*)curl);
}

bool XrdMonOtlp::Init(std::string& err)
{
   curl_global_init(CURL_GLOBAL_DEFAULT);
   curl = curl_easy_init();
   if (!curl) {err = "failed to create curl handle"; return false;}
   return true;
}

bool XrdMonOtlp::post(const std::string& url, const std::string& body,
                      std::string& err)
{
   CURL* c = (CURL*)curl;

   struct curl_slist* hdrs = nullptr;
   hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
   if (!authHdr.empty()) hdrs = curl_slist_append(hdrs, authHdr.c_str());

   int  backoff = 1;
   bool ok = false;
   for (int attempt = 0; attempt <= maxRetry; attempt++)
       {std::string resp;
        curl_easy_reset(c);
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, otlpDiscardCB);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
        if (insecure)
           {curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
           }

        CURLcode rc = curl_easy_perform(c);
        long code = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

        if (rc == CURLE_OK && code >= 200 && code < 300) {err.clear(); ok = true; break;}

        if (rc != CURLE_OK) err = curl_easy_strerror(rc);
           else err = "HTTP " + std::to_string(code) + " from OTLP endpoint";

        bool transient = (rc != CURLE_OK) || code == 429 || (code >= 500);
        if (!transient || attempt == maxRetry) break;
        sleep(backoff);
        if (backoff < 16) backoff *= 2;
       }

   curl_slist_free_all(hdrs);
   return ok;
}

bool XrdMonOtlp::PostLogs(const std::string& body, std::string& err)
{
   return post(logsURL, body, err);
}

bool XrdMonOtlp::PostTraces(const std::string& body, std::string& err)
{
   return post(tracesURL, body, err);
}

#endif
