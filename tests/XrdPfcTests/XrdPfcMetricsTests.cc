//------------------------------------------------------------------------------
// Cache metrics test (group "cache").
//
// XrdPfc::CacheMetrics is self-contained: it owns the registry handles and is
// fed deltas/events by the ResourceMonitor aggregation boundary. The plugin
// itself is a loadable module, so the source is compiled directly into this test
// (see CMakeLists). The test drives the same update methods the monitor calls
// and asserts both the metric shape and that the folded values land on the right
// labelled series.
//------------------------------------------------------------------------------

#include <string>

#include "XrdPfc/XrdPfcMetrics.hh"
#include "XrdPfc/XrdPfcStats.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

#include <gtest/gtest.h>

namespace
{
std::string scrape(XrdMetrics::Registry &reg)
{
   std::string out;
   XrdMetrics::PrometheusTextSerializer ser(out);
   reg.serialize(ser);
   return out;
}
}

TEST(XrdPfcMetrics, RegistersAndFoldsValues)
{
   XrdMetrics::Registry reg("xrootd");
   XrdPfc::CacheMetrics m(reg);

   XrdPfc::Stats d;
   d.m_BytesHit      = 100;
   d.m_BytesMissed   = 200;
   d.m_BytesBypassed = 50;
   d.m_BytesWritten  = 175;
   d.m_NCksumErrors  = 2;
   m.AddFileStats(d);

   m.FileOpened(/*existing*/false);   // opened + created
   m.FileOpened(/*existing*/true);    // opened only
   m.FileClosed();
   m.FilesRemoved(3);

   m.SetSpace(/*disk_total*/1000, /*disk_used*/400, /*data_used*/300,
              /*meta_total*/80,   /*meta_used*/20);

   const std::string out = scrape(reg);

// Byte counters by origin and the standalone counters.
//
   EXPECT_NE(out.find("xrootd_cache_bytes_total{origin=\"hit\"} 100\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_bytes_total{origin=\"miss\"} 200\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_bytes_total{origin=\"bypass\"} 50\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_bytes_written_total 175\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_checksum_errors_total 2\n"),
             std::string::npos);

// File lifecycle counts.
//
   EXPECT_NE(out.find("xrootd_cache_files_total{event=\"opened\"} 2\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_files_total{event=\"created\"} 1\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_files_total{event=\"closed\"} 1\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_files_total{event=\"removed\"} 3\n"),
             std::string::npos);

// Space gauges (bytes).
//
   EXPECT_NE(out.find("xrootd_cache_disk_bytes{state=\"total\"} 1000\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_disk_bytes{state=\"used\"} 400\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_data_used_bytes 300\n"), std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_meta_bytes{state=\"total\"} 80\n"),
             std::string::npos);
   EXPECT_NE(out.find("xrootd_cache_meta_bytes{state=\"used\"} 20\n"),
             std::string::npos);
}
