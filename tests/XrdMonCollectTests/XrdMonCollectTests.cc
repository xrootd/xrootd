//------------------------------------------------------------------------------
// Unit tests for the xrdmoncollect decoder/correlator. Packets are hand-built
// in the on-the-wire (network byte order) layout described by
// src/XrdXrootd/XrdXrootdMonData.hh.
//------------------------------------------------------------------------------

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdNet/XrdNetUtils.hh"
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

TEST_F(Transfer, SuccessfulCloseStateIsSuccessful)
{
  feedClose();  // a plain close (no error block)

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["transfer"]["operation_state"], "Successful");
  EXPECT_FALSE(j["transfer"].contains("error_message"));
  EXPECT_EQ(dec.GetStats().failed, 0u);
}

namespace
{
// XrdXrootdMonStatERR: ecode(4) + ecat(1) + rsvd(3) + null-terminated message.
std::vector<unsigned char> errBlock(int32_t ecode, uint8_t ecat,
                                    const std::string& msg)
{
   W w;
   w.u32((uint32_t)ecode);
   w.u8(ecat); w.u8(0); w.u8(0); w.u8(0);   // ecat + rsvd[3]
   w.raw(msg); w.u8(0);                      // null-terminated message
   return w.b;
}
}

// A failed open emits a self-contained isError record (no open/close pair).
TEST_F(Transfer, FailedOpenEmitsFailedState)
{
  feedUserMap();

  W body;
  body.u32(0);                              // fileID union (0 for isError)
  body.u32(7);                              // inline user dictid
  std::string lfn = "/store/data/missing.root";
  body.raw(lfn); body.u8(0);                // null-terminated lfn
  auto eb = errBlock(3011 /*kXR_NotAuthorized-ish*/, 5 /*monErrAuth*/,
                     "permission denied");
  body.raw(eb);

  auto payload = todRec(kCloseT, 42);
  auto r = rec(5 /*isError*/, 0x01 /*hasLFN*/, body.b);
  payload.insert(payload.end(), r.begin(), r.end());
  auto pkt = packet('f', kStod, payload);
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["type"], "transfer");
  EXPECT_EQ(j["file"]["lfn"], "/store/data/missing.root");
  EXPECT_EQ(j["user"]["name"], "alice");          // resolved from the inline dictid
  EXPECT_EQ(j["transfer"]["operation_state"], "Failed");
  EXPECT_EQ(j["transfer"]["error_code"], 3011);
  EXPECT_EQ(j["transfer"]["error_category"], "auth");
  EXPECT_EQ(j["transfer"]["error_message"], "permission denied");
  EXPECT_EQ(dec.GetStats().failed, 1u);
}

// An aborted transfer is reported as an isClose carrying a trailing error block.
TEST_F(Transfer, AbortedTransferCloseHasError)
{
  feedUserMap();
  feedOpen();

  W body;
  body.u32(100);                            // fileID (matches the open)
  body.u64(4096);                           // Xfr.read (partial)
  body.u64(0);                              // Xfr.readv
  body.u64(0);                              // Xfr.write
  // OPS (48 bytes)
  body.u32(2); body.u32(0); body.u32(0);    // read/readv/write ops
  body.u16(0); body.u16(0);                 // rsMin/rsMax
  body.u64(0);                              // rsegs
  body.u32(0); body.u32(0);                 // rdMin/rdMax
  body.u32(0); body.u32(0);                 // rvMin/rvMax
  body.u32(0); body.u32(0);                 // wrMin/wrMax
  auto eb = errBlock(3006 /*kXR_IOError-ish*/, 2 /*monErrRead*/,
                     "read error: connection reset");
  body.raw(eb);

  auto payload = todRec(kCloseT, 42);
  auto r = rec(0 /*isClose*/, 0x02 | 0x08 /*hasOPS|hasERR*/, body.b);
  payload.insert(payload.end(), r.begin(), r.end());
  auto pkt = packet('f', kStod, payload);
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["file"]["lfn"], "/store/data/file.root");  // joined to the open
  EXPECT_EQ(j["transfer"]["read_bytes"], 4096);          // partial bytes preserved
  EXPECT_EQ(j["transfer"]["read_ops"], 2);
  EXPECT_EQ(j["transfer"]["operation_state"], "Failed");
  EXPECT_EQ(j["transfer"]["error_category"], "read");
  EXPECT_EQ(j["transfer"]["error_message"], "read error: connection reset");
  EXPECT_EQ(dec.GetStats().failed, 1u);
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
  EXPECT_EQ(rd["file"]["lfn"], "/path/f.root");
  json cl = json::parse(docs[1]);
  EXPECT_EQ(cl["type"], "close");
  EXPECT_EQ(cl["read_bytes"], 2048);
}

TEST_F(Transfer, AggregatesIntoMetricsRegistry)
{
  // Re-run the open/close/user sequence through a decoder bound to a registry.
  XrdMetrics::Collector collector("");
  std::string sink;
  XrdMonDecode d([&](const std::string& s){ sink = s; }, nullptr,
                 false, false, false, false, &collector.subsystem(""));

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
  XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
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
  // A redirect is a concluded-operation report: type:"transfer" with
  // operation_state "Redirected" and the destination under "redirect".
  EXPECT_EQ(j["type"], "transfer");
  EXPECT_EQ(j["transfer"]["operation"], "open-read");
  EXPECT_EQ(j["transfer"]["operation_state"], "Redirected");
  EXPECT_EQ(j["redirect"]["kind"], "remote");
  EXPECT_EQ(j["redirect"]["target_host"], "host.example");
  EXPECT_EQ(j["redirect"]["target_port"], 1094);
  EXPECT_EQ(j["file"]["lfn"], "/store/data/f.root");
  EXPECT_EQ(j["user"]["name"], "bob");
  EXPECT_EQ(j["client"]["host"], "cli.example.org");
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

namespace
{
// Feed a 'U' (SciTags) experiment/activity map for dictid 7.
void feedActivity(XrdMonDecode& dec, int expId, int actId)
{
   W body; body.u32(7);
   std::string info = "&Uc=7&Ec=" + std::to_string(expId) +
                      "&Ac=" + std::to_string(actId);
   std::vector<unsigned char> pl = body.b;
   pl.insert(pl.end(), info.begin(), info.end());
   auto pkt = packet('U', kStod, pl);
   dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());
}

// Write a SciTags registry JSON to a temp file; return its path.
std::string writeScitags(const std::string& name, const std::string& body)
{
   std::string path = std::string(::testing::TempDir()) + "/" + name;
   std::ofstream(path) << body;
   return path;
}
}

// With a SciTags registry loaded, the numeric experiment/activity ids are
// additionally mapped to names, and the experiment name fills the VO when no
// token or auth VO is present.
TEST_F(Transfer, ScitagsRegistryMapsActivityAndVo)
{
  std::string collector = writeScitags("scitags-map.json",
     R"({"experiments":[{"expId":2,"expName":"atlas","activities":[
         {"activityId":7,"activityName":"production"},
         {"activityId":8,"activityName":"analysis"}]}]})");
  ASSERT_TRUE(dec.LoadScitags(collector));

  feedUserMap();          // descriptor tail has no &o= -> no auth VO, no token
  feedActivity(dec, 2, 7);
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["activity"]["experiment_id"], 2);
  EXPECT_EQ(j["activity"]["activity_id"], 7);
  EXPECT_EQ(j["activity"]["experiment"], "atlas");
  EXPECT_EQ(j["activity"]["activity"], "production");
  EXPECT_EQ(j["user"]["vo"], "atlas");          // VO fallback from the experiment
}

// A token VO must win over the SciTags experiment-name fallback.
TEST_F(Transfer, ScitagsVoYieldsToToken)
{
  std::string collector = writeScitags("scitags-vo.json",
     R"({"experiments":[{"expId":2,"expName":"atlas","activities":[]}]})");
  ASSERT_TRUE(dec.LoadScitags(collector));

  feedUserMap();
  { W body; body.u32(7);
    std::string info = "&Uc=7&s=sub&o=cms&r=prod";   // token carries VO "cms"
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('T', kStod, pl);
    dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }
  feedActivity(dec, 2, 7);
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["user"]["vo"], "cms");            // token VO, not the SciTags "atlas"
  EXPECT_EQ(j["activity"]["experiment"], "atlas");
}

// Without a registry, only the numeric ids appear (no names, no VO fallback).
TEST_F(Transfer, ScitagsNumericWithoutRegistry)
{
  feedUserMap();
  feedActivity(dec, 2, 7);
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["activity"]["experiment_id"], 2);
  EXPECT_EQ(j["activity"]["activity_id"], 7);
  EXPECT_FALSE(j["activity"].contains("experiment"));
  EXPECT_FALSE(j["activity"].contains("activity"));
  EXPECT_FALSE(j["user"].contains("vo"));
}

// A missing registry file is reported, leaving numeric ids untouched.
TEST(XrdMonCollect, ScitagsMissingFileReturnsFalse)
{
  XrdMonDecode dec([](const std::string&){});
  EXPECT_FALSE(dec.LoadScitags("/nonexistent/scitags.json"));
}

// A background refresh (LoadScitagsJson) swaps the registry whole: a later load
// with the same ids but new names/VO is reflected on subsequent transfers.
TEST_F(Transfer, ScitagsJsonReloadReflectsUpdate)
{
  ASSERT_TRUE(dec.LoadScitagsJson(
     R"({"experiments":[{"expId":2,"expName":"atlas","activities":[
        {"activityId":7,"activityName":"production"}]}]})"));
  ASSERT_TRUE(dec.LoadScitagsJson(           // the published registry changed
     R"({"experiments":[{"expId":2,"expName":"cms","activities":[
        {"activityId":7,"activityName":"analysis"}]}]})"));

  feedUserMap();
  feedActivity(dec, 2, 7);
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["activity"]["experiment"], "cms");
  EXPECT_EQ(j["activity"]["activity"], "analysis");
  EXPECT_EQ(j["user"]["vo"], "cms");
}

// A failed re-fetch (unparseable, or no "experiments" array) returns false and
// leaves the previously loaded registry intact.
TEST_F(Transfer, ScitagsJsonBadInputKeepsRegistry)
{
  ASSERT_TRUE(dec.LoadScitagsJson(
     R"({"experiments":[{"expId":2,"expName":"atlas","activities":[
        {"activityId":7,"activityName":"production"}]}]})"));
  EXPECT_FALSE(dec.LoadScitagsJson("not json at all"));
  EXPECT_FALSE(dec.LoadScitagsJson(R"({"no_experiments":true})"));

  feedUserMap();
  feedActivity(dec, 2, 7);
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["activity"]["experiment"], "atlas");   // unchanged
  EXPECT_EQ(j["user"]["vo"], "atlas");
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
  EXPECT_FALSE(j["client"].contains("site"));       // no &S= -> no client.site
}

TEST_F(Transfer, ClientSiteAdvertised)
{
  // A client that sets XRD_SITE/XRDSITE makes the server append &S= to the
  // login appinfo; the collector surfaces it as client.site.
  feedUserMapTail(dec, "&R=v5.6.1&x=xrdcp&y=&S=CERN-PROD&I=4");
  feedOpen();
  feedClose();

  ASSERT_FALSE(lastDoc.empty());
  json j = json::parse(lastDoc);
  EXPECT_EQ(j["client"]["site"], "CERN-PROD");
  EXPECT_EQ(j["client"]["version"], "v5.6.1");  // neighbouring fields intact
  EXPECT_EQ(j["app"]["name"], "xrdcp");
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
  EXPECT_EQ(j["type"], "transfer");          // a clean write produces a whole file
}

namespace
{
// A close carrying chosen byte totals and recFlag, for the fixture's file 100.
std::vector<unsigned char> closePkt(int64_t rd, int64_t rv, int64_t wr,
                                    uint8_t flag)
{
   W body;
   body.u32(100);              // fileID (matches feedOpen)
   body.u64((uint64_t)rd);     // Xfr.read
   body.u64((uint64_t)rv);     // Xfr.readv
   body.u64((uint64_t)wr);     // Xfr.write
   auto payload = todRec(kCloseT, 42);
   auto r = rec(0 /*isClose*/, flag, body.b);
   payload.insert(payload.end(), r.begin(), r.end());
   return packet('f', kStod, payload);
}
}

// A read that covered the whole file (read+readv >= size at open) is a transfer.
TEST_F(Transfer, WholeFileReadIsTransfer)
{
  feedUserMap();
  feedOpen();                                  // fsz = 123456
  auto pkt = closePkt(123456, 0, 0, 0);        // read exactly the whole file
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["type"], "transfer");
}

// A read that touched only part of the file is finer-grained data access.
TEST_F(Transfer, PartialReadIsAccess)
{
  feedUserMap();
  feedOpen();                                  // fsz = 123456
  auto pkt = closePkt(4096, 0, 0, 0);          // read a small slice
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["type"], "access");
  EXPECT_EQ(j["transfer"]["operation"], "read");   // shared schema otherwise
}

// readv bytes count toward whole-file coverage just like plain reads.
TEST_F(Transfer, WholeFileReadvIsTransfer)
{
  feedUserMap();
  feedOpen();
  auto pkt = closePkt(60000, 70000, 0, 0);     // 130000 >= 123456
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["type"], "transfer");
}

// A write cut short by a forced (disconnect-driven) close is partial access.
TEST_F(Transfer, ForcedWriteIsAccess)
{
  feedUserMap();
  feedOpen();
  auto pkt = closePkt(0, 0, 2097152, 0x01 /*forced*/);
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["type"], "access");
  EXPECT_EQ(j["transfer"]["operation"], "write");
}

// A close with no matching open has no known size, so it cannot be proven a
// whole-file transfer: it is reported as access.
TEST_F(Transfer, OrphanCloseIsAccess)
{
  auto pkt = closePkt(10485760, 0, 0, 0);      // no preceding open
  dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());

  json j = json::parse(lastDoc);
  EXPECT_EQ(j["type"], "access");
  EXPECT_EQ(j["transfer"]["open_seen"], false);
}

// A partial-access close increments the accesses counter, not transfers.
TEST_F(Transfer, AccessAggregatesIntoMetrics)
{
  XrdMetrics::Collector collector("");
  std::string sink;
  XrdMonDecode d([&](const std::string& s){ sink = s; }, nullptr,
                 false, false, false, false, &collector.subsystem(""));

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
  { W body; body.u32(100); body.u64(4096); body.u64(0); body.u64(0);
    auto payload = todRec(kCloseT, 42);
    auto r = rec(0, 0, body.b);                // partial read -> access
    payload.insert(payload.end(), r.begin(), r.end());
    auto pkt = packet('f', kStod, payload);
    d.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size()); }

  std::string out;
  XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_accesses_total{server=\"10.0.0.1:9930\"} 1"),
            std::string::npos) << out;
  EXPECT_EQ(out.find("xrootd_collector_transfers_total"), std::string::npos) << out;
}

// Feed a '=' server-ident record so srv.ident.host is populated for the
// LAN/WAN heuristic. The fixture decoder is keyed by ("10.0.0.1:9930", kStod).
static void feedIdent(XrdMonDecode& dec, const std::string& host)
{
   std::string info = "=/xrootd.1:2@" + host +
                      "\n&site=S&port=1094&inst=mgr&pgm=xrootd&ver=v6";
   W body; body.u32(0);
   std::vector<unsigned char> pl = body.b;
   pl.insert(pl.end(), info.begin(), info.end());
   auto pkt = packet('=', kStod, pl);
   dec.Process("10.0.0.1:9930", (const char*)pkt.data(), pkt.size());
}

TEST_F(Transfer, IsLocalWhenSameDomain)
{
  feedIdent(dec, "srv.example.org");   // client is wn.example.org -> same domain
  feedUserMap();
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  ASSERT_TRUE(j["transfer"].contains("is_local"));
  EXPECT_EQ(j["transfer"]["is_local"], true);
}

TEST_F(Transfer, IsRemoteWhenDifferentDomain)
{
  feedIdent(dec, "srv.other.net");     // different registered domain -> remote
  feedUserMap();
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  ASSERT_TRUE(j["transfer"].contains("is_local"));
  EXPECT_EQ(j["transfer"]["is_local"], false);
}

TEST_F(Transfer, IsLocalAbsentWithoutServerHost)
{
  // No '=' ident -> server host unknown -> heuristic cannot decide.
  feedUserMap();
  feedOpen();
  feedClose();

  json j = json::parse(lastDoc);
  EXPECT_FALSE(j["transfer"].contains("is_local"));
}

namespace
{
// Feed a u/open/close trio from a chosen UDP source so server.* reflects that
// sender (the fixture hardwires 10.0.0.1; loopback needs ::1).
void feedTransferFrom(XrdMonDecode& dec, const std::string& src)
{
  { W body; body.u32(7);
    std::string info = "xroot/alice.123:4@wn.example.org\n";
    std::vector<unsigned char> pl = body.b;
    pl.insert(pl.end(), info.begin(), info.end());
    auto pkt = packet('u', kStod, pl);
    dec.Process(src, (const char*)pkt.data(), pkt.size()); }
  { W body; body.u32(100); body.u64(123456); body.u32(7);
    std::string lfn = "/store/data/file.root"; body.raw(lfn); body.u8(0);
    auto payload = todRec(kOpenT, 42);
    auto r = rec(1, 0x01 | 0x02, body.b);
    payload.insert(payload.end(), r.begin(), r.end());
    auto pkt = packet('f', kStod, payload);
    dec.Process(src, (const char*)pkt.data(), pkt.size()); }
  { W body; body.u32(100); body.u64(1024); body.u64(0); body.u64(0);
    auto payload = todRec(kCloseT, 42);
    auto r = rec(0, 0, body.b);
    payload.insert(payload.end(), r.begin(), r.end());
    auto pkt = packet('f', kStod, payload);
    dec.Process(src, (const char*)pkt.data(), pkt.size()); }
}
}

// A co-located server reports from the loopback address; the collector should
// substitute the local FQDN rather than emit the literal "::1".
TEST(XrdMonCollect, LoopbackServerResolvesToLocalHost)
{
  std::string doc;
  XrdMonDecode dec([&](const std::string& d){ doc = d; });  // resolve on (default)
  feedTransferFrom(dec, "::1:9930");

  ASSERT_FALSE(doc.empty());
  json j = json::parse(doc);
  EXPECT_EQ(j["server"]["ip"], "::1");                 // numeric source preserved

  const char* me = XrdNetUtils::MyHostName();
  if (me && *me && std::string(me).find(':') == std::string::npos)
     {EXPECT_EQ(j["server"]["hostname"], me);
      EXPECT_EQ(j["server"]["name"], me);
      EXPECT_NE(j["server"]["name"], "::1");
     }
}

// With resolution disabled, the loopback source stays numeric.
TEST(XrdMonCollect, NoResolveKeepsLoopbackNumeric)
{
  std::string doc;
  XrdMonDecode dec([&](const std::string& d){ doc = d; });
  dec.SetResolveHosts(false);
  feedTransferFrom(dec, "::1:9930");

  json j = json::parse(doc);
  EXPECT_EQ(j["server"]["name"], "::1");
  EXPECT_FALSE(j["server"].contains("hostname"));
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
  XrdMetrics::Collector collector("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &collector.subsystem(""));

  for (uint8_t seq : {0, 1, 3, 4})   // 2 is missing -> one lost packet
     {auto p = userPkt(seq, seq);
      dec.Process("h:1", (const char*)p.data(), p.size());}

  EXPECT_EQ(dec.GetStats().lost, 1u);
  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
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

namespace
{
// Minimal 'u' user map for dictid 7 on a chosen sender.
void feedUser7(XrdMonDecode& dec, const std::string& src)
{
   W body; body.u32(7);
   std::string info = "xroot/alice.1:2@wn.example.org\n";
   std::vector<unsigned char> pl = body.b;
   pl.insert(pl.end(), info.begin(), info.end());
   auto pkt = packet('u', kStod, pl);
   dec.Process(src, (const char*)pkt.data(), pkt.size());
}

// An 'f' open record for a chosen fileID/lfn (user dictid 7).
void feedOpenId(XrdMonDecode& dec, const std::string& src, uint32_t fileID,
                const std::string& lfn)
{
   W body; body.u32(fileID); body.u64(123456); body.u32(7);
   body.raw(lfn); body.u8(0);
   auto payload = todRec(kOpenT, 42);
   auto r = rec(1 /*isOpen*/, 0x01 | 0x02 /*hasLFN|hasRW*/, body.b);
   payload.insert(payload.end(), r.begin(), r.end());
   auto pkt = packet('f', kStod, payload);
   dec.Process(src, (const char*)pkt.data(), pkt.size());
}

// An 'f' in-flight transfer (isXfr) snapshot for a fileID; touches the open.
void feedXfrId(XrdMonDecode& dec, const std::string& src, uint32_t fileID)
{
   W body; body.u32(fileID);
   auto payload = todRec(kCloseT, 42);
   auto r = rec(3 /*isXfr*/, 0, body.b);
   payload.insert(payload.end(), r.begin(), r.end());
   auto pkt = packet('f', kStod, payload);
   dec.Process(src, (const char*)pkt.data(), pkt.size());
}

// An 'f' close record for a fileID (no OPS, minimal byte totals).
void feedCloseId(XrdMonDecode& dec, const std::string& src, uint32_t fileID)
{
   W body; body.u32(fileID); body.u64(1024); body.u64(0); body.u64(0);
   auto payload = todRec(kCloseT, 42);
   auto r = rec(0 /*isClose*/, 0, body.b);
   payload.insert(payload.end(), r.begin(), r.end());
   auto pkt = packet('f', kStod, payload);
   dec.Process(src, (const char*)pkt.data(), pkt.size());
}
}

// A long-lived but still-active open (kept warm by its in-flight xfr snapshots)
// must survive eviction while cold, stranded opens are dropped first.
TEST(XrdMonCollect, WarmEntrySurvivesEviction)
{
  std::string doc;
  XrdMonDecode dec([&](const std::string& d){ doc = d; });
  dec.SetMaxBytes(2000);                         // room for ~16 open entries
  feedUser7(dec, "h:1");
  feedOpenId(dec, "h:1", 1, "/warm/file.root");  // the long-lived, active open

  for (uint32_t id = 100; id < 200; id++)        // a flood of cold opens
     {feedOpenId(dec, "h:1", id, "/cold/file.root");
      feedXfrId(dec, "h:1", 1);                   // keep the warm open at the MRU
     }
  EXPECT_GT(dec.GetStats().evicted, 0u);         // the budget was enforced

  feedCloseId(dec, "h:1", 1);                     // the warm open still joins
  json jw = json::parse(doc);
  EXPECT_EQ(jw["transfer"]["open_seen"], true);
  EXPECT_EQ(jw["file"]["lfn"], "/warm/file.root");

  feedCloseId(dec, "h:1", 100);                   // an early cold open was evicted
  json jc = json::parse(doc);
  EXPECT_EQ(jc["transfer"]["open_seen"], false);
}

// The resident state stays within the byte budget no matter how many distinct
// opens arrive without their closes.
TEST(XrdMonCollect, MemoryStaysUnderBudget)
{
  XrdMonDecode dec([](const std::string&){});
  dec.SetMaxBytes(4000);
  feedUser7(dec, "h:1");
  for (uint32_t id = 1; id <= 500; id++)
     feedOpenId(dec, "h:1", id, "/store/data/some/long/path/file.root");

  EXPECT_LE(dec.ResidentBytes(), 4000u);
  EXPECT_GT(dec.GetStats().evicted, 0u);
}

// On the normal path every open is released by its close: with session
// correlation off (the default) resident memory returns to the baseline and
// nothing is evicted.
TEST(XrdMonCollect, CloseReleasesMemory)
{
  XrdMonDecode dec([](const std::string&){});   // unbounded, sessions off
  feedUser7(dec, "h:1");
  std::size_t base = dec.ResidentBytes();        // just the user entry

  for (uint32_t id = 1; id <= 50; id++)
     feedOpenId(dec, "h:1", id, "/store/data/file.root");
  EXPECT_GT(dec.ResidentBytes(), base);          // opens are charged

  for (uint32_t id = 1; id <= 50; id++)
     feedCloseId(dec, "h:1", id);
  EXPECT_EQ(dec.ResidentBytes(), base);          // each close releases its open
  EXPECT_EQ(dec.GetStats().evicted, 0u);
}

// With session correlation on, closes fold a bounded record into the user's
// rollup, so resident memory stays bounded by the recent-file cap across many
// open/close cycles (it does not grow without limit).
TEST(XrdMonCollect, SessionRollupStaysBounded)
{
  XrdMonDecode dec([](const std::string&){});   // unbounded budget
  dec.SetEmitSessions(true);
  feedUser7(dec, "h:1");

  for (uint32_t id = 1; id <= 80; id++)          // fill past the recent-file cap
     {feedOpenId(dec, "h:1", id, "/store/data/file.root");
      feedCloseId(dec, "h:1", id);}
  std::size_t capped = dec.ResidentBytes();

  for (uint32_t id = 81; id <= 200; id++)        // many more cycles
     {feedOpenId(dec, "h:1", id, "/store/data/file.root");
      feedCloseId(dec, "h:1", id);}
  EXPECT_LE(dec.ResidentBytes(), capped);        // capped: no unbounded growth
  EXPECT_EQ(dec.GetStats().evicted, 0u);
}

// An incarnation idle past the server TTL is reclaimed whole; a freshly-seen
// incarnation is left alone.
TEST(XrdMonCollect, IdleServerReaped)
{
  XrdMonDecode dec([](const std::string&){});
  dec.SetServerTTL(100);

  feedTransferFrom(dec, "10.0.0.1:9930");         // server A (leaves a user entry)
  EXPECT_GT(dec.ResidentBytes(), 0u);

  dec.ReapServers(time(nullptr) + 1000);          // A is now well past its TTL
  EXPECT_EQ(dec.GetStats().reaped, 1u);
  EXPECT_EQ(dec.ResidentBytes(), 0u);             // A's state was reclaimed

  feedTransferFrom(dec, "10.0.0.2:9930");         // server B, just seen
  EXPECT_GT(dec.ResidentBytes(), 0u);
  dec.ReapServers(time(nullptr));                 // B is fresh -> survives
  EXPECT_EQ(dec.GetStats().reaped, 1u);
  EXPECT_GT(dec.ResidentBytes(), 0u);
}

// With no budget, no count cap and no TTL, behaviour is unchanged: everything
// is retained and nothing is evicted.
TEST(XrdMonCollect, UnboundedKeepsEverything)
{
  XrdMonDecode dec([](const std::string&){});
  for (uint32_t id = 1; id <= 200; id++)
     {auto p = userPkt(id, (uint8_t)id);
      dec.Process("h:1", (const char*)p.data(), p.size());}

  EXPECT_EQ(dec.GetStats().evicted, 0u);
  EXPECT_GT(dec.ResidentBytes(), 0u);
}

TEST(XrdMonCollect, FrmStageAndPurge)
{
  XrdMetrics::Collector collector("");
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); }, nullptr,
                   false, false, false, false, &collector.subsystem(""));

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
  EXPECT_EQ(x["user"]["name"], "alice");
  EXPECT_EQ(x["file"]["lfn"], "/store/data/f.root");
  json p = json::parse(docs[1]);
  EXPECT_EQ(p["operation"], "purge");
  EXPECT_EQ(p["file"]["size"], 1048576);
  EXPECT_EQ(dec.GetStats().frmEvents, 2u);

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_frm_total{server=\"h:1\",op=\"purge\"} 1"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_frm_purge_bytes_total{server=\"h:1\"} 1048576"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, SessionDiscAndActiveGauge)
{
  XrdMetrics::Collector collector("");
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); }, nullptr,
                   false, false, false, false, &collector.subsystem(""));
  dec.SetEmitSessions(true);

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

  ASSERT_EQ(docs.size(), 1u);                  // the session document
  json j = json::parse(docs[0]);
  EXPECT_EQ(j["type"], "session");
  EXPECT_EQ(j["user"]["name"], "bob");
  EXPECT_EQ(j["client"]["host"], "cli.example.org");
  // The file was opened but never closed, so the session rollup counts no files.
  ASSERT_TRUE(j.contains("session"));
  EXPECT_EQ(j["session"]["files"], 0);
  EXPECT_FALSE(j["session"].contains("recent_files"));
  EXPECT_EQ(dec.GetStats().discs, 1u);

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_sessions_total{server=\"h:1\"} 1"),
            std::string::npos) << out;
  // One file opened, none closed -> active gauge is 1.
  EXPECT_NE(out.find("xrootd_collector_active_transfers{server=\"h:1\"} 1"),
            std::string::npos) << out;
}

namespace
{
// A 'u' user map for an arbitrary dictid.
void feedUserN(XrdMonDecode& dec, const std::string& src, uint32_t dictid)
{
   W body; body.u32(dictid);
   std::string info = "xroot/u" + std::to_string(dictid) + ".1:2@wn.example.org\n";
   std::vector<unsigned char> pl = body.b;
   pl.insert(pl.end(), info.begin(), info.end());
   auto pkt = packet('u', kStod, pl);
   dec.Process(src, (const char*)pkt.data(), pkt.size());
}

// Open then close one file (fileID/user/lfn) moving `rd` read bytes / `wr` write
// bytes, with file size `fsz` captured at open.
void openClose(XrdMonDecode& dec, const std::string& src, uint32_t fileID,
               uint32_t user, int64_t fsz, int64_t rd, int64_t wr,
               const std::string& lfn)
{
   { W body; body.u32(fileID); body.u64((uint64_t)fsz); body.u32(user);
     body.raw(lfn); body.u8(0);
     auto payload = todRec(kOpenT, 42);
     auto r = rec(1 /*isOpen*/, 0x01 | 0x02, body.b);
     payload.insert(payload.end(), r.begin(), r.end());
     auto pkt = packet('f', kStod, payload);
     dec.Process(src, (const char*)pkt.data(), pkt.size()); }
   { W body; body.u32(fileID); body.u64((uint64_t)rd); body.u64(0);
     body.u64((uint64_t)wr);
     auto payload = todRec(kCloseT, 42);
     auto r = rec(0 /*isClose*/, 0, body.b);
     payload.insert(payload.end(), r.begin(), r.end());
     auto pkt = packet('f', kStod, payload);
     dec.Process(src, (const char*)pkt.data(), pkt.size()); }
}

// A session disconnect (isDisc) for a user dictid.
void feedDisc(XrdMonDecode& dec, const std::string& src, uint32_t user)
{
   W disc; disc.u32(user);
   auto payload = todRec(kCloseT, 42);
   auto dr = rec(4 /*isDisc*/, 0, disc.b);
   payload.insert(payload.end(), dr.begin(), dr.end());
   auto pkt = packet('f', kStod, payload);
   dec.Process(src, (const char*)pkt.data(), pkt.size());
}
}

// A session's closed files are aggregated into the 'session' document at
// disconnect: running totals plus a recent-file list. The per-file transfer/
// access documents are still emitted independently.
TEST(XrdMonCollect, SessionAggregatesFileActivity)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); });
  dec.SetEmitSessions(true);

  feedUserN(dec, "h:1", 7);
  openClose(dec, "h:1", 1, 7, 1000,  1000, 0, "/a.root");   // whole read -> transfer
  openClose(dec, "h:1", 2, 7, 1000,  1000, 0, "/b.root");   // whole read -> transfer
  openClose(dec, "h:1", 3, 7, 100000, 4096, 0, "/c.root");  // partial   -> access
  feedDisc(dec, "h:1", 7);

  // Three close documents, then the session document.
  ASSERT_EQ(docs.size(), 4u);
  json j = json::parse(docs.back());
  EXPECT_EQ(j["type"], "session");
  EXPECT_EQ(j["user"]["name"], "u7");
  EXPECT_EQ(j["session"]["files"], 3);
  EXPECT_EQ(j["session"]["transfers"], 2);
  EXPECT_EQ(j["session"]["accesses"], 1);
  EXPECT_EQ(j["session"]["read_bytes"], 1000 + 1000 + 4096);
  EXPECT_FALSE(j["session"].contains("write_bytes"));
  ASSERT_TRUE(j["session"].contains("recent_files"));
  ASSERT_EQ(j["session"]["recent_files"].size(), 3u);
  EXPECT_EQ(j["session"]["recent_files"][2]["lfn"], "/c.root");
  EXPECT_EQ(j["session"]["recent_files"][2]["type"], "access");
  EXPECT_EQ(j["session"]["recent_files"][0]["type"], "transfer");

  // The individual close documents were still emitted (not replaced).
  EXPECT_EQ(json::parse(docs[0])["type"], "transfer");
  EXPECT_EQ(json::parse(docs[2])["type"], "access");
}

// The recent-file list is capped while the running totals cover every file.
TEST(XrdMonCollect, SessionRecentFilesCapped)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); });
  dec.SetEmitSessions(true);

  feedUserN(dec, "h:1", 7);
  for (uint32_t i = 1; i <= 100; i++)
     openClose(dec, "h:1", i, 7, 1000, 1000, 0, "/data/file.root");
  feedDisc(dec, "h:1", 7);

  json j = json::parse(docs.back());
  EXPECT_EQ(j["type"], "session");
  EXPECT_EQ(j["session"]["files"], 100);              // every file counted
  EXPECT_EQ(j["session"]["transfers"], 100);
  EXPECT_EQ(j["session"]["recent_files"].size(), 64u);// list bounded (cap)
}

// Closes are folded into the owning user's session only; two concurrent users
// do not co-mingle their file activity.
TEST(XrdMonCollect, SessionsDoNotCrossUsers)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); });
  dec.SetEmitSessions(true);

  feedUserN(dec, "h:1", 7);
  feedUserN(dec, "h:1", 8);
  openClose(dec, "h:1", 1, 7, 1000, 1000, 0, "/seven.root");
  openClose(dec, "h:1", 2, 8, 1000, 1000, 0, "/eight.root");
  openClose(dec, "h:1", 3, 8, 1000, 1000, 0, "/eight2.root");
  feedDisc(dec, "h:1", 7);

  json j = json::parse(docs.back());
  EXPECT_EQ(j["user"]["name"], "u7");
  EXPECT_EQ(j["session"]["files"], 1);                // only user 7's one file
  ASSERT_EQ(j["session"]["recent_files"].size(), 1u);
  EXPECT_EQ(j["session"]["recent_files"][0]["lfn"], "/seven.root");
}

// Session correlation is opt-in: with it off (the default) a disconnect emits
// no session document and no per-session rollup is accumulated, so the closes
// release their memory just like the non-session path.
TEST(XrdMonCollect, SessionsDisabledByDefault)
{
  std::vector<std::string> docs;
  XrdMonDecode dec([&](const std::string& d){ docs.push_back(d); });
  // No SetEmitSessions(true): sessions are disabled.

  feedUserN(dec, "h:1", 7);
  std::size_t base = dec.ResidentBytes();
  openClose(dec, "h:1", 1, 7, 1000, 1000, 0, "/a.root");
  openClose(dec, "h:1", 2, 7, 1000, 1000, 0, "/b.root");
  feedDisc(dec, "h:1", 7);

  // Two close documents were emitted; the disconnect produced nothing.
  EXPECT_EQ(docs.size(), 2u);
  for (const auto& d : docs)
     EXPECT_NE(json::parse(d)["type"], "session");
  EXPECT_EQ(dec.GetStats().discs, 1u);             // disconnect still counted
  EXPECT_EQ(dec.ResidentBytes(), base);            // no rollup retained
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
  EXPECT_EQ(j["server"]["site"], "T1_DE_KIT");
  EXPECT_EQ(j["server"]["hostname"], "srv.example.org");
  EXPECT_EQ(j["server"]["instance"], "manager");
  EXPECT_EQ(j["server"]["program"], "xrootd");
  EXPECT_EQ(j["server"]["version"], "v6.1.0");
  EXPECT_EQ(j["server"]["port"], 1094);
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
  XrdMetrics::Collector collector("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &collector.subsystem(""));

  // First snapshot establishes the baseline (no counter movement).
  auto p1 = gPacket('O', "{\"event\":\"oss_stats\",\"reads\":100,\"writes\":10,"
                         "\"slow_reads\":4}");
  dec.Process("h:1", (const char*)p1.data(), p1.size());
  // Second snapshot: +50 reads, +5 writes, +1 slow_read.
  auto p2 = gPacket('O', "{\"event\":\"oss_stats\",\"reads\":150,\"writes\":15,"
                         "\"slow_reads\":5}");
  dec.Process("h:1", (const char*)p2.data(), p2.size());

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_oss_ops_total{server=\"h:1\",op=\"read\"} 50"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_oss_ops_total{server=\"h:1\",op=\"write\"} 5"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_oss_slow_ops_total{server=\"h:1\",op=\"read\"} 1"),
            std::string::npos) << out;
}

TEST(XrdMonCollect, GStreamPfcAndTpcMetrics)
{
  XrdMetrics::Collector collector("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &collector.subsystem(""));

  auto pfc = gPacket('C', "{\"event\":\"file_close\",\"b_hit\":2048,"
                          "\"b_miss\":1024,\"b_prefetch\":512}");
  dec.Process("h:1", (const char*)pfc.data(), pfc.size());

  auto tpc = gPacket('P', "{\"TPC\":\"xroot\",\"Xeq\":{\"RC\":0,\"Type\":\"pull\"},"
                          "\"Size\":1048576}");
  dec.Process("h:1", (const char*)tpc.data(), tpc.size());

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
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
  XrdMetrics::Collector collector("");
  XrdMonDecode dec([](const std::string&){}, nullptr,
                   false, false, false, false, &collector.subsystem(""));

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

  std::string out; XrdMetrics::PrometheusTextSerializer ser(out); collector.serialize(ser);
  EXPECT_NE(out.find("xrootd_collector_throttle_io_total{server=\"h:1\"} 30"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_throttle_io_active{server=\"h:1\"} 4"),
            std::string::npos) << out;
  EXPECT_NE(out.find("xrootd_collector_http_requests_total{server=\"h:1\",method=\"GET\",status=\"200\"} 5"),
            std::string::npos) << out;
}
