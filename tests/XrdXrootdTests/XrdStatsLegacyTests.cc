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
  auto& g = reg.subsystem("link");
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
  auto& g = reg.subsystem("poll");
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
  auto& g = reg.subsystem("buff");
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

TEST(XrdStatsLegacy, XrootdBlockFromRegistry)
{
  // Each field gets a distinct value (its position in the format) so a wrong
  // metric-name mapping cannot be masked.
  Registry reg("xrootd");
  auto& g = reg.subsystem("");                 // empty subsystem -> flat names
  g.counter("requests_total").noLabels() += 1;

  auto& ops = g.counter("ops_total", {"op"});
  ops.withLabelValues({"open"})    += 2;
  ops.withLabelValues({"refresh"}) += 3;
  ops.withLabelValues({"read"})    += 4;
  ops.withLabelValues({"preread"}) += 5;
  ops.withLabelValues({"readv"})   += 6;
  ops.withLabelValues({"writev"})  += 8;
  ops.withLabelValues({"write"})   += 10;
  ops.withLabelValues({"sync"})    += 11;
  ops.withLabelValues({"getfile"}) += 12;
  ops.withLabelValues({"putfile"}) += 13;
  ops.withLabelValues({"misc"})    += 14;
  g.counter("readv_segments_total").noLabels()  += 7;
  g.counter("writev_segments_total").noLabels() += 9;

  auto& sig = g.counter("signatures_total", {"result"});
  sig.withLabelValues({"ok"})      += 15;
  sig.withLabelValues({"bad"})     += 16;
  sig.withLabelValues({"ignored"}) += 17;

  g.counter("async_ops_total").noLabels()      += 18;
  g.intGauge("async_max").noLabels()           = 19;
  g.counter("async_rejected_total").noLabels() += 20;
  g.counter("errors_total").noLabels()         += 21;
  g.counter("redirects_total").noLabels()      += 22;
  g.counter("stalls_total").noLabels()         += 23;

  auto& lgn = g.counter("logins_total", {"result"});
  lgn.withLabelValues({"attempt"})  += 24;
  lgn.withLabelValues({"authfail"}) += 25;
  lgn.withLabelValues({"auth"})     += 26;
  lgn.withLabelValues({"noauth"})   += 27;

  MetricSnapshot snap = snapshotOf(reg);
  char buff[1024];
  std::string xml(buff, XrdStatsLegacy::Xrootd(snap, buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"xrootd\"><num>1</num>"
      "<ops><open>2</open><rf>3</rf><rd>4</rd><pr>5</pr>"
      "<rv>6</rv><rs>7</rs>"
      "<wv>8</wv><ws>9</ws><wr>10</wr>"
      "<sync>11</sync><getf>12</getf><putf>13</putf><misc>14</misc></ops>"
      "<sig><ok>15</ok><bad>16</bad><ign>17</ign></sig>"
      "<aio><num>18</num><max>19</max><rej>20</rej></aio>"
      "<err>21</err><rdr>22</rdr><dly>23</dly>"
      "<lgn><num>24</num><af>25</af><au>26</au><ua>27</ua></lgn></stats>");
}

TEST(XrdStatsLegacy, OfsBlockFromRegistry)
{
  Registry reg("xrootd");
  auto& g = reg.subsystem("ofs");
  auto& open = g.intGauge("files_open", {"mode"});
  open.withLabelValues({"read"})  = 1;
  open.withLabelValues({"write"}) = 2;
  open.withLabelValues({"posc"})  = 3;
  g.counter("unpersisted_total").noLabels() += 4;
  g.intGauge("handles").noLabels()          = 5;
  g.counter("redirects_total").noLabels()   += 6;
  g.counter("started_total").noLabels()     += 7;
  g.counter("replies_total").noLabels()     += 8;
  g.counter("errors_total").noLabels()      += 9;
  g.counter("delays_total").noLabels()      += 10;
  auto& ev = g.counter("events_total", {"result"});
  ev.withLabelValues({"ok"})    += 11;
  ev.withLabelValues({"error"}) += 12;
  auto& tpc = g.counter("tpc_total", {"result"});
  tpc.withLabelValues({"granted"}) += 13;
  tpc.withLabelValues({"denied"})  += 14;
  tpc.withLabelValues({"error"})   += 15;
  tpc.withLabelValues({"expired"}) += 16;

  MetricSnapshot snap = snapshotOf(reg);
  char buff[512];
  std::string xml(buff, XrdStatsLegacy::Ofs(snap, "server", buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"ofs\"><role>server</role>"
      "<opr>1</opr><opw>2</opw><opp>3</opp><ups>4</ups><han>5</han>"
      "<rdr>6</rdr><bxq>7</bxq><rep>8</rep><err>9</err><dly>10</dly>"
      "<sok>11</sok><ser>12</ser>"
      "<tpc><grnt>13</grnt><deny>14</deny><err>15</err><exp>16</exp></tpc>"
      "</stats>");
}
