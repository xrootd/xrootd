//------------------------------------------------------------------------------
// Unit tests for the transfer map of the f-stream (fstat) monitoring, and for
// the size of the monitoring datagrams.
//
// The monitor runs with the "xfr" option, so each open file gets a slot in the
// transfer map, and with the largest buffer sizes that the configuration
// accepts. See XrdXrootdMonTestUtils.hh for how the tests get the datagrams.
//
// The transfer map is global to the process, so each test closes all files
// that it opens.
//------------------------------------------------------------------------------

#include "XrdXrootdMonTestUtils.hh"

#include "XrdXrootd/XrdXrootdFileStats.hh"
#include "XrdXrootd/XrdXrootdMonFMap.hh"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace XrdXrootdMonTest;

namespace {

// The largest buffer size that the configuration accepts
const int maxBsz = 65535;

// The number of files that the transfer map holds: Open() does not use the
// last of the maps.
const int mapFiles = (XrdXrootdMonFMap::mapNum - 1) * XrdXrootdMonFMap::fmSize;

//------------------------------------------------------------------------------
//! A set of files that are open on the f-stream. The destructor closes them
//! as the file table does, so that the next test gets an empty map.
//------------------------------------------------------------------------------

class OpenFiles
{
public:
  explicit OpenFiles(int n) : files(n)
  {
    for (auto &f : files) {
      f.Init();
      XrdXrootdMonFile::Open(&f, "/f", 0x12345678, false);
    }
  }

  ~OpenFiles()
  {
    for (auto &f : files)
      Close(f);
  }

  static void Close(XrdXrootdFileStats &f)
  {
    if (f.MonEnt != -1)
      XrdXrootdMonFile::Close(&f, false);
  }

  std::vector<XrdXrootdFileStats> files;
};

// The map of a cookie
int MapOf(const XrdXrootdFileStats &f)
{
  return (f.MonEnt & 0xffff) >> XrdXrootdMonFMap::fmShft;
}

using MonXfrTest = MonTest;

// A cookie of -1 tells the callers that the file is not on the f-stream, so it
// gets no close record. The cookie of the last slot of the last map is 0xffff,
// which is -1 as a short. So no file may get it, also when the map is full.
TEST_F(MonXfrTest, NoFileGetsTheCookieMinusOne)
{
  OpenFiles open(mapFiles + XrdXrootdMonFMap::fmSize);
  auto &f = open.files;
  EXPECT_EQ(std::count_if(f.begin(), f.end(),
                          [](const auto &s) { return s.MonEnt == -1; }), 0);
}

// The map holds a fixed number of files. They all get a real slot.
TEST_F(MonXfrTest, FilesUpToTheMapSizeGetASlot)
{
  const int lastMap = XrdXrootdMonFMap::mapNum - 1;
  OpenFiles open(mapFiles);
  auto &f = open.files;
  EXPECT_EQ(std::count_if(f.begin(), f.end(),
                          [=](const auto &s) { return MapOf(s) < lastMap; }),
            mapFiles);
}

// A file that gets no slot, because the map is full, must still get its close
// record
TEST_F(MonXfrTest, FileWithoutASlotGetsItsCloseRecord)
{
  OpenFiles open(mapFiles + 1);
  auto &extra = open.files.back();
  env->rcv.Drain(); // the open records

  const kXR_unt32 fileID = extra.FileID;
  OpenFiles::Close(extra);
  Flush();
  auto closes = Records(env->rcv.Drain(), XrdXrootdMonFileHdr::isClose);
  EXPECT_EQ(std::count_if(closes.begin(), closes.end(),
                          [=](const auto &r) { return FileID(r) == fileID; }),
            1);
}

// Each file of a full map with I/O gets an xfr record. Next() used to stop
// before the last slot of the map, so that file got none.
TEST_F(MonXfrTest, EveryFileOfAMapGetsAnXfrRecord)
{
  OpenFiles open(XrdXrootdMonFMap::fmSize);
  std::set<kXR_unt32> fileIDs;
  for (auto &f : open.files) {
    f.xfrXeq = 1;
    fileIDs.insert(f.FileID);
  }
  env->rcv.Drain(); // the open records

  Flush(); // with "xfr 1", each flush also reports the I/O
  std::set<kXR_unt32> seen;
  for (const auto &r : Records(env->rcv.Drain(), XrdXrootdMonFileHdr::isXfr))
    if (fileIDs.count(FileID(r)))
      seen.insert(FileID(r));
  EXPECT_EQ(seen.size(), fileIDs.size());
}

// The configuration accepts an fstat buffer of 65535 bytes, but a datagram
// larger than maxDgram cannot be sent. Such a buffer was lost each time that
// it was full, so its records never arrived. Disconnect records are 8 bytes
// long, so they fill the buffer up to 65528 bytes.
TEST_F(MonXfrTest, FullFStreamBufferArrives)
{
  const int nDisc = 10000; // 80000 bytes, more than one buffer
  for (int i = 0; i < nDisc; i++)
    XrdXrootdMonFile::Disc(i + 1);
  Flush();
  auto discs = Records(env->rcv.Drain(), XrdXrootdMonFileHdr::isDisc);
  EXPECT_EQ(discs.size(), static_cast<size_t>(nDisc));
}

// The same applies to the buffer of the file stream (mbuff), which goes out
// when it is full
TEST_F(MonXfrTest, FullTraceBufferArrives)
{
  ASSERT_NE(XrdXrootdMonitor::altMon, nullptr);
  const int nOpen = maxBsz / static_cast<int>(sizeof(XrdXrootdMonTrace));
  for (int i = 0; i < nOpen; i++)
    XrdXrootdMonitor::altMon->Open(i + 1, 0);

  auto traces = Stream(env->rcv.Drain(), XROOTD_MON_MAPTRCE);
  ASSERT_FALSE(traces.empty()) << "the full buffer did not arrive";
  for (const auto &d : traces)
    EXPECT_LE(d.size(), static_cast<size_t>(XrdXrootdMonitor::maxDgram));
}

} // namespace

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  MonConfig cfg;
  cfg.fsOpts = XROOTD_MON_FSXFR;
  cfg.fstatBsz = maxBsz;
  cfg.monBsz = maxBsz;
  env = new MonEnv(cfg);
  ::testing::AddGlobalTestEnvironment(env);
  return RUN_ALL_TESTS();
}
