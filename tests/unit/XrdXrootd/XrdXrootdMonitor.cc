//------------------------------------------------------------------------------
// Unit tests for the static helpers of XrdXrootdMonitor, which need no
// configuration of the monitor.
//------------------------------------------------------------------------------

#include "XrdXrootd/XrdXrootdMonitor.hh"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

// CopyText() truncates the text so that it and its null byte fit the room
TEST(MonitorCopyTextTest, TruncatesToTheRoom)
{
  char buff[8];
  EXPECT_EQ(XrdXrootdMonitor::CopyText(buff, 4, "abcdefghij", 10), 3);
  EXPECT_STREQ(buff, "abc");
}

// CopyText() copies only the length that the caller gives
TEST(MonitorCopyTextTest, CopiesTheGivenLength)
{
  char buff[8];
  EXPECT_EQ(XrdXrootdMonitor::CopyText(buff, sizeof(buff), "abcdefghij", 2), 2);
  EXPECT_STREQ(buff, "ab");
}

// CopyText() fills the rest of the room with null bytes
TEST(MonitorCopyTextTest, NullPadsTheRoom)
{
  char buff[16];
  memset(buff, 'x', sizeof(buff));
  XrdXrootdMonitor::CopyText(buff, sizeof(buff), "abc", 3);
  EXPECT_EQ(std::string(buff + 3, sizeof(buff) - 3),
            std::string(sizeof(buff) - 3, '\0'));
}
