/******************************************************************************/
/*                                                                            */
/*                     X r d M o n D e c o d e . c c                          */
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

#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>

#include <unistd.h>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"

using json = nlohmann::json;

/******************************************************************************/
/*               N e t w o r k - o r d e r   r e a d e r s                    */
/******************************************************************************/

namespace
{
inline uint16_t rd16(const unsigned char* p)
   {return (uint16_t(p[0]) << 8) | uint16_t(p[1]);}

inline uint32_t rd32(const unsigned char* p)
   {return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);}

inline uint64_t rd64(const unsigned char* p)
   {return (uint64_t(rd32(p)) << 32) | uint64_t(rd32(p + 4));}

inline int32_t  ri32(const unsigned char* p) {return (int32_t) rd32(p);}
inline int64_t  ri64(const unsigned char* p) {return (int64_t) rd64(p);}

inline double   rdbl(const unsigned char* p)
   {uint64_t u = rd64(p); double d; std::memcpy(&d, &u, sizeof(d)); return d;}

// Extract the value of an `&key=value` (or leading `key=value`) field from a
// CGI-style string; returns empty if the key is absent.
//
std::string cgiVal(const std::string& s, const char* key)
{
   std::string k(key);
   std::size_t start;
   std::string amp = "&" + k + "=";
   auto pos = s.find(amp);
   if (pos != std::string::npos) start = pos + amp.size();
   else if (s.compare(0, k.size() + 1, k + "=") == 0) start = k.size() + 1;
   else return "";
   auto end = s.find('&', start);
   return s.substr(start, end == std::string::npos ? std::string::npos
                                                   : end - start);
}

// Name of a terminal-error category (XrdXrootdMonStatERR.ecat / monErrCat).
//
const char* errCatName(int c)
{
   switch(c)
         {case monErrOpen:  return "open";
          case monErrRead:  return "read";
          case monErrWrite: return "write";
          case monErrClose: return "close";
          case monErrAuth:  return "auth";
          default:          return "unknown";
         }
}

// Format a Unix time as an ISO-8601 UTC string. Zero/negative => empty.
//
std::string isoTime(int32_t t)
{
   if (t <= 0) return "";
   time_t tt = (time_t)t;
   struct tm tmv;
   char buf[32];
   gmtime_r(&tt, &tmv);
   strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
   return buf;
}

// Best-effort classification of a host string as an IP literal vs a hostname.
// IPv6 if it contains ':'; IPv4 if non-empty and only digits and dots; else a
// hostname. The server may emit either, depending on its DNS-resolution config.
//
bool isIPLiteral(const std::string& h)
{
   if (h.empty()) return false;
   if (h.find(':') != std::string::npos) return true;   // IPv6
   for (char c : h) if (c != '.' && (c < '0' || c > '9')) return false;
   return true;                                          // IPv4 dotted-quad
}

// Registered-domain part of a host name (everything after the first label),
// lower-cased. Empty for an IP literal or a single-label name — i.e. when no
// LAN/WAN comparison can be made.
//
std::string hostDomain(const std::string& h)
{
   if (h.empty() || isIPLiteral(h)) return "";
   auto dot = h.find('.');
   if (dot == std::string::npos) return "";
   std::string d = h.substr(dot + 1);
   for (char& c : d) c = (char)std::tolower((unsigned char)c);
   return d;
}

// A loopback UDP source: the collector is co-located with the server it
// monitors, so the datagrams arrive from the local machine.
//
bool isLoopback(const std::string& ip)
{
   return ip == "::1" || ip.rfind("127.", 0) == 0
       || ip.rfind("::ffff:127.", 0) == 0;
}

// A loopback host name ("localhost", "localhost.localdomain", ...): the named
// counterpart of isLoopback(), produced when a server reverse-resolves a
// loopback peer. Replaced with the real FQDN just like a loopback literal is
// replaced with the public address.
//
bool isLocalName(const std::string& h)
{
   return h == "localhost" || h.rfind("localhost.", 0) == 0;
}

// Whether an authentication method can put a genuine VO into the auth CGI's
// &o= (XrdSecEntity.vorg): gsi fills it from a VOMS attribute certificate,
// sss unpacks it from the trusted key-holder's registered entity, and the
// http(s) bridge forwards the VOMS-derived value from the TLS client cert.
// ztn never sets vorg at login (the token identity arrives on the 'T' stream,
// which takes precedence anyway) but is accepted as a token-class method.
// unix/krb5/pwd/host never convey a VO — anything a custom seclib puts there
// is noise, and surfaced as fake VOs in dashboards before this gate.
//
bool authConveysVO(const std::string& m)
{
   return m == "gsi" || m == "sss" || m == "ztn"
       || m == "https" || m == "http";
}

// Whole-file transfer vs. partial access. XRootD serves both whole-file copies
// (the file moved in or out in its entirety) and finer-grained remote data
// access (sparse/partial reads, random or appended writes). A close is a
// whole-file "transfer" when:
//   * it carries write bytes and ended cleanly — a completed write produces the
//     whole file; a write cut short by a forced close or an error block is
//     partial, so it is an "access"; or
//   * it is read-only and covered the whole file, i.e. read+readv bytes reached
//     the size captured at open (only decidable when that open size is known).
// Everything else (short reads, reads with no known open size, aborted writes)
// is a partial "access". Callers map true -> "transfer", false -> "access".
//
bool wholeFileClose(bool haveOpen, int64_t fsz, int64_t rdBytes,
                    int64_t rvBytes, int64_t wrBytes, bool forced, bool hasErr)
{
   if (wrBytes > 0) return !(forced || hasErr);
   return haveOpen && fsz > 0 && (rdBytes + rvBytes) >= fsz;
}

// Cap on the per-session recent-file list (UserInfo::sRecent). The running
// session totals always cover every closed file; only this most-recent detail
// list is bounded, keeping a long-lived session's memory in check.
constexpr std::size_t kSessionFilesMax = 64;

// FNV-1a hash used to synthesize deterministic OpenTelemetry trace/span ids from
// the monitoring stream's own correlation keys (server incarnation, user dictid,
// file id). Deterministic ids let a downstream tracing backend stitch a client
// session (login -> per-file operation) back together with no wire change.
//
uint64_t fnv1a(const std::string& s, uint64_t seed)
{
   uint64_t h = seed;
   for (unsigned char c : s) {h ^= c; h *= 0x100000001b3ULL;}
   return h;
}
std::string hexOf(uint64_t v, int nbytes)
{
   static const char* d = "0123456789abcdef";
   std::string out((std::size_t)nbytes * 2, '0');
   for (int i = nbytes * 2 - 1; i >= 0; --i) {out[i] = d[v & 0xf]; v >>= 4;}
   return out;
}
// 16-byte (32-hex) OTel traceId keying a client session; 8-byte (16-hex) spanId.
// Two FNV streams with distinct seeds give the 128-bit trace id.
//
std::string traceIdOf(const std::string& key)
{
   return hexOf(fnv1a(key, 0xcbf29ce484222325ULL), 8)
        + hexOf(fnv1a(key, 0x9e3779b97f4a7c15ULL), 8);
}
std::string spanIdOf(const std::string& key)
{
   return hexOf(fnv1a(key, 0xcbf29ce484222325ULL), 8);
}
// Session (trace) key: sender + server incarnation + user dictid.
std::string sessKey(const std::string& src, int32_t stod, uint32_t userID)
{
   return src + "|" + std::to_string(stod) + "|" + std::to_string(userID);
}
// Unix seconds -> OTLP nanoseconds as a decimal string (OTLP encodes 64-bit
// times as strings; a JSON number would lose the low digits). Empty for t<=0.
//
std::string unixNano(int64_t secs)
{
   if (secs <= 0) return "";
   return std::to_string((uint64_t)secs * 1000000000ULL);
}
}

/******************************************************************************/
/*                    r e s o l v e L o c a l H o s t                         */
/******************************************************************************/

// The local FQDN, used as the hostname of a server reporting from the loopback
// address (where there is no useful name before the '=' ident arrives, so
// server.hostname otherwise showed the literal "::1"). Resolved once, from the
// constructor (i.e. at startup, before the receive loop begins), and reused for
// every loopback incarnation.
//
// The resolution is done here rather than lazily on the first loopback packet so
// the single-threaded UDP receive/serializer loop stays lookup-free by
// construction: MyHostName() can block on a name lookup. Only the loopback case
// is named this way — a remote server self-identifies its host on the '='
// (MAPIDNT) stream, which otelResource() already prefers; a blocking reverse-DNS
// lookup of an arbitrary remote IP is deliberately never done.
//
void XrdMonDecode::resolveLocalHost()
{
   const char* me = XrdNetUtils::MyHostName();
   std::string h = me ? me : "";
   if (!h.empty() && !isIPLiteral(h) && !isLocalName(h)) localHost = h;

// When the advertised FQDN is itself a loopback name (a host whose
// "hostname -f" is localhost, common in containers), fall back to the kernel
// host name for the address lookup below; it usually resolves to the real
// interface addresses even when the canonical name does not.
//
   std::string lookup = localHost;
   if (lookup.empty())
      {char hn[256] = {0};
       if (!gethostname(hn, sizeof(hn) - 1) && *hn && !isLocalName(hn))
          lookup = hn;
      }
   if (lookup.empty()) return;

// Also resolve the name's own addresses once, caching the first public
// (non-loopback) address of each family. These stand in for loopback literals
// that would otherwise surface in client.address/server.address. On a multi-
// homed host the addresses the name resolves to are the ones remote peers
// would use, which makes them the canonical public identity.
//
   std::vector<XrdNetAddr> aVec;
   if (!XrdNetUtils::GetAddrs(lookup, aVec, nullptr, XrdNetUtils::allIPv64,
                              XrdNetUtils::NoPortRaw))
      for (XrdNetAddr& a : aVec)
          {if (a.isLoopback()) continue;
           std::string& slot = (a.isIPType(XrdNetAddrInfo::IPv4) || a.isMapped())
                             ? localIP4 : localIP6;
           if (!slot.empty()) continue;
           char buf[64];
           if (a.Format(buf, sizeof(buf), XrdNetAddrInfo::fmtAddr,
                        XrdNetAddrInfo::prefipv4 | XrdNetAddrInfo::noPortRaw) > 0)
              slot = buf;
          }

// Interface-scan fallback: when name resolution could not produce a non-
// loopback address for a family (common where /etc/hosts pins the host name
// to 127.0.1.1), take the first public — else first private — up interface
// address of that family instead.
//
   if (localIP4.empty() || localIP6.empty())
      {XrdOucTList* ifList = nullptr;
       if (XrdNetIF::GetIF(&ifList) > 0)
          {std::string pub4, prv4, pub6, prv6;
           while (ifList)
                 {std::string a = ifList->text;
                  if (a.size() > 1 && a.front() == '[' && a.back() == ']')
                     a = a.substr(1, a.size() - 2);
                  bool v6 = a.find(':') != std::string::npos;
                  std::string& slot = ifList->sval[1] ? (v6 ? prv6 : prv4)
                                                      : (v6 ? pub6 : pub4);
                  if (slot.empty()) slot = a;
                  XrdOucTList* next = ifList->next;
                  delete ifList; ifList = next;
                 }
           if (localIP4.empty()) localIP4 = !pub4.empty() ? pub4 : prv4;
           if (localIP6.empty()) localIP6 = !pub6.empty() ? pub6 : prv6;
          }
      }

// If the FQDN was unusable, name this host from a public address (a one-time
// reverse lookup — still at startup, never in the receive loop), falling back
// to the kernel host name.
//
   if (localHost.empty() && (!localIP4.empty() || !localIP6.empty()))
      {std::string spec = !localIP4.empty() ? localIP4
                                            : "[" + localIP6 + "]";
       XrdNetAddr na;
       const char* n = nullptr;
       if (!na.Set(spec.c_str(), 0)) n = na.Name();
       if (n && !isIPLiteral(n) && !isLocalName(n)) localHost = n;
          else localHost = lookup;
      }
}

/******************************************************************************/
/*                             p u b l i c F o r                              */
/******************************************************************************/

// The public stand-in for an address that would be emitted as a loopback
// literal: the cached local address of the same family, else the other
// family, else the FQDN, else the literal unchanged. Non-loopback input is
// returned as-is.
//
std::string XrdMonDecode::publicFor(const std::string& ip) const
{
   if (!resolveHosts || !isLoopback(ip)) return ip;
   const std::string& same  = (ip == "::1") ? localIP6 : localIP4;
   const std::string& other = (ip == "::1") ? localIP4 : localIP6;
   if (!same.empty())      return same;
   if (!other.empty())     return other;
   if (!localHost.empty()) return localHost;
   return ip;
}

/******************************************************************************/
/*                               s e t F i l e                                */
/******************************************************************************/

// Set the OpenTelemetry file.path, file.name, and file.directory attributes
// from an LFN (skipping an empty path), plus the xrootd.dataset capture when
// a --dataset pattern is set. The pattern was compiled once at startup; it is
// matched once per emitted document, in the serializer thread — never in the
// UDP receive path.
//
void XrdMonDecode::setFile(json& a, const std::string& lfn) const
{
   if (lfn.empty()) return;
   a["file.path"] = lfn;
   auto slash = lfn.rfind('/');
   if (slash != std::string::npos && slash + 1 < lfn.size())
      a["file.name"] = lfn.substr(slash + 1);
   if (slash != std::string::npos)
      a["file.directory"] = slash ? lfn.substr(0, slash) : "/";
   if (hasDataset)
      {regmatch_t m[2];
       if (!regexec(&datasetRe, lfn.c_str(), 2, m, 0)
           && m[1].rm_so >= 0 && m[1].rm_eo >= m[1].rm_so)
          a["xrootd.dataset"] = lfn.substr(m[1].rm_so, m[1].rm_eo - m[1].rm_so);
      }
}

/******************************************************************************/
/*                      S e t D a t a s e t R e g e x                         */
/******************************************************************************/

bool XrdMonDecode::SetDatasetRegex(const std::string& pattern)
{
   if (hasDataset) {regfree(&datasetRe); hasDataset = false;}
   if (pattern.empty()) return true;
   if (regcomp(&datasetRe, pattern.c_str(), REG_EXTENDED)) return false;
   hasDataset = true;
   return true;
}

/******************************************************************************/
/*                            S e r v e r F o r                               */
/******************************************************************************/

XrdMonDecode::Server& XrdMonDecode::ServerFor(const std::string& src,
                                              int32_t stod)
{
   std::string key = src;
   key += '|';
   key += std::to_string(stod);
   Server& srv = servers[key];
   srv.lastSeen = time(nullptr);   // for idle-incarnation reaping

// Record the sender's hostname once per server incarnation (cached in the
// Server), so the per-document otelResource() stays lookup-free. Only a loopback
// sender is named here, from the process-wide local FQDN (itself resolved at
// most once); a '=' ident host still takes precedence in otelResource().
//
   if (resolveHosts && !srv.resolved)
      {std::string ip = src.substr(0, src.rfind(':'));
       if (isLoopback(ip)) srv.resolvedHost = localHost;
       srv.resolved = true;
      }
   return srv;
}

/******************************************************************************/
/*                         o t e l R e s o u r c e                            */
/******************************************************************************/

void XrdMonDecode::otelResource(json& j, const std::string& src, int32_t stod,
                                const Server& srv)
{
   json& r = j["resource"];
   std::string ip = src.substr(0, src.rfind(':'));   // UDP source (strip :port)

   // server.address precedence: the '=' ident's advertised host (when it is a
   // real name, not an IP literal — "localhost" is renamed to the real FQDN),
   // else the reverse-resolved sender, else the numeric source IP with
   // loopback replaced by the public address. It is the single canonical
   // server-name field (a separate host.name would always duplicate it), and
   // service.instance.id the single instance field.
   //
   std::string host;
   if (!srv.ident.host.empty() && !isIPLiteral(srv.ident.host))
      host = (resolveHosts && isLocalName(srv.ident.host) && !localHost.empty())
           ? localHost : srv.ident.host;
   else if (!srv.resolvedHost.empty())
      host = srv.resolvedHost;
   std::string name = host.empty() ? publicFor(ip) : host;

   r["service.name"]        = "xrootd";
   r["service.instance.id"] = srv.ident.inst.empty() ? name : srv.ident.inst;
   r["server.address"]      = name;
   if (srv.ident.port > 0)       r["server.port"]            = srv.ident.port;
   if (!srv.ident.ver.empty())   r["service.version"]        = srv.ident.ver;
   // A site of only dots is XrdOucSiteName's sanitization of an all-invalid
   // name (e.g. a stray XRDSITE env var inherited by a server with no
   // all.sitename directive) — it carries no information, so drop it.
   if (!srv.ident.site.empty() &&
       srv.ident.site.find_first_not_of('.') != std::string::npos)
                                 r["xrootd.server.site"]     = srv.ident.site;
   if (!srv.ident.pgm.empty())   r["xrootd.server.program"]  = srv.ident.pgm;
   if (srv.sID)                  r["xrootd.server.id"]       = srv.sID;
   r["xrootd.server.incarnation"] = stod;             // incarnation key
}

/******************************************************************************/
/*                            o t e l B e g i n                               */
/******************************************************************************/

void XrdMonDecode::otelBegin(json& j, const char* eventName, int32_t tSecs,
                             bool error)
{
   j["scope"]["name"] = "xrdmoncollect";
   if (tSecs > 0)
      {j["@timestamp"]   = isoTime(tSecs);
       j["timeUnixNano"] = unixNano(tSecs);
      }
   j["observedTimeUnixNano"] = unixNano((int64_t)time(nullptr));
   j["severityNumber"] = error ? 17 : 9;               // OTel SeverityNumber
   j["severityText"]   = error ? "ERROR" : "INFO";
   j["attributes"]["event.name"] = eventName;
}

/******************************************************************************/
/*                             e m i t S p a n                                */
/******************************************************************************/

void XrdMonDecode::emitSpan(const json& src, const char* name, int32_t tBeg,
                            int32_t tEnd, const std::string& parentSpanId)
{
   if (!emitSpans || !doc) return;

// A span reuses the log record's resource, event attributes and trace/span ids,
// but swaps the log envelope (severity/timeUnixNano) for the OTLP span fields
// (name/kind/start-end/status), so a tracing backend gets a real span while the
// log keeps the detail. The ids match the log's, so the two correlate.
//
   json sp;
   sp["resource"] = src.at("resource");
   if (src.contains("scope"))      sp["scope"]      = src["scope"];
   if (src.contains("attributes")) sp["attributes"] = src["attributes"];
   sp["traceId"] = src.value("traceId", std::string());
   sp["spanId"]  = src.value("spanId",  std::string());
   if (!parentSpanId.empty()) sp["parentSpanId"] = parentSpanId;

   sp["name"] = name;
   sp["kind"] = "SPAN_KIND_SERVER";
   if (tBeg > 0)
      {sp["startTimeUnixNano"] = unixNano(tBeg);
       sp["@timestamp"]        = isoTime(tBeg);
      }
   sp["endTimeUnixNano"] = unixNano(tEnd > 0 ? tEnd : tBeg);

   json status;
   if (src.value("severityText", std::string()) == "ERROR")
      {status["code"] = "STATUS_CODE_ERROR";
       if (src.contains("attributes") &&
           src["attributes"].contains("error.message"))
          status["message"] = src["attributes"]["error.message"];
      }
      else status["code"] = "STATUS_CODE_OK";
   sp["status"] = status;

   stats.spans++;
   doc(sp.dump());
}

/******************************************************************************/
/*                         o t e l I d e n t i t y                            */
/******************************************************************************/

std::string XrdMonDecode::otelIdentity(json& a, const Server& srv,
                                       uint32_t userID)
{
   std::string vo;

   auto uit = srv.users.find(userID);
   if (uit != srv.users.end())
      {const UserInfo& u = uit->second;
       Touch(u.lru);   // a referenced session is active: keep it warm
       if (!u.user.empty())       a["user.name"]            = u.user;
       if (!u.prot.empty())       a["xrootd.user.protocol"] = u.prot;
       if (!u.authMethod.empty()) a["xrootd.auth.method"]   = u.authMethod;
       // Client endpoint: semconv client.address carries the numeric IP from
       // the '&a=' login CGI; an older server only sends the descriptor host,
       // which may be a reverse-resolved name. Loopback values are renamed to
       // this host's public identity (publicFor/localHost). The resolved
       // hostname, when it adds information, is kept in xrootd.client.host.
       std::string chost = u.host;
       if (resolveHosts && isLocalName(chost) && !localHost.empty())
          chost = localHost;
       std::string caddr = !u.addr.empty() ? publicFor(u.addr)
                         : (isIPLiteral(chost) ? publicFor(chost) : chost);
       if (!caddr.empty())
          {a["client.address"] = caddr;
           if      (u.ipVersion == 4) a["network.type"] = "ipv4";
           else if (u.ipVersion == 6) a["network.type"] = "ipv6";
           if (!u.clientVer.empty()) a["xrootd.client.version"] = u.clientVer;
           if (!u.site.empty())      a["xrootd.client.site"]    = u.site;
          }
       if (!chost.empty() && !isIPLiteral(chost) && chost != caddr)
          a["xrootd.client.host"] = chost;
       // Application info: structured (&x=/&y=) plus the raw 'i' blob, the
       // latter only when it adds information over the login &y= appinfo.
       if (!u.appName.empty()) a["xrootd.app.name"] = u.appName;
       if (!u.appInfo.empty()) a["xrootd.app.info"] = u.appInfo;
       auto iit = srv.infos.find(u.raw);
       if (iit != srv.infos.end())
          {Touch(iit->second.lru);
           if (iit->second.val != u.appInfo)
              a["xrootd.app.raw"] = iit->second.val;
          }
       // VO/role/groups: prefer the token ('T'); fall back to the auth CGI.
       if (!u.vo.empty())     {vo = u.vo;         a["wlcg.vo"]     = u.vo;}
       if (!u.role.empty())   a["wlcg.role"]   = u.role;
       if (!u.groups.empty()) a["wlcg.groups"] = u.groups;
      }

   // Token identity ('T' stream) and experiment/activity ('U' stream) are
   // keyed by the same user dictid as the 'u' map.
   auto tit = srv.tokens.find(userID);
   if (tit != srv.tokens.end())
      {const TokenInfo& t = tit->second;
       Touch(t.lru);
       if (!t.subject.empty()) a["user.id"]     = t.subject;
       if (!t.vo.empty())     {vo = t.vo;        a["wlcg.vo"]     = t.vo;}
       if (!t.role.empty())    a["wlcg.role"]   = t.role;
       if (!t.groups.empty())  a["wlcg.groups"] = t.groups;
      }
   auto ait = srv.activity.find(userID);
   if (ait != srv.activity.end())
      {Touch(ait->second.lru);
       int expId = ait->second.experiment;
       int actId = ait->second.activity;
       if (expId) a["scitags.experiment_id"] = expId;
       if (actId) a["scitags.activity_id"]   = actId;

       // Map the numeric SciTags ids to human names via the loaded registry.
       // The names stand on their own (dashboards group by them); they are
       // deliberately not folded into wlcg.vo, which carries only genuine VO
       // information from the token or a VO-bearing auth method. The lock
       // covers a background refresh thread swapping the registry; lookups
       // copy out the names.
       std::string expName, actName;
       {std::lock_guard<std::mutex> lk(scitagsMtx);
        if (expId)
           {auto eit = sciExp.find(expId);
            if (eit != sciExp.end()) expName = eit->second;
           }
        if (expId && actId)
           {auto kit = sciAct.find(((long long)expId << 32) | actId);
            if (kit != sciAct.end()) actName = kit->second;
           }
       }
       if (!expName.empty()) a["scitags.experiment"] = expName;
       if (!actName.empty()) a["scitags.activity"]   = actName;
      }

   return vo;
}

/******************************************************************************/
/*                          f o l d S e s s i o n                             */
/******************************************************************************/

void XrdMonDecode::foldSession(Server& srv, uint32_t userID,
                               const std::string& lfn, int64_t bytes,
                               bool write, bool whole, bool error, int32_t tWin)
{
   if (!emitSessions) return;              // session correlation disabled
   auto uit = srv.users.find(userID);
   if (uit == srv.users.end()) return;     // user dictid unknown -> nothing to do
   UserInfo& u = uit->second;

   u.sFiles++;
   if (whole) u.sTransfers++; else u.sAccesses++;
   if (error) u.sErrors++;
   if (write) u.sWriteBytes += bytes; else u.sReadBytes += bytes;
   if (u.sFirst == 0 || (tWin > 0 && tWin < u.sFirst)) u.sFirst = tWin;
   if (tWin > u.sLast) u.sLast = tWin;

   u.sRecent.push_back(UserInfo::FileSummary{lfn, bytes, write, whole});
   if (u.sRecent.size() > kSessionFilesMax) u.sRecent.pop_front();

// The rollup grew; re-charge the entry against the budget and keep it warm (an
// active session is one whose files are still closing).
//
   Recharge(u.lru, bytesOf(u));
   Touch(u.lru);
}

/******************************************************************************/
/*                          o t e l S e s s i o n                             */
/******************************************************************************/

void XrdMonDecode::otelSession(json& a, const UserInfo& u)
{
   a["xrootd.session.files"]     = u.sFiles;
   a["xrootd.session.transfers"] = u.sTransfers;
   a["xrootd.session.accesses"]  = u.sAccesses;
   if (u.sErrors)     a["xrootd.session.errors"]      = u.sErrors;
   if (u.sReadBytes)  a["xrootd.session.read_bytes"]  = u.sReadBytes;
   if (u.sWriteBytes) a["xrootd.session.write_bytes"] = u.sWriteBytes;
   if (u.sFirst > 0)  a["xrootd.session.start_time"]  = isoTime(u.sFirst);
   if (u.sLast  > 0)  a["xrootd.session.end_time"]    = isoTime(u.sLast);
   if (u.sFirst > 0 && u.sLast >= u.sFirst)
      a["xrootd.session.duration"] = u.sLast - u.sFirst;

   if (!u.sRecent.empty())
      {json files = json::array();
       for (const auto& f : u.sRecent)
          {json fj;
           fj["file.path"]            = f.lfn;
           fj["xrootd.transfer.kind"] = f.whole ? "transfer" : "access";
           fj["xrootd.operation"]     = f.write ? "write" : "read";
           fj["xrootd.bytes"]         = f.bytes;
           files.push_back(std::move(fj));
          }
       a["xrootd.session.recent_files"] = std::move(files);
      }
}

/******************************************************************************/
/*                          L o a d S c i t a g s                             */
/******************************************************************************/

bool XrdMonDecode::LoadScitags(const std::string& path)
{
   std::ifstream in(path);
   if (!in) return false;
   std::ostringstream ss;
   ss << in.rdbuf();
   return LoadScitagsJson(ss.str());
}

bool XrdMonDecode::LoadScitagsJson(const std::string& text)
{
   json doc;
   try    {doc = json::parse(text);}
   catch (const std::exception&) {return false;}

   auto exps = doc.find("experiments");
   if (exps == doc.end() || !exps->is_array()) return false;

// Build the new tables outside the lock, then swap them in. A failed/partial
// parse never reaches here, so the live registry is only ever replaced whole.
//
   std::unordered_map<int, std::string>       exp;
   std::unordered_map<long long, std::string> act;
   for (const auto& e : *exps)
      {if (!e.contains("expId")) continue;
       int expId = e["expId"].get<int>();
       if (e.contains("expName") && e["expName"].is_string())
          exp[expId] = e["expName"].get<std::string>();
       auto acts = e.find("activities");
       if (acts == e.end() || !acts->is_array()) continue;
       for (const auto& a : *acts)
          {if (!a.contains("activityId") || !a.contains("activityName")) continue;
           int actId = a["activityId"].get<int>();
           act[((long long)expId << 32) | actId] =
                  a["activityName"].get<std::string>();
          }
      }

   std::lock_guard<std::mutex> lk(scitagsMtx);
   sciExp.swap(exp);
   sciAct.swap(act);
   return true;
}

/******************************************************************************/
/*                  S a v e S t a t e  /  L o a d S t a t e                   */
/******************************************************************************/

namespace
{
// On-disk state format version. Bump whenever the persisted shape changes; a
// mismatched snapshot is discarded on load (one restart's blind window instead
// of misdecoded state). v2: per-stream pseq tracking.
constexpr int kStateVersion = 2;

// One list of Stats counters shared by save and load so they cannot diverge.
#define XRDMON_STATS_FIELDS(X) \
   X(packets) X(malformed) X(records) X(mapUser) X(mapPath) X(mapInfo)    \
   X(mapIdnt) X(mapTokn) X(mapUeac) X(opens) X(closes) X(xfrs) X(discs)   \
   X(docs) X(failed) X(orphanCls) X(traces) X(gevents) X(redirs) X(spans) \
   X(frmEvents) X(lost) X(evicted) X(reaped) X(unknown)
}

bool XrdMonDecode::SaveState(const std::string& path) const
{
   json j;
   j["version"] = kStateVersion;
   j["saved"]   = (int64_t)time(nullptr);

   json& jst = j["stats"];
#define X(fld) jst[#fld] = stats.fld;
   XRDMON_STATS_FIELDS(X)
#undef X

   j["gsprev"] = gsPrev;

   json& jsv = j["servers"] = json::object();
   for (const auto& [key, s] : servers)
      {json o;
       o["sid"]  = s.sID;
       o["pseq"] = s.lastPseq;   // per-stream-class map
       o["seen"] = (int64_t)s.lastSeen;
       o["resolved"] = s.resolved;
       if (!s.resolvedHost.empty()) o["rhost"]    = s.resolvedHost;
       if (!s.identRaw.empty())     o["identraw"] = s.identRaw;
       o["ident"] = {{"site", s.ident.site}, {"host", s.ident.host},
                     {"inst", s.ident.inst}, {"pgm",  s.ident.pgm},
                     {"ver",  s.ident.ver},  {"user", s.ident.user},
                     {"port", s.ident.port}};

       json& ju = o["users"] = json::object();
       for (const auto& [id, u] : s.users)
          {json e = {{"raw",  u.raw},       {"user",   u.user},
                     {"prot", u.prot},      {"host",   u.host},
                     {"addr", u.addr},      {"auth",   u.authMethod},
                     {"vo",   u.vo},        {"role",   u.role},
                     {"groups", u.groups},  {"cver",   u.clientVer},
                     {"app",  u.appName},   {"info",   u.appInfo},
                     {"site", u.site},      {"ipv",    u.ipVersion}};
           if (u.sFiles || u.sErrors)
              {json& ss = e["session"];
               ss = {{"files", u.sFiles},     {"xfers", u.sTransfers},
                     {"accs",  u.sAccesses},  {"errs",  u.sErrors},
                     {"rb",    u.sReadBytes}, {"wb",    u.sWriteBytes},
                     {"first", u.sFirst},     {"last",  u.sLast}};
               json& fr = ss["recent"] = json::array();
               for (const auto& f : u.sRecent)
                   fr.push_back({{"lfn", f.lfn}, {"b", f.bytes},
                                 {"w", f.write}, {"whole", f.whole}});
              }
           ju[std::to_string(id)] = std::move(e);
          }

       json& jf = o["files"] = json::object();
       for (const auto& [id, f] : s.files)
           jf[std::to_string(id)] = {{"lfn", f.lfn},   {"user", f.user},
                                     {"fsz", f.fsz},   {"topen", f.tOpen},
                                     {"rw",  f.rw}};

       json& jp = o["paths"] = json::object();
       for (const auto& [id, e] : s.paths) jp[std::to_string(id)] = e.val;

       json& ji = o["infos"] = json::object();
       for (const auto& [k, e] : s.infos) ji[k] = e.val;

       json& jt = o["tokens"] = json::object();
       for (const auto& [id, t] : s.tokens)
           jt[std::to_string(id)] = {{"sub", t.subject}, {"name", t.username},
                                     {"vo",  t.vo},      {"role", t.role},
                                     {"groups", t.groups}};

       json& ja = o["activity"] = json::object();
       for (const auto& [id, a] : s.activity)
           ja[std::to_string(id)] = {{"exp", a.experiment}, {"act", a.activity}};

       jsv[key] = std::move(o);
      }

// Atomic write: a partial .tmp never replaces a good snapshot.
//
   std::string tmp = path + ".tmp";
   std::ofstream out(tmp, std::ios::trunc);
   if (!out) return false;
   out << j.dump();
   out.flush();
   if (!out) {out.close(); unlink(tmp.c_str()); return false;}
   out.close();
   if (std::rename(tmp.c_str(), path.c_str()) != 0)
      {unlink(tmp.c_str()); return false;}
   return true;
}

bool XrdMonDecode::LoadState(const std::string& path, long maxAgeSec,
                             std::string& note)
{
   note.clear();
   std::ifstream in(path);
   if (!in) return false;             // no snapshot: silent fresh start
   std::ostringstream ss;
   ss << in.rdbuf();
   in.close();
   unlink(path.c_str());              // a snapshot is good for one restart only

   json j = json::parse(ss.str(), nullptr, false);
   if (j.is_discarded() || !j.is_object())
      {note = "state file is not valid JSON; starting fresh";
       return false;}

   int version = j.value("version", -1);
   if (version != kStateVersion)
      {note = "state file has format version " + std::to_string(version)
            + " (expected " + std::to_string(kStateVersion)
            + "); starting fresh";
       return false;}

   int64_t age = (int64_t)time(nullptr) - j.value("saved", (int64_t)0);
   if (age < 0 || age > maxAgeSec)
      {note = "state snapshot is " + std::to_string(age) + "s old (limit "
            + std::to_string(maxAgeSec) + "s); starting fresh";
       return false;}

   try
      {const json& jst = j.at("stats");
#define X(fld) stats.fld = jst.value(#fld, (uint64_t)0);
       XRDMON_STATS_FIELDS(X)
#undef X

       if (auto it = j.find("gsprev"); it != j.end())
          gsPrev = it->get<std::unordered_map<std::string, uint64_t>>();

       // Restore incarnations oldest-first so the rebuilt LRU order
       // approximates recency (entries of long-idle servers evict first).
       const json& jsv = j.at("servers");
       std::vector<std::pair<int64_t, std::string>> order;
       for (const auto& [key, o] : jsv.items())
           order.emplace_back(o.value("seen", (int64_t)0), key);
       std::sort(order.begin(), order.end());

       size_t nUsers = 0, nFiles = 0;
       for (const auto& [seen, key] : order)
          {const json& o = jsv.at(key);
           Server& srv  = servers[key];
           srv.sID      = o.value("sid", (int64_t)0);
           if (auto ps = o.find("pseq"); ps != o.end() && ps->is_object())
              srv.lastPseq = ps->get<std::unordered_map<std::string, int>>();
           srv.lastSeen = (time_t)seen;
           srv.resolved = o.value("resolved", false);
           srv.resolvedHost = o.value("rhost", std::string());
           srv.identRaw     = o.value("identraw", std::string());
           if (auto it = o.find("ident"); it != o.end())
              {srv.ident.site = it->value("site", std::string());
               srv.ident.host = it->value("host", std::string());
               srv.ident.inst = it->value("inst", std::string());
               srv.ident.pgm  = it->value("pgm",  std::string());
               srv.ident.ver  = it->value("ver",  std::string());
               srv.ident.user = it->value("user", std::string());
               srv.ident.port = it->value("port", 0);
              }

           if (auto it = o.find("users"); it != o.end())
              for (const auto& [id, e] : it->items())
                 {UserInfo u;
                  u.raw        = e.value("raw",    std::string());
                  u.user       = e.value("user",   std::string());
                  u.prot       = e.value("prot",   std::string());
                  u.host       = e.value("host",   std::string());
                  u.addr       = e.value("addr",   std::string());
                  u.authMethod = e.value("auth",   std::string());
                  u.vo         = e.value("vo",     std::string());
                  u.role       = e.value("role",   std::string());
                  u.groups     = e.value("groups", std::string());
                  u.clientVer  = e.value("cver",   std::string());
                  u.appName    = e.value("app",    std::string());
                  u.appInfo    = e.value("info",   std::string());
                  u.site       = e.value("site",   std::string());
                  u.ipVersion  = e.value("ipv",    0);
                  if (auto sn = e.find("session"); sn != e.end())
                     {u.sFiles      = sn->value("files", 0u);
                      u.sTransfers  = sn->value("xfers", 0u);
                      u.sAccesses   = sn->value("accs",  0u);
                      u.sErrors     = sn->value("errs",  0u);
                      u.sReadBytes  = sn->value("rb", (int64_t)0);
                      u.sWriteBytes = sn->value("wb", (int64_t)0);
                      u.sFirst      = sn->value("first", 0);
                      u.sLast       = sn->value("last",  0);
                      if (auto fr = sn->find("recent"); fr != sn->end())
                         for (const auto& f : *fr)
                             u.sRecent.push_back(
                                {f.value("lfn", std::string()),
                                 f.value("b", (int64_t)0),
                                 f.value("w", false), f.value("whole", false)});
                     }
                  uint32_t k32 = (uint32_t)std::stoul(id);
                  std::size_t w = bytesOf(u);
                  lruPut(&srv, Dict::Users, srv.users, k32, k32, std::string(),
                         std::move(u), w);
                  nUsers++;
                 }

           if (auto it = o.find("files"); it != o.end())
              for (const auto& [id, e] : it->items())
                 {OpenFile f;
                  f.lfn   = e.value("lfn", std::string());
                  f.user  = e.value("user", 0u);
                  f.fsz   = e.value("fsz", (int64_t)0);
                  f.tOpen = e.value("topen", 0);
                  f.rw    = e.value("rw", false);
                  uint32_t k32 = (uint32_t)std::stoul(id);
                  std::size_t w = bytesOf(f);
                  lruPut(&srv, Dict::Files, srv.files, k32, k32, std::string(),
                         std::move(f), w);
                  nFiles++;
                 }

           if (auto it = o.find("paths"); it != o.end())
              for (const auto& [id, v] : it->items())
                 {StringEntry e;
                  e.val = v.get<std::string>();
                  uint32_t k32 = (uint32_t)std::stoul(id);
                  std::size_t w = bytesOf(e, std::string());
                  lruPut(&srv, Dict::Paths, srv.paths, k32, k32, std::string(),
                         std::move(e), w);
                 }

           if (auto it = o.find("infos"); it != o.end())
              for (const auto& [ikey, v] : it->items())
                 {StringEntry e;
                  e.val = v.get<std::string>();
                  std::size_t w = bytesOf(e, ikey);
                  lruPut(&srv, Dict::Infos, srv.infos, ikey, 0, ikey,
                         std::move(e), w);
                 }

           if (auto it = o.find("tokens"); it != o.end())
              for (const auto& [id, e] : it->items())
                 {TokenInfo t;
                  t.subject  = e.value("sub",    std::string());
                  t.username = e.value("name",   std::string());
                  t.vo       = e.value("vo",     std::string());
                  t.role     = e.value("role",   std::string());
                  t.groups   = e.value("groups", std::string());
                  uint32_t k32 = (uint32_t)std::stoul(id);
                  std::size_t w = bytesOf(t);
                  lruPut(&srv, Dict::Tokens, srv.tokens, k32, k32,
                         std::string(), std::move(t), w);
                 }

           if (auto it = o.find("activity"); it != o.end())
              for (const auto& [id, e] : it->items())
                 {UserActivity a;
                  a.experiment = e.value("exp", 0);
                  a.activity   = e.value("act", 0);
                  uint32_t k32 = (uint32_t)std::stoul(id);
                  std::size_t w = bytesOf(a);
                  lruPut(&srv, Dict::Activity, srv.activity, k32, k32,
                         std::string(), std::move(a), w);
                 }
          }

       note = "restored " + std::to_string(servers.size())
            + " server incarnation(s), " + std::to_string(nUsers)
            + " user(s), " + std::to_string(nFiles)
            + " open file(s) from a " + std::to_string(age)
            + "s old snapshot";
       return true;
      }
   catch (const std::exception& e)
      {// A partial restore would leave dangling LRU accounting: start fresh.
       servers.clear();
       gsPrev.clear();
       lru.clear();
       lruBytes = 0;
       stats = Stats{};
       note = std::string("state file could not be decoded (") + e.what()
            + "); starting fresh";
       return false;
      }
}

/******************************************************************************/
/*                             P r o c e s s                                  */
/******************************************************************************/

namespace
{
// Defined with DecodeGStream below; also needed here for the pseq class.
const char* gsProvider(unsigned char t);

// Stream label for the malformed metric, from the header code byte. Bounded
// cardinality: anything but the known codes collapses into "unknown".
const char* streamOf(unsigned char code)
{
   switch(code)
         {case XROOTD_MON_MAPUSER: return "u";
          case XROOTD_MON_MAPPATH: return "d";
          case XROOTD_MON_MAPINFO: return "i";
          case XROOTD_MON_MAPTOKN: return "T";
          case XROOTD_MON_MAPUEAC: return "U";
          case XROOTD_MON_MAPIDNT: return "=";
          case XROOTD_MON_MAPXFER: return "x";
          case XROOTD_MON_MAPPURG: return "p";
          case XROOTD_MON_MAPFSTA: return "f";
          case XROOTD_MON_MAPTRCE: return "t";
          case XROOTD_MON_MAPGSTA: return "g";
          case XROOTD_MON_MAPREDR: return "r";
          default:                 return "unknown";
         }
}
}

void XrdMonDecode::Malformed(const std::string& src, unsigned char code,
                             const char* reason)
{
   stats.malformed++;
   if (metrics)
      metrics->counterSeries("malformed_total",
           "structurally invalid monitor packets by stream and reason",
           {{"server", src}, {"stream", streamOf(code)},
            {"reason", reason}}) += 1;
}

bool XrdMonDecode::Process(const std::string& src, const char* buff, int blen)
{
   const unsigned char* p = (const unsigned char*)buff;

   stats.packets++;

// Every packet starts with an 8-byte XrdXrootdMonHeader.
//
   if (blen < 8)
      {Malformed(src, blen >= 1 ? p[0] : 0, "short_packet"); return false;}

   unsigned char code = p[0];
   int           plen = rd16(p + 2);
   int32_t       stod = ri32(p + 4);

   if (plen < 8 || plen > blen) {Malformed(src, code, "bad_plen"); return false;}

   Server& srv = ServerFor(src, stod);

// Packet-loss estimate from the header pseq (wrapping at 256). The sequence
// counters are per stream class, NOT per destination: the f-stream and each
// g-stream provider stamp their own independent counters, while the trace/
// redirect/map streams share the per-destination one ("main"); see
// XrdXrootdMonFile::Flush and XrdXrootdGSReal vs XrdXrootdMonitor::Send. A
// forward gap within a class means lost packets; a small backward step is
// reordering (UDP) and is ignored.
//
   unsigned char pseq = p[1];
   {const char* sclass = "main";
    std::string gclass;
    if (code == XROOTD_MON_MAPFSTA) sclass = "f";
       else if (code == XROOTD_MON_MAPGSTA)
       {gclass = "g:";              // per provider (top byte of the sID)
        gclass += (plen >= 24 ? gsProvider(p[16]) : "unknown");
        sclass = gclass.c_str();
       }
    auto it = srv.lastPseq.find(sclass);
    if (it == srv.lastPseq.end()) srv.lastPseq.emplace(sclass, pseq);
       else
       {int gap = ((int)pseq - ((it->second + 1) & 0xff)) & 0xff;
        if (gap > 0 && gap < 128)
           {stats.lost += gap;
            if (metrics)
               metrics->counterSeries("packets_lost_total",
                    "estimated lost packets (pseq gaps)",
                    {{"server", src}, {"stream", sclass}}) += gap;
           }
        it->second = pseq;
       }
   }

   switch(code)
         {case XROOTD_MON_MAPUSER:
          case XROOTD_MON_MAPPATH:
          case XROOTD_MON_MAPINFO:
          case XROOTD_MON_MAPTOKN:
          case XROOTD_MON_MAPUEAC:
               // XrdXrootdMonMap: header(8) + dictid(4) + info[]
               if (plen < 12) {Malformed(src, code, "short_packet"); return false;}
               {uint32_t dictid = rd32(p + 8);
                DecodeMap(code, srv, dictid, (const char*)(p + 12), plen - 12);
               }
               break;

          case XROOTD_MON_MAPIDNT:
               // Server self-identification: header(8) + dictid(4, =0) + info[]
               if (plen < 12) {Malformed(src, code, "short_packet"); return false;}
               DecodeIdent(src, stod, srv, (const char*)(p + 12), plen - 12);
               break;

          case XROOTD_MON_MAPXFER:
          case XROOTD_MON_MAPPURG:
               // FRM stage/migrate ('x') and purge ('p'): map record with
               // dictid 0 and info "<who>\n<path>[\n&cgi]".
               if (plen < 12) {Malformed(src, code, "short_packet"); return false;}
               DecodeFrm(src, stod, srv, code, (const char*)(p + 12), plen - 12);
               break;

          case XROOTD_MON_MAPFSTA:
               DecodeFStream(src, stod, srv, p + 8, plen - 8);
               break;

          case XROOTD_MON_MAPTRCE:
               DecodeTStream(src, stod, srv, p + 8, plen - 8);
               break;

          case XROOTD_MON_MAPGSTA:
               DecodeGStream(src, stod, srv, p, plen);
               break;

          case XROOTD_MON_MAPREDR:
               DecodeRStream(src, stod, srv, p, plen);
               break;

          default:
               stats.unknown++;
               if (dumpRaw && raw)
                  {json j = {{"code", std::string(1, (char)code)},
                             {"server", src}, {"stod", stod},
                             {"len", plen}, {"note", "unhandled stream"}};
                   raw(j.dump());
                  }
               break;
         }

   // Insertions enforce the budget inline (lruPut), so nothing to do here.
   return true;
}

/******************************************************************************/
/*                        L R U   b o o k k e e p i n g                       */
/******************************************************************************/

namespace
{
// Fixed per-entry overhead charged on top of held strings: an approximation of
// the unordered_map node plus the LRU list node plus the value struct. Exact
// allocator behaviour is unknowable here; this only needs to be a stable,
// representative weight so the budget tracks real growth.
//
constexpr std::size_t kEntryOverhead = 96;
}

std::size_t XrdMonDecode::bytesOf(const UserInfo& u)
{
   std::size_t recent = 0;
   for (const auto& f : u.sRecent) recent += sizeof(f) + f.lfn.size();
   return kEntryOverhead + u.raw.size() + u.user.size() + u.prot.size()
        + u.host.size() + u.addr.size() + u.authMethod.size() + u.vo.size()
        + u.role.size() + u.groups.size() + u.clientVer.size()
        + u.appName.size() + u.appInfo.size() + u.site.size() + recent;
}

std::size_t XrdMonDecode::bytesOf(const OpenFile& f)
{  return kEntryOverhead + f.lfn.size(); }

std::size_t XrdMonDecode::bytesOf(const TokenInfo& t)
{
   return kEntryOverhead + t.subject.size() + t.username.size() + t.vo.size()
        + t.role.size() + t.groups.size();
}

std::size_t XrdMonDecode::bytesOf(const UserActivity&)
{  return kEntryOverhead; }

std::size_t XrdMonDecode::bytesOf(const StringEntry& s, const std::string& key)
{  return kEntryOverhead + s.val.size() + key.size(); }

// Drop the current least-recently-used entry: erase it from its owning map and
// unlink its node. The node is copied out first so its (Infos) string key
// survives the map erase.
//
void XrdMonDecode::EvictFront()
{
   LruNode n = lru.front();
   lruBytes -= n.bytes;
   switch(n.dict)
         {case Dict::Users:    n.srv->users.erase(n.ikey);    break;
          case Dict::Files:    n.srv->files.erase(n.ikey);    break;
          case Dict::Paths:    n.srv->paths.erase(n.ikey);    break;
          case Dict::Infos:    n.srv->infos.erase(n.skey);    break;
          case Dict::Tokens:   n.srv->tokens.erase(n.ikey);   break;
          case Dict::Activity: n.srv->activity.erase(n.ikey); break;
         }
   lru.pop_front();
   stats.evicted++;
}

// Evict least-recently-used entries until both the byte budget and the optional
// entry-count cap are satisfied. The byte budget evicts down to a low-water mark
// (15/16 of the budget) to avoid evicting on every subsequent insertion.
//
void XrdMonDecode::EnforceBudget()
{
   std::size_t low = maxBytes - maxBytes/16;
   while (!lru.empty()
       && ((maxBytes   && lruBytes   > low)
        || (maxEntries && lru.size() > maxEntries)))
        EvictFront();
}

/******************************************************************************/
/*                          R e a p S e r v e r s                             */
/******************************************************************************/

void XrdMonDecode::ReapServers(time_t now)
{
   if (!serverTTL) return;

   for (auto it = servers.begin(); it != servers.end(); )
      {Server& s = it->second;
       if (!s.lastSeen || now - s.lastSeen <= serverTTL) {++it; continue;}

       // Unlink every entry's LRU node and uncharge its bytes before the maps
       // (and the Server) are destroyed.
       auto purge = [&](auto& m)
          {for (auto& kv : m) {lruBytes -= kv.second.lru->bytes;
                               lru.erase(kv.second.lru);}};
       purge(s.users); purge(s.files); purge(s.paths);
       purge(s.infos); purge(s.tokens); purge(s.activity);

       // gsPrev is keyed by sender (not by incarnation), so only drop its
       // counter baselines when no other live incarnation shares this sender;
       // otherwise that live series would lose one interval re-establishing it.
       std::string pre = it->first.substr(0, it->first.rfind('|')) + '|';
       bool shared = false;
       for (auto& kv : servers)
           if (&kv.second != &s && kv.first.compare(0, pre.size(), pre) == 0)
              {shared = true; break;}
       if (!shared)
          for (auto g = gsPrev.begin(); g != gsPrev.end(); )
              {if (g->first.compare(0, pre.size(), pre) == 0) g = gsPrev.erase(g);
                  else ++g;
              }

       it = servers.erase(it);
       stats.reaped++;
      }
}

/******************************************************************************/
/*                            D e c o d e M a p                               */
/******************************************************************************/

void XrdMonDecode::DecodeMap(unsigned char code, Server& srv,
                             uint32_t dictid, const char* info, int ilen)
{
   stats.records++;

// The info is "<first line>\n<extra>"; the first line is the identity/path
// descriptor. Make a bounded std::string (the buffer is not guaranteed null
// terminated up to ilen).
//
   std::string text(info, ilen > 0 ? ilen : 0);
   std::string first = text.substr(0, text.find('\n'));

   if (code == XROOTD_MON_MAPUSER)
      {stats.mapUser++;
       UserInfo u;
       u.raw = first;
       // Descriptor: <prot>/<user>.<pid>:<sfd>@<host>
       auto slash = first.find('/');
       auto at    = first.rfind('@');
       if (slash != std::string::npos)
          {u.prot = first.substr(0, slash);
           std::string rest = first.substr(slash + 1);
           auto dot = rest.find('.');
           u.user = rest.substr(0, dot);
          }
       if (at != std::string::npos) u.host = first.substr(at + 1);
       // CGI tail (after the descriptor line): login appinfo is always present
       // (&a= &R= &x= &y= &I=); auth info (&p= &o= &r= &g=) only with "...
       // auth". cgiVal scans the whole string; the descriptor line carries no
       // "&key=". The &a= numeric client IP (added in 6.x) is preferred over
       // the descriptor @host, which may be a reverse-resolved name.
       u.addr       = cgiVal(text, "a");
       u.authMethod = cgiVal(text, "p");
       u.vo         = cgiVal(text, "o");
       // Gate the auth-CGI VO once per login: only methods that can actually
       // convey one keep it (see authConveysVO). The 'T' token VO is separate.
       if (!authConveysVO(u.authMethod)) u.vo.clear();
       u.role       = cgiVal(text, "r");
       u.groups     = cgiVal(text, "g");
       u.clientVer  = cgiVal(text, "R");
       u.appName    = cgiVal(text, "x");
       u.appInfo    = cgiVal(text, "y");
       u.site       = cgiVal(text, "S");
       std::string iv = cgiVal(text, "I");
       if (!iv.empty()) u.ipVersion = atoi(iv.c_str());
       std::size_t w = bytesOf(u);
       lruPut(&srv, Dict::Users, srv.users, dictid, dictid, std::string(),
              std::move(u), w);
      }
      else if (code == XROOTD_MON_MAPPATH)
              {stats.mapPath++;
               // info is "<who>\n<lfn>"; keep the lfn for 't'-stream lookups.
               auto nl = text.find('\n');
               StringEntry e;
               e.val = (nl == std::string::npos) ? text : text.substr(nl + 1);
               std::size_t w = bytesOf(e, std::string());
               lruPut(&srv, Dict::Paths, srv.paths, dictid, dictid, std::string(),
                      std::move(e), w);
              }
      else if (code == XROOTD_MON_MAPINFO)
              {stats.mapInfo++;
               // 'i' (appinfo): "<descriptor>\n<appinfo>". The descriptor matches
               // the 'u' user descriptor, so key by it to enrich transfers.
               auto nl = text.find('\n');
               if (nl != std::string::npos)
                  {std::string ikey = text.substr(0, nl);
                   StringEntry e;
                   e.val = text.substr(nl + 1);
                   std::size_t w = bytesOf(e, ikey);
                   lruPut(&srv, Dict::Infos, srv.infos, ikey, 0, ikey,
                          std::move(e), w);
                  }
              }
      else if (code == XROOTD_MON_MAPTOKN)
              {stats.mapTokn++;
               // 'T' (token): CGI "&Uc=<dictid>&s=&n=&o=&r=&g=", keyed by the
               // same user dictid as the 'u' map so it joins onto transfers.
               TokenInfo t;
               t.subject  = cgiVal(text, "s");
               t.username = cgiVal(text, "n");
               t.vo       = cgiVal(text, "o");
               t.role     = cgiVal(text, "r");
               t.groups   = cgiVal(text, "g");
               std::size_t w = bytesOf(t);
               lruPut(&srv, Dict::Tokens, srv.tokens, dictid, dictid,
                      std::string(), std::move(t), w);
              }
      else    {stats.mapUeac++;
               // 'U' (user experiment/activity): CGI "&Uc=<dictid>&Ec=&Ac=",
               // the SciTags experiment/activity flow labels.
               UserActivity a;
               std::string ec = cgiVal(text, "Ec");
               std::string ac = cgiVal(text, "Ac");
               if (!ec.empty()) a.experiment = atoi(ec.c_str());
               if (!ac.empty()) a.activity   = atoi(ac.c_str());
               std::size_t w = bytesOf(a);
               lruPut(&srv, Dict::Activity, srv.activity, dictid, dictid,
                      std::string(), std::move(a), w);
              }

   if (dumpRaw && raw)
      {json j = {{"code", std::string(1, (char)code)},
                 {"dictid", dictid}, {"info", first}};
       raw(j.dump());
      }
}

/******************************************************************************/
/*                          D e c o d e I d e n t                             */
/******************************************************************************/

void XrdMonDecode::DecodeIdent(const std::string& src, int32_t stod,
                               Server& srv, const char* info, int ilen)
{
   stats.records++;
   stats.mapIdnt++;

// The identity record is "=/<user>.<pid>:<sid>@<host>\n&site=&port=&inst=&pgm=
// &ver=". The first line carries the login user and host, the second a CGI tail.
//
   std::string text(info, ilen > 0 ? ilen : 0);
   std::string first = text.substr(0, text.find('\n'));

   ServerIdent& id = srv.ident;
   auto slash = first.find('/');
   auto at    = first.rfind('@');
   if (slash != std::string::npos)
      {auto end = (at != std::string::npos) ? at : first.size();
       if (end > slash + 1) id.user = first.substr(slash + 1, end - slash - 1);
      }
   if (at != std::string::npos) id.host = first.substr(at + 1);

   id.site = cgiVal(text, "site");
   id.inst = cgiVal(text, "inst");
   id.pgm  = cgiVal(text, "pgm");
   id.ver  = cgiVal(text, "ver");
   std::string port = cgiVal(text, "port");
   if (!port.empty()) id.port = atoi(port.c_str());

   if (dumpRaw && raw)
      {json j = {{"code", "="}, {"info", first}};
       raw(j.dump());
      }

// Emit a server-identity document, but only when the content changes (the
// server re-sends this every monitor "ident" interval, default hourly). All of
// the identity now lives in the resource block (service.*/server.*/xrootd.*).
//
   if (text == srv.identRaw) return;
   srv.identRaw = text;

   json j;
   otelResource(j, src, stod, srv);
   otelBegin(j, "xrootd.server_ident", (int32_t)time(nullptr), false);
   if (doc) doc(j.dump());
}

/******************************************************************************/
/*                            D e c o d e F r m                               */
/******************************************************************************/

void XrdMonDecode::DecodeFrm(const std::string& src, int32_t stod, Server& srv,
                             unsigned char code, const char* info, int ilen)
{
   stats.records++;
   stats.frmEvents++;

// info is "<who>\n<path>[\n&tod=&sz=&at=&ct=&mt=&fn=]" (the CGI tail is present
// for purge records). 'x' carries stage and migrate, 'p' carries purge.
//
   std::string text(info, ilen > 0 ? ilen : 0);
   auto nl1  = text.find('\n');
   std::string who  = text.substr(0, nl1);
   std::string rest = (nl1 == std::string::npos) ? "" : text.substr(nl1 + 1);
   auto nl2  = rest.find('\n');
   std::string path = rest.substr(0, nl2);
   std::string cgi  = (nl2 == std::string::npos) ? "" : rest.substr(nl2 + 1);

   const char* op = (code == XROOTD_MON_MAPPURG) ? "purge" : "transfer";

// Event time (and size) come from the CGI tail on purge records; parse them
// first so the envelope carries the right @timestamp/timeUnixNano.
//
   int64_t sz = -1;
   int32_t tEvt = 0;
   std::string szS, todS;
   if (!cgi.empty()) {szS = cgiVal(cgi, "sz"); todS = cgiVal(cgi, "tod");}
   if (!todS.empty()) tEvt = (int32_t)atoll(todS.c_str());

   json j;
   otelResource(j, src, stod, srv);
   otelBegin(j, "xrootd.frm", tEvt, false);
   json& a = j["attributes"];
   a["xrootd.operation"] = op;         // "purge" or "transfer" (stage/migrate)
   setFile(a, path);

// who is "<prot>/<user>.<pid>:<sid>@<host>". The FRM agent is the actor here,
// so it has no user dictionary entry; parse the descriptor inline.
//
   auto slash = who.find('/');
   auto at    = who.rfind('@');
   if (slash != std::string::npos)
      {a["xrootd.user.protocol"] = who.substr(0, slash);
       std::string r = who.substr(slash + 1);
       a["user.name"] = r.substr(0, r.find('.'));
      }
   if (at != std::string::npos)
      {std::string h = who.substr(at + 1);
       if (resolveHosts && isLocalName(h) && !localHost.empty()) h = localHost;
       a["client.address"] = publicFor(h);
      }

   if (!szS.empty()) {sz = atoll(szS.c_str()); a["file.size"] = sz;}

   if (metrics)
      {metrics->counterSeries("frm_total", "FRM stage/purge events",
                        {{"server", src}, {"op", op}}) += 1;
       if (code == XROOTD_MON_MAPPURG && sz > 0)
          metrics->counterSeries("frm_purge_bytes_total",
                        "bytes purged by FRM", {{"server", src}}) += sz;
      }

   if (doc) doc(j.dump());
}

/******************************************************************************/
/*                        D e c o d e F S t r e a m                           */
/******************************************************************************/

void XrdMonDecode::DecodeFStream(const std::string& src, int32_t stod,
                                 Server& srv, const unsigned char* p, int len)
{
   int     off  = 0;
   int32_t tWin = 0;

   while(off + 8 <= len)
        {const unsigned char* rec = p + off;
         unsigned char recType = rec[0];
         unsigned char recFlag = rec[1];
         int           recSize = rd16(rec + 2);

         if (recSize < 8 || off + recSize > len)
            {Malformed(src, XROOTD_MON_MAPFSTA, "bad_record"); break;}
         stats.records++;

         if (dumpRaw && raw)
            {json j = {{"fstream_rec", (int)recType}, {"flag", (int)recFlag},
                       {"size", recSize}, {"id", rd32(rec + 4)}};
             raw(j.dump());
            }

         switch(recType)
               {case XrdXrootdMonFileHdr::isTime:
                     // Hdr(8) + tBeg(4) + tEnd(4) + sID(8)
                     if (recSize >= 24)
                        {tWin    = ri32(rec + 12);   // tEnd
                         srv.sID = ri64(rec + 16);
                        }
                     break;

                case XrdXrootdMonFileHdr::isOpen:
                     {stats.opens++;
                      uint32_t fileID = rd32(rec + 4);
                      OpenFile of;
                      of.fsz   = ri64(rec + 8);
                      of.tOpen = tWin;
                      of.rw    = (recFlag & XrdXrootdMonFileHdr::hasRW) != 0;
                      if (recFlag & XrdXrootdMonFileHdr::hasLFN && recSize > 20)
                         {of.user = rd32(rec + 16);
                          const char* l = (const char*)(rec + 20);
                          int maxL = recSize - 20;
                          of.lfn.assign(l, strnlen(l, maxL));
                         }
                      std::size_t w = bytesOf(of);
                      lruPut(&srv, Dict::Files, srv.files, fileID, fileID,
                             std::string(), std::move(of), w);
                     }
                     break;

                case XrdXrootdMonFileHdr::isClose:
                     {stats.closes++;
                      uint32_t fileID = rd32(rec + 4);
                      EmitClose(src, stod, srv, fileID, recFlag, rec, recSize,
                                tWin);
                     }
                     break;

                case XrdXrootdMonFileHdr::isXfr:
                     // In-flight snapshot (interval byte totals for an open
                     // file). Counted; drives the active-transfer gauge below.
                     // Also keeps a still-active open warm in the LRU so a long
                     // but live transfer is not evicted ahead of cold strays.
                     {stats.xfrs++;
                      uint32_t fileID = rd32(rec + 4);
                      auto fit = srv.files.find(fileID);
                      if (fit != srv.files.end()) Touch(fit->second.lru);
                     }
                     break;

                case XrdXrootdMonFileHdr::isDisc:
                     {stats.discs++;
                      uint32_t userID = rd32(rec + 4);
                      EmitDisc(src, stod, srv, userID, tWin);
                     }
                     break;

                case XrdXrootdMonFileHdr::isError:
                     // Terminal report for a failed/aborted operation that never
                     // produced an isClose (e.g. a failed open).
                     EmitError(src, stod, srv, recFlag, rec, recSize, tWin);
                     break;

                default:
                     break;
               }

         off += recSize;
        }

// Reflect the current number of open files (transfers in progress) for this
// server as a gauge. Computed from the open-file table, so it tracks the opens
// and closes processed in this packet.
//
   if (metrics)
      metrics->gaugeSeries("active_transfers",
                     "files currently open (transfers in progress)",
                     {{"server", src}}) = (double)srv.files.size();
}

/******************************************************************************/
/*                             E m i t D i s c                                */
/******************************************************************************/

void XrdMonDecode::EmitDisc(const std::string& src, int32_t stod, Server& srv,
                            uint32_t userID, int32_t tWin)
{
   if (!emitSessions) return;             // session documents disabled

   json j;
   otelResource(j, src, stod, srv);
   otelBegin(j, "xrootd.session", tWin, false);
   json& a = j["attributes"];
   otelIdentity(a, srv, userID);

// Attach the session's aggregated file activity (counters plus a capped recent-
// file list) accumulated from every close that named this user (see foldSession).
//
   auto uit = srv.users.find(userID);
   if (uit != srv.users.end()) otelSession(a, uit->second);

// The session document is the root span of the client's trace (login ->
// disconnect); each per-file operation carries the same traceId.
//
   std::string sess = sessKey(src, stod, userID);
   j["traceId"] = traceIdOf(sess);
   j["spanId"]  = spanIdOf(sess + "|session");

   if (metrics)
      metrics->counterSeries("sessions_total",
                       "client sessions ended", {{"server", src}}) += 1;

   if (doc) doc(j.dump());

// The session span is the trace root (no parent), spanning the folded activity
// from the first close to this disconnect.
//
   int32_t sBeg = (uit != srv.users.end() && uit->second.sFirst > 0)
                ? uit->second.sFirst : 0;
   emitSpan(j, "session", sBeg, tWin, std::string());
}

/******************************************************************************/
/*                            E m i t C l o s e                               */
/******************************************************************************/

void XrdMonDecode::EmitClose(const std::string& src, int32_t stod, Server& srv,
                             uint32_t fileID, unsigned char recFlag,
                             const unsigned char* rec, int recSize, int32_t tWin)
{
// Always-present transfer byte totals (XrdXrootdMonStatXFR after the 8-byte hdr).
//
   if (recSize < 8 + 24) {Malformed(src, XROOTD_MON_MAPFSTA, "bad_record"); return;}
   int64_t rdBytes = ri64(rec + 8);
   int64_t rvBytes = ri64(rec + 16);
   int64_t wrBytes = ri64(rec + 24);

   int durSecs = -1;
   std::string vo;
   bool     haveOpen = false;  // matched the open record (so fsz is known)
   int64_t  openFsz  = 0;      // file size captured at open
   uint32_t openUser = 0;      // user dictid from the open (for session rollup)
   std::string openLfn;        // lfn from the open (for the session rollup)
   int32_t  openTBeg = 0;      // open time (for the file-operation span start)

// Terminal status is needed up front: it drives the log severity and the
// whole-file-vs-access decision. "forced" (disconnect-driven) is not a failure
// on its own; only a trailing XrdXrootdMonStatERR (hasERR) is.
//
   const bool forced = (recFlag & XrdXrootdMonFileHdr::forced) != 0;
   const bool hasErr = (recFlag & XrdXrootdMonFileHdr::hasERR) != 0;

// One OTel log record: process-level j["resource"] (server) plus event-level
// j["attributes"] with dotted semantic-convention keys. Empty/zero fields are
// omitted. See README.md for the field-to-semconv/WLCG mapping.
//
   json j;
   otelResource(j, src, stod, srv);
   otelBegin(j, "xrootd.transfer", tWin, hasErr);
   json& a = j["attributes"];

   a["xrootd.transfer.forced_close"] = forced;
   a["xrootd.transfer.read_bytes"]   = rdBytes;
   a["xrootd.transfer.readv_bytes"]  = rvBytes;
   a["xrootd.transfer.write_bytes"]  = wrBytes;
   // Explicit read/write categorisation (WLCG operation_type): any write bytes
   // make it a write, otherwise it is a read.
   a["xrootd.operation"] = (wrBytes > 0) ? "write" : "read";

// Join the matching open record (held since the open packet) to recover the
// path, the user, and the open time. Resolve the user dictid if we have it.
//
   auto fit = srv.files.find(fileID);
   if (fit != srv.files.end())
      {const OpenFile& of = fit->second;
       a["xrootd.transfer.open_seen"] = true;
       haveOpen = true;
       openFsz  = of.fsz;
       openUser = of.user;
       openLfn  = of.lfn;
       openTBeg = of.tOpen;
       setFile(a, of.lfn);
       a["file.size"] = of.fsz;
       a["xrootd.file.read_write"] = of.rw;
       if (of.tOpen > 0) a["xrootd.transfer.start_time"] = isoTime(of.tOpen);
       if (of.tOpen > 0 && tWin > 0) {durSecs = tWin - of.tOpen;
                                      a["xrootd.transfer.duration"] = durSecs;}

       vo = otelIdentity(a, srv, of.user);

       // LAN/WAN heuristic: tag the transfer local when the client and the
       // reporting server share a registered domain. Only decidable when both
       // are resolvable host names (not IP literals); otherwise left unset.
       // The client name lives in xrootd.client.host now that client.address
       // is numeric; fall back to client.address for pre-&a= servers.
       if (!srv.ident.host.empty())
          {std::string ch = a.value("xrootd.client.host",
                            a.value("client.address", std::string()));
           std::string cd = hostDomain(ch);
           std::string sd = hostDomain(srv.ident.host);
           if (!cd.empty() && !sd.empty())
              a["xrootd.transfer.is_local"] = (cd == sd);
          }
       LruDrop(fit->second.lru);   // unlink before erasing the map entry
       srv.files.erase(fit);
      }
      else {a["xrootd.transfer.open_seen"] = false; stats.orphanCls++;}

// Whole-file transfer vs. partial access. The document schema is identical; only
// xrootd.transfer.kind differs (see wholeFileClose). A write needs to have ended
// cleanly to count as a whole-file producer, so this must be decided after the
// forced/error flags are known.
//
   const bool whole  = wholeFileClose(haveOpen, openFsz, rdBytes, rvBytes,
                                      wrBytes, forced, hasErr);
   a["xrootd.transfer.kind"] = whole ? "transfer" : "access";

// Trace context: the client session is the trace (keyed by the open's user
// dictid); this file open->close is a span within it.
//
   j["traceId"] = traceIdOf(sessKey(src, stod, openUser));
   j["spanId"]  = spanIdOf(src + "|" + std::to_string(stod)
                              + "|f" + std::to_string(fileID));

// Fold this close into its session rollup (emitted in the 'session' document at
// disconnect). Only possible when the open was joined, which carries the user.
//
   if (haveOpen)
      {const bool    write = wrBytes > 0;
       const int64_t moved = write ? wrBytes : rdBytes + rvBytes;
       foldSession(srv, openUser, openLfn, moved, write, whole, hasErr, tWin);
      }

// Optional op-count detail (XrdXrootdMonStatOPS) when "ops" was configured.
//
   if ((recFlag & XrdXrootdMonFileHdr::hasOPS) && recSize >= 8 + 24 + 48)
      {const unsigned char* o = rec + 8 + 24;
       a["xrootd.transfer.read_ops"]   = ri32(o + 0);
       a["xrootd.transfer.readv_ops"]  = ri32(o + 4);
       a["xrootd.transfer.write_ops"]  = ri32(o + 8);
       a["xrootd.transfer.readv_segs"] = ri64(o + 16);

       // Request-size extremes use 0x7fffffff as the "unset" sentinel; omit
       // them rather than emit a misleading minimum.
       //
       auto minmax = [&](const char* kmn, const char* kmx, int32_t mn, int32_t mx)
                       {if (mn != 0x7fffffff) {a[kmn] = mn; a[kmx] = mx;}};
       minmax("xrootd.transfer.read_min",  "xrootd.transfer.read_max",
              ri32(o + 24), ri32(o + 28));
       minmax("xrootd.transfer.readv_min", "xrootd.transfer.readv_max",
              ri32(o + 32), ri32(o + 36));
       minmax("xrootd.transfer.write_min", "xrootd.transfer.write_max",
              ri32(o + 40), ri32(o + 44));

       // Optional sum-of-squares (XrdXrootdMonStatSSQ) when "ssq" configured.
       //
       if ((recFlag & XrdXrootdMonFileHdr::hasSSQ) && recSize >= 8 + 24 + 48 + 32)
          {const unsigned char* s = o + 48;
           a["xrootd.transfer.read_sumsq"]  = rdbl(s + 0);
           a["xrootd.transfer.readv_sumsq"] = rdbl(s + 8);
           a["xrootd.transfer.rsegs_sumsq"] = rdbl(s + 16);
           a["xrootd.transfer.write_sumsq"] = rdbl(s + 24);
          }
      }

// Terminal status (WLCG operation_state). A close carrying a trailing
// XrdXrootdMonStatERR (hasERR) reports an aborted/failed transfer; the error
// block trails any XrdXrootdMonStatOPS/SSQ blocks. Otherwise the close is the
// authoritative success report.
//
   int errOff = 8 + 24;
   if (recFlag & XrdXrootdMonFileHdr::hasOPS)
      {errOff += 48;
       if (recFlag & XrdXrootdMonFileHdr::hasSSQ) errOff += 32;
      }
   if ((recFlag & XrdXrootdMonFileHdr::hasERR) && recSize >= errOff + 8)
      {std::string cat = otelError(a, rec + errOff, recSize - errOff);
       stats.failed++;
       if (metrics)
          metrics->counterSeries("failed_operations_total",
                       "operations that concluded unsuccessfully",
                       {{"server", src},
                        {"category", cat.empty() ? "unknown" : cat}})
                  += 1;
      }
   else a["xrootd.operation_state"] = "Successful";

// Aggregate into bounded-cardinality Prometheus series (label only by the
// reporting server). Per-transfer detail stays in the document sink; here we
// keep just totals and distributions suitable for time-series storage.
//
   if (metrics)
      {std::vector<XrdMetrics::ConstLabel> sl = {{"server", src}};
       if (whole)
          metrics->counterSeries("transfers_total",
                           "completed whole-file transfers seen", sl) += 1;
          else
          metrics->counterSeries("accesses_total",
                           "completed partial-access closes seen", sl) += 1;
       metrics->counterSeries("read_bytes_total",
                        "bytes read (read+readv)", sl) += rdBytes + rvBytes;
       metrics->counterSeries("write_bytes_total",
                        "bytes written", sl) += wrBytes;
       if (!vo.empty())
          metrics->counterSeries("vo_transfers_total",
                        "completed transfers per VO",
                        {{"server", src}, {"vo", vo}}) += 1;
       if (a.contains("xrootd.transfer.is_local"))
          metrics->counterSeries("locality_transfers_total",
                        "completed transfers by client/server locality",
                        {{"server", src}, {"locality",
                         a["xrootd.transfer.is_local"].get<bool>() ? "local"
                                                                   : "remote"}})
                  += 1;
       metrics->histogramSeries("transfer_size_bytes",
                        "bytes moved per transfer",
                        {1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11})
               .observe((double)(rdBytes + rvBytes + wrBytes));
       if (durSecs >= 0)
          metrics->histogramSeries("transfer_duration_seconds",
                        "transfer wall-clock duration",
                        {1,5,15,60,300,1800,7200}).observe(durSecs);
      }

   stats.docs++;
   if (doc) doc(j.dump());

// Companion span for this file operation (open -> close), child of the session
// span. Start at the open time when known, else the close window.
//
   emitSpan(j, (wrBytes > 0) ? "write" : "read",
            openTBeg > 0 ? openTBeg : tWin, tWin,
            spanIdOf(sessKey(src, stod, openUser) + "|session"));
}

/******************************************************************************/
/*                             o t e l E r r o r                              */
/******************************************************************************/

std::string XrdMonDecode::otelError(json& a, const unsigned char* err,
                                    int errLen)
{
// XrdXrootdMonStatERR: ecode(4) + ecat(1) + rsvd(3) + null-terminated message.
//
   if (errLen < 8) return "";
   std::string cat = errCatName((unsigned char)err[4]);
   a["xrootd.operation_state"] = "Failed";
   a["error.type"]             = cat;           // semconv low-cardinality type
   a["xrootd.error.code"]      = ri32(err);
   const char* m = (const char*)(err + 8);
   std::string msg(m, strnlen(m, errLen - 8));
   if (!msg.empty()) a["error.message"] = msg;
   return cat;
}

/******************************************************************************/
/*                            E m i t E r r o r                               */
/******************************************************************************/

void XrdMonDecode::EmitError(const std::string& src, int32_t stod, Server& srv,
                             unsigned char recFlag, const unsigned char* rec,
                             int recSize, int32_t tWin)
{
// Self-contained terminal report for an operation that never produced an
// isClose (e.g. a failed/denied open). Layout: Hdr(8) + ufn{user(4)+lfn} +
// XrdXrootdMonStatERR. The lfn and user are carried inline (hasLFN) because a
// failed open creates no path/open dictionary entry to join against.
//
   if (recSize < 8 + 4) {Malformed(src, XROOTD_MON_MAPFSTA, "bad_record"); return;}

   json j;
   otelResource(j, src, stod, srv);
   otelBegin(j, "xrootd.transfer", tWin, true);   // a terminal error: ERROR
   json& a = j["attributes"];

   uint32_t user = 0;
   int off = 8;
   if ((recFlag & XrdXrootdMonFileHdr::hasLFN) && recSize > 12)
      {user = rd32(rec + 8);
       const char* l = (const char*)(rec + 12);
       std::string lfn(l, strnlen(l, recSize - 12));
       setFile(a, lfn);
       off = 12 + (int)lfn.size() + 1;       // past the lfn's terminating null
       otelIdentity(a, srv, user);
      }

   std::string cat = otelError(a, rec + off, recSize - off);

// Trace context: session trace keyed by the (inline) user dictid; the failed
// operation is its own span.
//
   std::string sess = sessKey(src, stod, user);
   j["traceId"] = traceIdOf(sess);
   j["spanId"]  = spanIdOf(sess + "|err|" + std::to_string(tWin));

   if (metrics)
      metrics->counterSeries("failed_operations_total",
                   "operations that concluded unsuccessfully",
                   {{"server", src},
                    {"category", cat.empty() ? "unknown" : cat}}) += 1;

   stats.failed++;
   stats.docs++;
   if (doc) doc(j.dump());

// Companion span for the failed operation (zero-duration, ERROR status), child
// of the session span.
//
   emitSpan(j, cat.empty() ? "operation" : cat.c_str(), tWin, tWin,
            spanIdOf(sess + "|session"));
}

/******************************************************************************/
/*                        D e c o d e T S t r e a m                           */
/******************************************************************************/

void XrdMonDecode::DecodeTStream(const std::string& src, int32_t stod,
                                 Server& srv, const unsigned char* p, int len)
{
   int32_t tWin = 0;

// The "t" stream is an array of fixed 16-byte XrdXrootdMonTrace records. The
// first byte discriminates the record; values with the high bit clear are I/O
// (read/write) entries, the rest are markers (open/close/disc/window/...).
// A payload that is not a whole number of records was truncated somewhere;
// the leftover bytes are dropped, but the packet is flagged.
//
   if (len % 16) Malformed(src, XROOTD_MON_MAPTRCE, "trailing_bytes");

   for (int off = 0; off + 16 <= len; off += 16)
       {const unsigned char* a0 = p + off;       // arg0 (8)
        const unsigned char* a1 = p + off + 8;   // arg1 (4)
        const unsigned char* a2 = p + off + 12;  // arg2 (4)
        unsigned char disc = a0[0];

        stats.traces++;

        if (disc == XROOTD_MON_WINDOW) {tWin = ri32(a2); continue;}

        if (!traces) continue;   // only counting unless trace emission is on

        json j;
        otelResource(j, src, stod, srv);
        json& a = j["attributes"];
        const char* ev = nullptr;   // event.name, set once the record type known

        auto lfnOf = [&](uint32_t id)
            {auto it = srv.paths.find(id);
             if (it != srv.paths.end())
                {Touch(it->second.lru); setFile(a, it->second.val);}
             a["xrootd.file.id"] = id;
            };

        if ((disc & 0x80) == 0)            // read/write I/O entry
           {int64_t  offset = ri64(a0);
            int32_t  length = ri32(a1);
            ev = length < 0 ? "xrootd.write" : "xrootd.read";
            a["xrootd.io.offset"] = offset;
            a["xrootd.io.length"] = length < 0 ? -(int64_t)length
                                               :  (int64_t)length;
            lfnOf(rd32(a2));
           }
        else switch(disc)
           {case XROOTD_MON_OPEN:
                 {unsigned char b[8]; std::memcpy(b, a0, 8); b[0] = 0;
                  ev = "xrootd.open"; a["file.size"] = (int64_t)rd64(b);
                  lfnOf(rd32(a2));
                 }
                 break;
            case XROOTD_MON_CLOSE:
                 {uint64_t rB = (uint64_t)rd32(a0 + 4) << a0[1];
                  uint64_t wB = (uint64_t)rd32(a1)     << a0[2];
                  ev = "xrootd.close";
                  a["xrootd.transfer.read_bytes"]  = rB;
                  a["xrootd.transfer.write_bytes"] = wB; lfnOf(rd32(a2));
                 }
                 break;
            case XROOTD_MON_DISC:
                 {ev = "xrootd.disconnect";
                  a["xrootd.session.duration"] = ri32(a1);
                  otelIdentity(a, srv, rd32(a2));
                 }
                 break;
            case XROOTD_MON_READV:
            case XROOTD_MON_READU:
                 ev = "xrootd.readv"; lfnOf(rd32(a2));
                 break;
            case XROOTD_MON_APPID:
                 {char b[13]; std::memcpy(b, a0 + 4, 12); b[12] = 0;
                  ev = "xrootd.appid"; a["xrootd.app.info"] = b;
                 }
                 break;
            default: continue;   // REDHOST and anything else: skip
           }

        otelBegin(j, ev, tWin, false);
        if (doc) doc(j.dump());
       }
}

/******************************************************************************/
/*                        D e c o d e G S t r e a m                           */
/******************************************************************************/

namespace
{
const char* gsProvider(unsigned char t)
{
   switch(t)
         {case XROOTD_MON_GSCCM: return "ccm";
          case XROOTD_MON_GSPFC: return "pfc";
          case XROOTD_MON_GSTCP: return "tcp";
          case XROOTD_MON_GSTPC: return "tpc";
          case XROOTD_MON_GSTHR: return "throttle";
          case XROOTD_MON_GSOSS: return "oss";
          case XROOTD_MON_GSHTP: return "http";
          default:               return "unknown";
         }
}

// Read an unsigned integer field from a g-stream JSON payload (0 if absent).
//
uint64_t jU(const json& j, const char* key)
{
   auto it = j.find(key);
   if (it == j.end() || !it->is_number()) return 0;
   if (it->is_number_unsigned()) return it->get<uint64_t>();
   long long v = it->get<long long>();
   return v < 0 ? 0 : (uint64_t)v;
}

// Aggregate one parsed g-stream record into bounded-cardinality Prometheus
// series. `prev` retains the last cumulative value for providers (oss) that
// report running totals, so they become counter deltas.
//
void gsAggregate(XrdMetrics::Subsystem* M,
                 std::unordered_map<std::string, uint64_t>& prev,
                 unsigned char provByte, const std::string& src, const json& j)
{
   std::vector<XrdMetrics::ConstLabel> srv = {{"server", src}};

// Turn a running total into a counter increment (skip the first observation,
// which only establishes the baseline; treat a decrease as a counter reset).
//
   auto delta = [&](const char* name, const char* help, std::vector<XrdMetrics::ConstLabel> lbl,
                    const std::string& key, uint64_t cur)
      {auto it = prev.find(key);
       bool first = (it == prev.end());
       uint64_t pv = first ? 0 : it->second;
       prev[key] = cur;
       if (first) return;
       uint64_t d = cur >= pv ? cur - pv : cur;
       if (d) M->counterSeries(name, help, lbl) += d;
      };

   switch(provByte)
         {case XROOTD_MON_GSOSS:   // cumulative op counters
               {static const std::pair<const char*,const char*> ops[] =
                   {{"read","reads"},{"write","writes"},{"stat","stats"},
                    {"pgread","pgreads"},{"pgwrite","pgwrites"},{"readv","readvs"},
                    {"dirlist","dirlists"},{"truncate","truncates"},
                    {"unlink","unlinks"},{"chmod","chmods"},{"open","opens"},
                    {"rename","renames"}};
                for (auto& op : ops)
                    {std::vector<XrdMetrics::ConstLabel> l = {{"server", src}, {"op", op.first}};
                     std::string base = src + "|oss|" + op.first;
                     delta("oss_ops_total",
                           "OSS plugin operations", l, base, jU(j, op.second));
                     std::string slowKey = std::string("slow_") + op.second;
                     delta("oss_slow_ops_total",
                           "OSS plugin slow operations", l, base + "|slow",
                           jU(j, slowKey.c_str()));
                    }
               }
               break;

          case XROOTD_MON_GSPFC:   // per file_close event
               {auto ev = j.find("event");
                if (ev == j.end() || *ev != "file_close") break;
                M->counterSeries("pfc_files_total",
                           "proxy-cache file closes", srv) += 1;
                auto pfcBytes = [&](const char* source, const char* field)
                   {uint64_t v = jU(j, field);
                    if (v) M->counterSeries("pfc_bytes_total",
                                "proxy-cache bytes by source",
                                {{"server", src}, {"source", source}}) += v;
                   };
                pfcBytes("hit",      "b_hit");
                pfcBytes("miss",     "b_miss");
                pfcBytes("bypass",   "b_bypass");
                pfcBytes("disk",     "b_todisk");
                pfcBytes("prefetch", "b_prefetch");
               }
               break;

          case XROOTD_MON_GSTPC:   // per completed third-party copy
               {std::string type = "unknown";
                int rc = 0;
                auto xq = j.find("Xeq");
                if (xq != j.end() && xq->is_object())
                   {auto t = xq->find("Type");
                    if (t != xq->end() && t->is_string()) type = t->get<std::string>();
                    auto rcit = xq->find("RC");
                    if (rcit != xq->end() && rcit->is_number())
                       rc = rcit->get<int>();
                   }
                uint64_t size = jU(j, "Size");
                M->counterSeries("tpc_total", "third-party copies",
                           {{"server", src}, {"type", type},
                            {"result", rc == 0 ? "ok" : "error"}}) += 1;
                if (size)
                   M->counterSeries("tpc_bytes_total",
                              "third-party copy bytes",
                              {{"server", src}, {"type", type}}) += size;
                M->histogramSeries("tpc_size_bytes",
                             "third-party copy size",
                             {1e6,1e7,1e8,1e9,1e10,1e11}).observe((double)size);
               }
               break;

          case XROOTD_MON_GSTHR:   // throttle plugin
               {auto ev = j.find("event");
                if (ev == j.end() || *ev != "throttle_update") break;
                delta("throttle_io_total",
                      "throttle plugin I/O operations", srv,
                      src + "|thr|io_total", jU(j, "io_total"));
                auto ia = j.find("io_active");
                if (ia != j.end() && ia->is_number())
                   M->gaugeSeries("throttle_io_active",
                            "throttle plugin in-flight I/O", srv)
                    = ia->get<double>();
               }
               break;

          case XROOTD_MON_GSHTP:   // HTTP request activity (cumulative per op/status)
               {for (auto it = j.begin(); it != j.end(); ++it)
                    {const std::string& key = it.key();
                     if (key.compare(0, 5, "HTTP_") != 0 || !it->is_object())
                        continue;
                     auto us = key.find('_', 5);
                     std::string method = key.substr(5, us == std::string::npos
                                                        ? std::string::npos : us - 5);
                     std::string status = us == std::string::npos ? ""
                                                                  : key.substr(us + 1);
                     std::vector<XrdMetrics::ConstLabel> l = {{"server", src}, {"method", method},
                                           {"status", status}};
                     delta("http_requests_total",
                           "HTTP requests by method and status", l,
                           src + "|http|" + key, jU(*it, "count"));
                    }
               }
               break;

          default: break;         // ccm/tcp: forwarded, not yet aggregated
         }
}
}

void XrdMonDecode::DecodeGStream(const std::string& src, int32_t stod,
                                 Server& srv, const unsigned char* p, int plen)
{
// XrdXrootdMonGS: header(8) + tBeg(4) + tEnd(4) + sID(8); the provider type is
// the top byte of sID. The remainder is newline-separated plugin records
// (JSON or CGI), produced by the oss/pfc/throttle/tpc/http g-streams.
//
   if (plen < 24) {Malformed(src, XROOTD_MON_MAPGSTA, "short_packet"); return;}
   int32_t  tBeg = ri32(p + 8);
   int32_t  tEnd = ri32(p + 12);
   uint64_t sID  = rd64(p + 16);
   unsigned char provByte = (unsigned char)(sID >> 56);
   const char* provider = gsProvider(provByte);

   const char* body = (const char*)(p + 24);
   int blen = plen - 24;

   int start = 0;
   for (int i = 0; i <= blen; i++)
       {if (i == blen || body[i] == '\n')
           {int n = i - start;
            if (n > 0)
               {stats.gevents++;
                if (gstream || metrics)
                   {std::string line(body + start, n);
                    json payload = json::parse(line, nullptr, false);

                    // (a) aggregate known providers into bounded metrics.
                    if (metrics && !payload.is_discarded())
                       gsAggregate(metrics, gsPrev, provByte, src, payload);

                    // (b) forward the record (structured payload) as a document.
                    if (gstream && doc)
                       {json j;
                        otelResource(j, src, stod, srv);
                        otelBegin(j, "xrootd.gstream", tEnd ? tEnd : tBeg, false);
                        json& a = j["attributes"];
                        a["xrootd.gstream.provider"] = provider;
                        if (payload.is_discarded())
                             a["xrootd.gstream.data"] = line;
                        else a["xrootd.gstream.data"] = payload;
                        doc(j.dump());
                       }
                   }
               }
            start = i + 1;
           }
       }
}

/******************************************************************************/
/*                        D e c o d e R S t r e a m                           */
/******************************************************************************/

namespace
{
// Operation that triggered a redirect (low nibble of the record Type byte).
//
const char* redirOp(unsigned char op)
{
   switch(op)
         {case XROOTD_MON_CHMOD:   return "chmod";
          case XROOTD_MON_LOCATE:  return "locate";
          case XROOTD_MON_OPENDIR: return "opendir";
          case XROOTD_MON_OPENC:   return "open";
          case XROOTD_MON_OPENR:   return "open-read";
          case XROOTD_MON_OPENW:   return "open-write";
          case XROOTD_MON_MKDIR:   return "mkdir";
          case XROOTD_MON_MV:      return "mv";
          case XROOTD_MON_PREP:    return "prepare";
          case XROOTD_MON_QUERY:   return "query";
          case XROOTD_MON_RM:      return "rm";
          case XROOTD_MON_RMDIR:   return "rmdir";
          case XROOTD_MON_STAT:    return "stat";
          case XROOTD_MON_TRUNC:   return "truncate";
          default:                 return "unknown";
         }
}
}

void XrdMonDecode::DecodeRStream(const std::string& src, int32_t stod,
                                 Server& srv, const unsigned char* p, int plen)
{
// XrdXrootdMonBurr: header(8) + sID block(8, first byte = REDSID) + an array of
// 8-byte XrdXrootdMonRedir records. A redirect record is followed by a variable
// "<host>:<path>" string occupying its Dent (slot) count of further records.
//
   const int RSZ = 8;
   if (plen < 16) {Malformed(src, XROOTD_MON_MAPREDR, "short_packet"); return;}

   int32_t tWin = 0;
   int off = 16;                              // past header + sID block
   while(off + RSZ <= plen)
        {const unsigned char* rec = p + off;
         unsigned char type = rec[0];
         stats.records++;

         if (type == XROOTD_MON_REDTIME)       // timing mark: arg1 = time
            {tWin = ri32(rec + 4); off += RSZ; continue;}

         if (!(type & 0x80))                   // not a redirect entry; skip one
            {off += RSZ; continue;}

         unsigned slots = rec[1];              // Dent: following string slots
         int      port  = rd16(rec + 2);
         uint32_t did   = rd32(rec + 4);       // user/session dictid

         // The "<host>:<path>" string spans the next `slots` records. A slot
         // count that runs past the packet means the record was truncated:
         // salvage what is there, but flag the packet.
         const char* sp = (const char*)(p + off + RSZ);
         int savail = plen - (off + RSZ);
         int slen = (int)slots * RSZ;
         if (slen > savail)
            {Malformed(src, XROOTD_MON_MAPREDR, "truncated_string");
             slen = savail;
            }
         std::string hp(sp, slen > 0 ? strnlen(sp, slen) : 0);

         stats.redirs++;
         const char* kind = (type & 0xf0) == XROOTD_MON_REDLOCAL
                          ? "local" : "remote";
         if (metrics)
            metrics->counterSeries("redirects_total",
                          "client redirects issued by the server",
                          {{"server", src}, {"kind", kind}}) += 1;
         if (redirects)
            {// A redirect concludes the operation from this (redirector) node's
             // point of view, so it is reported in the same concluded-operation
             // schema as closes and errors, with xrootd.operation_state
             // "Redirected" and the destination under xrootd.redirect.*.
             json j;
             otelResource(j, src, stod, srv);
             otelBegin(j, "xrootd.transfer", tWin, false);
             json& a = j["attributes"];

             a["xrootd.operation"]       = redirOp(type & 0x0f);
             a["xrootd.operation_state"] = "Redirected";
             a["xrootd.redirect.kind"]        = kind;
             a["xrootd.redirect.target.port"] = port;
             // hp is "<host>:<path>": the host is the redirect target, the path
             // the lfn the client is being redirected for.
             auto colon = hp.find(':');
             if (colon != std::string::npos)
                {if (colon > 0)
                    a["xrootd.redirect.target.address"] = hp.substr(0, colon);
                 setFile(a, hp.substr(colon + 1));
                }
                else if (!hp.empty()) a["xrootd.redirect.target.address"] = hp;

             otelIdentity(a, srv, did);

             std::string sess = sessKey(src, stod, did);
             j["traceId"] = traceIdOf(sess);
             j["spanId"]  = spanIdOf(sess + "|redir|" + std::to_string(tWin));

             stats.docs++;
             if (doc) doc(j.dump());

             // Companion span for the redirect, child of the session span.
             emitSpan(j, "redirect", tWin, tWin,
                      spanIdOf(sess + "|session"));
            }

         off += RSZ * (1 + slots);
        }
}
