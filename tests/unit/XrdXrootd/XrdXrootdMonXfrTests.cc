//------------------------------------------------------------------------------
// Unit tests for the transfer map of the f-stream (fstat) monitoring.
//
// The monitor runs with the "xfr" option, so each open file gets a slot in the
// transfer map. See XrdXrootdMonTestUtils.hh for how the tests get the
// datagrams.
//
// The transfer map is global to the process, so each test closes all files
// that it opens.
//------------------------------------------------------------------------------

#include "XrdXrootdMonTestUtils.hh"

#include "XrdXrootd/XrdXrootdFileStats.hh"
#include "XrdXrootd/XrdXrootdMonFMap.hh"

#include <algorithm>
#include <string>
#include <vector>

using namespace XrdXrootdMonTest;

namespace {

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

} // namespace

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  MonConfig cfg;
  cfg.fsOpts = XROOTD_MON_FSXFR;
  env = new MonEnv(cfg);
  ::testing::AddGlobalTestEnvironment(env);
  return RUN_ALL_TESTS();
}
