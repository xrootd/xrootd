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
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
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
}

/******************************************************************************/
/*                         C o n s t r u c t o r                              */
/******************************************************************************/

XrdHttpMetricsExporter::XrdHttpMetricsExporter(XrdSysError *eDest,
                                               const char *confg,
                                               const char *parms)
                      : m_log(eDest)
{
// The optional plugin parameter overrides the served path. It must start with
// a slash to be a valid request path; otherwise the default is kept.
//
   if (parms && *parms == '/') m_cfg.path = parms;

// Read metrics.* directives from the configuration file.
//
   Configure(confg);

// Resolve the shared instance name (configured value or hostname). It labels
// both the Pushgateway series and the OTLP resource.
//
   if (m_cfg.instance.empty())
      {char host[256]; host[0] = 0;
       gethostname(host, sizeof(host)-1);
       m_cfg.instance = host;
      }

// Register a liveness gauge so the endpoint always returns at least one series,
// even before any other subsystem has registered metrics.
//
   XrdMetrics::Default().group("metrics").intGauge("endpoint_up", {}, {},
            "1 if the metrics endpoint is configured").noLabels() = 1;

   if (m_log) m_log->Say("Config metrics endpoint at ", m_cfg.path.c_str());

// Start the background pushers for any configured destinations.
//
   if (!m_cfg.pushURL.empty() || !m_cfg.otelURL.empty())
      {
#ifdef XRDMETRICS_HAVE_CURL
       if (!m_cfg.pushURL.empty())
          {m_pushThread = std::thread(&XrdHttpMetricsExporter::PushLoop, this);
           if (m_log) m_log->Say("Config metrics push (Pushgateway) to ",
                                 m_cfg.pushURL.c_str());
          }
       if (!m_cfg.otelURL.empty())
          {m_otelThread = std::thread(&XrdHttpMetricsExporter::OtelLoop, this);
           if (m_log) m_log->Say("Config metrics push (OTLP) to ",
                                 m_cfg.otelURL.c_str());
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
/*                            C o n f i g u r e                               */
/******************************************************************************/

void XrdHttpMetricsExporter::Configure(const char *confg)
{
   if (!confg) return;

   XrdOucGatherConf conf(XrdHttpMetricsExporterCfg::DirectiveList(), m_log);
   if (conf.Gather(confg, XrdOucGatherConf::full_lines) < 0) return;

   XrdHttpMetricsExporterCfg::Parse(conf, m_cfg);
}

/******************************************************************************/
/*                             P u s h L o o p                                */
/******************************************************************************/

void XrdHttpMetricsExporter::PushLoop()
{
#ifdef XRDMETRICS_HAVE_CURL
// Build the per-instance gateway URL: <base>/metrics/job/<job>/instance/<inst>
//
   std::string url = XrdHttpMetricsExporterCfg::PushgatewayURL(
                        m_cfg.pushURL, m_cfg.pushJob, m_cfg.instance);

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL* curl = curl_easy_init();
   if (!curl) return;

   while(!m_stop)
        {std::unique_lock<std::mutex> lk(m_pushMtx);
         m_pushCV.wait_for(lk, std::chrono::seconds(m_cfg.pushEvery),
                           [this]{return m_stop.load();});
         lk.unlock();
         if (m_stop) break;

         std::string body;
         XrdMetrics::PrometheusTextSerializer ser(body);
         XrdMetrics::Default().serialize(ser);
         XrdMetrics::Default().runTextCollectors(body);

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
// Carry the server identity into the OTLP Resource so each datapoint can be
// attributed to this server instance.
//
   std::vector<XrdMetrics::ConstLabel> resource =
      {{"service.name", "xrootd"}, {"service.instance.id", m_cfg.instance}};

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL* curl = curl_easy_init();
   if (!curl) return;

   while(!m_stop)
        {std::unique_lock<std::mutex> lk(m_pushMtx);
         m_pushCV.wait_for(lk, std::chrono::seconds(m_cfg.otelEvery),
                           [this]{return m_stop.load();});
         lk.unlock();
         if (m_stop) break;

         std::string body;
         XrdMetrics::OtelJsonSerializer ser(body, "xrootd", resource);
         XrdMetrics::Default().serialize(ser);

         CURLcode rc = XrdHttpMetricsExporterPush::Send(curl, m_cfg.otelURL,
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
   return !strcmp(verb, "GET") && path && m_cfg.path == path;
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
   static XrdMetrics::Counter& scrapes = XrdMetrics::Default().group("metrics")
            .counter("scrapes_total", {}, {},
                     "Number of times the metrics endpoint has been scraped")
            .noLabels();
   ++scrapes;

   std::string body;
   XrdMetrics::PrometheusTextSerializer ser(body);
   XrdMetrics::Default().serialize(ser);
   XrdMetrics::Default().runTextCollectors(body);

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
