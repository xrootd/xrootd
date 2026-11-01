#include "XrdCl/XrdClXRootDResponses.hh"

#include <gtest/gtest.h>

using XrdCl::StatInfo;

TEST(StatInfoTest, GetModTimeAsStringPrintsUtc)
{
  StatInfo info( "id", 0, 0, 1724238173 );
  EXPECT_EQ( info.GetModTimeAsString(), "2024-08-21 11:02:53" );
}

TEST(StatInfoTest, GetModTimeAsStringPrintsTheSecondsOfATimeAfterTheLastYear)
{
  // the year of this time does not fit in the int of struct tm
  StatInfo info( "id", 0, 0, 4611686018427387904ULL );
  EXPECT_EQ( info.GetModTimeAsString(), "4611686018427387904" );
}
