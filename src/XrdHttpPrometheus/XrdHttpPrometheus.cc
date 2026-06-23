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

#include <cstring>

#include "XrdHttpPrometheus/XrdHttpPrometheus.hh"
#include "XrdMetrics/XrdMetrics.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdVersion.hh"

// Content type for the Prometheus text exposition format (version 0.0.4).
//
namespace
{
const char *ctype = "Content-Type: text/plain; version=0.0.4; charset=utf-8";
}

/******************************************************************************/
/*                         C o n s t r u c t o r                              */
/******************************************************************************/

XrdHttpPrometheus::XrdHttpPrometheus(XrdSysError *eDest, const char *parms)
                 : m_log(eDest), m_path("/metrics")
{
// The optional plugin parameter overrides the served path. It must start with
// a slash to be a valid request path; otherwise the default is kept.
//
   if (parms && *parms == '/') m_path = parms;

// Register a liveness gauge so the endpoint always returns at least one series,
// even before any other subsystem has registered metrics.
//
   XrdMetricsRegistry::Default().Gauge("xrootd_metrics_endpoint_up",
            "1 if the Prometheus metrics endpoint is configured").set(1);

   if (m_log) m_log->Say("Config Prometheus metrics endpoint at ", m_path.c_str());
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

   return req.SendSimpleResp(200, nullptr, ctype, body.c_str(),
                             (long long)body.size());
}

/******************************************************************************/
/*                    X r d H t t p G e t E x t H a n d l e r                 */
/******************************************************************************/

extern "C" XrdHttpExtHandler *XrdHttpGetExtHandler(XrdHttpExtHandlerArgs)
{
   return new XrdHttpPrometheus(eDest, parms);
}

XrdVERSIONINFO(XrdHttpGetExtHandler, XrdHttpPrometheus);
