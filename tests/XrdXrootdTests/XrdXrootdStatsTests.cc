//------------------------------------------------------------------------------
// Golden-output regression test for the xrootd summary statistics XML.
//
// The Prometheus migration exposes these counters through the XrdMetrics
// registry but must leave the legacy <stats id="xrootd"> wire format
// byte-for-byte unchanged (xrd.report consumers depend on it). This test sets
// each counter to a distinct value (in the order Stats() emits them) and pins
// the exact XML string.
//------------------------------------------------------------------------------

#include "XrdXrootd/XrdXrootdStats.hh"

#include <gtest/gtest.h>

TEST(XrdXrootdStats, SummaryXmlIsByteStable)
{
  XrdXrootdStats stats(nullptr);

  stats.Count    = 1;
  stats.openCnt  = 2;
  stats.Refresh  = 3;
  stats.readCnt  = 4;
  stats.prerCnt  = 5;
  stats.rvecCnt  = 6;
  stats.rsegCnt  = 7;
  stats.wvecCnt  = 8;
  stats.wsegCnt  = 9;
  stats.writeCnt = 10;
  stats.syncCnt  = 11;
  stats.getfCnt  = 12;
  stats.putfCnt  = 13;
  stats.miscCnt  = 14;
  stats.aokSCnt  = 15;
  stats.badSCnt  = 16;
  stats.ignSCnt  = 17;
  stats.AsyncNum = 18;
  stats.AsyncMax = 19;
  stats.AsyncRej = 20;
  stats.errorCnt = 21;
  stats.redirCnt = 22;
  stats.stallCnt = 23;
  stats.LoginAT  = 24;
  stats.AuthBad  = 25;
  stats.LoginAU  = 26;
  stats.LoginUA  = 27;

  char buff[4096];
  int len = stats.Stats(buff, sizeof(buff));

  const char *expected =
    "<stats id=\"xrootd\"><num>1</num>"
    "<ops><open>2</open><rf>3</rf><rd>4</rd><pr>5</pr>"
    "<rv>6</rv><rs>7</rs>"
    "<wv>8</wv><ws>9</ws><wr>10</wr>"
    "<sync>11</sync><getf>12</getf><putf>13</putf><misc>14</misc></ops>"
    "<sig><ok>15</ok><bad>16</bad><ign>17</ign></sig>"
    "<aio><num>18</num><max>19</max><rej>20</rej></aio>"
    "<err>21</err><rdr>22</rdr><dly>23</dly>"
    "<lgn><num>24</num><af>25</af><au>26</au><ua>27</ua></lgn></stats>";

  EXPECT_EQ(std::string(buff, len), expected);
}
