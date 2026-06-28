//------------------------------------------------------------------------------
// XrdStatsLegacy renders the legacy "<stats id=...>" XML blocks from a snapshot
// of the new XrdMetrics registry. These tests populate a local registry with a
// distinct value per field (so a wrong metric-name mapping cannot be masked by a
// shared or zero value) and assert the rendered block matches the historical
// format byte-for-byte. The info and proc blocks do not read the registry.
//------------------------------------------------------------------------------

#include <string>

#include "Xrd/XrdStatsLegacy.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

#include <gtest/gtest.h>

using namespace XrdMetrics;

namespace
{
MetricSnapshot snapshotOf(Registry& reg)
{
  MetricSnapshot snap;
  reg.serialize(snap);
  return snap;
}
}

TEST(XrdStatsLegacy, InfoBlock)
{
  char buff[256];
  std::string xml(buff, XrdStatsLegacy::Info("host.example.org", 1094, "anon",
                                             buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"info\"><host>host.example.org</host>"
      "<port>1094</port><name>anon</name></stats>");
}

TEST(XrdStatsLegacy, ProcBlockShape)
{
  char buff[256];
  std::string xml(buff, XrdStatsLegacy::Proc(buff, sizeof(buff)));
  EXPECT_EQ(xml.rfind("<stats id=\"proc\"><usr><s>", 0), 0u);
  EXPECT_NE(xml.find("</u></usr><sys><s>"), std::string::npos);
  EXPECT_NE(xml.rfind("</u></sys></stats>"),
            std::string::npos);
}

TEST(XrdStatsLegacy, LinkBlockFromRegistry)
{
  Registry reg("xrootd");
  auto& g = reg.group("link");
  g.intGauge("connections").noLabels()      = 3;
  g.intGauge("connections_max").noLabels()  = 7;
  g.counter("connections_total").noLabels() += 100;
  auto& bytes = g.counter("bytes_total", {"dir"});
  bytes.withLabelValues({"in"})  += 4096;
  bytes.withLabelValues({"out"}) += 8192;
  g.counter("connect_seconds_total").noLabels()     += 60;
  g.counter("timeouts_total").noLabels()            += 2;
  g.counter("stalls_total").noLabels()              += 1;
  g.counter("sendfile_interrupts_total").noLabels() += 5;

  MetricSnapshot snap = snapshotOf(reg);
  char buff[512];
  std::string xml(buff, XrdStatsLegacy::Link(snap, buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"link\"><num>3</num><maxn>7</maxn><tot>100</tot>"
      "<in>4096</in><out>8192</out><ctime>60</ctime><tmo>2</tmo>"
      "<stall>1</stall><sfps>5</sfps></stats>");
}

TEST(XrdStatsLegacy, PollBlockFromRegistry)
{
  Registry reg("xrootd");
  auto& g = reg.group("poll");
  g.intGauge("attached").noLabels()        = 10;
  g.intGauge("enabled").noLabels()         = 8;
  g.counter("events_total").noLabels()     += 200;
  g.counter("interrupts_total").noLabels() += 4;

  MetricSnapshot snap = snapshotOf(reg);
  char buff[256];
  std::string xml(buff, XrdStatsLegacy::Poll(snap, buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"poll\"><att>10</att><en>8</en><ev>200</ev>"
      "<int>4</int></stats>");
}

TEST(XrdStatsLegacy, BuffBlockSplicesXlStats)
{
  Registry reg("xrootd");
  auto& g = reg.group("buff");
  g.counter("requests_total").noLabels()    += 50;
  g.intGauge("memory_bytes").noLabels()     = 1048576;
  g.intGauge("buffers").noLabels()          = 12;
  g.counter("adjustments_total").noLabels() += 3;

  MetricSnapshot snap = snapshotOf(reg);
  char buff[512];
  std::string xml(buff, XrdStatsLegacy::Buff(snap,
      "<stats id=\"buffxl\"><x>1</x></stats>", buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"buff\"><reqs>50</reqs><mem>1048576</mem>"
      "<buffs>12</buffs><adj>3</adj>"
      "<stats id=\"buffxl\"><x>1</x></stats></stats>");
}
