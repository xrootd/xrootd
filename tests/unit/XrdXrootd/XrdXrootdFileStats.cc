//------------------------------------------------------------------------------
// Unit tests for XrdXrootdFileStats: the first error of a file, which the
// f-stream close record reports. These tests do not start the monitor.
//------------------------------------------------------------------------------

#include "XProtocol/XProtocol.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"

#include <gtest/gtest.h>

// A file without an f-stream open record gets no close record, so its errors
// are not kept
TEST(FileStatsTest, SetErrIgnoresAFileWithoutAMonitorEntry)
{
  XrdXrootdFileStats stats;
  stats.Init();
  stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
  EXPECT_EQ(stats.firstErr().op, 0);
}

// The error of a file with a monitor entry is kept
TEST(FileStatsTest, SetErrKeepsTheCodeAndTheOperation)
{
  XrdXrootdFileStats stats;
  stats.Init();
  stats.MonEnt = 0;
  stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
  EXPECT_EQ(stats.firstErr().code, kXR_IOError);
  EXPECT_EQ(stats.firstErr().op, XrdXrootdMonStatERR::errRead);
}

// A later error does not replace the first error
TEST(FileStatsTest, SetErrKeepsOnlyTheFirstError)
{
  XrdXrootdFileStats stats;
  stats.Init();
  stats.MonEnt = 0;
  stats.setErr(kXR_IOError, XrdXrootdMonStatERR::errRead);
  stats.setErr(kXR_FSError, XrdXrootdMonStatERR::errClose);
  EXPECT_EQ(stats.firstErr().code, kXR_IOError);
  EXPECT_EQ(stats.firstErr().op, XrdXrootdMonStatERR::errRead);
}
