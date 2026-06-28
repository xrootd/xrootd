//------------------------------------------------------------------------------
// Unit tests for the XrdHttpMetricsExporter configuration helpers: directive
// parsing (XrdHttpMetricsExporterCfg::Parse over an XrdOucGatherConf seeded
// from a string) and Pushgateway URL construction. These exercise the pure
// logic factored out of the plugin, with no curl, threads, or registry.
//------------------------------------------------------------------------------

#include <string>

#include "XrdHttpMetricsExporter/XrdHttpMetricsExporterConfig.hh"
#include "XrdOuc/XrdOucGatherConf.hh"

#include <gtest/gtest.h>

using namespace XrdHttpMetricsExporterCfg;

namespace
{
// Parse a config blob through the real gatherer + Parse, as the plugin does at
// startup (full_lines is the level the plugin uses). useData feeds the blob
// directly so no temporary file is needed.
Config parse(const char *text)
{
   XrdOucGatherConf conf(DirectiveList(), nullptr);
   EXPECT_TRUE(conf.useData(text));
   Config cfg;
   Parse(conf, cfg);
   return cfg;
}
}

TEST(MetricsExporterConfig, Defaults)
{
   Config cfg;   // the struct's own defaults, before any parsing
   EXPECT_EQ(cfg.path, "/metrics");
   EXPECT_TRUE(cfg.instance.empty());
   EXPECT_TRUE(cfg.pushURL.empty());
   EXPECT_EQ(cfg.pushJob, "xrootd");
   EXPECT_EQ(cfg.pushEvery, 30);
   EXPECT_TRUE(cfg.otelURL.empty());
   EXPECT_EQ(cfg.otelEvery, 30);
}

TEST(MetricsExporterConfig, ParsesAllDirectives)
{
   Config cfg = parse(
      "metrics.path /m\n"
      "metrics.instance node42\n"
      "metrics.pushurl http://gw:9091\n"
      "metrics.pushinterval 15\n"
      "metrics.pushjob storage\n"
      "metrics.otelurl http://otel:4318/v1/metrics\n"
      "metrics.otelinterval 45\n");

   EXPECT_EQ(cfg.path, "/m");
   EXPECT_EQ(cfg.instance, "node42");
   EXPECT_EQ(cfg.pushURL, "http://gw:9091");
   EXPECT_EQ(cfg.pushEvery, 15);
   EXPECT_EQ(cfg.pushJob, "storage");
   EXPECT_EQ(cfg.otelURL, "http://otel:4318/v1/metrics");
   EXPECT_EQ(cfg.otelEvery, 45);
}

TEST(MetricsExporterConfig, KeepsDefaultsForAbsentDirectives)
{
   Config cfg = parse("metrics.otelurl http://otel:4318/v1/metrics\n");

   EXPECT_EQ(cfg.path, "/metrics");        // untouched default
   EXPECT_EQ(cfg.pushJob, "xrootd");       // untouched default
   EXPECT_TRUE(cfg.pushURL.empty());       // push stays disabled
   EXPECT_EQ(cfg.otelURL, "http://otel:4318/v1/metrics");
}

TEST(MetricsExporterConfig, RejectsNonPositiveIntervals)
{
   Config cfg = parse("metrics.pushinterval 0\n"
                      "metrics.otelinterval -5\n");

   EXPECT_EQ(cfg.pushEvery, 30);   // default kept, not clobbered by 0
   EXPECT_EQ(cfg.otelEvery, 30);   // default kept, not clobbered by -5
}

TEST(MetricsExporterConfig, IgnoresUnknownAndValuelessDirectives)
{
   Config cfg = parse("metrics.bogus whatever\n"
                      "metrics.pushurl\n"            // no value
                      "metrics.pushjob batch\n");

   EXPECT_TRUE(cfg.pushURL.empty());     // valueless line ignored
   EXPECT_EQ(cfg.pushJob, "batch");      // valid line still applied
}

TEST(MetricsExporterConfig, PushgatewayURLBuild)
{
   EXPECT_EQ(PushgatewayURL("http://gw:9091", "xrootd", "node1"),
             "http://gw:9091/metrics/job/xrootd/instance/node1");
}

TEST(MetricsExporterConfig, PushgatewayURLTrimsTrailingSlash)
{
   EXPECT_EQ(PushgatewayURL("http://gw:9091/", "xrootd", "node1"),
             "http://gw:9091/metrics/job/xrootd/instance/node1");
}
