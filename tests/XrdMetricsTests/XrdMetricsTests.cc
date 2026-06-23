//------------------------------------------------------------------------------
// Unit tests for the XrdMetrics module and its Prometheus text serializer.
//------------------------------------------------------------------------------

#include <thread>
#include <vector>

#include "XrdMetrics/XrdMetrics.hh"

#include <gtest/gtest.h>

using namespace testing;

TEST(XrdMetrics, CounterScrape)
{
  XrdMetricsRegistry reg;
  auto& c = reg.Counter("xrootd_test_total", "A test");
  c.inc();
  c.inc(2);
  EXPECT_EQ(c.value(), 3u);

  std::string out;
  reg.Scrape(out);
  EXPECT_EQ(out,
            "# HELP xrootd_test_total A test\n"
            "# TYPE xrootd_test_total counter\n"
            "xrootd_test_total 3\n");
}

TEST(XrdMetrics, GaugeSetIncDec)
{
  XrdMetricsRegistry reg;
  auto& g = reg.Gauge("g", "G");
  g.set(2.5);
  g.inc(1.0);
  g.dec(0.5);
  EXPECT_DOUBLE_EQ(g.value(), 3.0);

  std::string out;
  reg.Scrape(out);
  EXPECT_EQ(out, "# HELP g G\n# TYPE g gauge\ng 3\n");
}

TEST(XrdMetrics, LabelsAreSortedAndEscaped)
{
  XrdMetricsRegistry reg;
  reg.Counter("c", "C", {{"b", "2"}, {"a", "1"}}).inc(5);
  reg.Counter("c", "C", {{"q", "a\"b\\c"}}).inc(1);

  std::string out;
  reg.Scrape(out);
  EXPECT_EQ(out,
            "# HELP c C\n"
            "# TYPE c counter\n"
            "c{a=\"1\",b=\"2\"} 5\n"
            "c{q=\"a\\\"b\\\\c\"} 1\n");
}

TEST(XrdMetrics, SameNameLabelsReturnSameInstrument)
{
  XrdMetricsRegistry reg;
  auto& a = reg.Counter("c", "C", {{"x", "1"}});
  auto& b = reg.Counter("c", "C", {{"x", "1"}});
  EXPECT_EQ(&a, &b);
  a.inc();
  EXPECT_EQ(b.value(), 1u);
}

TEST(XrdMetrics, HistogramBucketsCumulative)
{
  XrdMetricsRegistry reg;
  auto& h = reg.Histogram("h", "H", {1, 2, 5});
  h.observe(0.5);
  h.observe(1.5);
  h.observe(3);
  h.observe(10);

  std::string out;
  reg.Scrape(out);
  EXPECT_EQ(out,
            "# HELP h H\n"
            "# TYPE h histogram\n"
            "h_bucket{le=\"1\"} 1\n"
            "h_bucket{le=\"2\"} 2\n"
            "h_bucket{le=\"5\"} 3\n"
            "h_bucket{le=\"+Inf\"} 4\n"
            "h_sum 15\n"
            "h_count 4\n");
}

TEST(XrdMetrics, HistogramWithLabels)
{
  XrdMetricsRegistry reg;
  reg.Histogram("h", "H", {1}, {{"op", "read"}}).observe(0.5);

  std::string out;
  reg.Scrape(out);
  EXPECT_EQ(out,
            "# HELP h H\n"
            "# TYPE h histogram\n"
            "h_bucket{op=\"read\",le=\"1\"} 1\n"
            "h_bucket{op=\"read\",le=\"+Inf\"} 1\n"
            "h_sum{op=\"read\"} 0.5\n"
            "h_count{op=\"read\"} 1\n");
}

TEST(XrdMetrics, ConcurrentIncrements)
{
  XrdMetricsRegistry reg;
  auto& c = reg.Counter("c", "C");

  const int nthreads = 8, niter = 100000;
  std::vector<std::thread> th;
  for (int i = 0; i < nthreads; i++)
    th.emplace_back([&] { for (int j = 0; j < niter; j++) c.inc(); });
  for (auto& t : th) t.join();

  EXPECT_EQ(c.value(), (uint64_t)nthreads * niter);
}

TEST(XrdMetrics, DefaultRegistryIsShared)
{
  EXPECT_EQ(&XrdMetricsRegistry::Default(), &XrdMetricsRegistry::Default());
}
