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
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdNet/XrdNetUtils.hh"
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
   return ip == "::1" || ip == "127.0.0.1" || ip == "::ffff:127.0.0.1";
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
}

/******************************************************************************/
/*                            L o c a l H o s t                               */
/******************************************************************************/

// The local FQDN, used as the hostname of a server reporting from the loopback
// address (where there is no useful name before the '=' ident arrives, so
// server.hostname otherwise showed the literal "::1"). Resolved at most once
// for the whole process: MyHostName() is the same regardless of which server
// reports, so it is cached and reused for every loopback incarnation.
//
// Only the loopback case is handled this way: a remote server self-identifies
// its host on the '=' (MAPIDNT) stream, which fillServer() already prefers. A
// blocking reverse-DNS lookup of an arbitrary remote IP is deliberately avoided
// — it would stall the single-threaded UDP receive loop (a non-resolving
// address blocks for the full resolver timeout) and drop packets.
//
const std::string& XrdMonDecode::LocalHost()
{
   if (!localHostDone)
      {localHostDone = true;
       const char* me = XrdNetUtils::MyHostName();
       std::string h = me ? me : "";
       if (!h.empty() && !isIPLiteral(h)) localHost = h;
      }
   return localHost;
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
// Server), so the per-document fillServer() stays lookup-free. Only a loopback
// sender is named here, from the process-wide local FQDN (itself resolved at
// most once); a '=' ident host still takes precedence in fillServer().
//
   if (resolveHosts && !srv.resolved)
      {std::string ip = src.substr(0, src.rfind(':'));
       if (isLoopback(ip)) srv.resolvedHost = LocalHost();
       srv.resolved = true;
      }
   return srv;
}

/******************************************************************************/
/*                           f i l l S e r v e r                              */
/******************************************************************************/

void XrdMonDecode::fillServer(json& j, const std::string& src, int32_t stod,
                              const Server& srv)
{
   json& server = j["server"];
   std::string ip = src.substr(0, src.rfind(':'));   // UDP source (strip :port)
   server["ip"]    = ip;
   server["start"] = stod;                            // incarnation key
   if (srv.sID)                 server["id"]       = srv.sID;
   if (!srv.ident.site.empty()) server["site"]     = srv.ident.site;
   if (!srv.ident.inst.empty()) server["instance"] = srv.ident.inst;

   // Hostname precedence: the '=' ident's advertised host (when it is a real
   // name, not an IP literal), else the reverse-resolved sender, else none.
   // server.name falls back to the numeric IP when no name is available.
   //
   std::string host;
   if (!srv.ident.host.empty() && !isIPLiteral(srv.ident.host))
      host = srv.ident.host;
   else if (!srv.resolvedHost.empty())
      host = srv.resolvedHost;

   if (!host.empty()) server["hostname"] = host;
   server["name"] = host.empty() ? ip : host;
}

/******************************************************************************/
/*                           f i l l C l i e n t                              */
/******************************************************************************/

std::string XrdMonDecode::fillClient(json& j, const Server& srv, uint32_t userID)
{
   std::string vo;

   auto uit = srv.users.find(userID);
   if (uit != srv.users.end())
      {const UserInfo& u = uit->second;
       Touch(u.lru);   // a referenced session is active: keep it warm
       json& user = j["user"];
       if (!u.user.empty()) user["name"]     = u.user;
       if (!u.prot.empty()) user["protocol"] = u.prot;
       if (!u.raw.empty())  user["raw"]      = u.raw;
       if (!u.authMethod.empty()) user["auth_method"] = u.authMethod;
       // Client endpoint: classify the descriptor host as IP vs hostname.
       if (!u.host.empty())
          {json& client = j["client"];
           client["host"] = u.host;
           if (isIPLiteral(u.host)) client["ip"]       = u.host;
           else                     client["hostname"] = u.host;
           if (!u.clientVer.empty()) client["version"]    = u.clientVer;
           if (u.ipVersion)          client["ip_version"] = u.ipVersion;
           if (!u.site.empty())      client["site"]       = u.site;
          }
       // Application info: structured (&x=/&y=) plus the raw 'i' blob.
       if (!u.appName.empty() || !u.appInfo.empty())
          {json& app = j["app"];
           if (!u.appName.empty()) app["name"] = u.appName;
           if (!u.appInfo.empty()) app["info"] = u.appInfo;
          }
       auto iit = srv.infos.find(u.raw);
       if (iit != srv.infos.end())
          {Touch(iit->second.lru); j["app"]["raw"] = iit->second.val;}
       // VO/role/groups: prefer the token ('T'); fall back to the auth CGI.
       if (!u.vo.empty())     {vo = u.vo;        user["vo"]     = u.vo;}
       if (!u.role.empty())   user["role"]   = u.role;
       if (!u.groups.empty()) user["groups"] = u.groups;
      }

   // Token identity ('T' stream) and experiment/activity ('U' stream) are
   // keyed by the same user dictid as the 'u' map.
   auto tit = srv.tokens.find(userID);
   if (tit != srv.tokens.end())
      {const TokenInfo& t = tit->second;
       Touch(t.lru);
       json& user = j["user"];
       if (!t.subject.empty()) user["subject"] = t.subject;
       if (!t.vo.empty())     {vo = t.vo;       user["vo"]     = t.vo;}
       if (!t.role.empty())    user["role"]   = t.role;
       if (!t.groups.empty())  user["groups"] = t.groups;
      }
   auto ait = srv.activity.find(userID);
   if (ait != srv.activity.end())
      {Touch(ait->second.lru);
       int expId = ait->second.experiment;
       int actId = ait->second.activity;
       json& act = j["activity"];
       if (expId) act["experiment_id"] = expId;
       if (actId) act["activity_id"]   = actId;

       // Map the numeric SciTags ids to human names via the loaded registry.
       // The experiment name doubles as a VO, used only as a last resort (the
       // token and auth CGI both take precedence). The lock covers a background
       // refresh thread swapping the registry; lookups copy out the names.
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
       if (!expName.empty())
          {act["experiment"] = expName;
           if (vo.empty()) {vo = expName; j["user"]["vo"] = expName;}
          }
       if (!actName.empty()) act["activity"] = actName;
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
/*                          f i l l S e s s i o n                             */
/******************************************************************************/

void XrdMonDecode::fillSession(json& j, const UserInfo& u)
{
   json& s = j["session"];
   s["files"]       = u.sFiles;
   s["transfers"]   = u.sTransfers;
   s["accesses"]    = u.sAccesses;
   if (u.sErrors)     s["errors"]      = u.sErrors;
   if (u.sReadBytes)  s["read_bytes"]  = u.sReadBytes;
   if (u.sWriteBytes) s["write_bytes"] = u.sWriteBytes;
   if (u.sFirst > 0)  s["start_time"]  = isoTime(u.sFirst);
   if (u.sLast  > 0)  s["end_time"]    = isoTime(u.sLast);
   if (u.sFirst > 0 && u.sLast >= u.sFirst) s["duration_s"] = u.sLast - u.sFirst;

   if (!u.sRecent.empty())
      {json files = json::array();
       for (const auto& f : u.sRecent)
          {json fj;
           fj["lfn"]   = f.lfn;
           fj["type"]  = f.whole ? "transfer" : "access";
           fj["operation"] = f.write ? "write" : "read";
           fj["bytes"] = f.bytes;
           files.push_back(std::move(fj));
          }
       s["recent_files"] = std::move(files);
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
/*                             P r o c e s s                                  */
/******************************************************************************/

bool XrdMonDecode::Process(const std::string& src, const char* buff, int blen)
{
   const unsigned char* p = (const unsigned char*)buff;

   stats.packets++;

// Every packet starts with an 8-byte XrdXrootdMonHeader.
//
   if (blen < 8) {stats.malformed++; return false;}

   unsigned char code = p[0];
   int           plen = rd16(p + 2);
   int32_t       stod = ri32(p + 4);

   if (plen < 8 || plen > blen) {stats.malformed++; return false;}

   Server& srv = ServerFor(src, stod);

// Packet-loss estimate. The server stamps every datagram to one destination
// with a single sequence counter (header pseq, wrapping at 256), regardless of
// stream. A forward gap means lost packets; a small backward step is reordering
// (UDP) and is ignored.
//
   unsigned char pseq = p[1];
   if (srv.lastPseq >= 0)
      {int gap = ((int)pseq - ((srv.lastPseq + 1) & 0xff)) & 0xff;
       if (gap > 0 && gap < 128)
          {stats.lost += gap;
           if (metrics)
              metrics->counterSeries("xrootd_collector_packets_lost_total",
                   "estimated lost packets (pseq gaps)",
                   {{"server", src}}) += gap;
          }
      }
   srv.lastPseq = pseq;

   switch(code)
         {case XROOTD_MON_MAPUSER:
          case XROOTD_MON_MAPPATH:
          case XROOTD_MON_MAPINFO:
          case XROOTD_MON_MAPTOKN:
          case XROOTD_MON_MAPUEAC:
               // XrdXrootdMonMap: header(8) + dictid(4) + info[]
               if (plen < 12) {stats.malformed++; return false;}
               {uint32_t dictid = rd32(p + 8);
                DecodeMap(code, srv, dictid, (const char*)(p + 12), plen - 12);
               }
               break;

          case XROOTD_MON_MAPIDNT:
               // Server self-identification: header(8) + dictid(4, =0) + info[]
               if (plen < 12) {stats.malformed++; return false;}
               DecodeIdent(src, stod, srv, (const char*)(p + 12), plen - 12);
               break;

          case XROOTD_MON_MAPXFER:
          case XROOTD_MON_MAPPURG:
               // FRM stage/migrate ('x') and purge ('p'): map record with
               // dictid 0 and info "<who>\n<path>[\n&cgi]".
               if (plen < 12) {stats.malformed++; return false;}
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
        + u.host.size() + u.authMethod.size() + u.vo.size() + u.role.size()
        + u.groups.size() + u.clientVer.size() + u.appName.size()
        + u.appInfo.size() + u.site.size() + recent;
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
       // (&R= &x= &y= &I=); auth info (&p= &o= &r= &g=) only with "... auth".
       // cgiVal scans the whole string; the descriptor line carries no "&key=".
       u.authMethod = cgiVal(text, "p");
       u.vo         = cgiVal(text, "o");
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

   if (dumpRaw && raw)
      {json j = {{"code", "="}, {"info", first}};
       raw(j.dump());
      }

// Emit a server-identity document, but only when the content changes (the
// server re-sends this every monitor "ident" interval, default hourly).
//
   if (text == srv.identRaw) return;
   srv.identRaw = text;

   json j;
   j["type"] = "server_ident";
   fillServer(j, src, stod, srv);   // hostname/site/instance/name from id
   json& server = j["server"];
   if (!id.pgm.empty()) server["program"] = id.pgm;
   if (!id.ver.empty()) server["version"] = id.ver;
   std::string port = cgiVal(text, "port");
   if (!port.empty())   server["port"]    = atoi(port.c_str());
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

   json j;
   j["type"]      = "frm";
   j["operation"] = op;
   fillServer(j, src, stod, srv);
   if (!path.empty()) j["file"]["lfn"] = path;

// who is "<prot>/<user>.<pid>:<sid>@<host>". The FRM agent is the actor here,
// so it has no user dictionary entry; parse the descriptor inline.
//
   auto slash = who.find('/');
   auto at    = who.rfind('@');
   if (slash != std::string::npos)
      {j["user"]["protocol"] = who.substr(0, slash);
       std::string r = who.substr(slash + 1);
       j["user"]["name"] = r.substr(0, r.find('.'));
      }
   if (at != std::string::npos) j["client"]["host"] = who.substr(at + 1);

   int64_t sz = -1;
   if (!cgi.empty())
      {std::string s = cgiVal(cgi, "sz");
       if (!s.empty()) {sz = atoll(s.c_str()); j["file"]["size"] = sz;}
       std::string tod = cgiVal(cgi, "tod");
       if (!tod.empty()) j["@timestamp"] = isoTime((int32_t)atoll(tod.c_str()));
      }

   if (metrics)
      {metrics->counterSeries("xrootd_collector_frm_total", "FRM stage/purge events",
                        {{"server", src}, {"op", op}}) += 1;
       if (code == XROOTD_MON_MAPPURG && sz > 0)
          metrics->counterSeries("xrootd_collector_frm_purge_bytes_total",
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

         if (recSize < 8 || off + recSize > len) {stats.malformed++; break;}
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
      metrics->gaugeSeries("xrootd_collector_active_transfers",
                     "files currently open (transfers in progress)",
                     {{"server", src}}) = (double)srv.files.size();
}

/******************************************************************************/
/*                             E m i t D i s c                                */
/******************************************************************************/

void XrdMonDecode::EmitDisc(const std::string& src, int32_t stod, Server& srv,
                            uint32_t userID, int32_t tWin)
{
   json j;
   j["type"] = "session";
   if (tWin > 0) j["@timestamp"] = isoTime(tWin);
   fillServer(j, src, stod, srv);
   fillClient(j, srv, userID);

// Attach the session's aggregated file activity (counters plus a capped recent-
// file list) accumulated from every close that named this user (see foldSession).
//
   auto uit = srv.users.find(userID);
   if (uit != srv.users.end()) fillSession(j, uit->second);

   if (metrics)
      metrics->counterSeries("xrootd_collector_sessions_total",
                       "client sessions ended", {{"server", src}}) += 1;

   if (doc) doc(j.dump());
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
   if (recSize < 8 + 24) {stats.malformed++; return;}
   int64_t rdBytes = ri64(rec + 8);
   int64_t rvBytes = ri64(rec + 16);
   int64_t wrBytes = ri64(rec + 24);

   int durSecs = -1;
   std::string vo;
   bool     haveOpen = false;  // matched the open record (so fsz is known)
   int64_t  openFsz  = 0;      // file size captured at open
   uint32_t openUser = 0;      // user dictid from the open (for session rollup)
   std::string openLfn;        // lfn from the open (for the session rollup)

// One per-transfer document in an OpenSearch-friendly nested schema (each nested
// object indexes as a dotted field, e.g. server.name). Empty/zero fields are
// omitted. See README.md for the field-to-WLCG mapping.
//
   json j;
   j["type"]       = "transfer";
   j["@timestamp"] = isoTime(tWin);
   fillServer(j, src, stod, srv);

   json& transfer = j["transfer"];
   transfer["end_time"]     = isoTime(tWin);
   transfer["forced_close"] = (recFlag & XrdXrootdMonFileHdr::forced) != 0;
   transfer["read_bytes"]   = rdBytes;
   transfer["readv_bytes"]  = rvBytes;
   transfer["write_bytes"]  = wrBytes;
   // Explicit read/write categorisation (WLCG operation_type): any write bytes
   // make it a write, otherwise it is a read.
   transfer["operation"]    = (wrBytes > 0) ? "write" : "read";

// Join the matching open record (held since the open packet) to recover the
// path, the user, and the open time. Resolve the user dictid if we have it.
//
   auto fit = srv.files.find(fileID);
   if (fit != srv.files.end())
      {const OpenFile& of = fit->second;
       transfer["open_seen"]  = true;
       haveOpen = true;
       openFsz  = of.fsz;
       openUser = of.user;
       openLfn  = of.lfn;
       json& file = j["file"];
       file["lfn"]        = of.lfn;
       file["size"]       = of.fsz;
       file["read_write"] = of.rw;
       transfer["start_time"] = isoTime(of.tOpen);
       if (of.tOpen > 0 && tWin > 0) {durSecs = tWin - of.tOpen;
                                      transfer["duration_s"] = durSecs;}

       vo = fillClient(j, srv, of.user);

       // LAN/WAN heuristic: tag the transfer local when the client and the
       // reporting server share a registered domain. Only decidable when both
       // are resolvable host names (not IP literals); otherwise left unset.
       if (!srv.ident.host.empty() && j.contains("client"))
          {auto cit = j["client"].find("host");
           if (cit != j["client"].end())
              {std::string cd = hostDomain(cit->get<std::string>());
               std::string sd = hostDomain(srv.ident.host);
               if (!cd.empty() && !sd.empty()) transfer["is_local"] = (cd == sd);
              }
          }
       LruDrop(fit->second.lru);   // unlink before erasing the map entry
       srv.files.erase(fit);
      }
      else {transfer["open_seen"] = false; stats.orphanCls++;}

// Whole-file transfer vs. partial access. The document schema is identical; only
// the type string differs (see wholeFileClose). A write needs to have ended
// cleanly to count as a whole-file producer, so this must be decided after the
// forced/error flags are known.
//
   const bool forced = (recFlag & XrdXrootdMonFileHdr::forced) != 0;
   const bool hasErr = (recFlag & XrdXrootdMonFileHdr::hasERR) != 0;
   const bool whole  = wholeFileClose(haveOpen, openFsz, rdBytes, rvBytes,
                                      wrBytes, forced, hasErr);
   j["type"] = whole ? "transfer" : "access";

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
       transfer["read_ops"]   = ri32(o + 0);
       transfer["readv_ops"]  = ri32(o + 4);
       transfer["write_ops"]  = ri32(o + 8);
       transfer["readv_segs"] = ri64(o + 16);

       // Request-size extremes use 0x7fffffff as the "unset" sentinel; omit
       // them rather than emit a misleading minimum.
       //
       auto minmax = [&](const char* kmn, const char* kmx, int32_t mn, int32_t mx)
                       {if (mn != 0x7fffffff) {transfer[kmn] = mn; transfer[kmx] = mx;}};
       minmax("read_min",  "read_max",  ri32(o + 24), ri32(o + 28));
       minmax("readv_min", "readv_max", ri32(o + 32), ri32(o + 36));
       minmax("write_min", "write_max", ri32(o + 40), ri32(o + 44));

       // Optional sum-of-squares (XrdXrootdMonStatSSQ) when "ssq" configured.
       //
       if ((recFlag & XrdXrootdMonFileHdr::hasSSQ) && recSize >= 8 + 24 + 48 + 32)
          {const unsigned char* s = o + 48;
           transfer["read_sumsq"]  = rdbl(s + 0);
           transfer["readv_sumsq"] = rdbl(s + 8);
           transfer["rsegs_sumsq"] = rdbl(s + 16);
           transfer["write_sumsq"] = rdbl(s + 24);
          }
      }

// Terminal status (WLCG operation_state). A close carrying a trailing
// XrdXrootdMonStatERR (hasERR) reports an aborted/failed transfer; the error
// block trails any XrdXrootdMonStatOPS/SSQ blocks. Otherwise the close is the
// authoritative success report. Note: "forced" (disconnect-driven) is not a
// failure on its own.
//
   int errOff = 8 + 24;
   if (recFlag & XrdXrootdMonFileHdr::hasOPS)
      {errOff += 48;
       if (recFlag & XrdXrootdMonFileHdr::hasSSQ) errOff += 32;
      }
   if ((recFlag & XrdXrootdMonFileHdr::hasERR) && recSize >= errOff + 8)
      {fillError(transfer, rec + errOff, recSize - errOff);
       stats.failed++;
       if (metrics)
          metrics->counterSeries("xrootd_collector_failed_operations_total",
                       "operations that concluded unsuccessfully",
                       {{"server", src},
                        {"category", transfer.value("error_category",
                                                    std::string("unknown"))}})
                  += 1;
      }
   else transfer["operation_state"] = "Successful";

// Aggregate into bounded-cardinality Prometheus series (label only by the
// reporting server). Per-transfer detail stays in the document sink; here we
// keep just totals and distributions suitable for time-series storage.
//
   if (metrics)
      {std::vector<XrdMetrics::ConstLabel> sl = {{"server", src}};
       if (whole)
          metrics->counterSeries("xrootd_collector_transfers_total",
                           "completed whole-file transfers seen", sl) += 1;
          else
          metrics->counterSeries("xrootd_collector_accesses_total",
                           "completed partial-access closes seen", sl) += 1;
       metrics->counterSeries("xrootd_collector_read_bytes_total",
                        "bytes read (read+readv)", sl) += rdBytes + rvBytes;
       metrics->counterSeries("xrootd_collector_write_bytes_total",
                        "bytes written", sl) += wrBytes;
       if (!vo.empty())
          metrics->counterSeries("xrootd_collector_vo_transfers_total",
                        "completed transfers per VO",
                        {{"server", src}, {"vo", vo}}) += 1;
       if (transfer.contains("is_local"))
          metrics->counterSeries("xrootd_collector_locality_transfers_total",
                        "completed transfers by client/server locality",
                        {{"server", src}, {"locality",
                         transfer["is_local"].get<bool>() ? "local" : "remote"}})
                  += 1;
       metrics->histogramSeries("xrootd_collector_transfer_size_bytes",
                        "bytes moved per transfer",
                        {1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11})
               .observe((double)(rdBytes + rvBytes + wrBytes));
       if (durSecs >= 0)
          metrics->histogramSeries("xrootd_collector_transfer_duration_seconds",
                        "transfer wall-clock duration",
                        {1,5,15,60,300,1800,7200}).observe(durSecs);
      }

   stats.docs++;
   if (doc) doc(j.dump());
}

/******************************************************************************/
/*                             f i l l E r r o r                              */
/******************************************************************************/

void XrdMonDecode::fillError(json& transfer, const unsigned char* err,
                             int errLen)
{
// XrdXrootdMonStatERR: ecode(4) + ecat(1) + rsvd(3) + null-terminated message.
//
   if (errLen < 8) return;
   transfer["operation_state"] = "Failed";
   transfer["error_code"]      = ri32(err);
   transfer["error_category"]  = errCatName((unsigned char)err[4]);
   const char* m = (const char*)(err + 8);
   std::string msg(m, strnlen(m, errLen - 8));
   if (!msg.empty()) transfer["error_message"] = msg;
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
   if (recSize < 8 + 4) {stats.malformed++; return;}

   json j;
   j["type"]       = "transfer";
   j["@timestamp"] = isoTime(tWin);
   fillServer(j, src, stod, srv);

   json& transfer = j["transfer"];
   transfer["end_time"] = isoTime(tWin);

   std::string vo;
   int off = 8;
   if ((recFlag & XrdXrootdMonFileHdr::hasLFN) && recSize > 12)
      {uint32_t user = rd32(rec + 8);
       const char* l = (const char*)(rec + 12);
       std::string lfn(l, strnlen(l, recSize - 12));
       j["file"]["lfn"] = lfn;
       off = 12 + (int)lfn.size() + 1;       // past the lfn's terminating null
       vo = fillClient(j, srv, user);
      }

   fillError(transfer, rec + off, recSize - off);

   if (metrics)
      metrics->counterSeries("xrootd_collector_failed_operations_total",
                   "operations that concluded unsuccessfully",
                   {{"server", src},
                    {"category", transfer.value("error_category",
                                                std::string("unknown"))}}) += 1;

   stats.failed++;
   stats.docs++;
   if (doc) doc(j.dump());
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
//
   for (int off = 0; off + 16 <= len; off += 16)
       {const unsigned char* a0 = p + off;       // arg0 (8)
        const unsigned char* a1 = p + off + 8;   // arg1 (4)
        const unsigned char* a2 = p + off + 12;  // arg2 (4)
        unsigned char disc = a0[0];

        stats.traces++;

        if (disc == XROOTD_MON_WINDOW) {tWin = ri32(a2); continue;}

        if (!traces) continue;   // only counting unless trace emission is on

        json j;
        if (tWin > 0) j["@timestamp"] = isoTime(tWin);
        fillServer(j, src, stod, srv);

        auto lfnOf = [&](uint32_t id)
            {auto it = srv.paths.find(id);
             if (it != srv.paths.end())
                {Touch(it->second.lru); j["file"]["lfn"] = it->second.val;}
             j["file"]["id"] = id;
            };

        if ((disc & 0x80) == 0)            // read/write I/O entry
           {int64_t  offset = ri64(a0);
            int32_t  length = ri32(a1);
            j["type"]   = length < 0 ? "write" : "read";
            j["offset"] = offset;
            j["length"] = length < 0 ? -(int64_t)length : (int64_t)length;
            lfnOf(rd32(a2));
           }
        else switch(disc)
           {case XROOTD_MON_OPEN:
                 {unsigned char b[8]; std::memcpy(b, a0, 8); b[0] = 0;
                  j["type"] = "open"; j["file_size"] = (int64_t)rd64(b);
                  lfnOf(rd32(a2));
                 }
                 break;
            case XROOTD_MON_CLOSE:
                 {uint64_t rB = (uint64_t)rd32(a0 + 4) << a0[1];
                  uint64_t wB = (uint64_t)rd32(a1)     << a0[2];
                  j["type"] = "close"; j["read_bytes"] = rB;
                  j["write_bytes"] = wB; lfnOf(rd32(a2));
                 }
                 break;
            case XROOTD_MON_DISC:
                 {j["type"] = "disconnect";
                  j["duration_s"] = ri32(a1);
                  fillClient(j, srv, rd32(a2));
                 }
                 break;
            case XROOTD_MON_READV:
            case XROOTD_MON_READU:
                 j["type"] = "readv"; lfnOf(rd32(a2));
                 break;
            case XROOTD_MON_APPID:
                 {char b[13]; std::memcpy(b, a0 + 4, 12); b[12] = 0;
                  j["type"] = "appid"; j["appinfo"] = b;
                 }
                 break;
            default: continue;   // REDHOST and anything else: skip
           }

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
void gsAggregate(XrdMetrics::MetricGroup* M,
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
                     delta("xrootd_collector_oss_ops_total",
                           "OSS plugin operations", l, base, jU(j, op.second));
                     std::string slowKey = std::string("slow_") + op.second;
                     delta("xrootd_collector_oss_slow_ops_total",
                           "OSS plugin slow operations", l, base + "|slow",
                           jU(j, slowKey.c_str()));
                    }
               }
               break;

          case XROOTD_MON_GSPFC:   // per file_close event
               {auto ev = j.find("event");
                if (ev == j.end() || *ev != "file_close") break;
                M->counterSeries("xrootd_collector_pfc_files_total",
                           "proxy-cache file closes", srv) += 1;
                auto pfcBytes = [&](const char* source, const char* field)
                   {uint64_t v = jU(j, field);
                    if (v) M->counterSeries("xrootd_collector_pfc_bytes_total",
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
                M->counterSeries("xrootd_collector_tpc_total", "third-party copies",
                           {{"server", src}, {"type", type},
                            {"result", rc == 0 ? "ok" : "error"}}) += 1;
                if (size)
                   M->counterSeries("xrootd_collector_tpc_bytes_total",
                              "third-party copy bytes",
                              {{"server", src}, {"type", type}}) += size;
                M->histogramSeries("xrootd_collector_tpc_size_bytes",
                             "third-party copy size",
                             {1e6,1e7,1e8,1e9,1e10,1e11}).observe((double)size);
               }
               break;

          case XROOTD_MON_GSTHR:   // throttle plugin
               {auto ev = j.find("event");
                if (ev == j.end() || *ev != "throttle_update") break;
                delta("xrootd_collector_throttle_io_total",
                      "throttle plugin I/O operations", srv,
                      src + "|thr|io_total", jU(j, "io_total"));
                auto ia = j.find("io_active");
                if (ia != j.end() && ia->is_number())
                   M->gaugeSeries("xrootd_collector_throttle_io_active",
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
                     delta("xrootd_collector_http_requests_total",
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
   if (plen < 24) {stats.malformed++; return;}
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
                        j["type"]     = "gstream";
                        j["provider"] = provider;
                        fillServer(j, src, stod, srv);
                        j["@timestamp"]   = isoTime(tEnd ? tEnd : tBeg);
                        if (payload.is_discarded()) j["data"] = line;
                           else j["data"] = payload;
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
   if (plen < 16) {stats.malformed++; return;}

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

         // The "<host>:<path>" string spans the next `slots` records.
         const char* sp = (const char*)(p + off + RSZ);
         int savail = plen - (off + RSZ);
         int slen = (int)slots * RSZ; if (slen > savail) slen = savail;
         std::string hp(sp, slen > 0 ? strnlen(sp, slen) : 0);

         stats.redirs++;
         const char* kind = (type & 0xf0) == XROOTD_MON_REDLOCAL
                          ? "local" : "remote";
         if (metrics)
            metrics->counterSeries("xrootd_collector_redirects_total",
                          "client redirects issued by the server",
                          {{"server", src}, {"kind", kind}}) += 1;
         if (redirects)
            {// A redirect concludes the operation from this (redirector) node's
             // point of view, so it is reported in the same type:"transfer"
             // concluded-operation schema as closes and errors, with
             // operation_state "Redirected" and the destination under "redirect".
             json j;
             j["type"] = "transfer";
             if (tWin > 0) j["@timestamp"] = isoTime(tWin);
             fillServer(j, src, stod, srv);

             json& transfer = j["transfer"];
             transfer["operation"]       = redirOp(type & 0x0f);
             transfer["operation_state"] = "Redirected";

             json& redirect = j["redirect"];
             redirect["kind"]        = kind;
             redirect["target_port"] = port;
             // hp is "<host>:<path>": the host is the redirect target, the path
             // the lfn the client is being redirected for.
             auto colon = hp.find(':');
             if (colon != std::string::npos)
                {if (colon > 0) redirect["target_host"] = hp.substr(0, colon);
                 j["file"]["lfn"] = hp.substr(colon + 1);
                }
                else if (!hp.empty()) redirect["target_host"] = hp;

             fillClient(j, srv, did);
             stats.docs++;
             if (doc) doc(j.dump());
            }

         off += RSZ * (1 + slots);
        }
}
