//------------------------------------------------------------------------------
// Unit tests for the next-generation XrdMetrics system: Counter and Gauge
// instruments, the label model, families, metric groups, the registry, and the
// Prometheus text serializer.
//------------------------------------------------------------------------------

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <thread>
#include <vector>

#include "XrdMetrics/XrdMetricsInstrument.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdMetrics/XrdMetricsValue.hh"

#include <gtest/gtest.h>

using namespace XrdMetrics;

namespace
{
// Serialize a registry to the Prometheus text format and return it.
std::string scrape(const Registry& reg)
{
  std::string out;
  PrometheusTextSerializer ser(out);
  reg.serialize(ser);
  return out;
}
}

/******************************************************************************/
/*                          v a l u e   f o r m a t                          */
/******************************************************************************/

TEST(XrdMetricsValue, IntegersAndDoubles)
{
  std::string s;
  appendValue(s, (uint64_t)0);          s.push_back('|');
  appendValue(s, (uint64_t)12345);      s.push_back('|');
  appendValue(s, (int64_t)-7);          s.push_back('|');
  appendValue(s, 1.0);                  s.push_back('|');
  appendValue(s, 0.5);                  s.push_back('|');
  appendValue(s, 1234567.0);            s.push_back('|');
  EXPECT_EQ(s, "0|12345|-7|1|0.5|1234567|");
}

TEST(XrdMetricsValue, NonFiniteTokens)
{
  std::string s;
  appendValue(s, std::numeric_limits<double>::infinity());  s.push_back(' ');
  appendValue(s, -std::numeric_limits<double>::infinity()); s.push_back(' ');
  appendValue(s, std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ(s, "+Inf -Inf NaN");
}

/******************************************************************************/
/*                              C o u n t e r                                */
/******************************************************************************/

TEST(XrdMetricsCounter, IncrementOperators)
{
  Registry reg("xrootd");
  auto& fam = reg.group("ops").counter("requests_total");
  Counter& c = fam.noLabels();

  ++c;
  c++;
  c += 5;
  EXPECT_EQ(c.value(), 7u);
  EXPECT_EQ(Counter::kind(), MetricKind::Counter);
}

TEST(XrdMetricsCounter, ConcurrentIncrements)
{
  Registry reg("xrootd");
  Counter& c = reg.group("ops").counter("c").noLabels();

  const int nthreads = 8, niter = 100000;
  std::vector<std::thread> th;
  for (int i = 0; i < nthreads; i++)
    th.emplace_back([&] { for (int j = 0; j < niter; j++) ++c; });
  for (auto& t : th) t.join();

  EXPECT_EQ(c.value(), (uint64_t)nthreads * niter);
}

/******************************************************************************/
/*                               G a u g e                                   */
/******************************************************************************/

TEST(XrdMetricsGauge, IntGaugeOperators)
{
  Registry reg("xrootd");
  IntGauge& g = reg.group("sched").intGauge("threads").noLabels();

  g = 8;
  g += 2;
  g -= 3;
  ++g;
  --g;
  EXPECT_EQ(g.value(), 7);
  EXPECT_EQ(IntGauge::kind(), MetricKind::Gauge);
}

TEST(XrdMetricsGauge, FloatGaugeCasLoop)
{
  Registry reg("xrootd");
  FloatGauge& g = reg.group("proc").floatGauge("ratio").noLabels();

  g = 2.5;
  g += 1.0;
  g -= 0.25;
  EXPECT_DOUBLE_EQ(g.value(), 3.25);
}

/******************************************************************************/
/*                               L a b e l s                                 */
/******************************************************************************/

TEST(XrdMetricsLabels, PrefixOrderGlobalConstVariable)
{
  Registry reg("xrootd", {{"instance", "h1"}});
  auto& fam = reg.group("ops").counter("requests_total", {"verb"},
                                        {{"proto", "xroot"}});
  Counter& c = fam.withLabelValues({"open"});

  // global instance, then family const proto, then variable verb, then a space.
  EXPECT_EQ(c.labels().prometheusPrefix(),
            "xrootd_ops_requests_total{instance=\"h1\",proto=\"xroot\",verb=\"open\"} ");
}

TEST(XrdMetricsLabels, PrefixBuiltOnceAndStable)
{
  Registry reg("xrootd");
  Counter& c = reg.group("g").counter("c", {"k"}).withLabelValues({"v"});
  const std::string* p1 = &c.labels().prometheusPrefix();
  ++c;
  const std::string* p2 = &c.labels().prometheusPrefix();
  EXPECT_EQ(p1, p2);   // same storage; never rebuilt on update
}

TEST(XrdMetricsLabels, ValueEscaping)
{
  Registry reg("xrootd");
  Counter& c = reg.group("g").counter("c", {"k"})
                  .withLabelValues({"a\"b\\c\nd"});
  EXPECT_EQ(c.labels().prometheusPrefix(),
            "xrootd_g_c{k=\"a\\\"b\\\\c\\nd\"} ");
}

TEST(XrdMetricsLabels, ForEachLabelStructuredOrder)
{
  Registry reg("xrootd", {{"instance", "h1"}});
  auto& fam = reg.group("g").counter("c", {"verb"}, {{"proto", "xroot"}});
  Counter& c = fam.withLabelValues({"open"});

  std::vector<std::pair<std::string, std::string>> seen;
  c.labels().forEachLabel([&](const std::string& k, const std::string& v)
                          { seen.emplace_back(k, v); });

  ASSERT_EQ(seen.size(), 3u);
  EXPECT_EQ(seen[0], std::make_pair(std::string("instance"), std::string("h1")));
  EXPECT_EQ(seen[1], std::make_pair(std::string("proto"), std::string("xroot")));
  EXPECT_EQ(seen[2], std::make_pair(std::string("verb"), std::string("open")));
}

TEST(XrdMetricsLabels, InvalidNamesRejected)
{
  Registry reg("xrootd");
  auto& grp = reg.group("g");
  EXPECT_THROW(grp.counter("bad-name"), std::invalid_argument);
  EXPECT_THROW(grp.counter("ok", {"bad-label"}), std::invalid_argument);
  EXPECT_THROW(grp.counter("ok2", {}, {{"bad-label", "v"}}),
               std::invalid_argument);
  EXPECT_NO_THROW(grp.counter("ok3", {"good_label"}, {{"also_good", "v"}}));
}

/******************************************************************************/
/*                              F a m i l y                                  */
/******************************************************************************/

TEST(XrdMetricsFamily, SameValuesReturnSameHandle)
{
  Registry reg("xrootd");
  auto& fam = reg.group("g").counter("c", {"k"});
  Counter& a = fam.withLabelValues({"x"});
  Counter& b = fam.withLabelValues({"x"});
  EXPECT_EQ(&a, &b);
  ++a;
  EXPECT_EQ(b.value(), 1u);

  Counter& d = fam.withLabelValues({"y"});
  EXPECT_NE(&a, &d);
}

TEST(XrdMetricsFamily, CardinalityCapFoldsToOverflow)
{
  Registry reg("xrootd");
  auto& fam = reg.group("g").counter("c", {"k"}, {}, {}, /*maxKids=*/2);
  Counter& a = fam.withLabelValues({"1"});
  Counter& b = fam.withLabelValues({"2"});
  Counter& c = fam.withLabelValues({"3"});   // over the cap
  Counter& d = fam.withLabelValues({"4"});   // over the cap

  EXPECT_NE(&a, &b);
  EXPECT_EQ(&c, &d);                          // both fold to one overflow series

  std::string out = scrape(reg);
  EXPECT_NE(out.find("__over_cardinality_limit__"), std::string::npos);
}

/******************************************************************************/
/*                            O b s e r v e d                                */
/******************************************************************************/

TEST(XrdMetricsObserved, ReadsSourceAtScrapeTime)
{
  Registry reg("xrootd");
  long long threads = 4;
  reg.group("sched").observeIntGauge("threads", {}, {}, "worker threads")
                    .add({}, [&]{ return (int64_t)threads; });

  EXPECT_NE(scrape(reg).find("xrootd_sched_threads 4\n"), std::string::npos);

  threads = 9;   // the source changed; a later scrape reflects it
  EXPECT_NE(scrape(reg).find("xrootd_sched_threads 9\n"), std::string::npos);
}

TEST(XrdMetricsObserved, CounterKindAndConstLabels)
{
  Registry reg("xrootd", {{"instance", "h1"}});
  unsigned long long hits = 7;
  reg.group("cache").observeCounter("evictions_total", {}, {{"tier", "ram"}},
                                    "evictions")
                    .add({}, [&]{ return hits; });

  const std::string out = scrape(reg);
  EXPECT_NE(out.find("# TYPE xrootd_cache_evictions_total counter\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_cache_evictions_total{instance=\"h1\",tier=\"ram\"} 7\n"),
            std::string::npos);
}

TEST(XrdMetricsObserved, MultiSeriesUnderOneFamily)
{
  Registry reg("xrootd");
  unsigned long long open = 3, read = 5;
  reg.group("ops").observeCounter("total", {"op"}, {}, "operations")
                  .add({"open"}, [&]{ return open; })
                  .add({"read"}, [&]{ return read; });

  const std::string out = scrape(reg);
  // One TYPE line for the family, one series per label value.
  size_t first = out.find("# TYPE xrootd_ops_total");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(out.find("# TYPE xrootd_ops_total", first + 1), std::string::npos);
  EXPECT_NE(out.find("xrootd_ops_total{op=\"open\"} 3\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_ops_total{op=\"read\"} 5\n"), std::string::npos);
}

/******************************************************************************/
/*                    R e g i s t r y   /   G r o u p                        */
/******************************************************************************/

TEST(XrdMetricsRegistryNG, GroupIsMemoized)
{
  Registry reg("xrootd");
  EXPECT_EQ(&reg.group("ops"), &reg.group("ops"));
}

TEST(XrdMetricsRegistryNG, FullNameComposition)
{
  Registry reg("xrootd");
  auto& fam = reg.group("ops").counter("requests_total");
  EXPECT_EQ(fam.name(), "xrootd_ops_requests_total");

  // Empty prefix and/or subsystem are skipped in the join.
  Registry bare("");
  auto& f2 = bare.group("").counter("just_a_name");
  EXPECT_EQ(f2.name(), "just_a_name");
}

TEST(XrdMetricsRegistryNG, DefaultIsStableSingleton)
{
  EXPECT_EQ(&Default(), &Default());
  EXPECT_EQ(Default().prefix(), "xrootd");
}

/******************************************************************************/
/*                P r o m e t h e u s   s e r i a l i z e r                   */
/******************************************************************************/

TEST(XrdMetricsPrometheus, FamilyHeaderAndSeries)
{
  Registry reg("xrootd");
  auto& reqs = reg.group("ops").counter("requests_total", {"verb"}, {},
                                         "Requests processed");
  reqs.withLabelValues({"open"}) += 3;
  reqs.withLabelValues({"read"}) += 5;

  std::string out = scrape(reg);
  EXPECT_NE(out.find("# HELP xrootd_ops_requests_total Requests processed\n"),
            std::string::npos);
  EXPECT_NE(out.find("# TYPE xrootd_ops_requests_total counter\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_ops_requests_total{verb=\"open\"} 3\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_ops_requests_total{verb=\"read\"} 5\n"),
            std::string::npos);

  // Exactly one TYPE line for the family.
  size_t first = out.find("# TYPE xrootd_ops_requests_total");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(out.find("# TYPE xrootd_ops_requests_total", first + 1),
            std::string::npos);
}

TEST(XrdMetricsPrometheus, GaugeRendersAndNoHelpWhenEmpty)
{
  Registry reg("xrootd");
  reg.group("sched").intGauge("threads").noLabels() = 8;

  std::string out = scrape(reg);
  EXPECT_EQ(out,
            "# TYPE xrootd_sched_threads gauge\n"
            "xrootd_sched_threads 8\n");
}

TEST(XrdMetricsPrometheus, BufferIsReusableAcrossScrapes)
{
  Registry reg("xrootd");
  Counter& c = reg.group("g").counter("c").noLabels();
  ++c;

  std::string out;
  PrometheusTextSerializer ser(out);
  reg.serialize(ser);
  std::string first = out;

  out.clear();
  reg.serialize(ser);
  EXPECT_EQ(out, first);
}
