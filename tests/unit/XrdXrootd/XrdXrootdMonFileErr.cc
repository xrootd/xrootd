//------------------------------------------------------------------------------
// Unit tests for the f-stream error records of XrdXrootdMonFile: the isError
// record of a failed open, and the error block of a close record (hasERR). The
// monitor runs with the "err", "lfn" and "ops" options. A second executable
// runs the same tests without the "lfn" option. See XrdXrootdMonTestUtils.hh
// for how the tests get the datagrams.
//------------------------------------------------------------------------------

#include "XrdXrootdMonTestUtils.hh"

#include "XProtocol/XProtocol.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"

#include <string>
#include <utility>
#include <vector>

using namespace XrdXrootdMonTest;

namespace {

const kXR_unt32 userID = 0x11223344;

// Offsets in an isError record: header, error block, user dictid, lfn
const size_t errOff = sizeof(XrdXrootdMonFileHdr);
const size_t userOff = errOff + sizeof(XrdXrootdMonStatERR);
const size_t lfnOff = userOff + sizeof(kXR_unt32);

// The size of a close record with the "ops" option and without an error
const size_t clsSize = sizeof(XrdXrootdMonFileHdr) + sizeof(XrdXrootdMonStatXFR)
                     + sizeof(XrdXrootdMonStatOPS);

// Send a failed open and get its isError record
Record FailedOpen(const std::string &path, kXR_unt32 fileID = 0)
{
  XrdXrootdMonFile::OpenErr(path.c_str(), userID, fileID, kXR_NotFound);
  Flush();
  auto recs = Records(env->rcv.Drain(), XrdXrootdMonFileHdr::isError);
  return recs.size() == 1 ? recs[0] : Record{0, 0, ""};
}

// Open and close a file and get its close record. The caller gives the errors
// of the file as (code, category) pairs.
Record OpenClose(const std::vector<std::pair<int, char>> &errs)
{
  XrdXrootdFileStats stats;
  stats.Init();
  XrdXrootdMonFile::Open(&stats, "/err/file", userID, false);
  for (const auto &[code, cat] : errs)
    stats.setErr(code, cat);
  XrdXrootdMonFile::Close(&stats, false);
  Flush();
  auto recs = Records(env->rcv.Drain(), XrdXrootdMonFileHdr::isClose);
  return recs.size() == 1 ? recs[0] : Record{0, 0, ""};
}

// Get the error block at the given offset of a record
XrdXrootdMonStatERR ErrBlock(const Record &r, size_t off)
{
  XrdXrootdMonStatERR e;
  memcpy(&e, r.data.data() + off, sizeof(e));
  e.ecode = ntohl(e.ecode);
  return e;
}

// Get the lfn of an isError record
std::string Lfn(const Record &r)
{
  const std::string room = r.data.substr(lfnOff);
  return room.substr(0, room.find('\0'));
}

using MonFileErr = MonFixture;

// A failed open gives an isError record with the kXR code and the category
TEST_F(MonFileErr, FailedOpenRecordHasTheCodeAndTheCategory)
{
  Record r = FailedOpen("/err/notfound");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";

  XrdXrootdMonStatERR e = ErrBlock(r, errOff);
  EXPECT_EQ(e.ecode, kXR_NotFound);
  EXPECT_EQ(e.ecat, XrdXrootdMonStatERR::errOpen);
}

// The user dictid comes after the error block
TEST_F(MonFileErr, FailedOpenRecordHasTheUserDictid)
{
  Record r = FailedOpen("/err/user");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";

  kXR_unt32 user;
  memcpy(&user, r.data.data() + userOff, sizeof(user));
  EXPECT_EQ(user, userID);
}

// The lfn comes after the user dictid. Only the lfn names the path when no "d"
// mapping record binds the dictid, so it is present also without "lfn".
TEST_F(MonFileErr, FailedOpenRecordHasTheLfn)
{
  Record r = FailedOpen("/err/lfn");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  ASSERT_TRUE(r.flag & XrdXrootdMonFileHdr::hasLFN);
  EXPECT_EQ(Lfn(r), "/err/lfn");
}

// The record uses the dictid of the "d" mapping record when there is one
TEST_F(MonFileErr, FailedOpenRecordKeepsTheMappedDictid)
{
  Record r = FailedOpen("/err/mapped", 0xabcdef01);
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  EXPECT_EQ(FileID(r), 0xabcdef01u);
}

// Without a mapped dictid, the record gets a new one
TEST_F(MonFileErr, FailedOpenRecordWithoutAMappingGetsANewDictid)
{
  Record r = FailedOpen("/err/unmapped");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  EXPECT_NE(FileID(r), 0u);
}

// A long lfn is truncated so that the record fits the buffer. The fixed part
// has the size of the fixed part of an open record, so the longest lfn is the
// same: 963 bytes for fbsz 1024.
TEST_F(MonFileErr, FailedOpenRecordWithALongLfnFitsTheBuffer)
{
  Record r = FailedOpen("/" + std::string(3999, 'z'));
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  EXPECT_LE(Lfn(r).size(), 963u);
}

// A truncated lfn is the start of the path
TEST_F(MonFileErr, FailedOpenRecordWithALongLfnHasAPrefixOfThePath)
{
  const std::string path = "/" + std::string(3999, 'z');
  Record r = FailedOpen(path);
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";

  const std::string lfn = Lfn(r);
  EXPECT_EQ(lfn, path.substr(0, lfn.size()));
}

// An isError record is a multiple of 8 bytes long, as all f-stream records
// are. Paths of 8 consecutive lengths give each possible padding of the lfn.
TEST_F(MonFileErr, FailedOpenRecordSizesAreMultiplesOf8)
{
  int nBad = 0;
  for (int n = 1; n <= 8; n++) {
    Record r = FailedOpen("/" + std::string(n, 'e'));
    ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
    if (r.data.size() % 8)
      nBad++;
  }
  EXPECT_EQ(nBad, 0)
    << "isError records with a size that is not a multiple of 8";
}

// The close record of a file with an error has the hasERR flag and one
// 8-byte error block at its end
TEST_F(MonFileErr, CloseRecordWithAnErrorHasAnErrorBlock)
{
  Record r = OpenClose({{kXR_IOError, XrdXrootdMonStatERR::errRead}});
  EXPECT_TRUE(r.flag & XrdXrootdMonFileHdr::hasERR);
  EXPECT_EQ(r.data.size(), clsSize + sizeof(XrdXrootdMonStatERR));
}

// The error block of the close record has the first error of the file
TEST_F(MonFileErr, CloseRecordHasTheFirstError)
{
  Record r = OpenClose({{kXR_IOError, XrdXrootdMonStatERR::errRead},
                        {kXR_FSError, XrdXrootdMonStatERR::errClose}});
  ASSERT_TRUE(r.flag & XrdXrootdMonFileHdr::hasERR);
  ASSERT_EQ(r.data.size(), clsSize + sizeof(XrdXrootdMonStatERR));

  XrdXrootdMonStatERR e = ErrBlock(r, clsSize);
  EXPECT_EQ(e.ecode, kXR_IOError);
  EXPECT_EQ(e.ecat, XrdXrootdMonStatERR::errRead);
}

// A close record without an error has no hasERR flag and no error block
TEST_F(MonFileErr, CloseRecordWithoutAnErrorHasNoErrorBlock)
{
  Record r = OpenClose({});
  EXPECT_FALSE(r.flag & XrdXrootdMonFileHdr::hasERR);
  EXPECT_EQ(r.data.size(), clsSize);
}

} // namespace

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MONFILEERR_NO_LFN
  env = new MonEnv(XROOTD_MON_FSERR | XROOTD_MON_FSOPS);
#else
  env = new MonEnv(XROOTD_MON_FSERR | XROOTD_MON_FSLFN | XROOTD_MON_FSOPS);
#endif
  ::testing::AddGlobalTestEnvironment(env);
  return RUN_ALL_TESTS();
}
