//------------------------------------------------------------------------------
// Scheduler metrics-reversal test.
//
// The scheduler's jobs/threads tallies now live in the process-wide XrdMetrics
// registry as their source of truth; the legacy <stats id="sched"> XML reads
// them via value(). This drives a self-contained scheduler (no Start(), so no
// worker threads run) and asserts that scheduling jobs increments the metric
// and that the new registry scrape and the legacy XML report the same value.
//------------------------------------------------------------------------------

#include <string>

#include "Xrd/XrdScheduler.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

#include <gtest/gtest.h>

namespace
{
class NoopJob : public XrdJob
{
public:
  void DoIt() override {}
};

// Extract the integer inside the <jobs>...</jobs> element of the sched XML.
long parseJobs(const std::string& xml)
{
  auto a = xml.find("<jobs>");
  auto b = xml.find("</jobs>");
  if (a == std::string::npos || b == std::string::npos) return -1;
  a += 6;
  return std::stol(xml.substr(a, b - a));
}
}

TEST(XrdSchedulerStats, JobsCounterReversedToRegistry)
{
  XrdScheduler sched(2, 8, 780);   // self-contained ctor; no Start() => no workers

  char buff[1024];
  long base = parseJobs(std::string(buff, sched.Stats(buff, sizeof(buff))));
  ASSERT_GE(base, 0);

  const int N = 5;
  NoopJob jobs[N];
  for (int i = 0; i < N; i++) sched.Schedule(&jobs[i]);

  long after = parseJobs(std::string(buff, sched.Stats(buff, sizeof(buff))));
  EXPECT_EQ(after, base + N);

  // The new registry must report the same jobs_total value the legacy XML does.
  std::string scrape;
  XrdMetrics::PrometheusTextSerializer ser(scrape);
  XrdMetrics::Default().serialize(ser);
  EXPECT_NE(scrape.find("xrootd_sched_jobs_total " + std::to_string(after) + "\n"),
            std::string::npos);
}
