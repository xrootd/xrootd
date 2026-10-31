//------------------------------------------------------------------------------
// Unit tests for the bounds of the monitoring records: the f-stream (fstat)
// records, the dictionary map records and the redirect records.
//
// The monitor runs without the "xfr" option and with the smallest fstat
// buffer that the configuration accepts. See XrdXrootdMonTestUtils.hh for how
// the tests get the datagrams.
//------------------------------------------------------------------------------

#include "XrdXrootdMonTestUtils.hh"

#include "XrdXrootd/XrdXrootdFileStats.hh"

#include <string>
#include <vector>

using namespace XrdXrootdMonTest;

namespace {

// The smallest fstat buffer that the configuration accepts. A maximum length
// path (4096 bytes) does not fit an open record in this buffer.
const int fstatBsz = 1024;

// A path that is much longer than the buffer
const std::string longPath = "/" + std::string(3999, 'x');

// A path that fits an open record in the buffer
const std::string shortPath = "/" + std::string(899, 'y');

// The fixed part of an open record before the lfn: header, file size and user
const size_t opnFixSz = sizeof(XrdXrootdMonFileHdr) + sizeof(long long)
                      + sizeof(kXR_unt32);

// Get the lfn of each open record whose lfn starts with the given character
std::vector<std::string> OpenLfns(const std::vector<std::string> &dgrams,
                                  char first)
{
  std::vector<std::string> lfns;
  for (const auto &r : Records(dgrams, XrdXrootdMonFileHdr::isOpen)) {
    if (!(r.flag & XrdXrootdMonFileHdr::hasLFN) || r.data.size() <= opnFixSz)
      continue;
    const std::string room = r.data.substr(opnFixSz);
    const std::string lfn = room.substr(0, room.find('\0'));
    if (lfn.size() > 1 && lfn[1] == first)
      lfns.push_back(lfn);
  }
  return lfns;
}

// Send open, close and disconnect records. Each open record with the long
// path almost fills the buffer, so almost every call sends a datagram.
std::vector<std::string> DriveMonitor(const std::string &path, int rounds)
{
  for (int i = 0; i < rounds; i++) {
    XrdXrootdFileStats stats;
    stats.Init();
    stats.fSize = 1234;
    XrdXrootdMonFile::Open(&stats, path.c_str(), 0x12345678, i & 1);
    if (stats.MonEnt != -1)
      XrdXrootdMonFile::Close(&stats, false);
    XrdXrootdMonFile::Disc(0x87654321);
  }
  return env->rcv.Drain();
}

// Get the text of each redirect entry: "<host>:<path>", with the null bytes
// that follow it in its slots
std::vector<std::string> RedirectTexts(const std::vector<std::string> &dgrams)
{
  const size_t slotSz = sizeof(XrdXrootdMonRedir);
  std::vector<std::string> texts;
  for (const auto &d : Stream(dgrams, XROOTD_MON_MAPREDR)) {
    size_t off = sizeof(XrdXrootdMonHeader) + sizeof(kXR_int64); // header, sID
    while (off + slotSz <= d.size()) {
      XrdXrootdMonRedir ent;
      memcpy(&ent, d.data() + off, sizeof(ent));
      off += slotSz;
      if (!(ent.arg0.rdr.Type & XROOTD_MON_REDIRECT))
        continue; // a timing mark
      const size_t tLen = ent.arg0.rdr.Dent * slotSz;
      texts.push_back(d.substr(off, tLen));
      off += tLen;
    }
  }
  return texts;
}

using MonFileTest = MonTest;

// An open record with a path longer than the buffer used to write past the end
// of the buffer.
TEST_F(MonFileTest, DatagramsNeverExceedTheBuffer)
{
  auto dgrams = FStream(DriveMonitor(longPath, 40));
  ASSERT_FALSE(dgrams.empty()) << "no f-stream datagrams arrived";

  for (const auto &d : dgrams)
    EXPECT_LE(d.size(), static_cast<size_t>(fstatBsz))
      << "datagram larger than fbsz";
}

// A record size that goes past the end of the datagram makes a collector
// reject the datagram
TEST_F(MonFileTest, RecordsTileEachDatagram)
{
  auto dgrams = FStream(DriveMonitor(longPath, 40));
  ASSERT_FALSE(dgrams.empty());

  for (const auto &d : dgrams) {
    std::vector<Record> recs;
    EXPECT_TRUE(WalkRecords(d, recs)) << "records do not fill the datagram";
  }
}

// A long lfn is truncated. What remains must be the start of the real path.
// For fbsz 1024, the record can use 1023 - 32 bytes (buffer end minus the
// datagram header and the time record), and its size is a multiple of 8, so
// 984 bytes. Its fixed part is 20 bytes and the lfn needs a null byte, so the
// longest lfn is 963 bytes.
TEST_F(MonFileTest, LongLfnIsAPrefixOfThePath)
{
  const size_t maxLfn = 963;
  auto lfns = OpenLfns(DriveMonitor(longPath, 40), 'x');
  ASSERT_FALSE(lfns.empty()) << "no open records with the long path";

  for (const auto &lfn : lfns) {
    EXPECT_LE(lfn.size(), maxLfn);
    EXPECT_EQ(lfn, longPath.substr(0, lfn.size()));
  }
}

// A path that fits the buffer must not be truncated
TEST_F(MonFileTest, ShortLfnIsComplete)
{
  auto lfns = OpenLfns(DriveMonitor(shortPath, 40), 'y');
  ASSERT_FALSE(lfns.empty()) << "no open records with the short path";

  for (const auto &lfn : lfns)
    EXPECT_EQ(lfn, shortPath);
}

// Each record is a multiple of 8 bytes long, so that the 64-bit fields of the
// next record stay aligned. Paths of 8 consecutive lengths give each possible
// padding of the lfn.
TEST_F(MonFileTest, RecordSizesAreMultiplesOf8)
{
  int nOpen = 0, nBad = 0;
  for (int n = 1; n <= 8; n++)
    for (const auto &r : Records(DriveMonitor("/" + std::string(n, 'z'), 10))) {
      if (r.type == XrdXrootdMonFileHdr::isOpen)
        nOpen++;
      if (r.data.size() % 8)
        nBad++;
    }
  ASSERT_GT(nOpen, 0) << "no open records arrived";
  EXPECT_EQ(nBad, 0) << "records with a size that is not a multiple of 8";
}

// The monitor is configured without "xfr", so no file gets a slot in the
// transfer map. Close records must still arrive.
TEST_F(MonFileTest, CloseRecordsAppearWithoutXfr)
{
  auto dgrams = DriveMonitor(longPath, 40);
  auto closes = Records(dgrams, XrdXrootdMonFileHdr::isClose);
  EXPECT_FALSE(closes.empty()) << "no close records without the xfr option";
}

// The callers send a close record only when MonEnt is not -1. So Close() must
// set it to -1 also for a file without a slot, or a second call sends a second
// close record.
TEST_F(MonFileTest, CloseMarksTheFileAsClosed)
{
  XrdXrootdFileStats stats;
  stats.Init();
  XrdXrootdMonFile::Open(&stats, shortPath.c_str(), 0x12345678, false);
  XrdXrootdMonFile::Close(&stats, false);
  EXPECT_EQ(stats.MonEnt, -1);
}

// Send a path map record for a user, and return the map records that arrive
std::vector<std::string> MapPath(const char *path)
{
  XrdXrootdMonitor::User user;
  user.Name = strdup("user.1:1@testhost");
  user.Len = strlen(user.Name);
  user.MapPath(path);
  return Stream(env->rcv.Drain(), XROOTD_MON_MAPPATH);
}

// The dictionary map record is a fixed-size object. A path longer than the
// object used to make the record size larger than the object, so the datagram
// carried stack memory after the record.
TEST_F(MonFileTest, MapRecordHoldsItsLength)
{
  auto maps = MapPath(longPath.c_str());
  ASSERT_EQ(maps.size(), 1u);

  XrdXrootdMonHeader h;
  memcpy(&h, maps[0].data(), sizeof(h));
  EXPECT_LE(ntohs(h.plen), sizeof(XrdXrootdMonMap));
}

// The map record carries "<user>\n<path>" and nothing after it
TEST_F(MonFileTest, MapRecordCarriesUserAndPath)
{
  auto maps = MapPath(shortPath.c_str());
  ASSERT_EQ(maps.size(), 1u);

  const size_t infoOff = sizeof(XrdXrootdMonHeader) + sizeof(kXR_unt32);
  EXPECT_EQ(maps[0].substr(infoOff), "user.1:1@testhost\n" + shortPath);
}

// A redirect entry holds "<host>:<path>" in the slots after it. A path longer
// than 1024 bytes used to fill the slots with no null byte, so a collector
// read the next entries as part of the path.
TEST_F(MonFileTest, RedirectPathEndsWithANullByte)
{
  // Each entry takes 66 slots of the 32 KiB buffer, so the buffer is sent
  // after about 30 entries
  for (int i = 0; i < 40; i++)
    XrdXrootdMonitor::Redirect(1, "rdrhost", 1094, XROOTD_MON_OPENR,
                               longPath.c_str());
  auto texts = RedirectTexts(env->rcv.Drain());
  ASSERT_FALSE(texts.empty()) << "no redirect datagrams arrived";

  const std::string expected = "rdrhost:" + longPath.substr(0, 1024);
  for (const auto &t : texts)
    EXPECT_EQ(t.substr(0, t.find('\0')), expected);
}

// CopyText() truncates the text so that it and its null byte fit the room
TEST(MonCopyText, TruncatesToTheRoom)
{
  char buff[8];
  EXPECT_EQ(XrdXrootdMonitor::CopyText(buff, 4, "abcdefghij", 10), 3);
  EXPECT_STREQ(buff, "abc");
}

// CopyText() copies only the length that the caller gives
TEST(MonCopyText, CopiesTheGivenLength)
{
  char buff[8];
  EXPECT_EQ(XrdXrootdMonitor::CopyText(buff, sizeof(buff), "abcdefghij", 2), 2);
  EXPECT_STREQ(buff, "ab");
}

// CopyText() fills the rest of the room with null bytes
TEST(MonCopyText, NullPadsTheRoom)
{
  char buff[16];
  memset(buff, 'x', sizeof(buff));
  XrdXrootdMonitor::CopyText(buff, sizeof(buff), "abc", 3);
  EXPECT_EQ(std::string(buff + 3, sizeof(buff) - 3),
            std::string(sizeof(buff) - 3, '\0'));
}

} // namespace

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  MonConfig cfg;
  cfg.fsOpts = XROOTD_MON_FSLFN | XROOTD_MON_FSOPS | XROOTD_MON_FSSSQ;
  cfg.fstatBsz = fstatBsz;
  cfg.destMode = XROOTD_MON_FSTA | XROOTD_MON_FILE | XROOTD_MON_REDR;
  env = new MonEnv(cfg);
  ::testing::AddGlobalTestEnvironment(env);
  return RUN_ALL_TESTS();
}
