#include "XrdSys/XrdSysTimer.hh"

#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <string>

namespace
{
// Local midnight of the day of t, from mktime(), which the implementation
// does not use any more
time_t MidnightByMktime(time_t t)
{
   struct tm tmv;
   localtime_r(&t, &tmv);
   tmv.tm_hour = tmv.tm_min = tmv.tm_sec = 0;
   tmv.tm_isdst = -1;
   return mktime(&tmv);
}

class XrdSysTimerMidnight : public ::testing::TestWithParam<const char*>
{
  protected:
    void SetUp() override
    {
       const char *tz = getenv("TZ");
       if (tz) savedTZ = tz;
       hadTZ = tz != nullptr;
       setenv("TZ", GetParam(), 1);
       tzset();
    }

    void TearDown() override
    {
       if (hadTZ) setenv("TZ", savedTZ.c_str(), 1);
          else    unsetenv("TZ");
       tzset();
    }

    std::string savedTZ;
    bool hadTZ = false;
};
}

// Two years from November 2023 in steps of 30 minutes less one second, thus
// the times cross the changes of daylight saving time
TEST_P(XrdSysTimerMidnight, IsTheLocalMidnightOfTheDay)
{
   for (time_t t = 1700000000; t < 1700000000 + 2L*366*86400; t += 1799)
       ASSERT_EQ(XrdSysTimer::Midnight(t), MidnightByMktime(t)) << "at " << t;
}

TEST_P(XrdSysTimerMidnight, OfTheEndOfTodayIsAfterNow)
{
   EXPECT_GT(XrdSysTimer::Midnight(1), time(nullptr));
}

TEST_P(XrdSysTimerMidnight, OfTheEndOfTodayIsAtMostOneDayAway)
{
   EXPECT_LE(XrdSysTimer::Midnight(1) - time(nullptr), 25*3600);
}

INSTANTIATE_TEST_SUITE_P(TimeZones, XrdSysTimerMidnight,
   ::testing::Values("UTC", "Europe/Zurich", "America/New_York",
                     "Australia/Lord_Howe", "Asia/Kolkata"));
