#ifndef __XRDMONOTLP_HH__
#define __XRDMONOTLP_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d M o n O t l p . h h                            */
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

#include <map>
#include <string>

#include "XrdOuc/XrdOucJson.hh"

//-----------------------------------------------------------------------------
//! XrdMonOtlpBatch converts the collector's decoded documents (the nested-JSON
//! OpenTelemetry-data-model form emitted by XrdMonDecode) into OTLP/JSON export
//! requests: an ExportLogsServiceRequest (resourceLogs -> scopeLogs ->
//! logRecords) for the concluded-operation logs, and an ExportTraceServiceRequest
//! (resourceSpans -> scopeSpans -> spans) for the --spans span documents.
//!
//! Records are grouped by their resource so one request carries one resource
//! block per server incarnation. The lightweight nested attributes/resource
//! objects are re-encoded into the OTLP typed KeyValue arrays; the log/span
//! envelope fields (severity, timeUnixNano, traceId/spanId, name/kind/status)
//! are already OTLP-compatible in the source document and pass through.
//!
//! No network dependency, so it is unit-testable on its own; the HTTP transport
//! lives in XrdMonOtlp below.
//-----------------------------------------------------------------------------

class XrdMonOtlpBatch
{
public:

//! Add one decoded document. A document carrying a "kind" field is a span (it
//! joins the traces request); any other document is a log record.
void add(const nlohmann::json& doc);

bool haveLogs()   const {return !logGroups.empty();}
bool haveTraces() const {return !spanGroups.empty();}

//! Serialize and clear the accumulated logs (resp. traces) as one OTLP/JSON
//! ExportLogsServiceRequest (resp. ExportTraceServiceRequest) body.
std::string takeLogsBody();
std::string takeTracesBody();

void clear() {logGroups.clear(); spanGroups.clear();}

private:

// One resource block: the resource attributes (as an OTLP KeyValue array) and
// the log records / spans sharing that resource.
struct Group
{
   nlohmann::json resource;                              // KeyValue array
   nlohmann::json records = nlohmann::json::array();     // logRecords or spans
};

// Keyed by the serialized resource object, so identical resources share a block.
std::map<std::string, Group> logGroups;
std::map<std::string, Group> spanGroups;

std::string takeBody(std::map<std::string, Group>& groups,
                     const char* resourceKey, const char* scopeKey,
                     const char* recordsKey);
};

#ifdef XRDMON_HAVE_CURL
//-----------------------------------------------------------------------------
//! XrdMonOtlp posts OTLP/JSON bodies to an OTLP/HTTP receiver (an OpenTelemetry
//! Collector, Grafana Alloy, or any OTLP endpoint): logs to <url>/v1/logs and
//! traces to <url>/v1/traces, with Content-Type application/json. It owns a
//! libcurl handle and retries transient failures with backoff. One instance is
//! used from a single (output) thread.
//-----------------------------------------------------------------------------

class XrdMonOtlp
{
public:

//! @param url      base OTLP/HTTP endpoint, e.g. "http://collector:4318". The
//!                 signal paths /v1/logs and /v1/traces are appended.
//! @param token    bearer token sent as "Authorization: Bearer <token>"
//!                 (empty for none).
//! @param insecure skip TLS certificate verification when true.
XrdMonOtlp(const std::string& url, const std::string& token, bool insecure);
~XrdMonOtlp();

//! Initialize the libcurl handle. @return true on success, else sets err.
bool Init(std::string& err);

bool PostLogs(const std::string& body, std::string& err);
bool PostTraces(const std::string& body, std::string& err);

private:

bool post(const std::string& url, const std::string& body, std::string& err);

void*       curl;       // CURL* (opaque to avoid leaking the header)
std::string logsURL;
std::string tracesURL;
std::string authHdr;    // "Authorization: Bearer <token>" or empty
bool        insecure;
int         maxRetry;
};
#endif
#endif
