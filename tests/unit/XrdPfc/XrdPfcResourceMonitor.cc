#include "XrdPfc/XrdPfcResourceMonitor.hh"

#include <gtest/gtest.h>

using namespace XrdPfc;

// Limits as in pfc.diskusage ... files 2m 3m 4m: the purge clamps its amount
// between the baseline and the nominal or the maximum file usage.
static constexpr long long MiB = 1024 * 1024;

TEST(ResourceMonitorTest, ClampBytesToRemoveKeepsAnAmountWhichLeavesUsageInRange)
{
   EXPECT_EQ(ResourceMonitor::clamp_bytes_to_remove(8 * MiB, 5 * MiB, 2 * MiB, 4 * MiB), 5 * MiB);
}

TEST(ResourceMonitorTest, ClampBytesToRemoveRemovesDownToTheLowLimitWhenTheAmountIsTooLarge)
{
   EXPECT_EQ(ResourceMonitor::clamp_bytes_to_remove(8 * MiB, 14 * MiB, 2 * MiB, 3 * MiB), 6 * MiB);
}

TEST(ResourceMonitorTest, ClampBytesToRemoveRemovesDownToTheHighLimitWhenTheAmountIsTooSmall)
{
   EXPECT_EQ(ResourceMonitor::clamp_bytes_to_remove(8 * MiB, 1 * MiB, 2 * MiB, 4 * MiB), 4 * MiB);
}
