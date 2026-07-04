//------------------------------------------------------------------------------
// Unit tests for the process-wide metrics configuration (XrdMetrics::Config):
// directive parsing over an XrdOucGatherConf seeded from a string (as the core
// does at startup), subsystem enable/disable evaluation, global-label
// composition (auto-seeded from XRDPROG/XRDROLE plus metrics.label), and
// Pushgateway URL construction. These exercise the pure parse/predicate logic
// with no curl, threads, or registry mutation (Load() is not called).
//------------------------------------------------------------------------------

#include <cstdlib>
#include <string>

#include "XrdMetrics/XrdMetricsConfig.hh"
#include "XrdOuc/XrdOucGatherConf.hh"

#include <gtest/gtest.h>

using namespace XrdMetrics;

namespace
{
// Parse a config blob through the real gatherer + Config::parse, as the core
// does (full_lines is the gather level used). useData feeds the blob directly so
// no temporary file is needed.
Config parse(const char *text)
{
   XrdOucGatherConf conf(Config::DirectiveList(), nullptr);
   EXPECT_TRUE(conf.useData(text));
   Config cfg;
   cfg.parse(conf);
   return cfg;
}

bool hasLabel(const std::vector<ConstLabel>& v, const std::string& k,
              const std::string& val)
{
   for (auto& kv : v) if (kv.first == k) return kv.second == val;
   return false;
}
}

TEST(MetricsConfig, Defaults)
{
   Config cfg;
   EXPECT_TRUE(cfg.enabled);
   EXPECT_EQ(cfg.path, "/metrics");
   EXPECT_TRUE(cfg.instance.empty());
   EXPECT_TRUE(cfg.pushURL.empty());
   EXPECT_EQ(cfg.pushJob, "xrootd");
   EXPECT_EQ(cfg.pushEvery, 30);
   EXPECT_TRUE(cfg.otelURL.empty());
   EXPECT_EQ(cfg.otelEvery, 30);
   EXPECT_TRUE(cfg.disabledSubsys.empty());
   EXPECT_TRUE(cfg.enabledSubsys.empty());
}

TEST(MetricsConfig, ParsesExporterDirectives)
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

TEST(MetricsConfig, KeepsDefaultsForAbsentDirectives)
{
   Config cfg = parse("metrics.otelurl http://otel:4318/v1/metrics\n");

   EXPECT_EQ(cfg.path, "/metrics");
   EXPECT_EQ(cfg.pushJob, "xrootd");
   EXPECT_TRUE(cfg.pushURL.empty());
   EXPECT_EQ(cfg.otelURL, "http://otel:4318/v1/metrics");
}

TEST(MetricsConfig, RejectsNonPositiveIntervals)
{
   Config cfg = parse("metrics.pushinterval 0\n"
                      "metrics.otelinterval -5\n");

   EXPECT_EQ(cfg.pushEvery, 30);
   EXPECT_EQ(cfg.otelEvery, 30);
}

TEST(MetricsConfig, IgnoresUnknownAndValuelessDirectives)
{
   Config cfg = parse("metrics.bogus whatever\n"
                      "metrics.pushurl\n"            // no value
                      "metrics.pushjob batch\n");

   EXPECT_TRUE(cfg.pushURL.empty());
   EXPECT_EQ(cfg.pushJob, "batch");
}

TEST(MetricsConfig, MasterEnableParsed)
{
   EXPECT_FALSE(parse("metrics.enable no\n").enabled);
   EXPECT_FALSE(parse("metrics.enable false\n").enabled);
   EXPECT_TRUE (parse("metrics.enable yes\n").enabled);
   EXPECT_TRUE (parse("metrics.enable 1\n").enabled);
}

TEST(MetricsConfig, SubsystemsAllowAndDeny)
{
   Config cfg = parse("metrics.subsystems +sched -proxy link\n");

   EXPECT_EQ(cfg.disabledSubsys.count("proxy"), 1u);
   EXPECT_EQ(cfg.enabledSubsys.count("sched"), 1u);
   EXPECT_EQ(cfg.enabledSubsys.count("link"), 1u);   // bare token = allow
}

TEST(MetricsConfig, SubsystemEnabledDenyWins)
{
   Config cfg = parse("metrics.subsystems -sched\n");
   EXPECT_FALSE(cfg.subsystemEnabled("sched"));
   EXPECT_TRUE (cfg.subsystemEnabled("link"));   // no allow-list => default on
}

TEST(MetricsConfig, SubsystemEnabledAllowListRestricts)
{
   Config cfg = parse("metrics.subsystems sched\n");
   EXPECT_TRUE (cfg.subsystemEnabled("sched"));
   EXPECT_FALSE(cfg.subsystemEnabled("link"));    // not in allow-list
}

TEST(MetricsConfig, MasterSwitchOffDisablesAll)
{
   Config cfg = parse("metrics.enable no\n"
                      "metrics.subsystems sched\n");
   EXPECT_FALSE(cfg.subsystemEnabled("sched"));    // master off beats allow-list
}

TEST(MetricsConfig, GlobalLabelsAutoSeedAndOverride)
{
   setenv("XRDPROG", "cmsd", 1);
   setenv("XRDROLE", "supervisor", 1);

   Config cfg = parse("metrics.label cluster prod-eu\n"
                      "all.role server\n");
   auto labels = cfg.buildGlobalLabels();

   EXPECT_TRUE(hasLabel(labels, "program", "cmsd"));        // from XRDPROG
   EXPECT_TRUE(hasLabel(labels, "role", "supervisor"));     // XRDROLE beats all.role
   EXPECT_TRUE(hasLabel(labels, "cluster", "prod-eu"));     // user label

   unsetenv("XRDPROG");
   unsetenv("XRDROLE");
}

TEST(MetricsConfig, RoleFallsBackToAllRole)
{
   unsetenv("XRDROLE");
   Config cfg = parse("all.role manager\n");
   EXPECT_TRUE(hasLabel(cfg.buildGlobalLabels(), "role", "manager"));
}

TEST(MetricsConfig, UserLabelOverridesAutoSeed)
{
   setenv("XRDPROG", "xrootd", 1);
   Config cfg = parse("metrics.label program frontend\n");
   EXPECT_TRUE(hasLabel(cfg.buildGlobalLabels(), "program", "frontend"));
   unsetenv("XRDPROG");
}

TEST(MetricsConfig, PushgatewayURLBuild)
{
   EXPECT_EQ(Config::PushgatewayURL("http://gw:9091", "xrootd", "node1"),
             "http://gw:9091/metrics/job/xrootd/instance/node1");
}

TEST(MetricsConfig, PushgatewayURLTrimsTrailingSlash)
{
   EXPECT_EQ(Config::PushgatewayURL("http://gw:9091/", "xrootd", "node1"),
             "http://gw:9091/metrics/job/xrootd/instance/node1");
}

TEST(MetricsConfig, ScrapeTTLDefault)
{
   Config cfg;
   EXPECT_EQ(cfg.scrapeTTL, 10);
}

TEST(MetricsConfig, ScrapeTTLParsed)
{
   EXPECT_EQ(parse("metrics.scrapettl 5\n").scrapeTTL, 5);
   EXPECT_EQ(parse("metrics.scrapettl 0\n").scrapeTTL, 0);   // 0 = disabled
   EXPECT_EQ(parse("metrics.scrapettl 60\n").scrapeTTL, 60);
}

TEST(MetricsConfig, ScrapeTTLNegativeIgnored)
{
   Config cfg = parse("metrics.scrapettl -1\n");
   EXPECT_EQ(cfg.scrapeTTL, 10);   // keeps default
}

TEST(MetricsConfig, ScrapeRateLimitDefault)
{
   Config cfg;
   EXPECT_EQ(cfg.scrapeRateLimit, 100);
}

TEST(MetricsConfig, ScrapeRateLimitParsed)
{
   EXPECT_EQ(parse("metrics.scraperatelimit 50\n").scrapeRateLimit, 50);
   EXPECT_EQ(parse("metrics.scraperatelimit 0\n").scrapeRateLimit, 0);   // 0 = unlimited
   EXPECT_EQ(parse("metrics.scraperatelimit 200\n").scrapeRateLimit, 200);
}

TEST(MetricsConfig, ScrapeRateLimitNegativeIgnored)
{
   Config cfg = parse("metrics.scraperatelimit -10\n");
   EXPECT_EQ(cfg.scrapeRateLimit, 100);   // keeps default
}

TEST(MetricsConfig, DefaultsIncludeScrapeLimits)
{
   Config cfg;
   EXPECT_EQ(cfg.scrapeTTL, 10);
   EXPECT_EQ(cfg.scrapeRateLimit, 100);
}

// --- Auth directives --------------------------------------------------------

TEST(MetricsConfig, RequireAuthDefault)
{
   Config cfg;
   EXPECT_FALSE(cfg.requireAuth);
}

TEST(MetricsConfig, RequireAuthParsed)
{
   EXPECT_TRUE (parse("metrics.requireauth yes\n").requireAuth);
   EXPECT_TRUE (parse("metrics.requireauth 1\n").requireAuth);
   EXPECT_TRUE (parse("metrics.requireauth true\n").requireAuth);
   EXPECT_FALSE(parse("metrics.requireauth no\n").requireAuth);
   EXPECT_FALSE(parse("metrics.requireauth false\n").requireAuth);
   EXPECT_FALSE(parse("metrics.requireauth 0\n").requireAuth);
}

TEST(MetricsConfig, AuthTokenDefault)
{
   Config cfg;
   EXPECT_TRUE(cfg.authToken.empty());
}

TEST(MetricsConfig, AuthTokenParsed)
{
   EXPECT_EQ(parse("metrics.authtoken mysecret\n").authToken, "mysecret");
   EXPECT_EQ(parse("metrics.authtoken abc123\n").authToken, "abc123");
}

TEST(MetricsConfig, AuthTokenEmptyValueIgnored)
{
   // Directive with no value leaves field at default.
   Config cfg = parse("metrics.authtoken\n");
   EXPECT_TRUE(cfg.authToken.empty());
}

TEST(MetricsConfig, AuthPasswordDefault)
{
   Config cfg;
   EXPECT_TRUE(cfg.authPassword.empty());
}

TEST(MetricsConfig, AuthPasswordParsed)
{
   EXPECT_EQ(parse("metrics.authpassword hunter2\n").authPassword, "hunter2");
   EXPECT_EQ(parse("metrics.authpassword s3cr3t\n").authPassword, "s3cr3t");
}

TEST(MetricsConfig, AuthPasswordEmptyValueIgnored)
{
   Config cfg = parse("metrics.authpassword\n");
   EXPECT_TRUE(cfg.authPassword.empty());
}

TEST(MetricsConfig, AuthDirectivesIndependent)
{
   // All three auth directives can coexist.
   Config cfg = parse("metrics.requireauth yes\n"
                      "metrics.authtoken tok\n"
                      "metrics.authpassword pw\n");
   EXPECT_TRUE (cfg.requireAuth);
   EXPECT_EQ   (cfg.authToken,    "tok");
   EXPECT_EQ   (cfg.authPassword, "pw");
}
