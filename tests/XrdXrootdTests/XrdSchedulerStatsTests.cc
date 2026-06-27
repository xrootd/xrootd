//------------------------------------------------------------------------------
// Scheduler metrics-reversal test.
//
// The scheduler's jobs/threads tallies and live-state gauges now live in the
// process-wide XrdMetrics registry as their source of truth; the legacy
// <stats id="sched"> XML reads them via value(). This drives a self-contained
// scheduler (no Start(), so no worker threads run and queued jobs stay queued)
// and asserts that scheduling jobs moves both the new-registry series and the
// legacy XML, and that the two always agree.
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

// Extract the integer inside the named element of the sched XML, e.g. "jobs".
long parseTag(const std::string& xml, const std::string& tag)
{
  auto a = xml.find("<" + tag + ">");
  auto b = xml.find("</" + tag + ">");
  if (a == std::string::npos || b == std::string::npos) return -1;
  a += tag.size() + 2;
  return std::stol(xml.substr(a, b - a));
}

std::string scrapeRegistry()
{
  std::string out;
  XrdMetrics::PrometheusTextSerializer ser(out);
  XrdMetrics::Default().serialize(ser);
  return out;
}
}

TEST(XrdSchedulerStats, CountersReversedToRegistry)
{
  XrdScheduler sched(2, 8, 780);   // self-contained ctor; no Start() => no workers

  char buff[1024];
  std::string xml(buff, sched.Stats(buff, sizeof(buff)));
  const long baseJobs = parseTag(xml, "jobs");
  ASSERT_GE(baseJobs, 0);

  const int N = 5;
  NoopJob jobs[N];
  for (int i = 0; i < N; i++) sched.Schedule(&jobs[i]);

  xml.assign(buff, sched.Stats(buff, sizeof(buff)));
  const long jobsTotal = parseTag(xml, "jobs");
  const long inQueue   = parseTag(xml, "inq");
  const long maxInQ    = parseTag(xml, "maxinq");

  // No workers ran, so every scheduled job is still queued.
  EXPECT_EQ(jobsTotal, baseJobs + N);
  EXPECT_EQ(inQueue, N);
  EXPECT_EQ(maxInQ, N);

  // The new registry must report the same values the legacy XML does.
  const std::string scrape = scrapeRegistry();
  EXPECT_NE(scrape.find("xrootd_sched_jobs_total " + std::to_string(jobsTotal) + "\n"),
            std::string::npos);
  EXPECT_NE(scrape.find("xrootd_sched_jobs_in_queue " + std::to_string(inQueue) + "\n"),
            std::string::npos);
  EXPECT_NE(scrape.find("xrootd_sched_queue_length_max " + std::to_string(maxInQ) + "\n"),
            std::string::npos);
}
