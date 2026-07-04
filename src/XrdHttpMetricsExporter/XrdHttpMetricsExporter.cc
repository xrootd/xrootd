/******************************************************************************/
/*                                                                            */
/*          X r d H t t p M e t r i c s E x p o r t e r . c c                  */
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

#include <chrono>
#include <cstring>
#include <unistd.h>

#ifdef XRDMETRICS_HAVE_CURL
#include <curl/curl.h>
#include "XrdHttpMetricsExporter/XrdHttpMetricsExporterPush.hh"
#endif

#include "XrdHttpMetricsExporter/XrdHttpMetricsExporter.hh"
#include "XrdMetrics/XrdMetricsConfig.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdVersion.hh"

namespace
{
// Content type for the Prometheus text exposition format (version 0.0.4).
//
const char *promType = "Content-Type: text/plain; version=0.0.4; charset=utf-8";

// Content type for the OpenTelemetry OTLP/JSON encoding.
//
const char *otelType = "Content-Type: application/json";

// The serialize-time subsystem filter: a group is emitted only when the shared
// configuration enables it (this also honours the master switch).
//
XrdMetrics::Collector::GroupFilter SubsystemFilter()
{
   return [](const std::string &group)
          {return XrdMetrics::Config::Instance().subsystemEnabled(group);};
}
}

/******************************************************************************/
/*                         C o n s t r u c t o r                              */
/******************************************************************************/

XrdHttpMetricsExporter::XrdHttpMetricsExporter(XrdSysError *eDest,
                                               const char *confg,
                                               const char *parms)
                      : m_log(eDest)
{
// Ensure the shared configuration is loaded. The server core loads it early
// (before any family is created); this is an idempotent fallback for when the
// plugin is used outside that path, and a no-op once the core has loaded it.
//
   XrdMetrics::Config &cfg = XrdMetrics::Config::Instance();
   cfg.Load(confg, eDest);

// The served path defaults to the configured value; the optional plugin
// parameter overrides it but must start with a slash to be a valid request path.
//
   m_path = cfg.path;
   if (parms && *parms == '/') m_path = parms;

// Resolve the instance name (configured value or hostname). It labels both the
// Pushgateway series and the OTLP resource.
//
   m_instance = cfg.instance;
   if (m_instance.empty())
      {char host[256]; host[0] = 0;
       gethostname(host, sizeof(host)-1);
       m_instance = host;
      }

// Register a liveness gauge so the endpoint always returns at least one series,
// even before any other subsystem has registered metrics.
//
   XrdMetrics::Default().subsystem("metrics").gauge<std::int64_t>("endpoint_up", "1 if the metrics endpoint is configured") = 1;

   if (m_log) m_log->Say("Config metrics endpoint at ", m_path.c_str());

// Start the background pushers for any configured destinations.
//
   if (!cfg.pushURL.empty() || !cfg.otelURL.empty())
      {
#ifdef XRDMETRICS_HAVE_CURL
       if (!cfg.pushURL.empty())
          {m_pushThread = std::thread(&XrdHttpMetricsExporter::PushLoop, this);
           if (m_log) m_log->Say("Config metrics push (Pushgateway) to ",
                                 cfg.pushURL.c_str());
          }
       if (!cfg.otelURL.empty())
          {m_otelThread = std::thread(&XrdHttpMetricsExporter::OtelLoop, this);
           if (m_log) m_log->Say("Config metrics push (OTLP) to ",
                                 cfg.otelURL.c_str());
          }
#else
       if (m_log) m_log->Say("Config warning: metrics push requires libcurl; "
                             "ignoring metrics.pushurl/metrics.otelurl");
#endif
      }
}

/******************************************************************************/
/*                          D e s t r u c t o r                               */
/******************************************************************************/

XrdHttpMetricsExporter::~XrdHttpMetricsExporter()
{
   m_stop = true;
   m_pushCV.notify_all();
   if (m_pushThread.joinable()) m_pushThread.join();
   if (m_otelThread.joinable()) m_otelThread.join();
}

/******************************************************************************/
/*                             P u s h L o o p                                */
/******************************************************************************/

void XrdHttpMetricsExporter::PushLoop()
{
#ifdef XRDMETRICS_HAVE_CURL
   XrdMetrics::Config &cfg = XrdMetrics::Config::Instance();

// Build the per-instance gateway URL: <base>/metrics/job/<job>/instance/<inst>
//
   std::string url = XrdMetrics::Config::PushgatewayURL(cfg.pushURL, cfg.pushJob,
                                                        m_instance);
   int every = cfg.pushEvery;
   auto filter = SubsystemFilter();

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL* curl = curl_easy_init();
   if (!curl) return;

   while(!m_stop)
        {std::unique_lock<std::mutex> lk(m_pushMtx);
         m_pushCV.wait_for(lk, std::chrono::seconds(every),
                           [this]{return m_stop.load();});
         lk.unlock();
         if (m_stop) break;

         std::string body;
         XrdMetrics::PrometheusTextSerializer ser(body);
         XrdMetrics::CollectorRegistry::instance().serialize(ser, filter);
         for (auto *r : XrdMetrics::CollectorRegistry::instance().collectors()) r->runTextCollectors(body);

         CURLcode rc = XrdHttpMetricsExporterPush::Send(curl, url, promType,
                                                        true, body);
         if (rc != CURLE_OK && m_log)
            m_log->Say("Metrics push (Pushgateway) failed: ",
                       curl_easy_strerror(rc));
        }

   curl_easy_cleanup(curl);
#endif
}

/******************************************************************************/
/*                             O t e l L o o p                                */
/******************************************************************************/

void XrdHttpMetricsExporter::OtelLoop()
{
#ifdef XRDMETRICS_HAVE_CURL
   XrdMetrics::Config &cfg = XrdMetrics::Config::Instance();

// Carry the server identity into the OTLP Resource so each datapoint can be
// attributed to this server instance. CollectorRegistry::serialize() adds each
// Collector's own global labels (program/role/cluster) to its resource block
// on top of these.
//
   char host[256]; host[0] = 0;
   gethostname(host, sizeof(host)-1);
   std::vector<XrdMetrics::ConstLabel> resource =
      {{"service.name", "xrootd"}, {"service.instance.id", m_instance},
       {"service.version", XrdVERSION}, {"host.name", host}};
   std::string url = cfg.otelURL;
   int every = cfg.otelEvery;
   auto filter = SubsystemFilter();

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL* curl = curl_easy_init();
   if (!curl) return;

   while(!m_stop)
        {std::unique_lock<std::mutex> lk(m_pushMtx);
         m_pushCV.wait_for(lk, std::chrono::seconds(every),
                           [this]{return m_stop.load();});
         lk.unlock();
         if (m_stop) break;

         std::string body;
         XrdMetrics::OtelJsonSerializer ser(body, "xrootd", resource);
         XrdMetrics::CollectorRegistry::instance().serialize(ser, filter);

         CURLcode rc = XrdHttpMetricsExporterPush::Send(curl, url,
                                                        otelType, false, body);
         if (rc != CURLE_OK && m_log)
            m_log->Say("Metrics push (OTLP) failed: ", curl_easy_strerror(rc));
        }

   curl_easy_cleanup(curl);
#endif
}

/******************************************************************************/
/*                          M a t c h e s P a t h                             */
/******************************************************************************/

bool XrdHttpMetricsExporter::MatchesPath(const char *verb, const char *path)
{
   return !strcmp(verb, "GET") && path && m_path == path;
}

/******************************************************************************/
/*                           P r o c e s s R e q                              */
/******************************************************************************/

int XrdHttpMetricsExporter::ProcessReq(XrdHttpExtReq &req)
{
   if (req.verb != "GET")
      return req.SendSimpleResp(405, nullptr, nullptr,
                                "Only GET is supported for metrics.", 0);

// Cache the scrape counter handle once (creating a family per request would
// register a duplicate each time); the static init is thread-safe.
//
   static XrdMetrics::Counter<std::uint64_t>& scrapes =
      XrdMetrics::Default().subsystem("metrics")
         .counter<std::uint64_t>("scrapes_total",
                                 "Number of times the metrics endpoint has been scraped");
   static XrdMetrics::Counter<std::uint64_t>& rate_limited =
      XrdMetrics::Default().subsystem("metrics")
         .counter<std::uint64_t>("scrapes_rate_limited_total",
                                 "Scrapes rejected by the per-second rate limit");
   static XrdMetrics::Counter<std::uint64_t>& cache_hits =
      XrdMetrics::Default().subsystem("metrics")
         .counter<std::uint64_t>("scrapes_cache_hits_total",
                                 "Scrapes served from the TTL cache");
   ++scrapes;

   XrdMetrics::Config &cfg = XrdMetrics::Config::Instance();
   auto now = std::chrono::steady_clock::now();
   using Seconds = std::chrono::seconds;

   std::unique_lock<std::mutex> lk(m_scrapeMtx);

   // Rate limiter: fixed 1-second window; reset on window expiry.
   if (m_windowStart == std::chrono::steady_clock::time_point{} ||
       now - m_windowStart >= Seconds(1))
      {m_windowStart = now; m_reqInWindow = 0;}
   ++m_reqInWindow;
   if (cfg.scrapeRateLimit > 0 && m_reqInWindow > cfg.scrapeRateLimit)
      {lk.unlock();
       ++rate_limited;
       return req.SendSimpleResp(503, nullptr, "Retry-After: 1",
                                 "Too many scrape requests.", 0);
      }

   // TTL cache: serve the cached body if still fresh.
   if (cfg.scrapeTTL > 0 && !m_cachedBody.empty() &&
       now - m_cacheTime < Seconds(cfg.scrapeTTL))
      {std::string body = m_cachedBody;
       lk.unlock();
       ++cache_hits;
       return req.SendSimpleResp(200, nullptr, promType, body.c_str(),
                                 (long long)body.size());
      }

   // Fresh serialization; lock is held to prevent a thundering herd on miss.
   std::string body;
   XrdMetrics::PrometheusTextSerializer ser(body);
   XrdMetrics::CollectorRegistry::instance().serialize(ser, SubsystemFilter());
   for (auto *r : XrdMetrics::CollectorRegistry::instance().collectors())
       r->runTextCollectors(body);
   m_cachedBody = body;
   m_cacheTime  = now;
   lk.unlock();

   return req.SendSimpleResp(200, nullptr, promType, body.c_str(),
                             (long long)body.size());
}

/******************************************************************************/
/*                    X r d H t t p G e t E x t H a n d l e r                 */
/******************************************************************************/

extern "C" XrdHttpExtHandler *XrdHttpGetExtHandler(XrdHttpExtHandlerArgs)
{
   return new XrdHttpMetricsExporter(eDest, confg, parms);
}

XrdVERSIONINFO(XrdHttpGetExtHandler, XrdHttpMetricsExporter);
