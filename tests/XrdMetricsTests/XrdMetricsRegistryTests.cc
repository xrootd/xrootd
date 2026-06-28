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

// Serialize a registry to the OTLP/JSON format and return it.
std::string scrapeOtel(const Registry& reg, std::string scope = "xrootd",
                       std::vector<ConstLabel> res = {})
{
  std::string out;
  OtelJsonSerializer ser(out, std::move(scope), std::move(res));
  reg.serialize(ser);
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
/*                           H i s t o g r a m                               */
/******************************************************************************/

TEST(XrdMetricsHistogram, BucketsCumulativeAndLabels)
{
  Registry reg("xrootd");
  auto& h = reg.group("io").histogram("size_bytes", {1, 2, 5}, {}, {},
                                      "io sizes");
  h.noLabels().observe(0.5);
  h.noLabels().observe(1.5);
  h.noLabels().observe(3);
  h.noLabels().observe(10);

  const std::string out = scrape(reg);
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
  Registry reg("xrootd");
  reg.group("io").histogram("d", {1}, {"op"}, {}, "h")
                 .withLabelValues({"read"}).observe(0.5);

  const std::string out = scrape(reg);
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
  Registry reg("xrootd");
  auto& s = reg.group("xrootd").summary("request_bytes", {}, {}, "request sizes");
  s.noLabels().observe(100);
  s.noLabels().observe(250);
  s.noLabels().observe(50);

  const std::string out = scrape(reg);
  EXPECT_NE(out.find("# TYPE xrootd_xrootd_request_bytes summary\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_xrootd_request_bytes_sum 400\n"), std::string::npos);
  EXPECT_NE(out.find("xrootd_xrootd_request_bytes_count 3\n"), std::string::npos);
  // A quantile-less summary emits no per-quantile series.
  EXPECT_EQ(out.find("quantile="), std::string::npos);
}

TEST(XrdMetricsSummary, LabeledSeriesCarryLabels)
{
  Registry reg("xrootd");
  reg.group("io").summary("latency_seconds", {"op"}, {}, "op latency")
                 .withLabelValues({"read"}).observe(0.25);

  const std::string out = scrape(reg);
  EXPECT_NE(out.find("xrootd_io_latency_seconds_sum{op=\"read\"} 0.25\n"),
            std::string::npos);
  EXPECT_NE(out.find("xrootd_io_latency_seconds_count{op=\"read\"} 1\n"),
            std::string::npos);
}

TEST(XrdMetricsSummary, CachedHandleAccumulates)
{
  Registry reg("xrootd");
  auto& fam = reg.group("io").summary("bytes", {});
  Summary& h = fam.noLabels();        // cache the handle, then reuse it
  h.observe(10);
  h.observe(20);
  EXPECT_EQ(h.count(), 2u);
  EXPECT_DOUBLE_EQ(h.value_sum(), 30.0);
  EXPECT_EQ(&fam.noLabels(), &h);     // same series on repeat lookup
}

TEST(XrdMetricsSummary, DynamicSeriesDedupsFamily)
{
  Registry reg("xrootd");
  auto& g = reg.group("io");
  g.summarySeries("sz", "sizes", {{"dir", "in"}}).observe(5);
  g.summarySeries("sz", "sizes", {{"dir", "out"}}).observe(9);

  const std::string out = scrape(reg);
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
/*              D y n a m i c   s e r i e s   (* S e r i e s )               */
/******************************************************************************/

TEST(XrdMetricsDynamic, GetOrCreateDedupsFamily)
{
  Registry reg("");   // empty prefix: names pass through verbatim
  auto& g = reg.group("");

  ++g.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","stage"}});
  g.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","stage"}}) += 2;
  ++g.counterSeries("xrootd_collector_frm_total", "frm", {{"server","s1"},{"op","purge"}});
  g.gaugeSeries("xrootd_collector_active", "active", {{"server","s1"}}) = 4;
  g.histogramSeries("xrootd_collector_sz", "sizes", {10,100}, {{"op","read"}}).observe(50);

  const std::string out = scrape(reg);
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
  Registry reg("");
  auto& g = reg.group("");
  ++g.counterSeries("m", "h");
  EXPECT_THROW(g.gaugeSeries("m", "h"), std::invalid_argument);
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

/******************************************************************************/
/*                          O T e l   J S O N                                */
/******************************************************************************/

TEST(XrdMetricsOtel, EnvelopeAndMonotonicSum)
{
  Registry reg("xrootd");
  reg.group("sched").counter("jobs_total", {}, {}, "jobs scheduled")
                    .noLabels() += 3;

  const std::string out = scrapeOtel(reg);
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
  Registry reg("xrootd", {{"instance", "h1"}});
  reg.group("net").intGauge("conns", {"proto"}, {{"role", "server"}}, "conns")
                  .withLabelValues({"tcp"}) = 5;

  const std::string out = scrapeOtel(reg);
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
  Registry reg("xrootd");
  reg.group("g").floatGauge("temp", {}, {}, "t").noLabels() = 1.5;
  double cpu = 2.5;
  reg.group("proc").observeCounterF("cpu_seconds_total", {}, {}, "cpu")
                   .add({}, [&]{ return cpu; });

  const std::string out = scrapeOtel(reg);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out, "\"gauge\":{\"dataPoints\":["
                            "{\"attributes\":[],\"asDouble\":1.5,"));
  EXPECT_TRUE(contains(out, "\"name\":\"xrootd_proc_cpu_seconds_total\""));
  EXPECT_TRUE(contains(out, "\"isMonotonic\":true,\"dataPoints\":["
                            "{\"attributes\":[],\"asDouble\":2.5,"));
}

TEST(XrdMetricsOtel, HistogramBucketsAreDecumulated)
{
  Registry reg("xrootd");
  auto& h = reg.group("io").histogram("size_bytes", {1, 2, 5}, {}, {}, "io sizes");
  h.noLabels().observe(0.5);
  h.noLabels().observe(1.5);
  h.noLabels().observe(3);
  h.noLabels().observe(10);

  const std::string out = scrapeOtel(reg);
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
  Registry reg("xrootd");
  auto& s = reg.group("io").summary("bytes", {}, {}, "io sizes");
  s.noLabels().observe(100);
  s.noLabels().observe(250);

  const std::string out = scrapeOtel(reg);
  EXPECT_TRUE(jsonBalanced(out));
  EXPECT_TRUE(contains(out, "\"summary\":{\"dataPoints\":["));
  EXPECT_TRUE(contains(out, "\"count\":\"2\",\"sum\":350,"));
  EXPECT_FALSE(contains(out, "quantileValues"));
}

TEST(XrdMetricsOtel, ResourceAttributesAndEscaping)
{
  Registry reg("xrootd");
  reg.group("g").intGauge("x", {"label"}, {}, "h")
                .withLabelValues({"a\"b\\c"}) = 1;

  const std::string out =
      scrapeOtel(reg, "xrootd", {{"service.instance.id", "node-7"}});
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
  Registry reg("xrootd");
  reg.group("sched").counter("jobs_total").noLabels() += 7;
  reg.group("sched").intGauge("threads").noLabels() = 12;
  long long idle = 3;
  reg.group("sched").observeIntGauge("idle").add({}, [&]{ return (int64_t)idle; });
  reg.group("proc").floatGauge("load").noLabels() = 1.5;

  MetricSnapshot snap;
  reg.serialize(snap);

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
  Registry reg("xrootd");
  auto& f = reg.group("proc").counter("cpu_seconds_total", {"mode"});
  f.withLabelValues({"user"})   += 4;
  f.withLabelValues({"system"}) += 9;

  MetricSnapshot snap;
  reg.serialize(snap);

  EXPECT_EQ(snap.getInt("xrootd_proc_cpu_seconds_total{mode=\"user\"}"), 4);
  EXPECT_EQ(snap.getInt("xrootd_proc_cpu_seconds_total{mode=\"system\"}"), 9);
}
