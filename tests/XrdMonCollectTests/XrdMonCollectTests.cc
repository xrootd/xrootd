//------------------------------------------------------------------------------
// Unit tests for the xrdmoncollect decoder/correlator. Packets are hand-built
// in the on-the-wire (network byte order) layout described by
// src/XrdXrootd/XrdXrootdMonData.hh.
//------------------------------------------------------------------------------

#include <cstring>
#include <string>
#include <vector>

#include "XrdApps/XrdMonCollect/XrdMonDecode.hh"
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
  EXPECT_EQ(j["lfn"], "/store/data/file.root");
  EXPECT_EQ(j["user"], "alice");
  EXPECT_EQ(j["protocol"], "xroot");
  EXPECT_EQ(j["client_host"], "wn.example.org");
  EXPECT_EQ(j["read_bytes"], 10485760);
  EXPECT_EQ(j["read_ops"], 320);
  EXPECT_EQ(j["read_max"], 1048576);
  EXPECT_EQ(j["open_seen"], true);
  EXPECT_EQ(j["file_size"], 123456);
  EXPECT_EQ(j["duration_s"], kCloseT - kOpenT);
  EXPECT_EQ(j["server_id"], 42);

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
  EXPECT_EQ(j["open_seen"], false);
  EXPECT_EQ(j["read_bytes"], 10485760);
  EXPECT_FALSE(j.contains("lfn"));
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
  auto pkt = packet('t', kStod, std::vector<unsigned char>(16, 0));
  EXPECT_TRUE(dec.Process("1.2.3.4:5", (const char*)pkt.data(), pkt.size()));
  EXPECT_EQ(dec.GetStats().unknown, 1u);
}
