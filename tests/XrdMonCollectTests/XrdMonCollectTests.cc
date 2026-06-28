//------------------------------------------------------------------------------
// Unit tests for the xrdmoncollect decoder/correlator. Packets are hand-built
// in the on-the-wire (network byte order) layout described by
// src/XrdXrootd/XrdXrootdMonData.hh.
//------------------------------------------------------------------------------

#include <cstring>
#include <string>
#include <vector>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <gtest/gtest.h>

using json = nlohmann::json;

namespace
{
// Big-endian byte builder.
struct W
{
   std::vector<unsigned char> b;
   void u8 (uint8_t v)  {b.push_back(v);}
   void u16(uint16_t v) {b.push_back(v >> 8); b.push_back(v & 0xff);}
   void u32(uint32_t v) {for (int i = 3; i >= 0; i--) b.push_back((v >> (8*i)) & 0xff);}
   void u64(uint64_t v) {for (int i = 7; i >= 0; i--) b.push_back((v >> (8*i)) & 0xff);}
   void raw(const std::string& s) {b.insert(b.end(), s.begin(), s.end());}
   void raw(const std::vector<unsigned char>& v) {b.insert(b.end(), v.begin(), v.end());}
};

// One f-stream record: recType, recFlag, recSize(auto), then body (union+payload).
std::vector<unsigned char> rec(uint8_t type, uint8_t flag,
                               const std::vector<unsigned char>& body)
{
   W w;
   w.u8(type); w.u8(flag); w.u16((uint16_t)(4 + body.size()));
   w.raw(body);
   return w.b;
}

// Prepend an 8-byte monitor header and patch the packet length field.
std::vector<unsigned char> packet(char code, int32_t stod,
                                  const std::vector<unsigned char>& payload)
{
   W w;
   w.u8((uint8_t)code); w.u8(0); w.u16(0); w.u32((uint32_t)stod);
   w.raw(payload);
   uint16_t plen = (uint16_t)w.b.size();
   w.b[2] = plen >> 8; w.b[3] = plen & 0xff;
   return w.b;
}

// f-stream TOD (isTime) record with the given window end time.
std::vector<unsigned char> todRec(int32_t tEnd, int64_t sID)
{
   W body;
   body.u32(0);             // union: nRecs (unused here)
   body.u32((uint32_t)tEnd);// tBeg
   body.u32((uint32_t)tEnd);// tEnd
   body.u64((uint64_t)sID); // sID
   return rec(2 /*isTime*/, 0, body.b);
}

const int32_t kStod  = 1700000000;
const int32_t kOpenT = 1700000000;
const int32_t kCloseT= 1700000082;
}

// Build: 'u' user map, then 'f' open, then 'f' close -> one transfer doc.
class Transfer : public ::testing::Test
{
protected:
  std::string lastDoc;
  XrdMonDecode dec{[&](const std::string& d){ lastDoc = d; }};

  void feedUserMap()
  {
     W body; body.u32(7);                       // dictid
     std::vector<unsigned char> pl = body.b;
     std::string info = "xroot/alice.123:4@wn.example.org\nrole=prod";
     pl.insert(pl.end(), info.begin(), info.end());
     auto pkt = packet('u', kStod, pl);
     dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());
  }

  void feedOpen()
  {
     W body;
     body.u32(100);                 // fileID
     body.u64(123456);              // fsz
     body.u32(7);                   // user dictid
     std::string lfn = "/store/data/file.root";
     body.raw(lfn); body.u8(0);     // null terminated
     auto payload = todRec(kOpenT, 42);
     auto r = rec(1 /*isOpen*/, 0x01 | 0x02 /*hasLFN|hasRW*/, body.b);
     payload.insert(payload.end(), r.begin(), r.end());
     auto pkt = packet('f', kStod, payload);
     dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());
  }

  void feedClose()
  {
     W body;
     body.u32(100);                 // fileID
     body.u64(10485760);            // Xfr.read
     body.u64(0);                   // Xfr.readv
     body.u64(0);                   // Xfr.write
     // OPS (48 bytes)
     body.u32(320);                 // read ops
     body.u32(0);                   // readv ops
     body.u32(0);                   // write ops
     body.u16(0); body.u16(0);      // rsMin, rsMax
     body.u64(0);                   // rsegs
     body.u32(4096); body.u32(1048576); // rdMin, rdMax
     body.u32(0); body.u32(0);      // rvMin, rvMax
     body.u32(0); body.u32(0);      // wrMin, wrMax
     auto payload = todRec(kCloseT, 42);
     auto r = rec(0 /*isClose*/, 0x02 /*hasOPS*/, body.b);
     payload.insert(payload.end(), r.begin(), r.end());
     auto pkt = packet('f', kStod, payload);
     dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());
  }
};

TEST_F(Transfer, CorrelatesCloseWithOpenAndUser)
{
  feedUserMap();
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);

  EXPECT_EQ(j["type"], "transfer");
  EXPECT_EQ(j["file"]["lfn"], "/store/data/file.root");
  EXPECT_EQ(j["user"]["name"], "alice");
  EXPECT_EQ(j["user"]["protocol"], "xroot");
  EXPECT_EQ(j["client"]["host"], "wn.example.org");
  EXPECT_EQ(j["client"]["hostname"], "wn.example.org");  // not an IP literal
  EXPECT_EQ(j["transfer"]["read_bytes"], 10485760);
  EXPECT_EQ(j["transfer"]["operation"], "read");
  EXPECT_EQ(j["transfer"]["read_ops"], 320);
  EXPECT_EQ(j["transfer"]["read_max"], 1048576);
  EXPECT_EQ(j["transfer"]["open_seen"], true);
  EXPECT_EQ(j["file"]["size"], 123456);
  EXPECT_EQ(j["transfer"]["duration_s"], kCloseT - kOpenT);
  EXPECT_EQ(j["server"]["id"], 42);

  const XrdMonDecode::Stats& s = dec.GetStats();
  EXPECT_EQ(s.docs, 1u);
  EXPECT_EQ(s.opens, 1u);
  EXPECT_EQ(s.closes, 1u);
  EXPECT_EQ(s.mapUser, 1u);
  EXPECT_EQ(s.orphanCls, 0u);
}

TEST_F(Transfer, CloseWithoutOpenIsOrphan)
{
  feedClose();  // no preceding open

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["transfer"]["open_seen"], false);
  EXPECT_EQ(j["transfer"]["read_bytes"], 10485760);
  EXPECT_FALSE(j.contains("file"));
  EXPECT_EQ(dec.GetStats().orphanCls, 1u);
}

TEST(XrdMonCollect, ShortPacketIsMalformed)
{
  XrdMonDecode dec([](const std::string&){});
  char tiny[4] = {'f', 0, 0, 0};
  EXPECT_FALSE(dec.Process("1.2.3.4:5", tiny, sizeof(tiny)));
  EXPECT_EQ(dec.GetStats().malformed, 1u);
}

TEST(XrdMonCollect, UnknownStreamCounted)
{
  XrdMonDecode dec([](const std::string&){});
  // 'Z' is not a defined monitor code -> counted as unknown.
  auto pkt = packet('Z', kStod, std::vector<unsigned char>(16, 0));
  EXPECT_TRUE(dec.Process("1.2.3.4:5", (const char*)pkt.data(), pkt.size()));
  EXPECT_EQ(dec.GetStats().unknown, 1u);
}

namespace
{
// 16-byte XrdXrootdMonTrace record from arg0(8)/arg1(4)/arg2(4).
std::vector<unsigned char> trace(const std::vector<unsigned char>& a0,
                                 uint32_t a1, uint32_t a2)
{
   W w; w.raw(a0); w.u32(a1); w.u32(a2);
   return w.b;  // a0 must already be 8 bytes
}
std::vector<unsigned char> u64v(uint64_t v) {W w; w.u64(v); return w.b;}
}

TEST(XrdMonCollect, TStreamRecordsDecoded)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); },
                   nullptr, false, /*traces=*/true);

  // 'd' path map: dictid 50 -> /path/f.root
  {
    W body; body.u32(50);
    std::string info = "alice.1:2@host\n/path/f.root";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('d', kStod, pl);
    dec.Process("h:1", (const char*)pkt.data(), pkt.size());
  }

  // 't' packet: window, a read I/O on file 50, and a close on file 50.
  W payload;
  { std::vector<unsigned char> a0(8, 0); a0[0] = 0xe0;     // WINDOW
    payload.raw(trace(a0, 0, 1700000000)); }
  { auto a0 = u64v(4096);                                  // read offset 4096
    payload.raw(trace(a0, 1024, 50)); }                    // len 1024, file 50
  { std::vector<unsigned char> a0(8, 0); a0[0] = 0xc0;     // CLOSE
    a0[1] = 0; a0[2] = 0;                                   // shifts
    a0[4]=0; a0[5]=0; a0[6]=0x08; a0[7]=0x00;              // rVal = 2048
    payload.raw(trace(a0, 0, 50)); }
  auto pkt = packet('t', kStod, payload.b);
  dec.Process("h:1", (const char*)pkt.data(), pkt.size());

  EXPECT_EQ(dec.GetStats().traces, 3u);
  ASSERT_EQ(docs.size(), 2u);   // window emits nothing; read + close do
  json rd = json::parse(docs[0]);
  EXPECT_EQ(rd["type"], "read");
  EXPECT_EQ(rd["offset"], 4096);
  EXPECT_EQ(rd["length"], 1024);
  EXPECT_EQ(rd["lfn"], "/path/f.root");
  json cl = json::parse(docs[1]);
  EXPECT_EQ(cl["type"], "close");
  EXPECT_EQ(cl["read_bytes"], 2048);
}

TEST_F(Transfer, AggregatesIntoMetricsRegistry)
{
  // Re-run the open/close/user sequence through a decoder bound to a registry.
  XrdMetrics::Registry reg("");
  std::string sink;
  XrdMonDecode d([&](const std::string& s){ sink = s; }, nullptr,
                 false, false, false, false, &reg.group(""));

  { W body; body.u32(7);
    std::string info = "xroot/alice.1:2@wn.example.org\n";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('u', kStod, pl);
    d.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }
  { W body; body.u32(100); body.u64(123456); body.u32(7);
    std::string lfn = "/store/data/file.root"; body.raw(lfn); body.u8(0);
    auto payload = todRec(kOpenT, 42);
    auto r = rec(1, 0x03, body.b);
    payload.insert(payload.end(), r.begin(), r.end());
    auto pkt = packet('f', kStod, payload);
    d.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }
  { W body; body.u32(100); body.u64(10485760); body.u64(0); body.u64(0);
    body.u32(320); body.u32(0); body.u32(0); body.u16(0); body.u16(0);
    body.u64(0); body.u32(4096); body.u32(1048576);
    body.u32(0); body.u32(0); body.u32(0); body.u32(0);
    auto payload = todRec(kCloseT, 42);
    auto r = rec(0, 0x02, body.b);
    payload.insert(payload.end(), r.begin(), r.end());
    auto pkt = packet('f', kStod, payload);
    d.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }

  std::string out;
  XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_transfers_total{server=\"10.0.0.1:9930\"} 1"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_collector_read_bytes_total{server=\"10.0.0.1:9930\"} 10485760"),
            std::string::npos);
  EXPECT_NE(out.find("# TYPE xrootd_collector_transfer_duration_seconds histogram"),
            std::string::npos);
}

TEST_F(Transfer, AppInfoEnrichesTransfer)
{
  feedUserMap();
  // 'i' (appinfo) map: same descriptor as the user, plus an appinfo body.
  { W body; body.u32(9);
    std::string info = "xroot/alice.123:4@wn.example.org\ntest-app-v1";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('i', kStod, pl);
    dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["app"]["raw"], "test-app-v1");
}

TEST(XrdMonCollect, RedirectStreamDecoded)
{
  std::string out;
  XrdMonDecode dec([&](const std::string& d){ out = d; }, nullptr,
                   false, false, false, /*redirects=*/true);

  { W body; body.u32(7);
    std::string info = "xroot/bob.1:2@cli.example.org\n";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('u', kStod, pl);
    dec.Process("mgr:9930", (const char*)pkt.data(), pkt.size()); }

  // r-stream: sID block (REDSID marker + 7 bytes), one redirect record, then
  // the "<host>:<path>" string occupying 4 (Dent) further 8-byte slots.
  W body;
  body.u8(0xf0); for (int i = 0; i < 7; i++) body.u8(0);
  body.u8(0x85); body.u8(4); body.u16(1094); body.u32(7);  // open-read, port, uid
  std::string hp = "host.example:/store/data/f.root";       // 31 chars + NUL = 32
  body.raw(hp); body.u8(0);
  auto pkt = packet('r', kStod, body.b);
  dec.Process("mgr:9930", (const char*)pkt.data(), pkt.size());

  EXPECT_EQ(dec.GetStats().redirs, 1u);
  json j = json::parse(out);
  EXPECT_EQ(j["type"], "redirect");
  EXPECT_EQ(j["operation"], "open-read");
  EXPECT_EQ(j["redirect_kind"], "remote");
  EXPECT_EQ(j["target_host"], "host.example");
  EXPECT_EQ(j["target_port"], 1094);
  EXPECT_EQ(j["path"], "/store/data/f.root");
  EXPECT_EQ(j["user"], "bob");
}

TEST_F(Transfer, TokenAndActivityEnrichTransfer)
{
  feedUserMap();
  // 'T' token map: keyed by the same user dictid (7) as the 'u' map.
  { W body; body.u32(7);
    std::string info = "&Uc=7&s=https://issuer/sub42&n=alice"
                       "&o=atlas&r=production&g=/atlas/prod";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('T', kStod, pl);
    dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }
  // 'U' user experiment/activity map (SciTags), same dictid.
  { W body; body.u32(7);
    std::string info = "&Uc=7&Ec=42&Ac=7";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('U', kStod, pl);
    dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["user"]["subject"], "https://issuer/sub42");
  EXPECT_EQ(j["user"]["vo"], "atlas");
  EXPECT_EQ(j["user"]["role"], "production");
  EXPECT_EQ(j["user"]["groups"], "/atlas/prod");
  EXPECT_EQ(j["activity"]["experiment_id"], 42);
  EXPECT_EQ(j["activity"]["activity_id"], 7);

  const XrdMonDecode::Stats& s = dec.GetStats();
  EXPECT_EQ(s.mapTokn, 1u);
  EXPECT_EQ(s.mapUeac, 1u);
}

// Feed a 'u' map for dictid 7 with a custom CGI tail after the descriptor.
static void feedUserMapTail(XrdMonDecode& dec, const std::string& tail)
{
   W body; body.u32(7);
   std::string info = "xroot/alice.123:4@198.51.100.7\n" + tail;
   std::vector<unsigned char> pl = body.b;
   pl.insert(pl.end(), info.begin(), info.end());
   auto pkt = packet('u', kStod, pl);
   dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());
}

TEST_F(Transfer, AuthTailEnrichesTransfer)
{
  // Full auth payload (as built by MonAuth, with the login appinfo appended).
  feedUserMapTail(dec, "&p=gsi&n=alice&h=198.51.100.7&o=atlas&r=production"
                       "&g=/atlas&m=&R=v5.6.1&x=xrdcp&y=&I=4");
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["user"]["auth_method"], "gsi");
  EXPECT_EQ(j["user"]["vo"], "atlas");          // auth-derived VO (no token here)
  EXPECT_EQ(j["user"]["role"], "production");
  EXPECT_EQ(j["client"]["version"], "v5.6.1");
  EXPECT_EQ(j["client"]["ip_version"], 4);
  EXPECT_EQ(j["app"]["name"], "xrdcp");
  // The descriptor host is an IPv4 literal -> client.ip, not client.hostname.
  EXPECT_EQ(j["client"]["ip"], "198.51.100.7");
  EXPECT_FALSE(j["client"].contains("hostname"));
}

TEST_F(Transfer, NoAuthLoginAppinfoStillEnriches)
{
  // Without "... auth" the 'u' tail is only the login appinfo (no &p=/&o=).
  feedUserMapTail(dec, "&R=v5.6.1&x=xrdcp&y=&I=6");
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["client"]["version"], "v5.6.1");
  EXPECT_EQ(j["client"]["ip_version"], 6);
  EXPECT_FALSE(j["user"].contains("auth_method"));  // no &p= without auth
  EXPECT_FALSE(j["user"].contains("vo"));           // no &o= and no token
}

TEST_F(Transfer, WriteOperationDerived)
{
  feedUserMap();
  feedOpen();
  // Custom close carrying only write bytes -> operation "write".
  W body;
  body.u32(100);                 // fileID
  body.u64(0);                   // Xfr.read
  body.u64(0);                   // Xfr.readv
  body.u64(2097152);             // Xfr.write
  auto payload = todRec(kCloseT, 42);
  auto r = rec(0 /*isClose*/, 0 /*no OPS*/, body.b);
  payload.insert(payload.end(), r.begin(), r.end());
  auto pkt = packet('f', kStod, payload);
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["transfer"]["operation"], "write");
  EXPECT_EQ(j["transfer"]["write_bytes"], 2097152);
}

namespace
{
// A minimal valid 'u' map packet (dictid + descriptor) with a chosen pseq.
std::vector<unsigned char> userPkt(uint32_t dictid, uint8_t pseq)
{
   W body; body.u32(dictid);
   std::string info = "xroot/u.1:2@h\n";
   std::vector<unsigned char> pl = body.b;
   pl.insert(pl.end(), info.begin(), info.end());
   auto pkt = packet('u', kStod, pl);
   pkt[1] = pseq;        // header pseq is the second byte
   return pkt;
}
}

TEST(XrdMonCollect, PacketLossDetected)
{
  XrdMetrics::Registry reg("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &reg.group(""));

  for (uint8_t seq : {0, 1, 3, 4})   // 2 is missing -> one lost packet
     {auto p = userPkt(seq, seq);
      dec.Process("h:1", (const char*)p.data(), p.size());}

  EXPECT_EQ(dec.GetStats().lost, 1u);
  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_packets_lost_total{server=\"h:1\"} 1"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, DictionaryEviction)
{
  XrdMonDecode dec([](const std::string&){});
  dec.SetMaxEntries(10);

  // Feed 100 distinct user dictids; the cap keeps the table bounded.
  for (uint32_t id = 1; id <= 100; id++)
     {auto p = userPkt(id, (uint8_t)id);
      dec.Process("h:1", (const char*)p.data(), p.size());}

  EXPECT_GT(dec.GetStats().evicted, 0u);
}

TEST(XrdMonCollect, FrmStageAndPurge)
{
  XrdMetrics::Registry reg("");
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); }, nullptr,
                   false, false, false, false, &reg.group(""));

  auto frm = [&](char code, const std::string& info)
     {W body; body.u32(0);                // dictid is 0 for x/p
      std::vector<unsigned char> pl = body.b;
      pl.insert(pl.end(), info.begin(), info.end());
      auto pkt = packet(code, kStod, pl);
      dec.Process("h:1", (const char*)pkt.data(), pkt.size());};

  frm('x', "xroot/alice.1:2@wn.example.org\n/store/data/f.root");
  frm('p', "xroot/alice.1:2@wn.example.org\n/store/data/f.root"
           "\n&tod=1700000000&sz=1048576&at=1&ct=2&mt=3&fn=l");

  ASSERT_EQ(docs.size(), 2u);
  json x = json::parse(docs[0]);
  EXPECT_EQ(x["type"], "frm");
  EXPECT_EQ(x["operation"], "transfer");
  EXPECT_EQ(x["user"], "alice");
  EXPECT_EQ(x["lfn"], "/store/data/f.root");
  json p = json::parse(docs[1]);
  EXPECT_EQ(p["operation"], "purge");
  EXPECT_EQ(p["size"], 1048576);
  EXPECT_EQ(dec.GetStats().frmEvents, 2u);

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_frm_total{server=\"h:1\",op=\"purge\"} 1"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_frm_purge_bytes_total{server=\"h:1\"} 1048576"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, SessionDiscAndActiveGauge)
{
  XrdMetrics::Registry reg("");
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); }, nullptr,
                   false, false, false, false, &reg.group(""));

  // 'u' user map: dictid 7 -> bob.
  { W body; body.u32(7);
    std::string info = "xroot/bob.1:2@cli.example.org\n";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('u', kStod, pl);
    dec.Process("h:1", (const char*)pkt.data(), pkt.size()); }

  // f-packet: an open (file 100) then a disconnect for user 7.
  { W body; body.u32(100); body.u64(123456); body.u32(7);
    std::string lfn = "/store/f.root"; body.raw(lfn); body.u8(0);
    auto payload = todRec(kOpenT, 42);
    auto r = rec(1 /*isOpen*/, 0x03 /*hasLFN|hasRW*/, body.b);
    payload.insert(payload.end(), r.begin(), r.end());
    W disc; disc.u32(7);                       // userID in the Hdr union
    auto dr = rec(4 /*isDisc*/, 0, disc.b);
    payload.insert(payload.end(), dr.begin(), dr.end());
    auto pkt = packet('f', kStod, payload);
    dec.Process("h:1", (const char*)pkt.data(), pkt.size()); }

  ASSERT_EQ(docs.size(), 1u);                  // the session_end document
  json j = json::parse(docs[0]);
  EXPECT_EQ(j["type"], "session_end");
  EXPECT_EQ(j["user"], "bob");
  EXPECT_EQ(j["client_host"], "cli.example.org");
  EXPECT_EQ(dec.GetStats().discs, 1u);

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_sessions_total{server=\"h:1\"} 1"),
            std::string::npos) << out;
  // One file opened, none closed -> active gauge is 1.
  EXPECT_NE(out.find("xrootd_collector_active_transfers{server=\"h:1\"} 1"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, ServerIdentDecoded)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); });

  std::string info = "=/xrootd.4321:99@srv.example.org"
                     "\n&site=T1_DE_KIT&port=1094&inst=manager&pgm=xrootd&ver=v6.1.0";
  W body; body.u32(0);   // dictid is 0 for '='
  std::vector<unsigned char> pl = body.b;
  pl.insert(pl.end(), info.begin(), info.end());
  auto pkt = packet('=', kStod, pl);
  dec.Process("srv:9930", (const char*)pkt.data(), pkt.size());
  // A second identical ident must not produce a duplicate document.
  dec.Process("srv:9930", (const char*)pkt.data(), pkt.size());

  ASSERT_EQ(docs.size(), 1u);
  json j = json::parse(docs[0]);
  EXPECT_EQ(j["type"], "server_ident");
  EXPECT_EQ(j["site"], "T1_DE_KIT");
  EXPECT_EQ(j["host"], "srv.example.org");
  EXPECT_EQ(j["instance"], "manager");
  EXPECT_EQ(j["program"], "xrootd");
  EXPECT_EQ(j["version"], "v6.1.0");
  EXPECT_EQ(j["port"], 1094);
  EXPECT_EQ(dec.GetStats().mapIdnt, 2u);
}

TEST(XrdMonCollect, GStreamForwarded)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); },
                   nullptr, false, false, /*gstream=*/true);

  W payload;
  payload.u32(1700000000);                 // tBeg
  payload.u32(1700000060);                 // tEnd
  payload.u64(((uint64_t)'O' << 56) | 7);  // sID, provider 'O' = oss
  payload.raw(std::string("{\"event\":\"oss_stats\",\"reads\":5}"));
  auto pkt = packet('g', kStod, payload.b);
  dec.Process("h:1", (const char*)pkt.data(), pkt.size());

  ASSERT_EQ(docs.size(), 1u);
  json j = json::parse(docs[0]);
  EXPECT_EQ(j["type"], "gstream");
  EXPECT_EQ(j["provider"], "oss");
  EXPECT_EQ(j["data"]["reads"], 5);
  EXPECT_EQ(dec.GetStats().gevents, 1u);
}

namespace
{
// A 'g' (g-stream) packet for one provider carrying a single JSON record.
std::vector<unsigned char> gPacket(char prov, const std::string& jsonRec)
{
   W payload;
   payload.u32(1700000000);                  // tBeg
   payload.u32(1700000060);                  // tEnd
   payload.u64(((uint64_t)(unsigned char)prov << 56) | 7);  // sID + provider
   payload.raw(jsonRec);
   return packet('g', kStod, payload.b);
}
}

TEST(XrdMonCollect, GStreamOssMetricsDelta)
{
  XrdMetrics::Registry reg("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &reg.group(""));

  // First snapshot establishes the baseline (no counter movement).
  auto p1 = gPacket('O', "{\"event\":\"oss_stats\",\"reads\":100,\"writes\":10,"
                         "\"slow_reads\":4}");
  dec.Process("h:1", (const char*)p1.data(), p1.size());
  // Second snapshot: +50 reads, +5 writes, +1 slow_read.
  auto p2 = gPacket('O', "{\"event\":\"oss_stats\",\"reads\":150,\"writes\":15,"
                         "\"slow_reads\":5}");
  dec.Process("h:1", (const char*)p2.data(), p2.size());

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_oss_ops_total{server=\"h:1\",op=\"read\"} 50"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_oss_ops_total{server=\"h:1\",op=\"write\"} 5"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_oss_slow_ops_total{server=\"h:1\",op=\"read\"} 1"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, GStreamPfcAndTpcMetrics)
{
  XrdMetrics::Registry reg("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &reg.group(""));

  auto pfc = gPacket('C', "{\"event\":\"file_close\",\"b_hit\":2048,"
                          "\"b_miss\":1024,\"b_prefetch\":512}");
  dec.Process("h:1", (const char*)pfc.data(), pfc.size());

  auto tpc = gPacket('P', "{\"TPC\":\"xroot\",\"Xeq\":{\"RC\":0,\"Type\":\"pull\"},"
                          "\"Size\":1048576}");
  dec.Process("h:1", (const char*)tpc.data(), tpc.size());

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_pfc_files_total{server=\"h:1\"} 1"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_pfc_bytes_total{server=\"h:1\",source=\"hit\"} 2048"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_tpc_total{server=\"h:1\",type=\"pull\",result=\"ok\"} 1"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_tpc_bytes_total{server=\"h:1\",type=\"pull\"} 1048576"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, GStreamThrottleAndHttpMetrics)
{
  XrdMetrics::Registry reg("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &reg.group(""));

  // throttle: baseline then +30 io_total, io_active gauge = 4.
  auto t1 = gPacket('R', "{\"event\":\"throttle_update\",\"io_wait\":1.5,"
                         "\"io_active\":2,\"io_total\":100}");
  dec.Process("h:1", (const char*)t1.data(), t1.size());
  auto t2 = gPacket('R', "{\"event\":\"throttle_update\",\"io_wait\":2.0,"
                         "\"io_active\":4,\"io_total\":130}");
  dec.Process("h:1", (const char*)t2.data(), t2.size());

  // http: baseline then +5 GET/200.
  auto h1 = gPacket('H', "{\"HTTP_GET_200\":{\"count\":10,\"success\":10}}");
  dec.Process("h:1", (const char*)h1.data(), h1.size());
  auto h2 = gPacket('H', "{\"HTTP_GET_200\":{\"count\":15,\"success\":15}}");
  dec.Process("h:1", (const char*)h2.data(), h2.size());

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); reg.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_throttle_io_total{server=\"h:1\"} 30"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_throttle_io_active{server=\"h:1\"} 4"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_http_requests_total{server=\"h:1\",method=\"GET\",status=\"200\"} 5"),
            std::string::npos) << out;
}
