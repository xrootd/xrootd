#ifndef __XRDHTTPMETRICSEXPORTER_HH__
#define __XRDHTTPMETRICSEXPORTER_HH__
/******************************************************************************/
/*                                                                            */
/*          X r d H t t p M e t r i c s E x p o r t e r . h h                  */
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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "XrdHttp/XrdHttpExtHandler.hh"

class XrdSysError;

//-----------------------------------------------------------------------------
//! XrdHttpMetricsExporter: an HTTP external handler that exposes the
//! process-wide XrdMetrics registry. It answers GET requests on a configurable
//! path (default /metrics) with the Prometheus text exposition format, and can
//! additionally push the same metrics to remote collectors in the background:
//!   - a Prometheus Pushgateway (PUT, text exposition format), and/or
//!   - an OTLP/HTTP receiver (POST, OpenTelemetry OTLP/JSON).
//! Either, both, or neither pusher may be active depending on configuration.
//-----------------------------------------------------------------------------

class XrdHttpMetricsExporter : public XrdHttpExtHandler
{
public:

bool MatchesPath(const char *verb, const char *path) override;

int  ProcessReq(XrdHttpExtReq &req) override;

int  Init(const char *cfgfile) override {return 0;}

     XrdHttpMetricsExporter(XrdSysError *eDest, const char *confg,
                            const char *parms);
    ~XrdHttpMetricsExporter();

private:

void Configure(const char *confg);
void PushLoop();      // Prometheus Pushgateway (text exposition format)
void OtelLoop();      // OTLP/HTTP receiver (OpenTelemetry OTLP/JSON)

XrdSysError *m_log;
std::string  m_path;       // request path served, e.g. "/metrics"
std::string  m_instance;   // shared instance label (default: hostname)

// Optional Prometheus Pushgateway push (active when m_pushURL is set).
//
std::string  m_pushURL;    // base gateway URL, e.g. http://gw:9091
std::string  m_pushJob   = "xrootd";
int          m_pushEvery = 30;     // seconds between pushes
std::thread  m_pushThread;

// Optional OTLP/HTTP push (active when m_otelURL is set).
//
std::string  m_otelURL;    // full OTLP/HTTP metrics URL, e.g. .../v1/metrics
int          m_otelEvery = 30;     // seconds between pushes
std::thread  m_otelThread;

// Shared shutdown signalling for the background pushers.
//
std::mutex              m_pushMtx;
std::condition_variable m_pushCV;
std::atomic<bool>       m_stop{false};
};
#endif
