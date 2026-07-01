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
MetricSnapshot snapshotOf(Collector& collector)
{
  MetricSnapshot snap;
  collector.serialize(snap);
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
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("link");
  subsystem.gauge<std::int64_t>("connections")      = 3;
  subsystem.gauge<std::int64_t>("connections_max")  = 7;
  subsystem.counter<std::uint64_t>("connections_total") += 100;
  auto& bytes = subsystem.counterFamily<std::uint64_t>("bytes_total", {}, {"dir"});
  bytes.labels({"in"})  += 4096;
  bytes.labels({"out"}) += 8192;
  subsystem.counter<std::uint64_t>("connect_seconds_total")     += 60;
  subsystem.counter<std::uint64_t>("timeouts_total")            += 2;
  subsystem.counter<std::uint64_t>("stalls_total")              += 1;
  subsystem.counter<std::uint64_t>("sendfile_interrupts_total") += 5;

  MetricSnapshot snap = snapshotOf(collector);
  char buff[512];
  std::string xml(buff, XrdStatsLegacy::Link(snap, buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"link\"><num>3</num><maxn>7</maxn><tot>100</tot>"
      "<in>4096</in><out>8192</out><ctime>60</ctime><tmo>2</tmo>"
      "<stall>1</stall><sfps>5</sfps></stats>");
}

TEST(XrdStatsLegacy, PollBlockFromRegistry)
{
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("poll");
  subsystem.gauge<std::int64_t>("attached")        = 10;
  subsystem.gauge<std::int64_t>("enabled")         = 8;
  subsystem.counter<std::uint64_t>("events_total")     += 200;
  subsystem.counter<std::uint64_t>("interrupts_total") += 4;

  MetricSnapshot snap = snapshotOf(collector);
  char buff[256];
  std::string xml(buff, XrdStatsLegacy::Poll(snap, buff, sizeof(buff)));
  EXPECT_EQ(xml,
      "<stats id=\"poll\"><att>10</att><en>8</en><ev>200</ev>"
      "<int>4</int></stats>");
}

TEST(XrdStatsLegacy, BuffBlockSplicesXlStats)
{
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("buff");
  subsystem.counter<std::uint64_t>("requests_total")    += 50;
  subsystem.gauge<std::int64_t>("memory_bytes")     = 1048576;
  subsystem.gauge<std::int64_t>("buffers")          = 12;
  subsystem.counter<std::uint64_t>("adjustments_total") += 3;

  MetricSnapshot snap = snapshotOf(collector);
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
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("");                 // empty subsystem -> flat names
  subsystem.counter<std::uint64_t>("requests_total") += 1;

  auto& ops = subsystem.counterFamily<std::uint64_t>("ops_total", {}, {"op"});
  ops.labels({"open"})    += 2;
  ops.labels({"refresh"}) += 3;
  ops.labels({"read"})    += 4;
  ops.labels({"preread"}) += 5;
  ops.labels({"readv"})   += 6;
  ops.labels({"writev"})  += 8;
  ops.labels({"write"})   += 10;
  ops.labels({"sync"})    += 11;
  ops.labels({"getfile"}) += 12;
  ops.labels({"putfile"}) += 13;
  ops.labels({"misc"})    += 14;
  subsystem.counter<std::uint64_t>("readv_segments_total")  += 7;
  subsystem.counter<std::uint64_t>("writev_segments_total") += 9;

  auto& sig = subsystem.counterFamily<std::uint64_t>("signatures_total", {}, {"result"});
  sig.labels({"ok"})      += 15;
  sig.labels({"bad"})     += 16;
  sig.labels({"ignored"}) += 17;

  subsystem.counter<std::uint64_t>("async_ops_total")      += 18;
  subsystem.gauge<std::int64_t>("async_max")           = 19;
  subsystem.counter<std::uint64_t>("async_rejected_total") += 20;
  subsystem.counter<std::uint64_t>("errors_total")         += 21;
  subsystem.counter<std::uint64_t>("redirects_total")      += 22;
  subsystem.counter<std::uint64_t>("stalls_total")         += 23;

  auto& lgn = subsystem.counterFamily<std::uint64_t>("logins_total", {}, {"result"});
  lgn.labels({"attempt"})  += 24;
  lgn.labels({"authfail"}) += 25;
  lgn.labels({"auth"})     += 26;
  lgn.labels({"noauth"})   += 27;

  MetricSnapshot snap = snapshotOf(collector);
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
  Collector collector("xrootd");
  auto& subsystem = collector.subsystem("ofs");
  auto& open = subsystem.gaugeFamily<std::int64_t>("files_open", {}, {"mode"});
  open.labels({"read"})  = 1;
  open.labels({"write"}) = 2;
  open.labels({"posc"})  = 3;
  subsystem.counter<std::uint64_t>("unpersisted_total") += 4;
  subsystem.gauge<std::int64_t>("handles")          = 5;
  subsystem.counter<std::uint64_t>("redirects_total")   += 6;
  subsystem.counter<std::uint64_t>("started_total")     += 7;
  subsystem.counter<std::uint64_t>("replies_total")     += 8;
  subsystem.counter<std::uint64_t>("errors_total")      += 9;
  subsystem.counter<std::uint64_t>("delays_total")      += 10;
  auto& ev = subsystem.counterFamily<std::uint64_t>("events_total", {}, {"result"});
  ev.labels({"ok"})    += 11;
  ev.labels({"error"}) += 12;
  auto& tpc = subsystem.counterFamily<std::uint64_t>("tpc_total", {}, {"result"});
  tpc.labels({"granted"}) += 13;
  tpc.labels({"denied"})  += 14;
  tpc.labels({"error"})   += 15;
  tpc.labels({"expired"}) += 16;

  MetricSnapshot snap = snapshotOf(collector);
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
