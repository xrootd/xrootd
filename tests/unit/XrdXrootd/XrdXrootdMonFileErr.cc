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
Record FailedOpen(const std::string &path,
                  char op = XrdXrootdMonStatERR::errOpen)
{
  XrdXrootdMonFile::OpenErr(path.c_str(), userID, kXR_NotFound, op);
  Flush();
  auto recs = Records(env->rcv.Drain(), XrdXrootdMonFileHdr::isError);
  return recs.size() == 1 ? recs[0] : Record{0, 0, ""};
}

// Open and close a file and get its close record. The caller gives the errors
// of the file as (code, operation) pairs.
Record OpenClose(const std::vector<std::pair<int, char>> &errs)
{
  XrdXrootdFileStats stats;
  stats.Init();
  XrdXrootdMonFile::Open(&stats, "/err/file", userID, false);
  for (const auto &[code, op] : errs)
    stats.setErr(code, op);
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

// The executable without "lfn" runs the same tests in a suite with another
// name, so that the names of its tests in CTest differ
#ifdef MONFILEERR_NO_LFN
#define MonFileErrSuite MonFileErrNoLfnTest
#else
#define MonFileErrSuite MonFileErrTest
#endif

using MonFileErrSuite = MonTest;

// A failed open gives an isError record with the kXR code and the operation,
// also for a path that does not exist
TEST_F(MonFileErrSuite, FailedOpenRecordHasTheCodeAndTheOperation)
{
  Record r = FailedOpen("/err/notfound");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";

  XrdXrootdMonStatERR e = ErrBlock(r, errOff);
  EXPECT_EQ(e.ecode, kXR_NotFound);
  EXPECT_EQ(e.eoper, XrdXrootdMonStatERR::errOpen);
}

// The user dictid comes after the error block
TEST_F(MonFileErrSuite, FailedOpenRecordHasTheUserDictid)
{
  Record r = FailedOpen("/err/user");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";

  kXR_unt32 user;
  memcpy(&user, r.data.data() + userOff, sizeof(user));
  EXPECT_EQ(user, userID);
}

// The lfn comes after the user dictid. The record has no dictid for the path,
// so only the lfn names it, and it is present also without "lfn".
TEST_F(MonFileErrSuite, FailedOpenRecordHasTheLfn)
{
  Record r = FailedOpen("/err/lfn");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  ASSERT_TRUE(r.flag & XrdXrootdMonFileHdr::hasLFN);
  EXPECT_EQ(Lfn(r), "/err/lfn");
}

// A failed open gets no dictid, so that frequent errors, such as a path that
// does not exist, do not use dictids or "d" mapping records
TEST_F(MonFileErrSuite, FailedOpenRecordHasNoDictid)
{
  Record r = FailedOpen("/err/nodictid");
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  EXPECT_EQ(FileID(r), 0u);
}

// A failed clone of an open gives the clone operation, not the open
TEST_F(MonFileErrSuite, FailedCloneRecordHasTheCloneOperation)
{
  Record r = FailedOpen("/err/clone", XrdXrootdMonStatERR::errClone);
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  EXPECT_EQ(ErrBlock(r, errOff).eoper, XrdXrootdMonStatERR::errClone);
}

// A long lfn is truncated so that the record fits the buffer. The fixed part
// has the size of the fixed part of an open record, so the longest lfn is the
// same: 963 bytes for fbsz 1024.
TEST_F(MonFileErrSuite, FailedOpenRecordWithALongLfnFitsTheBuffer)
{
  Record r = FailedOpen("/" + std::string(3999, 'z'));
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";
  EXPECT_LE(Lfn(r).size(), 963u);
}

// A truncated lfn is the start of the path
TEST_F(MonFileErrSuite, FailedOpenRecordWithALongLfnHasAPrefixOfThePath)
{
  const std::string path = "/" + std::string(3999, 'z');
  Record r = FailedOpen(path);
  ASSERT_GE(r.data.size(), lfnOff) << "no isError record";

  const std::string lfn = Lfn(r);
  EXPECT_EQ(lfn, path.substr(0, lfn.size()));
}

// An isError record is a multiple of 8 bytes long, as all f-stream records
// are. Paths of 8 consecutive lengths give each possible padding of the lfn.
TEST_F(MonFileErrSuite, FailedOpenRecordSizesAreMultiplesOf8)
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
TEST_F(MonFileErrSuite, CloseRecordWithAnErrorHasAnErrorBlock)
{
  Record r = OpenClose({{kXR_IOError, XrdXrootdMonStatERR::errRead}});
  EXPECT_TRUE(r.flag & XrdXrootdMonFileHdr::hasERR);
  EXPECT_EQ(r.data.size(), clsSize + sizeof(XrdXrootdMonStatERR));
}

// The error block of the close record has the first error of the file
TEST_F(MonFileErrSuite, CloseRecordHasTheFirstError)
{
  Record r = OpenClose({{kXR_IOError, XrdXrootdMonStatERR::errRead},
                        {kXR_FSError, XrdXrootdMonStatERR::errClose}});
  ASSERT_TRUE(r.flag & XrdXrootdMonFileHdr::hasERR);
  ASSERT_EQ(r.data.size(), clsSize + sizeof(XrdXrootdMonStatERR));

  XrdXrootdMonStatERR e = ErrBlock(r, clsSize);
  EXPECT_EQ(e.ecode, kXR_IOError);
  EXPECT_EQ(e.eoper, XrdXrootdMonStatERR::errRead);
}

// A close record without an error has no hasERR flag and no error block
TEST_F(MonFileErrSuite, CloseRecordWithoutAnErrorHasNoErrorBlock)
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
