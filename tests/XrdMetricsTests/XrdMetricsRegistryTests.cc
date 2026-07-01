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
std::string scrape(const Collector& collector)
{
  std::string out;
  PrometheusTextSerializer ser(out);
  collector.serialize(ser);
  return out;
}

// Serialize a registry to the OTLP/JSON format and return it.
std::string scrapeOtel(const Collector& collector, std::string scope = "xrootd",
                       std::vector<ConstLabel> res = {})
{
  std::string out;
  OtelJsonSerializer ser(out, std::move(scope), std::move(res));
  collector.serialize(ser);
  return out;
}

// Structural sanity check: every {}/[] is matched, ignoring those inside JSON
// strings. Cheaper than pulling in a JSON parser for a self-contained check.
bool jsonBalanced(const std::string& s)
{
  int curly = 0, square = 0;
  bool inStr = false, esc = false;
  for (char c : s)
      {if (inStr)
          {     if (esc)        esc = false;
           else if (c == '\\')  esc = true;
           else if (c == '"')   inStr = false;
           continue;
          }
       switch (c)
             {case '"': inStr = true;  break;
              case '{': ++curly;       break;
              case '}': --curly;       break;
              case '[': ++square;      break;
              case ']': --square;      break;
              default: break;
             }
       if (curly < 0 || square < 0) return false;
      }
  return curly == 0 && square == 0 && !inStr;
}

bool contains(const std::string& hay, const std::string& needle)
{
  return hay.find(needle) != std::string::npos;
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
  Collector collector("xrootd");
  auto& fam = collector.subsystem("ops").counter<std::uint64_t>("requests_total");
  Counter<std::uint64_t>& c = fam.noLabels();

  ++c;
  c++;
  c += 5;
  EXPECT_EQ(c.value(), 7u);
  EXPECT_EQ((Counter<std::uint64_t>::kind()), MetricKind::Counter);
}

TEST(XrdMetricsCounter, ConcurrentIncrements)
{
  Collector collector("xrootd");
  Counter<std::uint64_t>& c = collector.subsystem("ops").counter<std::uint64_t>("c").noLabels();

  const int nthreads = 8, niter = 100000;
  std::vector<std::thread> th;
  for (int i = 0; i < nthreads; i++)
    th.emplace_back([&] { for (int j = 0; j < niter; j++) ++c; });
  for (auto& t : th) t.join();

  EXPECT_EQ(c.value(), (uint64_t)nthreads * niter);
}

// Native floating-point counter (counter<double>): monotonic, accumulates
// fractional amounts, renders under TYPE counter.
TEST(XrdMetricsCounter, DoubleCounterAccumulatesAndRenders)
{
  Collector collector("xrootd");
  Counter<double>& c = collector.subsystem("proc")
                          .counter<double>("cpu_seconds_total", "cpu time")
                          .noLabels();
  c += 1.5;
  ++c;
  EXPECT_DOUBLE_EQ(c.value(), 2.5);
  EXPECT_EQ((Counter<double>::kind()), MetricKind::Counter);

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("# TYPE xrootd_proc_cpu_seconds_total counter"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_proc_cpu_seconds_total 2.5"), std::string::npos);
}

/******************************************************************************/
/*                               G a u g e                                   */
/******************************************************************************/

TEST(XrdMetricsGauge, IntGaugeOperators)
{
  Collector collector("xrootd");
  Gauge<std::int64_t>& g = collector.subsystem("sched").gauge<std::int64_t>("threads").noLabels();

  g = 8;
  g += 2;
  g -= 3;
  ++g;
  --g;
  EXPECT_EQ(g.value(), 7);
  EXPECT_EQ(Gauge<std::int64_t>::kind(), MetricKind::Gauge);
}

TEST(XrdMetricsGauge, FloatGaugeCasLoop)
{
  Collector collector("xrootd");
  Gauge<double>& g = collector.subsystem("proc").gauge<double>("ratio").noLabels();

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
  Collector collector("xrootd", {{"instance", "h1"}});
  auto& fam = collector.subsystem("ops").counter<std::uint64_t>("requests_total", {},
                                        {{"proto", "xroot"}}, {"verb"});
  Counter<std::uint64_t>& c = fam.withLabelValues({"open"});

  // global instance, then family const proto, then variable verb, then a space.
  EXPECT_EQ(c.labels().prometheusPrefix(),
            "xrootd_ops_requests_total{instance=\"h1\",proto=\"xroot\",verb=\"open\"} ");
}

TEST(XrdMetricsLabels, PrefixBuiltOnceAndStable)
{
  Collector collector("xrootd");
  Counter<std::uint64_t>& c = collector.subsystem("g").counter<std::uint64_t>("c", {}, {}, {"k"}).withLabelValues({"v"});
  const std::string* p1 = &c.labels().prometheusPrefix();
  ++c;
  const std::string* p2 = &c.labels().prometheusPrefix();
  EXPECT_EQ(p1, p2);   // same storage; never rebuilt on update
}

TEST(XrdMetricsLabels, ValueEscaping)
{
  Collector collector("xrootd");
  Counter<std::uint64_t>& c = collector.subsystem("g").counter<std::uint64_t>("c", {}, {}, {"k"})
                  .withLabelValues({"a\"b\\c\nd"});
  EXPECT_EQ(c.labels().prometheusPrefix(),
            "xrootd_g_c{k=\"a\\\"b\\\\c\\nd\"} ");
}

TEST(XrdMetricsLabels, ForEachLabelStructuredOrder)
{
  Collector collector("xrootd", {{"instance", "h1"}});
  auto& fam = collector.subsystem("g").counter<std::uint64_t>("c", {}, {{"proto", "xroot"}}, {"verb"});
  Counter<std::uint64_t>& c = fam.withLabelValues({"open"});

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
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("g");
  EXPECT_THROW(subsystem.counter<std::uint64_t>("bad-name"), std::invalid_argument);
  EXPECT_THROW(subsystem.counter<std::uint64_t>("ok", {}, {}, {"bad-label"}), std::invalid_argument);
  EXPECT_THROW(subsystem.counter<std::uint64_t>("ok2", {}, {{"bad-label", "v"}}),
               std::invalid_argument);
  EXPECT_NO_THROW(subsystem.counter<std::uint64_t>("ok3", {}, {{"also_good", "v"}}, {"good_label"}));
}

/******************************************************************************/
/*                              F a m i l y                                  */
/******************************************************************************/

TEST(XrdMetricsFamily, SameValuesReturnSameHandle)
{
  Collector collector("xrootd");
  auto& fam = collector.subsystem("g").counter<std::uint64_t>("c", {}, {}, {"k"});
  Counter<std::uint64_t>& a = fam.withLabelValues({"x"});
  Counter<std::uint64_t>& b = fam.withLabelValues({"x"});
  EXPECT_EQ(&a, &b);
  ++a;
  EXPECT_EQ(b.value(), 1u);

  Counter<std::uint64_t>& d = fam.withLabelValues({"y"});
  EXPECT_NE(&a, &d);
}

TEST(XrdMetricsFamily, CardinalityCapFoldsToOverflow)
{
  Collector collector("xrootd");
  auto& fam = collector.subsystem("g").counter<std::uint64_t>("c", {}, {}, {"k"}, /*maxKids=*/2);
  Counter<std::uint64_t>& a = fam.withLabelValues({"1"});
  Counter<std::uint64_t>& b = fam.withLabelValues({"2"});
  Counter<std::uint64_t>& c = fam.withLabelValues({"3"});   // over the cap
  Counter<std::uint64_t>& d = fam.withLabelValues({"4"});   // over the cap

  EXPECT_NE(&a, &b);
  EXPECT_EQ(&c, &d);                          // both fold to one overflow series

  std::string out = scrape(collector);
  EXPECT_NE(out.find("__over_cardinality_limit__"), std::string::npos);
}

/******************************************************************************/
/*                           H i s t o g r a m                               */
/******************************************************************************/

TEST(XrdMetricsHistogram, BucketsCumulativeAndLabels)
{
  Collector collector("xrootd");
  auto& h = collector.subsystem("io").histogram("size_bytes", {1, 2, 5}, "io sizes");
  h.noLabels().observe(0.5);
  h.noLabels().observe(1.5);
  h.noLabels().observe(3);
  h.noLabels().observe(10);

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("# TYPE xrootd_io_size_bytes histogram\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_size_bytes_bucket{le=\"1\"} 1\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_size_bytes_bucket{le=\"2\"} 2\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_size_bytes_bucket{le=\"5\"} 3\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_size_bytes_bucket{le=\"+Inf\"} 4\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_size_bytes_sum 15\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_size_bytes_count 4\n"), std::string::npos);
}

TEST(XrdMetricsHistogram, LabeledBucketsCarryLe)
{
  Collector collector("xrootd");
  collector.subsystem("io").histogram("d", {1}, "h", {}, {"op"})
                 .withLabelValues({"read"}).observe(0.5);

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("xrootd_io_d_bucket{op=\"read\",le=\"1\"} 1\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_io_d_bucket{op=\"read\",le=\"+Inf\"} 1\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_io_d_sum{op=\"read\"} 0.5\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_d_count{op=\"read\"} 1\n"), std::string::npos);
}

/******************************************************************************/
/*                             S u m m a r y                                 */
/******************************************************************************/

TEST(XrdMetricsSummary, SumAndCountUnlabeled)
{
  Collector collector("xrootd");
  auto& s = collector.subsystem("xrootd").summary("request_bytes", "request sizes");
  s.noLabels().observe(100);
  s.noLabels().observe(250);
  s.noLabels().observe(50);

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("# TYPE xrootd_xrootd_request_bytes summary\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_xrootd_request_bytes_sum 400\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_xrootd_request_bytes_count 3\n"), std::string::npos);
  // A quantile-less summary emits no per-quantile series.
  EXPECT_EQ(out.find("quantile="), std::string::npos);
}

TEST(XrdMetricsSummary, LabeledSeriesCarryLabels)
{
  Collector collector("xrootd");
  collector.subsystem("io").summary("latency_seconds", "op latency", {}, {"op"})
                 .withLabelValues({"read"}).observe(0.25);

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("xrootd_io_latency_seconds_sum{op=\"read\"} 0.25\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_io_latency_seconds_count{op=\"read\"} 1\n"),
            std::string::npos);
}

TEST(XrdMetricsSummary, CachedHandleAccumulates)
{
  Collector collector("xrootd");
  auto& fam = collector.subsystem("io").summary("bytes", {});
  Summary& h = fam.noLabels();        // cache the handle, then reuse it
  h.observe(10);
  h.observe(20);
  EXPECT_EQ(h.count(), 2u);
  EXPECT_DOUBLE_EQ(h.value_sum(), 30.0);
  EXPECT_EQ(&fam.noLabels(), &h);     // same series on repeat lookup
}

TEST(XrdMetricsSummary, DynamicSeriesDedupsFamily)
{
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("io");
  subsystem.summarySeries("sz", "sizes", {{"dir", "in"}}).observe(5);
  subsystem.summarySeries("sz", "sizes", {{"dir", "out"}}).observe(9);

  const std::string out = scrape(collector);
  // One family header, two series.
  EXPECT_EQ(out.find("# TYPE xrootd_io_sz summary\n"),
            out.rfind("# TYPE xrootd_io_sz summary\n"));
  EXPECT_NE(out.find("xrootd_io_sz_count{dir=\"in\"} 1\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_io_sz_count{dir=\"out\"} 1\n"), std::string::npos);
}

/******************************************************************************/
/*                            O b s e r v e d                                */
/******************************************************************************/

TEST(XrdMetricsObserved, ReadsSourceAtScrapeTime)
{
  Collector collector("xrootd");
  long long threads = 4;
  collector.subsystem("sched").observeGauge<std::int64_t>("threads", "worker threads")
                    .add({}, [&]{ return (int64_t)threads; });

  EXPECT_NE(scrape(collector).find("xrootd_sched_threads 4\n"), std::string::npos);

  threads = 9;   // the source changed; a later scrape reflects it
  EXPECT_NE(scrape(collector).find("xrootd_sched_threads 9\n"), std::string::npos);
}

TEST(XrdMetricsObserved, CounterKindAndConstLabels)
{
  Collector collector("xrootd", {{"instance", "h1"}});
  unsigned long long hits = 7;
  collector.subsystem("cache").observeCounter<std::uint64_t>("evictions_total", "evictions",
                                    {{"tier", "ram"}})
                    .add({}, [&]{ return hits; });

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("# TYPE xrootd_cache_evictions_total counter\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_cache_evictions_total{instance=\"h1\",tier=\"ram\"} 7\n"),
            std::string::npos);
}

TEST(XrdMetricsObserved, MultiSeriesUnderOneFamily)
{
  Collector collector("xrootd");
  unsigned long long open = 3, read = 5;
  collector.subsystem("ops").observeCounter<std::uint64_t>("total", "operations", {}, {"op"})
                  .add({"open"}, [&]{ return open; })
                  .add({"read"}, [&]{ return read; });

  const std::string out = scrape(collector);
  // One TYPE line for the family, one series per label value.
  size_t first = out.find("# TYPE xrootd_ops_total");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(out.find("# TYPE xrootd_ops_total", first + 1), std::string::npos);
  EXPECT_NE(out.find("xrootd_ops_total{op=\"open\"} 3\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_ops_total{op=\"read\"} 5\n"), std::string::npos);
}

/******************************************************************************/
/*              D y n a m i c   s e r i e s   (* S e r i e s )               */
/******************************************************************************/

TEST(XrdMetricsDynamic, GetOrCreateDedupsFamily)
{
  Collector collector("");   // empty prefix: names pass through verbatim
  auto& subsystem = collector.subsystem("");

  ++subsystem.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","stage"}});
  subsystem.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","stage"}}) += 2;
  ++subsystem.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","purge"}});
  subsystem.gaugeSeries("xrootd_collector_active", "active", {{"server","s1"}}) = 4;
  subsystem.histogramSeries("xrootd_collector_sz", "sizes", {10,100}, {{"op","read"}}).observe(50);

  const std::string out = scrape(collector);
  // One TYPE line for the deduped counter family.
  size_t first = out.find("# TYPE xrootd_collector_frm_total");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(out.find("# TYPE xrootd_collector_frm_total", first + 1), std::string::npos);
  EXPECT_NE(out.find("xrootd_collector_frm_total{server=\"s1\",op=\"stage\"} 3\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_collector_frm_total{server=\"s1\",op=\"purge\"} 1\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_collector_active{server=\"s1\"} 4\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_collector_sz_bucket{op=\"read\",le=\"100\"} 1\n"),
            std::string::npos);
}

TEST(XrdMetricsDynamic, TypeMismatchThrows)
{
  Collector collector("");
  auto& subsystem = collector.subsystem("");
  ++subsystem.counterSeries("m", "h");
  EXPECT_THROW(subsystem.gaugeSeries("m", "h"), std::invalid_argument);
}

// The *Series helpers are fed label values that are often remote-controlled, so
// their families are capped at kDynamicSeriesCap; combinations past the cap fold
// into a single __over_cardinality_limit__ overflow series instead of growing
// the series count without bound.
TEST(XrdMetricsDynamic, RemoteLabelsAreCardinalityCapped)
{
  Collector collector("");
  auto& subsystem = collector.subsystem("");

  for (std::size_t i = 0; i < XrdMetrics::kDynamicSeriesCap + 100; ++i)
      ++subsystem.counterSeries("evil_total", "h", {{"id", std::to_string(i)}});

  const std::string out = scrape(collector);
  EXPECT_NE(out.find("__over_cardinality_limit__"), std::string::npos);
}

/******************************************************************************/
/*                    R e g i s t r y   /   G r o u p                        */
/******************************************************************************/

TEST(XrdMetricsRegistryNG, GroupIsMemoized)
{
  Collector collector("xrootd");
  EXPECT_EQ(&collector.subsystem("ops"), &collector.subsystem("ops"));
}

TEST(XrdMetricsRegistryNG, FullNameComposition)
{
  Collector collector("xrootd");
  auto& fam = collector.subsystem("ops").counter<std::uint64_t>("requests_total");
  EXPECT_EQ(fam.name(), "xrootd_ops_requests_total");

  // Empty prefix and/or subsystem are skipped in the join.
  Collector bare("");
  auto& f2 = bare.subsystem("").counter<std::uint64_t>("just_a_name");
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
  Collector collector("xrootd");
  auto& reqs = collector.subsystem("ops").counter<std::uint64_t>("requests_total", "Requests processed", {}, {"verb"});
  reqs.withLabelValues({"open"}) += 3;
  reqs.withLabelValues({"read"}) += 5;

  std::string out = scrape(collector);
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
  Collector collector("xrootd");
  collector.subsystem("sched").gauge<std::int64_t>("threads").noLabels() = 8;

  std::string out = scrape(collector);
  EXPECT_EQ(out,
            "# TYPE xrootd_sched_threads gauge\n"
            "xrootd_sched_threads 8\n");
}

TEST(XrdMetricsPrometheus, BufferIsReusableAcrossScrapes)
{
  Collector collector("xrootd");
  Counter<std::uint64_t>& c = collector.subsystem("g").counter<std::uint64_t>("c").noLabels();
  ++c;

  std::string out;
  PrometheusTextSerializer ser(out);
  collector.serialize(ser);
  std::string first = out;

  out.clear();
  collector.serialize(ser);
  EXPECT_EQ(out, first);
}

/******************************************************************************/
/*                          O T e l   J S O N                                */
/******************************************************************************/

TEST(XrdMetricsOtel, EnvelopeAndMonotonicSum)
{
  Collector collector("xrootd");
  collector.subsystem("sched").counter<std::uint64_t>("jobs_total", "jobs scheduled")
                    .noLabels() += 3;

  const std::string out = scrapeOtel(collector);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_EQ(out.rfind("{\"resourceMetrics\":[{\"resource\":{},"
                      "\"scopeMetrics\":[{\"scope\":{\"name\":\"xrootd\"},"
                      "\"metrics\":[", 0), 0u);                 // starts with envelope
  EXPECT_NE(out.size(), 0u);
  EXPECT_EQ(out.compare(out.size() - 6, 6, "]}]}]}"), 0);       // closes envelope
  EXPECT_TRUE(contains(out, "\"name\":\"xrootd_sched_jobs_total\""));
  EXPECT_TRUE(contains(out, "\"description\":\"jobs scheduled\""));
  EXPECT_TRUE(contains(out, "\"sum\":{\"aggregationTemporality\":2,"
                            "\"isMonotonic\":true,\"dataPoints\":["));
  EXPECT_TRUE(contains(out, "\"attributes\":[],\"asInt\":\"3\","));
  EXPECT_TRUE(contains(out, "\"timeUnixNano\":\""));
}

TEST(XrdMetricsOtel, GaugeCarriesLabelsInOrder)
{
  Collector collector("xrootd", {{"instance", "h1"}});
  collector.subsystem("net").gauge<std::int64_t>("conns", "conns", {{"role", "server"}}, {"proto"})
                  .withLabelValues({"tcp"}) = 5;

  const std::string out = scrapeOtel(collector);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out, "\"gauge\":{\"dataPoints\":["));
  // forEachLabel order: global, family const, then variable.
  EXPECT_TRUE(contains(out,
      "\"attributes\":["
      "{\"key\":\"instance\",\"value\":{\"stringValue\":\"h1\"}},"
      "{\"key\":\"role\",\"value\":{\"stringValue\":\"server\"}},"
      "{\"key\":\"proto\",\"value\":{\"stringValue\":\"tcp\"}}],"
      "\"asInt\":\"5\","));
}

TEST(XrdMetricsOtel, FloatGaugeAndCounterUseAsDouble)
{
  Collector collector("xrootd");
  collector.subsystem("g").gauge<double>("temp", "t").noLabels() = 1.5;
  double cpu = 2.5;
  collector.subsystem("proc").observeCounter<double>("cpu_seconds_total", "cpu")
                   .add({}, [&]{ return cpu; });

  const std::string out = scrapeOtel(collector);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out, "\"gauge\":{\"dataPoints\":["
                            "{\"attributes\":[],\"asDouble\":1.5,"));
  EXPECT_TRUE(contains(out, "\"name\":\"xrootd_proc_cpu_seconds_total\""));
  EXPECT_TRUE(contains(out, "\"isMonotonic\":true,\"dataPoints\":["
                            "{\"attributes\":[],\"asDouble\":2.5,"));
}

TEST(XrdMetricsOtel, HistogramBucketsAreDecumulated)
{
  Collector collector("xrootd");
  auto& h = collector.subsystem("io").histogram("size_bytes", {1, 2, 5}, "io sizes");
  h.noLabels().observe(0.5);
  h.noLabels().observe(1.5);
  h.noLabels().observe(3);
  h.noLabels().observe(10);

  const std::string out = scrapeOtel(collector);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out, "\"histogram\":{\"aggregationTemporality\":2,"
                            "\"dataPoints\":["));
  EXPECT_TRUE(contains(out,
      "\"count\":\"4\",\"sum\":15,"
      "\"bucketCounts\":[\"1\",\"1\",\"1\",\"1\"],"
      "\"explicitBounds\":[1,2,5],"));
}

TEST(XrdMetricsOtel, SummaryCountAndSum)
{
  Collector collector("xrootd");
  auto& s = collector.subsystem("io").summary("bytes", "io sizes");
  s.noLabels().observe(100);
  s.noLabels().observe(250);

  const std::string out = scrapeOtel(collector);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out, "\"summary\":{\"dataPoints\":["));
  EXPECT_TRUE(contains(out, "\"count\":\"2\",\"sum\":350,"));
  EXPECT_FALSE(contains(out, "quantileValues"));
}

TEST(XrdMetricsOtel, ResourceAttributesAndEscaping)
{
  Collector collector("xrootd");
  collector.subsystem("g").gauge<std::int64_t>("x", "h", {}, {"label"})
                .withLabelValues({"a\"b\\c"}) = 1;

  const std::string out =
      scrapeOtel(collector, "xrootd", {{"service.instance.id", "node-7"}});
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out,
      "\"resource\":{\"attributes\":["
      "{\"key\":\"service.instance.id\","
      "\"value\":{\"stringValue\":\"node-7\"}}]}"));
  EXPECT_TRUE(contains(out, "\"stringValue\":\"a\\\"b\\\\c\""));
}

/******************************************************************************/
/*                          S n a p s h o t                                  */
/******************************************************************************/

TEST(XrdMetricsSnapshot, LooksUpValuesByName)
{
  Collector collector("xrootd");
  collector.subsystem("sched").counter<std::uint64_t>("jobs_total").noLabels() += 7;
  collector.subsystem("sched").gauge<std::int64_t>("threads").noLabels() = 12;
  long long idle = 3;
  collector.subsystem("sched").observeGauge<std::int64_t>("idle").add({}, [&]{ return (int64_t)idle; });
  collector.subsystem("proc").gauge<double>("load").noLabels() = 1.5;

  MetricSnapshot snap;
  collector.serialize(snap);

  EXPECT_EQ(snap.getInt("xrootd_sched_jobs_total"), 7);
  EXPECT_EQ(snap.getInt("xrootd_sched_threads"), 12);
  EXPECT_EQ(snap.getInt("xrootd_sched_idle"), 3);
  EXPECT_DOUBLE_EQ(snap.getDouble("xrootd_proc_load"), 1.5);
  EXPECT_EQ(snap.getInt("xrootd_proc_load"), 1);          // double truncated
  EXPECT_TRUE(snap.has("xrootd_sched_jobs_total"));
  EXPECT_FALSE(snap.has("xrootd_sched_missing"));
  EXPECT_EQ(snap.getInt("xrootd_sched_missing", -1), -1); // default
}

TEST(XrdMetricsSnapshot, KeysIncludeLabels)
{
  Collector collector("xrootd");
  auto& f = collector.subsystem("proc").counter<std::uint64_t>("cpu_seconds_total", {}, {}, {"mode"});
  f.withLabelValues({"user"})   += 4;
  f.withLabelValues({"system"}) += 9;

  MetricSnapshot snap;
  collector.serialize(snap);

  EXPECT_EQ(snap.getInt("xrootd_proc_cpu_seconds_total{mode=\"user\"}"), 4);
  EXPECT_EQ(snap.getInt("xrootd_proc_cpu_seconds_total{mode=\"system\"}"), 9);
}
