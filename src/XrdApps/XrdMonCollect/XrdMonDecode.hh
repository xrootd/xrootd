#ifndef __XRDMONDECODE_HH__
#define __XRDMONDECODE_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d M o n D e c o d e . h h                          */
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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "XrdOuc/XrdOucJson.hh"

namespace XrdMetrics { class MetricGroup; }

//-----------------------------------------------------------------------------
//! XrdMonDecode decodes XRootD detailed-monitoring UDP packets
//! (xrootd.monitor) and correlates the "f" (file-stats) stream against the
//! user dictionary into one document per completed transfer (file close).
//!
//! It is deliberately transport-agnostic: feed it datagrams via Process() and
//! receive finished transfer documents (and, optionally, raw per-record dumps)
//! through callbacks. All multi-byte fields in the wire format are network
//! byte order; see src/XrdXrootd/XrdXrootdMonData.hh for the layouts.
//-----------------------------------------------------------------------------

class XrdMonDecode
{
public:

//! Called with one finished transfer document, serialized as a single JSON
//! object (no trailing newline). The caller frames it for its sink.
using DocSink = std::function<void(const std::string& jsonDoc)>;

//! Optional: called with one JSON object per decoded record (dump mode).
using RawSink = std::function<void(const std::string& jsonRec)>;

//! Counters describing what has been decoded so far.
struct Stats
{
   uint64_t packets   = 0;   // datagrams seen
   uint64_t malformed = 0;   // datagrams rejected as too short / inconsistent
   uint64_t records   = 0;   // individual records decoded
   uint64_t mapUser   = 0;   // 'u' dictionary records
   uint64_t mapPath   = 0;   // 'd' dictionary records
   uint64_t mapInfo   = 0;   // 'i' dictionary records
   uint64_t mapIdnt   = 0;   // '=' server-identification records
   uint64_t mapTokn   = 0;   // 'T' token dictionary records
   uint64_t mapUeac   = 0;   // 'U' user experiment/activity records
   uint64_t opens     = 0;   // 'f' open records
   uint64_t closes    = 0;   // 'f' close records
   uint64_t xfrs      = 0;   // 'f' in-flight transfer snapshot records
   uint64_t discs     = 0;   // 'f' session disconnect records
   uint64_t docs      = 0;   // transfer documents emitted
   uint64_t failed    = 0;   // 'f' failed/aborted operation records (isError +
                             // hasERR closes)
   uint64_t orphanCls = 0;   // closes with no matching open
   uint64_t traces    = 0;   // 't' stream records decoded
   uint64_t gevents   = 0;   // 'g' stream records decoded
   uint64_t redirs    = 0;   // 'r' stream redirect records decoded
   uint64_t frmEvents = 0;   // 'x'/'p' FRM stage/migrate/purge records
   uint64_t lost      = 0;   // estimated lost packets (pseq gaps)
   uint64_t evicted   = 0;   // dictionary/open-file entries evicted (cap)
   uint64_t unknown   = 0;   // packets with an unhandled code
};

//! Process one UDP datagram received from sender `src` (used, together with
//! the server start time in the header, to key per-server state).
//!
//! @return true if the packet was structurally valid, false if malformed.
bool Process(const std::string& src, const char* buff, int blen);

const Stats& GetStats() const {return stats;}

//! Cap each per-server dictionary and the open-file table at `n` entries
//! (0 = unbounded). Bounds memory on long-lived busy servers; eviction is
//! approximate (hash order), so a dropped entry merely yields a document
//! missing that field, or an orphan close.
void SetMaxEntries(std::size_t n) {maxEntries = n;}

//! Enable/disable substituting the local FQDN for a loopback (co-located)
//! server's hostname (on by default). When off, server.name falls back to the
//! numeric source IP unless a '=' ident host is received. Remote servers are
//! always identified from their '=' ident, never reverse-resolved here (a
//! blocking lookup would stall the UDP receive loop).
void SetResolveHosts(bool v) {resolveHosts = v;}

//! Load a SciTags registry (scitags.org schema: a top-level "experiments"
//! array of {expId, expName, activities:[{activityId, activityName}]}). When
//! loaded, the numeric SciTags experiment/activity ids carried on the 'U'
//! stream are additionally mapped to their names (and the experiment name is
//! used as a VO fallback). Returns false on a missing/unparseable file (the
//! numeric ids are still emitted).
bool LoadScitags(const std::string& path);

//! Replace the SciTags registry from an in-memory JSON document (the same
//! schema as LoadScitags). The swap is mutex-guarded, so a background thread
//! can refresh the registry while the decode thread reads it. Returns false
//! (leaving the current registry intact) on a parse error or a document with
//! no "experiments" array.
bool LoadScitagsJson(const std::string& text);

//! @param emitTraces   emit a document per 't'-stream record (I/O, open,
//!                     close, disconnect) — high volume, off by default.
//! @param emitGstream  emit a document per 'g'-stream (plugin) record.
//! @param reg  optional metrics registry; when set, completed transfers are
//!             aggregated into bounded-cardinality Prometheus series.
         XrdMonDecode(DocSink docSink, RawSink rawSink = nullptr,
                      bool emitRaw = false, bool emitTraces = false,
                      bool emitGstream = false, bool emitRedirects = false,
                      XrdMetrics::MetricGroup* reg = nullptr)
                     : doc(std::move(docSink)), raw(std::move(rawSink)),
                       dumpRaw(emitRaw), traces(emitTraces),
                       gstream(emitGstream), redirects(emitRedirects),
                       metrics(reg) {}
        ~XrdMonDecode() {}

private:

// Identity parsed from a 'u' (MAPUSER) dictionary entry. The first line is the
// "<prot>/<user>.<pid>:<sfd>@<host>" descriptor; the rest is a CGI tail carrying
// the login appinfo (always: &R= &x= &y= &I=) and, when "xrootd.monitor ... auth"
// is configured, the authentication info (&p= &o= &r= &g=).
//
struct UserInfo
{
   std::string raw;        // full first line of the map info
   std::string user;
   std::string prot;
   std::string host;
   std::string authMethod; // &p= security protocol (gsi/krb5/sss/unix/ztn)
   std::string vo;         // &o= VO / organisation (auth-derived; token preferred)
   std::string role;       // &r= role
   std::string groups;     // &g= groups
   std::string clientVer;  // &R= client release (e.g. v5.6.1)
   std::string appName;    // &x= xrd.appname
   std::string appInfo;    // &y= xrd.info
   std::string site;       // &S= client-advertised site (xrd.site)
   int         ipVersion = 0; // &I= IP protocol (4 or 6); 0 = unknown
};

// Token identity from a 'T' (MAPTOKN) record, keyed by the user dictid.
//
struct TokenInfo
{
   std::string subject;   // s= token subject (sub claim)
   std::string username;  // n= mapped local user
   std::string vo;        // o= organisation / VO
   std::string role;      // r= role
   std::string groups;    // g= groups
};

// User experiment/activity from a 'U' (MAPUEAC) record (SciTags packet-marking
// flow labels), keyed by the user dictid.
//
struct UserActivity
{
   int experiment = 0;    // Ec= experiment id
   int activity   = 0;    // Ac= activity id
};

// Server self-identification from a '=' (MAPIDNT) record.
//
struct ServerIdent
{
   std::string site;
   std::string host;
   std::string inst;
   std::string pgm;
   std::string ver;
   std::string user;      // login user.pid of the reporting daemon
};

// An open file awaiting its close, from the 'f' stream open record.
//
struct OpenFile
{
   std::string lfn;
   uint32_t    user = 0;  // user dictid (0 if user monitoring off)
   int64_t     fsz  = 0;  // file size at open
   int32_t     tOpen = 0; // window time of the open packet
   bool        rw   = false;
};

// Per server-incarnation state, keyed by sender + server start time (stod).
//
struct Server
{
   std::unordered_map<uint32_t, UserInfo>    users;
   std::unordered_map<uint32_t, OpenFile>    files;
   std::unordered_map<uint32_t, std::string> paths;  // 'd' dictid -> lfn
   std::unordered_map<std::string, std::string> infos; // 'i' descriptor -> appinfo
   std::unordered_map<uint32_t, TokenInfo>    tokens;  // 'T' user dictid -> token
   std::unordered_map<uint32_t, UserActivity> activity;// 'U' user dictid -> scitag
   ServerIdent ident;        // '=' server self-identification
   std::string identRaw;     // last emitted identity (to de-duplicate docs)
   std::string resolvedHost; // reverse-resolved sender hostname (cached)
   bool    resolved = false; // resolvedHost computed yet (once per incarnation)
   int64_t sID = 0;
   int     lastPseq = -1;    // last packet sequence (header pseq) for loss det.
};

Server&  ServerFor(const std::string& src, int32_t stod);
void     DecodeMap(unsigned char code, Server& srv,
                   uint32_t dictid, const char* info, int ilen);
void     DecodeIdent(const std::string& src, int32_t stod, Server& srv,
                     const char* info, int ilen);
void     DecodeFrm(const std::string& src, int32_t stod, Server& srv,
                   unsigned char code, const char* info, int ilen);
void     DecodeFStream(const std::string& src, int32_t stod, Server& srv,
                       const unsigned char* p, int len);
void     EmitClose(const std::string& src, int32_t stod, Server& srv,
                   uint32_t fileID, unsigned char recFlag,
                   const unsigned char* rec, int recSize, int32_t tWin);
void     EmitDisc(const std::string& src, int32_t stod, Server& srv,
                  uint32_t userID, int32_t tWin);
void     EmitError(const std::string& src, int32_t stod, Server& srv,
                   unsigned char recFlag, const unsigned char* rec,
                   int recSize, int32_t tWin);
//! Fill the terminal-error fields (operation_state/error_code/category/message)
//! of a transfer document from a XrdXrootdMonStatERR block of `errLen` bytes.
void     fillError(nlohmann::json& transfer, const unsigned char* err,
                   int errLen);
void     DecodeTStream(const std::string& src, int32_t stod, Server& srv,
                       const unsigned char* p, int len);
void     DecodeGStream(const std::string& src, int32_t stod, Server& srv,
                       const unsigned char* p, int plen);
void     DecodeRStream(const std::string& src, int32_t stod, Server& srv,
                       const unsigned char* p, int plen);
void     Evict(Server& srv);

// Document-building helpers shared by every emitter so all document types use
// one nested, OpenSearch-friendly schema (server.*, client.*, user.*, ...).
//
//! Fill the j["server"] object (ip/start/id/hostname/site/instance/name).
void     fillServer(nlohmann::json& j, const std::string& src, int32_t stod,
                    const Server& srv);
//! Fill j["user"], j["client"], j["app"], j["activity"] from the user
//! dictionary entry (and the token/activity streams keyed by the same dictid).
//! Returns the resolved VO (token preferred, else auth CGI) for metric labels.
std::string fillClient(nlohmann::json& j, const Server& srv, uint32_t userID);

std::unordered_map<std::string, Server> servers;
// Previous cumulative values for g-stream providers that report running
// totals (oss), so they can be turned into Prometheus counter deltas. Keyed
// by "<server>|<provider>|<metric>".
std::unordered_map<std::string, uint64_t> gsPrev;
DocSink  doc;
RawSink  raw;
bool     dumpRaw;
bool     traces;
bool     gstream;
bool     redirects;
XrdMetrics::MetricGroup* metrics;
std::size_t maxEntries = 0;
bool     resolveHosts = true;

// Local FQDN substituted for a loopback (co-located) server, resolved at most
// once for the whole process (MyHostName is the same regardless of which
// server reports), then reused for every loopback incarnation.
const std::string& LocalHost();
std::string localHost;
bool        localHostDone = false;

// SciTags registry (loaded from --scitags), mapping numeric flow-label ids to
// human names. sciExp: expId -> expName (doubles as a VO); sciAct: the packed
// key (expId<<32)|activityId -> activityName. Empty when no registry is loaded.
// Guarded by scitagsMtx so a background refresh thread can swap them in while
// the decode thread reads them in fillClient().
std::mutex                                 scitagsMtx;
std::unordered_map<int, std::string>       sciExp;
std::unordered_map<long long, std::string> sciAct;

Stats    stats;
};
#endif
