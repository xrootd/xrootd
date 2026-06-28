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
#include "XrdHttpMetricsExporter/XrdHttpMetricsExporterConfig.hh"

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

// Parsed configuration (paths, push destinations, intervals). The instance
// field is resolved to the hostname at construction when left unset.
//
XrdHttpMetricsExporterCfg::Config m_cfg;

// Background pushers (Pushgateway text / OTLP JSON), each active only when its
// destination URL is configured.
//
std::thread  m_pushThread;
std::thread  m_otelThread;

// Shared shutdown signalling for the background pushers.
//
std::mutex              m_pushMtx;
std::condition_variable m_pushCV;
std::atomic<bool>       m_stop{false};
};
#endif
