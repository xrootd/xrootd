//------------------------------------------------------------------------------
// Golden-output regression test for the OFS summary statistics XML.
//
// XrdOfsStats now owns native XrdMetrics instruments (atomic counters/gauges) in
// the process-wide registry and reproduces the legacy <stats id="ofs"> wire
// format from a registry snapshot. This sets each field to a distinct value (in
// the order Report() emits them; counters via +=, gauges via =) and pins the
// exact XML, exercising the full register -> snapshot -> render round-trip.
//------------------------------------------------------------------------------

#include <string>

#include "XrdOfs/XrdOfsStats.hh"

#include <gtest/gtest.h>

TEST(XrdOfsStats, SummaryXmlIsByteStable)
{
  XrdOfsStats stats;
  stats.setRole("server");

  stats.Data.numOpenR    = 1;    // gauges: operator=
  stats.Data.numOpenW    = 2;
  stats.Data.numOpenP    = 3;
  stats.Data.numUnpsist  += 4;   // counters: operator+= (start at 0)
  stats.Data.numHandles  = 5;
  stats.Data.numRedirect += 6;
  stats.Data.numStarted  += 7;
  stats.Data.numReplies  += 8;
  stats.Data.numErrors   += 9;
  stats.Data.numDelays   += 10;
  stats.Data.numSeventOK += 11;
  stats.Data.numSeventER += 12;
  stats.Data.numTPCgrant += 13;
  stats.Data.numTPCdeny  += 14;
  stats.Data.numTPCerrs  += 15;
  stats.Data.numTPCexpr  += 16;

  char buff[1024];
  int len = stats.Report(buff, sizeof(buff));

  EXPECT_EQ(std::string(buff, len),
      "<stats id=\"ofs\"><role>server</role>"
      "<opr>1</opr><opw>2</opw><opp>3</opp><ups>4</ups><han>5</han>"
      "<rdr>6</rdr><bxq>7</bxq><rep>8</rep><err>9</err><dly>10</dly>"
      "<sok>11</sok><ser>12</ser>"
      "<tpc><grnt>13</grnt><deny>14</deny><err>15</err><exp>16</exp></tpc>"
      "</stats>");
}
