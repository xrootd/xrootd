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

#include <cstring>
#include <ctime>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
#include "XrdMetrics/XrdMetrics.hh"
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
   return servers[key];
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

   switch(code)
         {case XROOTD_MON_MAPUSER:
          case XROOTD_MON_MAPPATH:
          case XROOTD_MON_MAPINFO:
               // XrdXrootdMonMap: header(8) + dictid(4) + info[]
               if (plen < 12) {stats.malformed++; return false;}
               {uint32_t dictid = rd32(p + 8);
                DecodeMap(code, srv, dictid, (const char*)(p + 12), plen - 12);
               }
               break;

          case XROOTD_MON_MAPFSTA:
               DecodeFStream(src, stod, srv, p + 8, plen - 8);
               break;

          case XROOTD_MON_MAPTRCE:
               DecodeTStream(src, stod, srv, p + 8, plen - 8);
               break;

          case XROOTD_MON_MAPGSTA:
               DecodeGStream(src, stod, p, plen);
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

   return true;
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
       srv.users[dictid] = std::move(u);
      }
      else if (code == XROOTD_MON_MAPPATH)
              {stats.mapPath++;
               // info is "<who>\n<lfn>"; keep the lfn for 't'-stream lookups.
               auto nl = text.find('\n');
               srv.paths[dictid] = (nl == std::string::npos) ? text
                                                             : text.substr(nl + 1);
              }
      else    {stats.mapInfo++;
               // 'i' (appinfo): "<descriptor>\n<appinfo>". The descriptor matches
               // the 'u' user descriptor, so key by it to enrich transfers.
               auto nl = text.find('\n');
               if (nl != std::string::npos)
                  srv.infos[text.substr(0, nl)] = text.substr(nl + 1);
              }

   if (dumpRaw && raw)
      {json j = {{"code", std::string(1, (char)code)},
                 {"dictid", dictid}, {"info", first}};
       raw(j.dump());
      }
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
                      srv.files[fileID] = std::move(of);
                     }
                     break;

                case XrdXrootdMonFileHdr::isClose:
                     {stats.closes++;
                      uint32_t fileID = rd32(rec + 4);
                      EmitClose(src, stod, srv, fileID, recFlag, rec, recSize,
                                tWin);
                     }
                     break;

                default:  // isXfr (in-flight) and isDisc (session end): counted
                     break;               // but not yet turned into documents
               }

         off += recSize;
        }
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

   json j;
   j["type"]         = "transfer";
   j["@timestamp"]   = isoTime(tWin);
   j["server"]       = src;
   j["server_start"] = stod;
   j["server_id"]    = srv.sID;
   j["close_time"]   = isoTime(tWin);
   j["forced"]       = (recFlag & XrdXrootdMonFileHdr::forced) != 0;
   j["read_bytes"]   = rdBytes;
   j["readv_bytes"]  = rvBytes;
   j["write_bytes"]  = wrBytes;

// Join the matching open record (held since the open packet) to recover the
// path, the user, and the open time. Resolve the user dictid if we have it.
//
   auto fit = srv.files.find(fileID);
   if (fit != srv.files.end())
      {const OpenFile& of = fit->second;
       j["open_seen"]  = true;
       j["lfn"]        = of.lfn;
       j["file_size"]  = of.fsz;
       j["read_write"] = of.rw;
       j["open_time"]  = isoTime(of.tOpen);
       if (of.tOpen > 0 && tWin > 0) {durSecs = tWin - of.tOpen;
                                      j["duration_s"] = durSecs;}

       auto uit = srv.users.find(of.user);
       if (uit != srv.users.end())
          {j["user"]        = uit->second.user;
           j["protocol"]    = uit->second.prot;
           j["client_host"] = uit->second.host;
           j["user_raw"]    = uit->second.raw;
           // Enrich with the application info ('i' stream), joined by descriptor.
           auto iit = srv.infos.find(uit->second.raw);
           if (iit != srv.infos.end()) j["appinfo"] = iit->second;
          }
       srv.files.erase(fit);
      }
      else {j["open_seen"] = false; stats.orphanCls++;}

// Optional op-count detail (XrdXrootdMonStatOPS) when "ops" was configured.
//
   if ((recFlag & XrdXrootdMonFileHdr::hasOPS) && recSize >= 8 + 24 + 48)
      {const unsigned char* o = rec + 8 + 24;
       j["read_ops"]   = ri32(o + 0);
       j["readv_ops"]  = ri32(o + 4);
       j["write_ops"]  = ri32(o + 8);
       j["readv_segs"] = ri64(o + 16);

       // Request-size extremes use 0x7fffffff as the "unset" sentinel; omit
       // them rather than emit a misleading minimum.
       //
       auto minmax = [&](const char* kmn, const char* kmx, int32_t mn, int32_t mx)
                       {if (mn != 0x7fffffff) {j[kmn] = mn; j[kmx] = mx;}};
       minmax("read_min",  "read_max",  ri32(o + 24), ri32(o + 28));
       minmax("readv_min", "readv_max", ri32(o + 32), ri32(o + 36));
       minmax("write_min", "write_max", ri32(o + 40), ri32(o + 44));

       // Optional sum-of-squares (XrdXrootdMonStatSSQ) when "ssq" configured.
       //
       if ((recFlag & XrdXrootdMonFileHdr::hasSSQ) && recSize >= 8 + 24 + 48 + 32)
          {const unsigned char* s = o + 48;
           j["read_sumsq"]  = rdbl(s + 0);
           j["readv_sumsq"] = rdbl(s + 8);
           j["rsegs_sumsq"] = rdbl(s + 16);
           j["write_sumsq"] = rdbl(s + 24);
          }
      }

// Aggregate into bounded-cardinality Prometheus series (label only by the
// reporting server). Per-transfer detail stays in the document sink; here we
// keep just totals and distributions suitable for time-series storage.
//
   if (metrics)
      {XrdMetricsLabels sl = {{"server", src}};
       metrics->Counter("xrootd_collector_transfers_total",
                        "completed transfers seen", sl).inc();
       metrics->Counter("xrootd_collector_read_bytes_total",
                        "bytes read (read+readv)", sl).inc(rdBytes + rvBytes);
       metrics->Counter("xrootd_collector_write_bytes_total",
                        "bytes written", sl).inc(wrBytes);
       metrics->Histogram("xrootd_collector_transfer_size_bytes",
                        "bytes moved per transfer",
                        {1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11})
               .observe((double)(rdBytes + rvBytes + wrBytes));
       if (durSecs >= 0)
          metrics->Histogram("xrootd_collector_transfer_duration_seconds",
                        "transfer wall-clock duration",
                        {1,5,15,60,300,1800,7200}).observe(durSecs);
      }

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
        j["server"]     = src;
        j["server_start"] = stod;
        if (tWin > 0) j["@timestamp"] = isoTime(tWin);

        auto lfnOf = [&](uint32_t id)
            {auto it = srv.paths.find(id);
             if (it != srv.paths.end()) j["lfn"] = it->second;
             j["file"] = id;
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
                  uint32_t uid = rd32(a2);
                  auto it = srv.users.find(uid);
                  if (it != srv.users.end())
                     {j["user"] = it->second.user;
                      j["client_host"] = it->second.host;
                     }
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
}

void XrdMonDecode::DecodeGStream(const std::string& src, int32_t stod,
                                 const unsigned char* p, int plen)
{
// XrdXrootdMonGS: header(8) + tBeg(4) + tEnd(4) + sID(8); the provider type is
// the top byte of sID. The remainder is newline-separated plugin records
// (JSON or CGI), produced by the oss/pfc/throttle/tpc/http g-streams.
//
   if (plen < 24) {stats.malformed++; return;}
   int32_t  tBeg = ri32(p + 8);
   int32_t  tEnd = ri32(p + 12);
   uint64_t sID  = rd64(p + 16);
   const char* provider = gsProvider((unsigned char)(sID >> 56));

   const char* body = (const char*)(p + 24);
   int blen = plen - 24;

   int start = 0;
   for (int i = 0; i <= blen; i++)
       {if (i == blen || body[i] == '\n')
           {int n = i - start;
            if (n > 0)
               {stats.gevents++;
                if (gstream)
                   {std::string line(body + start, n);
                    json j;
                    j["type"]     = "gstream";
                    j["provider"] = provider;
                    j["server"]   = src;
                    j["server_start"] = stod;
                    j["@timestamp"]   = isoTime(tEnd ? tEnd : tBeg);
                    json payload = json::parse(line, nullptr, false);
                    if (payload.is_discarded()) j["data"] = line;
                       else j["data"] = payload;
                    if (doc) doc(j.dump());
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
         if (redirects)
            {json j;
             j["type"]          = "redirect";
             j["server"]        = src;
             j["server_start"]  = stod;
             if (tWin > 0) j["@timestamp"] = isoTime(tWin);
             j["operation"]     = redirOp(type & 0x0f);
             j["redirect_kind"] = (type & 0xf0) == XROOTD_MON_REDLOCAL
                                ? "local" : "remote";
             j["target_port"]   = port;
             auto colon = hp.find(':');
             if (colon != std::string::npos)
                {if (colon > 0) j["target_host"] = hp.substr(0, colon);
                 j["path"] = hp.substr(colon + 1);
                }
                else if (!hp.empty()) j["target_host"] = hp;

             auto uit = srv.users.find(did);
             if (uit != srv.users.end())
                {j["user"]        = uit->second.user;
                 j["client_host"] = uit->second.host;
                }
             if (doc) doc(j.dump());
            }

         off += RSZ * (1 + slots);
        }
}
