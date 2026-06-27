/******************************************************************************/
/*                                                                            */
/*               X r d H t t p P r o m e t h e u s . c c                      */
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

#ifdef XRDPROM_HAVE_CURL
#include <curl/curl.h>
#endif

#include "XrdHttpPrometheus/XrdHttpPrometheus.hh"
#include "XrdMetrics/XrdMetrics.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdVersion.hh"

// Content type for the Prometheus text exposition format (version 0.0.4).
//
namespace
{
const char *ctype = "Content-Type: text/plain; version=0.0.4; charset=utf-8";

size_t discardCB(char*, size_t sz, size_t nm, void*) {return sz * nm;}
}

/******************************************************************************/
/*                         C o n s t r u c t o r                              */
/******************************************************************************/

XrdHttpPrometheus::XrdHttpPrometheus(XrdSysError *eDest, const char *confg,
                                     const char *parms)
                 : m_log(eDest), m_path("/metrics")
{
// The optional plugin parameter overrides the served path. It must start with
// a slash to be a valid request path; otherwise the default is kept.
//
   if (parms && *parms == '/') m_path = parms;

// Read prometheus.* directives from the configuration file.
//
   Configure(confg);

// Register a liveness gauge so the endpoint always returns at least one series,
// even before any other subsystem has registered metrics.
//
   XrdMetricsRegistry::Default().Gauge("xrootd_metrics_endpoint_up",
            "1 if the Prometheus metrics endpoint is configured").set(1);

   if (m_log) m_log->Say("Config Prometheus metrics endpoint at ", m_path.c_str());

// Start the Pushgateway pusher if a destination was configured.
//
   if (!m_pushURL.empty())
      {if (m_pushInst.empty())
          {char host[256]; host[0] = 0;
           gethostname(host, sizeof(host)-1);
           m_pushInst = host;
          }
#ifdef XRDPROM_HAVE_CURL
       m_pushThread = std::thread(&XrdHttpPrometheus::PushLoop, this);
       if (m_log) m_log->Say("Config Prometheus push to ", m_pushURL.c_str());
#else
       if (m_log) m_log->Say("Config warning: Prometheus push requires libcurl; "
                             "ignoring prometheus.pushurl");
#endif
      }
}

/******************************************************************************/
/*                          D e s t r u c t o r                               */
/******************************************************************************/

XrdHttpPrometheus::~XrdHttpPrometheus()
{
   m_stop = true;
   m_pushCV.notify_all();
   if (m_pushThread.joinable()) m_pushThread.join();
}

/******************************************************************************/
/*                            C o n f i g u r e                               */
/******************************************************************************/

void XrdHttpPrometheus::Configure(const char *confg)
{
   if (!confg) return;

   XrdOucGatherConf conf("prometheus.path prometheus.pushurl "
                         "prometheus.pushinterval prometheus.pushjob "
                         "prometheus.pushinstance", m_log);
   if (conf.Gather(confg, XrdOucGatherConf::full_lines) < 0) return;

   char* key;
   while(conf.GetLine() && (key = conf.GetToken()))
        {char* val = conf.GetToken();
         if (!val || !*val) continue;
              if (!strcmp(key, "prometheus.path"))         m_path     = val;
         else if (!strcmp(key, "prometheus.pushurl"))      m_pushURL  = val;
         else if (!strcmp(key, "prometheus.pushjob"))      m_pushJob  = val;
         else if (!strcmp(key, "prometheus.pushinstance")) m_pushInst = val;
         else if (!strcmp(key, "prometheus.pushinterval"))
                 {int n = atoi(val); if (n > 0) m_pushEvery = n;}
        }
}

/******************************************************************************/
/*                             P u s h L o o p                                */
/******************************************************************************/

void XrdHttpPrometheus::PushLoop()
{
#ifdef XRDPROM_HAVE_CURL
// Build the per-instance gateway URL: <base>/metrics/job/<job>/instance/<inst>
//
   std::string url = m_pushURL;
   if (!url.empty() && url.back() == '/') url.pop_back();
   url += "/metrics/job/" + m_pushJob + "/instance/" + m_pushInst;

   curl_global_init(CURL_GLOBAL_DEFAULT);
   CURL* curl = curl_easy_init();
   if (!curl) return;

   struct curl_slist* hdrs = curl_slist_append(nullptr, ctype);

   while(!m_stop)
        {std::unique_lock<std::mutex> lk(m_pushMtx);
         m_pushCV.wait_for(lk, std::chrono::seconds(m_pushEvery),
                           [this]{return m_stop.load();});
         lk.unlock();
         if (m_stop) break;

         std::string body;
         XrdMetricsRegistry::Default().Scrape(body);
         XrdMetrics::PrometheusTextSerializer ser(body);
         XrdMetrics::Default().serialize(ser);

         curl_easy_reset(curl);
         curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
         curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
         curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCB);
         curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

         CURLcode rc = curl_easy_perform(curl);
         if (rc != CURLE_OK && m_log)
            m_log->Say("Prometheus push failed: ", curl_easy_strerror(rc));
        }

   curl_slist_free_all(hdrs);
   curl_easy_cleanup(curl);
#endif
}

/******************************************************************************/
/*                          M a t c h e s P a t h                             */
/******************************************************************************/

bool XrdHttpPrometheus::MatchesPath(const char *verb, const char *path)
{
   return !strcmp(verb, "GET") && path && m_path == path;
}

/******************************************************************************/
/*                           P r o c e s s R e q                              */
/******************************************************************************/

int XrdHttpPrometheus::ProcessReq(XrdHttpExtReq &req)
{
   if (req.verb != "GET")
      return req.SendSimpleResp(405, nullptr, nullptr,
                                "Only GET is supported for metrics.", 0);

   XrdMetricsRegistry::Default().Counter("xrootd_metrics_scrapes_total",
            "Number of times the metrics endpoint has been scraped").inc();

   std::string body;
   XrdMetricsRegistry::Default().Scrape(body);
   XrdMetrics::PrometheusTextSerializer ser(body);
   XrdMetrics::Default().serialize(ser);

   return req.SendSimpleResp(200, nullptr, ctype, body.c_str(),
                             (long long)body.size());
}

/******************************************************************************/
/*                    X r d H t t p G e t E x t H a n d l e r                 */
/******************************************************************************/

extern "C" XrdHttpExtHandler *XrdHttpGetExtHandler(XrdHttpExtHandlerArgs)
{
   return new XrdHttpPrometheus(eDest, confg, parms);
}

XrdVERSIONINFO(XrdHttpGetExtHandler, XrdHttpPrometheus);
