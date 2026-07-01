//------------------------------------------------------------------------------
// Unit tests for serialize-time subsystem filtering and multi-registry
// aggregation: serializeBody() with a group filter (the exporter's per-subsystem
// enable/disable), the OTLP one-resource-block-per-registry shape, and the
// process-wide registry directory (register/unregister and serializeAll).
//------------------------------------------------------------------------------

#include <algorithm>
#include <string>
#include <vector>

#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

#include <gtest/gtest.h>

using namespace XrdMetrics;

namespace
{
bool contains(const std::string& hay, const std::string& needle)
{
   return hay.find(needle) != std::string::npos;
}

size_t count(const std::string& hay, const std::string& needle)
{
   size_t n = 0, p = 0;
   while ((p = hay.find(needle, p)) != std::string::npos) {++n; p += needle.size();}
   return n;
}

bool inDirectory(Collector* r)
{
   auto regs = registries();
   return std::find(regs.begin(), regs.end(), r) != regs.end();
}
}

/******************************************************************************/
/*                       S u b s y s t e m   f i l t e r                     */
/******************************************************************************/

TEST(MetricsFilter, SkipsDisabledGroupPrometheus)
{
   Collector reg("xrootd");
   reg.subsystem("sched").counter("jobs_total", {}, {}, "jobs").noLabels() += 1;
   reg.subsystem("link").counter("total", {}, {}, "links").noLabels() += 2;

   std::string out;
   PrometheusTextSerializer ser(out);
   reg.serializeBody(ser, [](const std::string& g){return g != "sched";});

   EXPECT_TRUE(contains(out, "xrootd_link_total"));
   EXPECT_FALSE(contains(out, "xrootd_sched_jobs_total"));
   EXPECT_FALSE(contains(out, "sched"));   // no leftover # TYPE/# HELP header
}

TEST(MetricsFilter, EmptyFilterEmitsEverything)
{
   Collector reg("xrootd");
   reg.subsystem("sched").counter("jobs_total", {}, {}, "jobs").noLabels() += 1;
   reg.subsystem("link").counter("total", {}, {}, "links").noLabels() += 2;

   std::string out;
   PrometheusTextSerializer ser(out);
   reg.serializeBody(ser);   // no filter

   EXPECT_TRUE(contains(out, "xrootd_link_total"));
   EXPECT_TRUE(contains(out, "xrootd_sched_jobs_total"));
}

/******************************************************************************/
/*                   M u l t i - r e s o u r c e   O T L P                   */
/******************************************************************************/

TEST(MetricsAggregate, OtelOneResourcePerRegistry)
{
   Collector a("xrootd", {{"program", "xrootd"}});
   a.subsystem("sched").counter("jobs_total", {}, {}, "jobs").noLabels() += 1;

   Collector b("eos", {{"cluster", "prod"}});
   b.subsystem("ns").counter("files_total", {}, {}, "files").noLabels() += 5;

// Drive the serializer exactly as serializeAll() does, but over chosen
// registries so the result is independent of the process-wide directory.
//
   std::string out;
   OtelJsonSerializer ser(out);
   ser.begin();
   ser.beginResource(a.prefix(), a.globalLabels()); a.serializeBody(ser); ser.endResource();
   ser.beginResource(b.prefix(), b.globalLabels()); b.serializeBody(ser); ser.endResource();
   ser.end();

   EXPECT_EQ(count(out, "\"scopeMetrics\""), 2u);          // one block per registry
   EXPECT_EQ(count(out, "{\"resourceMetrics\":["), 1u);    // exactly one envelope
   EXPECT_TRUE(contains(out, "{\"key\":\"program\",\"value\":{\"stringValue\":\"xrootd\"}}"));
   EXPECT_TRUE(contains(out, "{\"key\":\"cluster\",\"value\":{\"stringValue\":\"prod\"}}"));
   EXPECT_TRUE(contains(out, "\"name\":\"xrootd_sched_jobs_total\""));
   EXPECT_TRUE(contains(out, "\"name\":\"eos_ns_files_total\""));
   EXPECT_TRUE(contains(out, "]}"));                       // closes cleanly
}

/******************************************************************************/
/*                     R e g i s t r y   d i r e c t o r y                   */
/******************************************************************************/

TEST(MetricsDirectory, RegisterIsIdempotentAndUnregisters)
{
   {Collector r("probe");
    EXPECT_FALSE(inDirectory(&r));
    registerRegistry(r);
    registerRegistry(r);                       // idempotent
    auto regs = registries();
    EXPECT_EQ(std::count(regs.begin(), regs.end(), &r), 1);
    unregisterRegistry(r);
    EXPECT_FALSE(inDirectory(&r));
    registerRegistry(r);                        // re-register for dtor path
   }
   // r is destroyed here; its destructor must remove it from the directory.
   // (No direct handle remains; rely on no dangling pointer in later scrapes.)
}

TEST(MetricsDirectory, SerializeAllAggregatesRegisteredRegistries)
{
   Collector a("alpha");
   a.subsystem("g").counter("c_total", {}, {}, "h").noLabels() += 1;
   Collector b("beta");
   b.subsystem("g").counter("c_total", {}, {}, "h").noLabels() += 2;

   registerRegistry(a);
   registerRegistry(b);

   std::string out;
   PrometheusTextSerializer ser(out);
   serializeAll(ser);   // walks the directory (Default + alpha + beta)

   EXPECT_TRUE(contains(out, "alpha_g_c_total"));
   EXPECT_TRUE(contains(out, "beta_g_c_total"));

   unregisterRegistry(a);
   unregisterRegistry(b);
   EXPECT_FALSE(inDirectory(&a));
   EXPECT_FALSE(inDirectory(&b));
}

TEST(MetricsDirectory, DefaultIsRegistered)
{
   (void)Default();
   EXPECT_TRUE(inDirectory(&Default()));
}
